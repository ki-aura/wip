// mini_ls.cpp
// Educational, single-file C++17 example:
//   - Approximates: ls -lAg   (long list, human sizes, includes dotfiles, sorted alphabetically)
//   - Multiple path arguments supported (so shell expansion like ../* "just works")
//   - Hybrid behaviour: files print as one line; directories print a header (ls-style only when multiple args)
//   - Symlink arguments are NOT followed (match ls default); symlink entries show "-> target"
//   - Clean structure + comments to aid extension and reuse.
//
// Notes:
//   * Uses std::filesystem for iteration + POSIX lstat/stat for metadata (group name, size, link count, type).
//   * Intentionally omits owner (matches -g behaviour).
//   * Time fields are omitted for brevity; easy to add later if desired.

// c++ -std=c++17 -Wall -Wextra -Wpedantic -o gls_minimal gls_minimal.cpp

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <filesystem>

#include <sys/stat.h>   // lstat/stat, S_IS* macros
#include <unistd.h>     // POSIX
#include <grp.h>        // getgrgid
// #include <pwd.h>     // (omitted intentionally; we don't show owner)

namespace fs = std::filesystem;

// ------------------------------------------------------------
// Formatting utilities
// ------------------------------------------------------------

// Human-readable size: 0B, 999B, 1.2K, 3.4M, 5.6G, 7.8T
static std::string human_size(off_t bytes) {
    static const char* units[] = {"B", "K", "M", "G", "T"};
    double val = static_cast<double>(bytes);
    int u = 0;
    while (val >= 1024.0 && u < 4) { val /= 1024.0; ++u; }
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(u == 0 ? 0 : 1) << val << units[u];
    return oss.str();
}

// Convert mode to "drwxr-xr-x" style; includes suid/sgid/sticky bits.
static std::string mode_to_string(mode_t m) {
    std::string s(10, '-');

    // Type
    if      (S_ISDIR(m))  s[0] = 'd';
    else if (S_ISLNK(m))  s[0] = 'l';
    else if (S_ISCHR(m))  s[0] = 'c';
    else if (S_ISBLK(m))  s[0] = 'b';
    else if (S_ISSOCK(m)) s[0] = 's';
    else if (S_ISFIFO(m)) s[0] = 'p';
    else                  s[0] = '-';

    // User
    s[1] = (m & S_IRUSR) ? 'r' : '-';
    s[2] = (m & S_IWUSR) ? 'w' : '-';
    s[3] = (m & S_IXUSR) ? 'x' : '-';

    // Group
    s[4] = (m & S_IRGRP) ? 'r' : '-';
    s[5] = (m & S_IWGRP) ? 'w' : '-';
    s[6] = (m & S_IXGRP) ? 'x' : '-';

    // Other
    s[7] = (m & S_IROTH) ? 'r' : '-';
    s[8] = (m & S_IWOTH) ? 'w' : '-';
    s[9] = (m & S_IXOTH) ? 'x' : '-';

    // Special bits
    if (m & S_ISUID) s[3] = (s[3] == 'x') ? 's' : 'S';
    if (m & S_ISGID) s[6] = (s[6] == 'x') ? 's' : 'S';
    if (m & S_ISVTX) s[9] = (s[9] == 'x') ? 't' : 'T';

    return s;
}

// Safe group lookup (falls back to numeric gid)
static std::string group_name(gid_t gid) {
    if (group* g = getgrgid(gid)) return g->gr_name ? g->gr_name : std::to_string(gid);
    return std::to_string(gid);
}

// ------------------------------------------------------------
// Stat helpers (lstat = do not follow symlinks)
// ------------------------------------------------------------

static bool lstat_path(const fs::path& p, struct stat& st) {
    return ::lstat(p.c_str(), &st) == 0;
}

static bool stat_path(const fs::path& p, struct stat& st) {
    return ::stat(p.c_str(), &st) == 0;
}

// ------------------------------------------------------------
// Printing
// ------------------------------------------------------------

// Long-format single entry:
//   perms nlink group size name [-> target if symlink]
static void print_long(const fs::path& path) {
    struct stat st{};
    if (!lstat_path(path, st)) {
        std::cerr << "mini_ls: cannot stat: " << path << "\n";
        return;
    }

    const std::string perms = mode_to_string(st.st_mode);
    const auto links = st.st_nlink;
    const std::string grp = group_name(st.st_gid);
    const std::string size = human_size(st.st_size);
    const std::string name = path.filename().string();

    // Widths are modest for readability; tune as needed.
    std::cout << perms << " "
              << std::setw(2) << links << "  "
              << std::setw(8) << grp << "  "
              << std::setw(6) << size << "  "
              << name;

    if (S_ISLNK(st.st_mode)) {
        std::error_code ec;
        fs::path target = fs::read_symlink(path, ec);
        if (!ec) std::cout << " -> " << target.string();
    }

    std::cout << "\n";
}

// List a directory's immediate entries (include dotfiles; skip "." and "..").
// Sorted alphabetically by filename. Uses lstat for each entry.
static void list_directory(const fs::path& dir) {
    // Ensure it's a directory by stat (not following symlink passed as arg).
    // Here, we use normal directory iteration (it follows the directory itself).
    std::error_code ec;
    if (!fs::exists(dir, ec) || !fs::is_directory(dir, ec)) {
        std::cerr << "mini_ls: not a directory: " << dir << "\n";
        return;
    }

    std::vector<fs::directory_entry> entries;
    for (const auto& e : fs::directory_iterator(dir, fs::directory_options::follow_directory_symlink)) {
        const std::string n = e.path().filename().string();
        if (n == "." || n == "..") continue; // include dotfiles, but skip self/parent
        entries.push_back(e);
    }

    std::sort(entries.begin(), entries.end(),
              [](const fs::directory_entry& a, const fs::directory_entry& b) {
                  return a.path().filename().string() < b.path().filename().string();
              });

    for (const auto& e : entries) {
        print_long(e.path());
    }
}

// ------------------------------------------------------------
// CLI plumbing
// ------------------------------------------------------------

struct RunConfig {
    // In a larger program, options and flags would live here.
    // Keeping this struct shows where extension points belong.
};

// Decide how to handle a single argument path based on type.
// We DO NOT follow symlink arguments (print symlink itself).
static void handle_target(const fs::path& p, bool show_header_if_needed) {
    struct stat st{};
    if (!lstat_path(p, st)) {
        std::cerr << "mini_ls: no such file or directory: " << p << "\n";
        return;
    }

    // If it's a symlink argument, treat as a file (do not follow).
    if (S_ISLNK(st.st_mode) || !S_ISDIR(st.st_mode)) {
        print_long(p);
        return;
    }

    // It's a directory (and not a symlink)
    if (show_header_if_needed) {
        std::cout << p.string() << ":\n";
    }
    list_directory(p);
}

// Expand "~" at start for convenience (simple form).
static fs::path expand_tilde(const std::string& arg) {
    if (!arg.empty() && arg[0] == '~') {
        if (const char* home = std::getenv("HOME")) {
            return fs::path(std::string(home) + arg.substr(1));
        }
    }
    return fs::path(arg);
}

int main(int argc, char* argv[]) {
    std::ios::sync_with_stdio(false);

    RunConfig cfg; // placeholder for future options

    // Collect targets. If none, default to current directory "."
    std::vector<fs::path> targets;
    if (argc <= 1) {
        targets.emplace_back(".");
    } else {
        for (int i = 1; i < argc; ++i) {
            targets.emplace_back(expand_tilde(argv[i]));
        }
    }

    const bool multiple = (targets.size() > 1);

    // Process each target in order given.
    for (std::size_t i = 0; i < targets.size(); ++i) {
        const bool need_header = multiple; // ls-style: only when multiple args
        handle_target(targets[i], need_header);
        if (i + 1 < targets.size()) std::cout << (need_header ? "\n" : "");
    }

    return 0;
}
