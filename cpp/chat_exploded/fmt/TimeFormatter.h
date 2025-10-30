#pragma once
#include <string>
#include <ctime>

class TimeFormatter {
public:
    std::string format(time_t mtime) const;
};
