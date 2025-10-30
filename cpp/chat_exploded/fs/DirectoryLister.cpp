#include "DirectoryLister.h"
#include <sys/stat.h>
#include <unistd.h>
#include <cerrno>
#include <cstdio>
#include <cstring>

std::vector<FileInfo> DirectoryLister::list(const std::string& path) {
    std::vector<FileInfo> result;
    DIR* d = opendir(path.c_str());
    if (!d) {
        perror(path.c_str());
        return result;
    }

    DirHandle dir(d);
    struct dirent* entry;

    while ((entry = readdir(dir.get())) != nullptr) {
        std::string full = path + "/" + entry->d_name;
        struct stat st{};
        if (lstat(full.c_str(), &st) == -1)
            continue;

        FileInfo fi(path, entry->d_name, st);

        if (S_ISLNK(st.st_mode)) {
            char buf[PATH_MAX];
            ssize_t len = readlink(full.c_str(), buf, sizeof(buf) - 1);
            if (len != -1) {
                buf[len] = '\0';
                fi.link_target = std::string(buf);
            }
        }

        result.push_back(std::move(fi));
    }

    return result;
}
