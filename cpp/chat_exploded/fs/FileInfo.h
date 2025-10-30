#pragma once
#include <string>
#include <sys/stat.h>
#include <optional>

struct FileInfo {
    std::string dir;
    std::string name;
    struct stat st{};
    std::optional<std::string> link_target;

    FileInfo(std::string path, const struct stat& s)
        : dir("."), name(std::move(path)), st(s) {}

    FileInfo(std::string d, std::string n, const struct stat& s)
        : dir(std::move(d)), name(std::move(n)), st(s) {}
};
