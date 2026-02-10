// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Eser KUBALI

#include "filter/parser.h"
#include "filter/condition.h"
#include "utils/core.h"
#include "utils/string.h"
#include <cctype>
#include <string_view>
#include <vector>

namespace filter {

namespace {

enum class TokenKind {
    Identifier,
    StringLiteral,
    Operator,
    LParen,
    RParen,
    Comma,
    End
};

struct Token {
    TokenKind kind{};
    std::string text;
    size_t pos = 0;
};

class QueryTokenizer {
public:
    explicit QueryTokenizer(std::string_view input) : input_(input) {}

    ErrorCode::Result<std::vector<Token>> tokenize() {
        std::vector<Token> tokens;
        while (pos_ < input_.size()) {
            const char ch = input_[pos_];
            if (std::isspace(static_cast<unsigned char>(ch))) {
                ++pos_;
                continue;
            }

            if (ch == '(') {
                tokens.push_back({TokenKind::LParen, "(", pos_++});
                continue;
            }
            if (ch == ')') {
                tokens.push_back({TokenKind::RParen, ")", pos_++});
                continue;
            }
            if (ch == ',') {
                tokens.push_back({TokenKind::Comma, ",", pos_++});
                continue;
            }

            if (ch == '\'' || ch == '"') {
                auto stringToken = readStringLiteral();
                if (!stringToken) {
                    return std::unexpected(stringToken.error());
                }
                tokens.push_back(std::move(*stringToken));
                continue;
            }

            if (ch == '=' || ch == '!' || ch == '<' || ch == '>') {
                tokens.push_back(readOperator());
                continue;
            }

            auto idToken = readIdentifier();
            if (!idToken) {
                return std::unexpected(idToken.error());
            }
            tokens.push_back(std::move(*idToken));
        }

        tokens.push_back({TokenKind::End, "", pos_});
        return tokens;
    }

private:
    ErrorCode::Result<Token> readStringLiteral() {
        const char quote = input_[pos_];
        const size_t start = pos_;
        ++pos_;

        std::string value;
        while (pos_ < input_.size()) {
            const char ch = input_[pos_++];
            if (ch == '\\' && pos_ < input_.size()) {
                value.push_back(input_[pos_++]);
                continue;
            }
            if (ch == quote) {
                return Token{TokenKind::StringLiteral, std::move(value), start};
            }
            value.push_back(ch);
        }

        return std::unexpected(ErrorCode::Error(Code::InvalidArgument, "Unterminated string literal in query."));
    }

    Token readOperator() {
        const size_t start = pos_;
        const char first = input_[pos_++];
        if (pos_ < input_.size()) {
            const char second = input_[pos_];
            if ((first == '!' && second == '=') ||
                (first == '<' && second == '=') ||
                (first == '>' && second == '=') ||
                (first == '=' && second == '=')) {
                ++pos_;
                return Token{TokenKind::Operator, std::string{first, second}, start};
            }
        }
        return Token{TokenKind::Operator, std::string(1, first), start};
    }

    ErrorCode::Result<Token> readIdentifier() {
        const size_t start = pos_;
        std::string value;
        while (pos_ < input_.size()) {
            const char ch = input_[pos_];
            if (std::isspace(static_cast<unsigned char>(ch)) || ch == '(' || ch == ')' || ch == ',' ||
                ch == '\'' || ch == '"' || ch == '=' || ch == '!' || ch == '<' || ch == '>') {
                break;
            }
            value.push_back(ch);
            ++pos_;
        }
        if (value.empty()) {
            return std::unexpected(ErrorCode::Error(Code::InvalidArgument, "Unexpected token in query."));
        }
        return Token{TokenKind::Identifier, std::move(value), start};
    }

    std::string_view input_;
    size_t pos_ = 0;
};

class QueryParser {
public:
    explicit QueryParser(const std::vector<Token>& tokens) : tokens_(tokens) {}

    ErrorCode::Result<FilterExpression> parse() {
        auto expr = parseOrExpression();
        if (!expr) {
            return std::unexpected(expr.error());
        }
        if (current().kind != TokenKind::End) {
            return std::unexpected(ErrorCode::Error(Code::InvalidArgument, "Unexpected trailing tokens in query."));
        }
        return expr;
    }

private:
    ErrorCode::Result<FilterExpression> parseOrExpression() {
        auto left = parseAndExpression();
        if (!left) return std::unexpected(left.error());

        while (matchOrOperator()) {
            auto right = parseAndExpression();
            if (!right) return std::unexpected(right.error());
            left = FilterExpression::makeOr({*left, *right});
        }
        return left;
    }

    ErrorCode::Result<FilterExpression> parseAndExpression() {
        auto left = parseUnaryExpression();
        if (!left) return std::unexpected(left.error());

        while (matchAndOperator()) {
            auto right = parseUnaryExpression();
            if (!right) return std::unexpected(right.error());
            left = FilterExpression::makeAnd({*left, *right});
        }
        return left;
    }

    ErrorCode::Result<FilterExpression> parseUnaryExpression() {
        if (matchNotOperator()) {
            auto expr = parseUnaryExpression();
            if (!expr) return std::unexpected(expr.error());
            return FilterExpression::makeNot(*expr);
        }
        return parsePrimaryExpression();
    }

    ErrorCode::Result<FilterExpression> parsePrimaryExpression() {
        if (match(TokenKind::LParen)) {
            auto expr = parseOrExpression();
            if (!expr) return std::unexpected(expr.error());
            if (!match(TokenKind::RParen)) {
                return std::unexpected(ErrorCode::Error(Code::InvalidArgument, "Missing closing parenthesis in query."));
            }
            return expr;
        }
        return parseConditionExpression();
    }

    ErrorCode::Result<FilterExpression> parseConditionExpression() {
        const auto fieldRes = consumeIdentifier("Expected field name in condition.");
        if (!fieldRes) return std::unexpected(fieldRes.error());
        const std::string fieldToken = *fieldRes;

        auto opRes = parseOperator();
        if (!opRes) return std::unexpected(opRes.error());
        const auto op = *opRes;

        const auto [field, customField] = resolveField(fieldToken);

        if (isUnaryOperator(op)) {
            auto condRes = buildCondition(field, customField, op, "");
            if (!condRes) return std::unexpected(condRes.error());
            return FilterExpression::makeCondition(*condRes);
        }

        if (op == FilterOperator::IN || op == FilterOperator::NOT_IN) {
            auto setValuesRes = parseSetValues();
            if (!setValuesRes) return std::unexpected(setValuesRes.error());
            auto setCondRes = buildSetCondition(field, customField, op, *setValuesRes);
            if (!setCondRes) return std::unexpected(setCondRes.error());
            return FilterExpression::makeCondition(*setCondRes);
        }

        auto valueRes = consumeValue("Expected value in condition.");
        if (!valueRes) return std::unexpected(valueRes.error());

        auto condRes = buildCondition(field, customField, op, *valueRes);
        if (!condRes) return std::unexpected(condRes.error());
        return FilterExpression::makeCondition(*condRes);
    }

    ErrorCode::Result<std::vector<std::string>> parseSetValues() {
        if (!match(TokenKind::LParen)) {
            return std::unexpected(ErrorCode::Error(Code::InvalidArgument, "IN/NOT_IN expects values inside parentheses."));
        }

        std::vector<std::string> values;
        while (true) {
            auto valueRes = consumeValue("Expected value in IN/NOT_IN list.");
            if (!valueRes) return std::unexpected(valueRes.error());
            values.push_back(*valueRes);

            if (match(TokenKind::Comma)) {
                continue;
            }
            break;
        }

        if (!match(TokenKind::RParen)) {
            return std::unexpected(ErrorCode::Error(Code::InvalidArgument, "Missing closing ')' for IN/NOT_IN list."));
        }

        return values;
    }

    ErrorCode::Result<FilterOperator> parseOperator() {
        if (current().kind == TokenKind::Operator) {
            const std::string symbol = consume().text;
            if (symbol == "=" || symbol == "==") return FilterOperator::EQUALS;
            if (symbol == "!=") return FilterOperator::NOT_EQUALS;
            if (symbol == ">") return FilterOperator::GREATER_THAN;
            if (symbol == "<") return FilterOperator::LESS_THAN;
            if (symbol == ">=") return FilterOperator::GREATER_THAN_OR_EQUAL;
            if (symbol == "<=") return FilterOperator::LESS_THAN_OR_EQUAL;
            return std::unexpected(ErrorCode::Error(Code::InvalidArgument, "Unsupported operator: " + symbol));
        }

        if (current().kind != TokenKind::Identifier) {
            return std::unexpected(ErrorCode::Error(Code::InvalidArgument, "Expected operator in condition."));
        }

        const std::string opToken = Utils::toUpper(consume().text);
        if (opToken == "IN") return FilterOperator::IN;
        if (opToken == "NOT_IN") return FilterOperator::NOT_IN;
        if (opToken == "CONTAINS" || opToken == "CT") return FilterOperator::CONTAINS;
        if (opToken == "CONTAINS_I" || opToken == "CTI") return FilterOperator::CONTAINS_I;
        if (opToken == "NOT_CONTAINS" || opToken == "NCT") return FilterOperator::NOT_CONTAINS;
        if (opToken == "NOT_CONTAINS_I" || opToken == "NCTI") return FilterOperator::NOT_CONTAINS_I;
        if (opToken == "STARTS_WITH" || opToken == "SW") return FilterOperator::STARTS_WITH;
        if (opToken == "STARTS_WITH_I" || opToken == "SWI") return FilterOperator::STARTS_WITH_I;
        if (opToken == "ENDS_WITH" || opToken == "EW") return FilterOperator::ENDS_WITH;
        if (opToken == "ENDS_WITH_I" || opToken == "EWI") return FilterOperator::ENDS_WITH_I;
        if (opToken == "REGEX" || opToken == "REGEX_MATCH" || opToken == "RX") return FilterOperator::REGEX;
        if (opToken == "EQUALS_I") return FilterOperator::EQUALS_I;
        if (opToken == "NOT_EQUALS_I") return FilterOperator::NOT_EQUALS_I;
        if (opToken == "IS_NULL" || opToken == "NULL") return FilterOperator::IS_NULL;
        if (opToken == "IS_NOT_NULL" || opToken == "NOT_NULL" || opToken == "NON_NULL") return FilterOperator::IS_NOT_NULL;
        if (opToken == "IS_PRESENT" || opToken == "PRESENT" || opToken == "EXISTS") return FilterOperator::IS_PRESENT;
        if (opToken == "IS_ABSENT" || opToken == "ABSENT" || opToken == "MISSING") return FilterOperator::IS_ABSENT;
        if (opToken == "NOT") {
            auto nextOpRes = consumeIdentifier("Expected operator after NOT.");
            if (!nextOpRes) return std::unexpected(nextOpRes.error());
            const std::string nextOp = Utils::toUpper(*nextOpRes);
            if (nextOp == "IN") return FilterOperator::NOT_IN;
            if (nextOp == "CONTAINS") return FilterOperator::NOT_CONTAINS;
            if (nextOp == "EQUALS") return FilterOperator::NOT_EQUALS;
            return std::unexpected(ErrorCode::Error(Code::InvalidArgument, "Unsupported NOT operator: " + nextOp));
        }
        if (opToken == "IS") {
            auto nextOpRes = consumeIdentifier("Expected IS operator qualifier.");
            if (!nextOpRes) return std::unexpected(nextOpRes.error());
            const std::string nextOp = Utils::toUpper(*nextOpRes);
            if (nextOp == "NULL") return FilterOperator::IS_NULL;
            if (nextOp == "PRESENT") return FilterOperator::IS_PRESENT;
            if (nextOp == "ABSENT") return FilterOperator::IS_ABSENT;
            if (nextOp == "NOT") {
                auto finalOpRes = consumeIdentifier("Expected token after IS NOT.");
                if (!finalOpRes) return std::unexpected(finalOpRes.error());
                const std::string finalOp = Utils::toUpper(*finalOpRes);
                if (finalOp == "NULL") return FilterOperator::IS_NOT_NULL;
                return std::unexpected(ErrorCode::Error(Code::InvalidArgument, "Unsupported IS NOT operator: " + finalOp));
            }
            return std::unexpected(ErrorCode::Error(Code::InvalidArgument, "Unsupported IS operator: " + nextOp));
        }
        if (opToken == "EQUALS" || opToken == "EQ") return FilterOperator::EQUALS;
        if (opToken == "NOT_EQUALS" || opToken == "NE") return FilterOperator::NOT_EQUALS;
        if (opToken == "GREATER_THAN" || opToken == "GT") return FilterOperator::GREATER_THAN;
        if (opToken == "LESS_THAN" || opToken == "LT") return FilterOperator::LESS_THAN;
        if (opToken == "GREATER_THAN_OR_EQUAL" || opToken == "GREATER_THAN_OR_EQUALS" || opToken == "GTE") return FilterOperator::GREATER_THAN_OR_EQUAL;
        if (opToken == "LESS_THAN_OR_EQUAL" || opToken == "LESS_THAN_OR_EQUALS" || opToken == "LTE") return FilterOperator::LESS_THAN_OR_EQUAL;

        return std::unexpected(ErrorCode::Error(Code::InvalidArgument, "Unsupported operator token: " + opToken));
    }

    static bool isUnaryOperator(FilterOperator op) {
        return op == FilterOperator::IS_NULL || op == FilterOperator::IS_NOT_NULL ||
               op == FilterOperator::IS_PRESENT || op == FilterOperator::IS_ABSENT;
    }

    static bool isRelationalOperator(FilterOperator op) {
        return op == FilterOperator::EQUALS || op == FilterOperator::NOT_EQUALS ||
               op == FilterOperator::GREATER_THAN || op == FilterOperator::LESS_THAN ||
               op == FilterOperator::GREATER_THAN_OR_EQUAL || op == FilterOperator::LESS_THAN_OR_EQUAL;
    }

    static std::pair<LogEntryField, std::optional<std::string>> resolveField(const std::string& token) {
        const std::string upper = Utils::toUpper(token);
        if (upper.rfind("CUSTOM.", 0) == 0 && token.size() > 7) {
            return {LogEntryField::CUSTOM, token.substr(7)};
        }

        const auto field = Utils::stringToLogEntryField(token);
        if (field != LogEntryField::UNKNOWN) {
            return {field, std::nullopt};
        }

        // Treat unknown field names as custom fields.
        return {LogEntryField::CUSTOM, token};
    }

    static ErrorCode::Result<FilterCondition> buildSetCondition(
        LogEntryField field,
        const std::optional<std::string>& customField,
        FilterOperator op,
        std::vector<std::string> values) {
        if (field == LogEntryField::CUSTOM) {
            return FilterCondition::createCustomSet(*customField, op, std::move(values));
        }
        return FilterCondition::createSet(field, op, std::move(values));
    }

    static ErrorCode::Result<FilterCondition> buildCondition(
        LogEntryField field,
        const std::optional<std::string>& customField,
        FilterOperator op,
        std::string value) {
        FilterCondition cond;
        cond.field = field;
        cond.op = op;
        cond.value = std::move(value);
        cond.caseSensitive = false;
        cond.customField = customField;

        if (field == LogEntryField::LEVEL && isRelationalOperator(op)) {
            cond.valueType = FilterValueType::LOG_LEVEL;
        } else if (field == LogEntryField::TIMESTAMP && isRelationalOperator(op)) {
            cond.valueType = FilterValueType::DATETIME;
        } else if (op == FilterOperator::CONTAINS || op == FilterOperator::NOT_CONTAINS ||
                   op == FilterOperator::STARTS_WITH || op == FilterOperator::ENDS_WITH ||
                   op == FilterOperator::REGEX || op == FilterOperator::CONTAINS_I ||
                   op == FilterOperator::NOT_CONTAINS_I || op == FilterOperator::STARTS_WITH_I ||
                   op == FilterOperator::ENDS_WITH_I || op == FilterOperator::EQUALS_I ||
                   op == FilterOperator::NOT_EQUALS_I) {
            cond.valueType = FilterValueType::STRING;
        } else {
            cond.valueType = FilterValueType::AUTO;
        }

        return cond;
    }

    bool match(TokenKind kind) {
        if (current().kind == kind) {
            ++idx_;
            return true;
        }
        return false;
    }

    bool matchKeyword(const std::string& keyword) {
        if (current().kind != TokenKind::Identifier) {
            return false;
        }
        if (Utils::toUpper(current().text) != keyword) {
            return false;
        }
        ++idx_;
        return true;
    }

    bool matchAndOperator() {
        return matchKeyword("AND") || matchLogicalSymbol("&&");
    }

    bool matchOrOperator() {
        return matchKeyword("OR") || matchLogicalSymbol("||");
    }

    bool matchNotOperator() {
        return matchKeyword("NOT") || matchLogicalSymbol("!");
    }

    bool matchLogicalSymbol(const char* symbol) {
        if (current().kind != TokenKind::Operator && current().kind != TokenKind::Identifier) {
            return false;
        }
        if (current().text != symbol) {
            return false;
        }
        ++idx_;
        return true;
    }

    ErrorCode::Result<std::string> consumeIdentifier(const std::string& errMsg) {
        if (current().kind != TokenKind::Identifier) {
            return std::unexpected(ErrorCode::Error(Code::InvalidArgument, errMsg));
        }
        return consume().text;
    }

    ErrorCode::Result<std::string> consumeValue(const std::string& errMsg) {
        if (current().kind != TokenKind::Identifier && current().kind != TokenKind::StringLiteral) {
            return std::unexpected(ErrorCode::Error(Code::InvalidArgument, errMsg));
        }
        return consume().text;
    }

    const Token& current() const {
        return tokens_[idx_];
    }

    const Token& consume() {
        return tokens_[idx_++];
    }

    const std::vector<Token>& tokens_;
    size_t idx_ = 0;
};

} // namespace

ErrorCode::Result<FilterExpression> parseQuery(const std::string& query) {
    QueryTokenizer tokenizer(query);
    auto tokensRes = tokenizer.tokenize();
    if (!tokensRes) {
        return std::unexpected(tokensRes.error());
    }
    QueryParser parser(*tokensRes);
    return parser.parse();
}

} // namespace filter
