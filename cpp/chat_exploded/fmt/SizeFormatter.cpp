#include "SizeFormatter.h"
#include <cstdio>

std::string SizeFormatter::format(off_t bytes) const {
    const char* units[] = {"B", "K", "M", "G", "T"};
    double size = static_cast<double>(bytes);
    int u = 0;
    while (size >= 1024.0 && u < 4) { size /= 1024.0; ++u; }

    char buf[32];
    if (u == 0) snprintf(buf, sizeof(buf), "%.0f%s", size, units[u]);
    else snprintf(buf, sizeof(buf), "%.1f%s", size, units[u]);
    return buf;
}
