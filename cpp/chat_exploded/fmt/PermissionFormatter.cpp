#include "PermissionFormatter.h"

std::string PermissionFormatter::format(mode_t mode) const {
    std::string p(10, '-');
    p[0] = S_ISDIR(mode) ? 'd' : S_ISLNK(mode) ? 'l' : '-';
    if (mode & S_IRUSR) p[1] = 'r';
    if (mode & S_IWUSR) p[2] = 'w';
    if (mode & S_IXUSR) p[3] = 'x';
    if (mode & S_IRGRP) p[4] = 'r';
    if (mode & S_IWGRP) p[5] = 'w';
    if (mode & S_IXGRP) p[6] = 'x';
    if (mode & S_IROTH) p[7] = 'r';
    if (mode & S_IWOTH) p[8] = 'w';
    if (mode & S_IXOTH) p[9] = 'x';
    return p;
}
