#include "utils/Time.h" // Include the new header
#include <sstream>
#include <iomanip>
#include <regex>
#include <mutex>
#include <iostream>
#include <stdlib.h> // for setenv, getenv, unsetenv
#include <string.h> // for strdup, free
#include <errno.h>  // for errno

namespace Utils {

static std::mutex localtimeMutex;

// Helper to validate if a date is valid (not normalized by mktime)
bool isTmValid(const std::tm& tm_orig, const std::tm& tm_new) {
    // Check if mktime/timegm normalized any fields
    return tm_new.tm_year == tm_orig.tm_year &&
           tm_new.tm_mon == tm_orig.tm_mon &&
           tm_new.tm_mday == tm_orig.tm_mday &&
           tm_new.tm_hour == tm_orig.tm_hour &&
           tm_new.tm_min == tm_orig.tm_min &&
           tm_new.tm_sec == tm_orig.tm_sec;
}

// Portable timegm implementation
time_t portable_timegm(struct tm *tm) {
    // Save original TZ
    char* original_tz = getenv("TZ");
    char* original_tz_copy = nullptr;
    if (original_tz) {
        original_tz_copy = strdup(original_tz);
        if (!original_tz_copy) {
            // Handle memory allocation failure
            return -1; 
        }
    }

    // Set TZ to UTC
    setenv("TZ", "UTC", 1);
    tzset();

    // Call mktime
    time_t ret = mktime(tm);

    // Restore original TZ
    if (original_tz_copy) {
        setenv("TZ", original_tz_copy, 1);
        free(original_tz_copy);
    } else {
        unsetenv("TZ");
    }
    tzset();

    return ret;
}
