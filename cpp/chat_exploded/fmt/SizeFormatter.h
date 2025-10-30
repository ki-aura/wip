#pragma once
#include <string>

class SizeFormatter {
public:
    std::string format(off_t bytes) const;
};
