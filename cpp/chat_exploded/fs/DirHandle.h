#pragma once
#include <dirent.h>

class DirHandle {
public:
    explicit DirHandle(DIR* d) : dir_(d) {}
    ~DirHandle() { if (dir_) closedir(dir_); }
    DirHandle(const DirHandle&) = delete;
    DirHandle& operator=(const DirHandle&) = delete;
    DirHandle(DirHandle&& other) noexcept : dir_(other.dir_) { other.dir_ = nullptr; }

    DIR* get() const { return dir_; }
    explicit operator bool() const { return dir_ != nullptr; }

private:
    DIR* dir_{};
};
