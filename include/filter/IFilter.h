#ifndef IFILTER_H
#define IFILTER_H

#include "core/LogTypes.h"

class IFilter {
public:
    virtual ~IFilter() = default;
    virtual bool matches(const LogEntry &entry) const = 0;
};

#endif // IFILTER_H
