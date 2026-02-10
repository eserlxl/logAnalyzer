// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Eser KUBALI

#include "utils.h"

using namespace filter;

// --- IP Address Filtering Tests ---

TEST_F(FilterTestFixture, EvaluateIpAddressComparison) {
    auto entry_ip1 = createLogEntry(LogLevel::INFO, "IP address 1", "ip.log", {{"client_ip", "192.168.1.100"}});
    auto entry_ip2 = createLogEntry(LogLevel::INFO, "IP address 2", "ip.log", {{"client_ip", "192.168.1.200"}});
    auto entry_ip_ipv6 = createLogEntry(LogLevel::INFO, "IP address IPv6", "ip.log", {{"client_ip", "2001:0db8:85a3:0000:0000:8a2e:0370:7334"}});
    auto entry_ip_invalid = createLogEntry(LogLevel::INFO, "Invalid IP", "ip.log", {{"client_ip", "invalid-ip"}});

    // EQUALS
    EXPECT_TRUE(createExpr(LogEntryField::CUSTOM, FilterOperator::EQUALS, "192.168.1.100", FilterValueType::IP_ADDRESS, true, "client_ip").evaluate(entry_ip1).value_or(false));
    EXPECT_FALSE(createExpr(LogEntryField::CUSTOM, FilterOperator::EQUALS, "192.168.1.200", FilterValueType::IP_ADDRESS, true, "client_ip").evaluate(entry_ip1).value_or(true));

    // GREATER_THAN
    EXPECT_TRUE(createExpr(LogEntryField::CUSTOM, FilterOperator::GREATER_THAN, "192.168.1.100", FilterValueType::IP_ADDRESS, true, "client_ip").evaluate(entry_ip2).value_or(false));
    EXPECT_FALSE(createExpr(LogEntryField::CUSTOM, FilterOperator::GREATER_THAN, "192.168.1.200", FilterValueType::IP_ADDRESS, true, "client_ip").evaluate(entry_ip2).value_or(true));

    // LESS_THAN_OR_EQUAL
    EXPECT_TRUE(createExpr(LogEntryField::CUSTOM, FilterOperator::LESS_THAN_OR_EQUAL, "192.168.1.200", FilterValueType::IP_ADDRESS, true, "client_ip").evaluate(entry_ip2).value_or(false));
    EXPECT_TRUE(createExpr(LogEntryField::CUSTOM, FilterOperator::LESS_THAN_OR_EQUAL, "192.168.1.200", FilterValueType::IP_ADDRESS, true, "client_ip").evaluate(entry_ip1).value_or(false));
    EXPECT_FALSE(createExpr(LogEntryField::CUSTOM, FilterOperator::LESS_THAN_OR_EQUAL, "192.168.1.100", FilterValueType::IP_ADDRESS, true, "client_ip").evaluate(entry_ip2).value_or(true));

    // Test with IPv6 address
    EXPECT_TRUE(createExpr(LogEntryField::CUSTOM, FilterOperator::EQUALS, "2001:0db8:85a3:0000:0000:8a2e:0370:7334", FilterValueType::IP_ADDRESS, true, "client_ip").evaluate(entry_ip_ipv6).value_or(false));

    // Test with invalid IP strings (should fail comparison)
    ASSERT_FALSE(createExpr(LogEntryField::CUSTOM, FilterOperator::EQUALS, "192.168.1.100", FilterValueType::IP_ADDRESS, true, "client_ip").evaluate(entry_ip_invalid).has_value());
    ASSERT_FALSE(createExpr(LogEntryField::CUSTOM, FilterOperator::GREATER_THAN, "invalid-ip", FilterValueType::IP_ADDRESS, true, "client_ip").evaluate(entry_ip1).has_value());
}

TEST_F(FilterTestFixture, EvaluateIpAddressEquals) {
    testIpComparison("192.168.1.1", "192.168.1.1", FilterOperator::EQUALS, true);
    testIpComparison("192.168.1.1", "192.168.1.2", FilterOperator::EQUALS, false);
    testIpComparison("::1", "::1", FilterOperator::EQUALS, true);
    testIpComparison("::1", "::2", FilterOperator::EQUALS, false);
    testIpComparison("192.168.1.1", "::1", FilterOperator::EQUALS, false); // Different families
}

TEST_F(FilterTestFixture, EvaluateIpAddressNotEquals) {
    testIpComparison("192.168.1.1", "192.168.1.2", FilterOperator::NOT_EQUALS, true);
    testIpComparison("192.168.1.1", "192.168.1.1", FilterOperator::NOT_EQUALS, false);
}

TEST_F(FilterTestFixture, EvaluateIpAddressGreaterThan) {
    testIpComparison("192.168.1.2", "192.168.1.1", FilterOperator::GREATER_THAN, true);
    testIpComparison("10.0.0.1", "192.168.1.1", FilterOperator::GREATER_THAN, false); // 10.0.0.1 < 192.168.1.1
    testIpComparison("::2", "::1", FilterOperator::GREATER_THAN, true);
    testIpComparison("2001:0db8::1", "::1", FilterOperator::GREATER_THAN, true);
    testIpComparison("192.168.1.1", "::1", FilterOperator::GREATER_THAN, false); // IPv4 less than IPv6 by arbitrary rule
}

TEST_F(FilterTestFixture, EvaluateIpAddressLessThan) {
    testIpComparison("192.168.1.1", "192.168.1.2", FilterOperator::LESS_THAN, true);
    testIpComparison("::1", "::2", FilterOperator::LESS_THAN, true);
    
    // Test comparison between IPv4 and IPv6
    // The implementation considers IPv4 to be less than IPv6.
    LogEntry entry_ipv6 = createLogEntry(LogLevel::INFO, "msg", "ip.log", {{"client_ip", "::1"}});
    FilterExpression expr_v6_lt_v4 = FilterExpression::create(createCondition(LogEntryField::CUSTOM, FilterOperator::LESS_THAN, "192.168.1.1", FilterValueType::IP_ADDRESS, true, "client_ip"));
    auto res1 = expr_v6_lt_v4.evaluate(entry_ipv6);
    ASSERT_TRUE(res1.has_value());
    EXPECT_FALSE(*res1); // ::1 (IPv6) is NOT less than 192.168.1.1 (IPv4)

    LogEntry entry_ipv4 = createLogEntry(LogLevel::INFO, "msg", "ip.log", {{"client_ip", "192.168.1.1"}});
    FilterExpression expr_v4_lt_v6 = FilterExpression::create(createCondition(LogEntryField::CUSTOM, FilterOperator::LESS_THAN, "::1", FilterValueType::IP_ADDRESS, true, "client_ip"));
    auto res2 = expr_v4_lt_v6.evaluate(entry_ipv4);
    ASSERT_TRUE(res2.has_value());
    EXPECT_TRUE(*res2); // 192.168.1.1 (IPv4) IS less than ::1 (IPv6)
}

TEST_F(FilterTestFixture, EvaluateIpAddressInvalidInput) {
    LogEntry entry = createLogEntry(LogLevel::INFO, "IP test", "ip.log", {{"ip_field", "invalid-ip"}});
    FilterCondition cond = createCondition(LogEntryField::CUSTOM, FilterOperator::EQUALS, "192.168.1.1", FilterValueType::IP_ADDRESS, true, "ip_field");
    FilterExpression expr = FilterExpression::create(cond);
    EXPECT_FALSE(expr.evaluate(entry).has_value());

    LogEntry entry2 = createLogEntry(LogLevel::INFO, "IP test", "ip.log", {{"ip_field", "192.168.1.1"}});
    cond = createCondition(LogEntryField::CUSTOM, FilterOperator::EQUALS, "invalid-ip", FilterValueType::IP_ADDRESS, true, "ip_field");
    expr = FilterExpression::create(cond);
    EXPECT_FALSE(expr.evaluate(entry2).has_value());
}
