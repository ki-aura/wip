#pragma once
#include "FileInfo.h"
#include "DirHandle.h"
#include <vector>

class DirectoryLister {
public:
    std::vector<FileInfo> list(const std::string& path);
};
