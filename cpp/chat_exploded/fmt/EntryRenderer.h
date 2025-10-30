#pragma once
#include "../fs/FileInfo.h"
#include "PermissionFormatter.h"
#include "SizeFormatter.h"
#include "TimeFormatter.h"
#include "Sanitizer.h"
#include <ostream>

class EntryRenderer {
public:
    EntryRenderer(const PermissionFormatter& pf,
                  const SizeFormatter& sf,
                  const TimeFormatter& tf,
                  const Sanitizer& s)
        : perms_(pf), size_(sf), time_(tf), sanitize_(s) {}

    void print(std::ostream& os, const FileInfo& info) const;

private:
    const PermissionFormatter& perms_;
    const SizeFormatter& size_;
    const TimeFormatter& time_;
    const Sanitizer& sanitize_;
};
