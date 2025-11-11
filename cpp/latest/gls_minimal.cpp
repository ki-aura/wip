// g++ -std=c++20 -Wall -Wextra -Wpedantic -O2 -o gls gls.cpp
// clang++ -std=c++20 -Wall -Wextra -Wpedantic -O2 -o gls gls.cpp

#include <algorithm>
#include <array>
#include <cassert>
#include <chrono>
#include <cstring>
#include <exception>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include <sys/stat.h>
#include <sys/types.h>
#include <pwd.h>
#include <grp.h>
#include <unistd.h>
#include <errno.h>
#include <limits.h>

namespace fs = std::filesystem;

// ---------- Strong types & small utilities ----------

enum class FileKind { Regular, Directory, Symlink, Block, Char, Fifo, Socket, Unknown };

[[nodiscard]] constexpr char filekind_char(FileKind k) noexcept {
    switch (k) {
        case FileKind::Regular:   return '-';
        case FileKind::Directory: return 'd';
        case FileKind::Symlink:   return 'l';
        case FileKind::Block:     return 'b';
        case FileKind::Char:      return 'c';
        case FileKind::Fifo:      return 'p';
        case FileKind::Socket:    return 's';
        default:                  return '?';
    }
}

[[nodiscard]] constexpr const char* unit_label(std::size_t idx) noexcept {
    constexpr const char* U[] = {"B","K","M","G","T","P","E"};
    return U[idx < std::size(U) ? idx : std::size(U)-1];
}

[[nodiscard]] std::string human_size(unsigned long long bytes) {
    // No exceptions thrown here: only arithmetic and string ops.
    long double v = static_cast<long double>(bytes);
    std::size_t u = 0;
    while (v >= 1024.0L && u + 1 < 7) { v /= 1024.0L; ++u; }
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(u == 0 ? 0 : 1) << v << unit_label(u);
    return oss.str();
}

[[nodiscard]] std::string time_to_string(std::time_t t) {
    // Use std::chrono and std::put_time for clarity.
    std::tm tm{};
    // localtime_r is thread-safe; guard against failure.
    if (!localtime_r(&t, &tm)) return "?";
    std::ostringstream oss;
    // Typical `ls -l` style: "MMM dd HH:MM"
    oss << std::put_time(&tm, "%b %e %H:%M");
    return oss.str();
}

[[nodiscard]] std::string user_name(uid_t uid) {
    if (passwd* pw = ::getpwuid(uid)) return pw->pw_name ? std::string(pw->pw_name) : std::to_string(uid);
    return std::to_string(uid);
}

[[nodiscard]] std::string group_name(gid_t gid) {
    if (group* gr = ::getgrgid(gid)) return gr->gr_name ? std::string(gr->gr_name) : std::to_string(gid);
    return std::to_string(gid);
}

[[nodiscard]] FileKind kind_from_mode(mode_t m) noexcept {
    switch (m & S_IFMT) {
        case S_IFREG:  return FileKind::Regular;
        case S_IFDIR:  return FileKind::Directory;
        case S_IFLNK:  return FileKind::Symlink;
        case S_IFBLK:  return FileKind::Block;
        case S_IFCHR:  return FileKind::Char;
        case S_IFIFO:  return FileKind::Fifo;
        case S_IFSOCK: return FileKind::Socket;
        default:       return FileKind::Unknown;
    }
}

[[nodiscard]] std::string perm_string(mode_t m) noexcept {
    // rwx bits + sticky/suid/sgid, e.g. "rwxr-xr-x"
    auto bit = [&](mode_t b, char c){ return (m & b) ? c : '-'; };
    std::string s;
    s.reserve(10);
    s.push_back(filekind_char(kind_from_mode(m)));

    s.push_back(bit(S_IRUSR,'r')); s.push_back(bit(S_IWUSR,'w'));
    s.push_back((m & S_ISUID) ? ((m & S_IXUSR) ? 's' : 'S') : bit(S_IXUSR,'x'));
    s.push_back(bit(S_IRGRP,'r')); s.push_back(bit(S_IWGRP,'w'));
    s.push_back((m & S_ISGID) ? ((m & S_IXGRP) ? 's' : 'S') : bit(S_IXGRP,'x'));
    s.push_back(bit(S_IROTH,'r')); s.push_back(bit(S_IWOTH,'w'));
    s.push_back((m & S_ISVTX) ? ((m & S_IXOTH) ? 't' : 'T') : bit(S_IXOTH,'x'));

    return s;
}

[[nodiscard]] std::optional<std::string> read_symlink_target(const fs::path& p) noexcept {
    std::array<char, PATH_MAX> buf{};
    // Use ::readlink to avoid following the link (we already used lstat).
    ssize_t n = ::readlink(p.c_str(), buf.data(), buf.size()-1);
    if (n < 0) return std::nullopt;
    buf[static_cast<std::size_t>(n)] = '\0';
    return std::string(buf.data());
}

// ---------- Data model ----------

struct FileEntry {
    fs::path path;       // full path for stat/readlink
    std::string name;    // display name (no directory prefix)
    FileKind kind{};
    mode_t mode{};
    nlink_t nlink{};
    uid_t uid{};
    gid_t gid{};
    off_t size{};
    blkcnt_t blocks{};
    std::time_t mtime{};
    std::optional<std::string> symlink_target; // only if kind == Symlink

    // Rule of Zero: all fields are value types; default special members suffice.
};

// ---------- RAII: error as exception, no raw owning pointers ----------

[[nodiscard]] FileEntry stat_path(const fs::path& p, std::string display_name) {
    struct ::stat st{};
    // lstat to not follow symlinks (like `ls -l`).
    if (::lstat(p.c_str(), &st) != 0) {
        throw std::system_error(errno, std::generic_category(),
                                "lstat failed on '" + p.string() + "'");
    }

    FileEntry fe;
    fe.path = p;
    fe.name = std::move(display_name);
    fe.kind = kind_from_mode(st.st_mode);
    fe.mode = st.st_mode;
    fe.nlink = st.st_nlink;
    fe.uid = st.st_uid;
    fe.gid = st.st_gid;
    fe.size = st.st_size;
    fe.blocks = st.st_blocks;
    fe.mtime = st.st_mtime;
    if (fe.kind == FileKind::Symlink) {
        fe.symlink_target = read_symlink_target(p);
    }
    return fe;
}

// ---------- Printing ----------

void print_entry(const FileEntry& e) {
    const std::string perms = perm_string(e.mode);
    const std::string owner = user_name(e.uid);
    const std::string group = group_name(e.gid);
    const std::string when  = time_to_string(e.mtime);

    // Try to emulate ls -alh column widths simply.
    std::cout << perms << ' '
              << std::setw(3) << e.nlink << ' '
              << std::left << std::setw(8) << owner << ' '
              << std::left << std::setw(8) << group << ' '
              << std::right << std::setw(6) << human_size(static_cast<unsigned long long>(e.size)) << ' '
              << when << ' '
              << e.name;

    if (e.kind == FileKind::Symlink && e.symlink_target) {
        std::cout << " -> " << *e.symlink_target;
    }
    std::cout << '\n';
}

// ---------- Directory listing ----------

struct DirListingResult {
    std::vector<FileEntry> entries;
    unsigned long long total_blocks_1k{}; // POSIX st_blocks are 512B; convert to 1K for display.
};

// RAII + algorithms: collect, sort, then print.
[[nodiscard]] DirListingResult list_directory_collect(const fs::path& dir) {
    DirListingResult R{};
    // Include '.' and '..' explicitly to mimic `-a`.
    // (filesystem iterators do not yield them.)
    for (auto name : {std::string("."), std::string("..")}) {
        fs::path p = dir / name;
        try {
            R.entries.push_back(stat_path(p, name));
        } catch (const std::exception&) {
            // If they don't exist (e.g., chroot oddities), skip.
        }
    }

    // Include all entries, including dotfiles. Do not recurse.
    fs::directory_options opts = fs::directory_options::skip_permission_denied;
    for (const auto& de : fs::directory_iterator(dir, opts)) {
        const std::string base = de.path().filename().string();
        try {
            R.entries.push_back(stat_path(de.path(), base));
        } catch (const std::exception& ex) {
            // Keep going; show a placeholder entry with error-ish marker.
            FileEntry err{};
            err.path = de.path();
            err.name = base + " [stat error]";
            err.kind = FileKind::Unknown;
            R.entries.push_back(std::move(err));
        }
    }

    // Sum blocks (convert 512B blocks to 1K by dividing by 2; guard overflow).
    unsigned long long blocks512 = 0;
    for (const auto& e : R.entries) {
        blocks512 += static_cast<unsigned long long>(e.blocks);
    }
    R.total_blocks_1k = blocks512 / 2ULL;

    // Sort by name (locale-insensitive simple compare).
    std::sort(R.entries.begin(), R.entries.end(),
              [](const FileEntry& a, const FileEntry& b) { return a.name < b.name; });

    return R;
}

void print_directory(const fs::path& dir) {
    DirListingResult R = list_directory_collect(dir);

    std::cout << "total " << R.total_blocks_1k << "\n";
    for (const auto& e : R.entries) {
        print_entry(e);
    }
}

// ---------- File or directory dispatcher ----------

void list_operand(const fs::path& p, bool print_heading) {
    // Use lstat to decide how to handle symlinks: if it’s a symlink to a dir,
    // `ls -l` lists the link itself (not the directory). We follow that.
    struct ::stat st{};
    if (::lstat(p.c_str(), &st) != 0) {
        throw std::system_error(errno, std::generic_category(),
                                "cannot access '" + p.string() + "'");
    }
    const FileKind k = kind_from_mode(st.st_mode);

    if (k == FileKind::Directory) {
        if (print_heading) {
            std::cout << p.string() << ":\n";
        }
        print_directory(p);
    } else {
        // Single line for files, symlinks, etc.
        print_entry(stat_path(p, p.filename().empty() ? p.string() : p.filename().string()));
    }
}

// ---------- Main (arguments as operands; no options) ----------

int main(int argc, char* argv[]) try {
    // Invariants: program name present.
    assert(argc >= 1 && argv && "argv must be non-null");

    // Collect operands (default to ".")
    std::vector<fs::path> operands;
    operands.reserve(static_cast<std::size_t>(argc > 1 ? argc - 1 : 1));

    if (argc == 1) {
        operands.emplace_back(".");
    } else {
        for (int i = 1; i < argc; ++i) {
            // No options parsing per spec: everything is an operand.
            operands.emplace_back(argv[i]);
        }
    }

    // Resolve and list each operand. Print headings if >1 and/or if operand is a directory.
    const bool multi = operands.size() > 1;

    // Use algorithms and exception-safe loop.
    bool first = true;
    for (const fs::path& op : operands) {
        if (!first) std::cout << '\n';
        first = false;

        list_operand(op, /*print_heading=*/multi);
    }

    return 0;
}
catch (const std::system_error& se) {
    // Do not swallow exceptions; print clear diagnostics.
    std::cerr << "error: " << se.code() << " - " << se.what() << "\n";
    return 2;
}
catch (const std::exception& ex) {
    std::cerr << "error: " << ex.what() << "\n";
    return 2;
}
