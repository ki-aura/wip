#pragma once
#include <string>
#include <sys/stat.h>

class PermissionFormatter {
public:
    std::string format(mode_t mode) const;
};
