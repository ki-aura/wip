#include "TimeFormatter.h"
#include <ctime>

std::string TimeFormatter::format(time_t mtime) const {
    time_t now = time(nullptr);
    struct tm* tm_info = localtime(&mtime);
    if (!tm_info) return "??? ?? ??:??";

    double diff = difftime(now, mtime);
    char buf[64];
    if (diff > 15778800 || diff < 0)
        strftime(buf, sizeof(buf), "%b %e  %Y", tm_info);
    else
        strftime(buf, sizeof(buf), "%b %e %H:%M", tm_info);
    return buf;
}
