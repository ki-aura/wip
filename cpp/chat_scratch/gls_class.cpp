// mini_ls.cpp
// Modern C++20, object-oriented, single-file implementation approximating: ls -lAg
// - Long format, human-readable sizes, include dotfiles, alphabetical sort
// - Multiple path args (shell expansion like ../* works)
// - Files -> single long line; Directories -> header (only if multiple args) + contents
// - Do NOT follow symlink arguments (list the link itself); show "-> target" for symlink entries
// - Clean separations: Options, FileInfo, IFormatter(LongFormatter), DirectoryLister, MiniLsApp
// - Hybrid error handling: Exceptions for fatal setup; Result<T> for per-entry failures

//  c++ -std=c++20 -Wall -Wextra -Wpedantic -g -fsanitize=address,undefined -o gls_class gls_class.cpp

#include <algorithm>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include <sys/stat.h>   // lstat/stat
#include <unistd.h>     // POSIX
#include <grp.h>        // getgrgid
// #include <pwd.h>     // intentionally omitted (-g shows group only)

namespace fs = std::filesystem;

// --------------------------- Result<T> (expected-like) ---------------------------
template <typename T>
class Result {
public:
    static Result ok(T v) {
        Result r;
        r.value_ = std::move(v);
        return r;
    }
    static Result fail(std::string err) {
        Result r;
        r.error_ = std::move(err);
        return r;
    }

    bool has_value() const noexcept { return value_.has_value(); }
    T&       value()       { return *value_; }
    const T& value() const { return *value_; }
    const std::string& error() const { return error_; }

private:
    std::optional<T> value_;
    std::string error_;
};

// --------------------------- Utility functions ---------------------------
static std::string human_size(off_t bytes) {
    static const char* units[] = {"B", "K", "M", "G", "T"};
    double val = static_cast<double>(bytes);
    int u = 0;
    while (val >= 1024.0 && u < 4) { val /= 1024.0; ++u; }
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(u == 0 ? 0 : 1) << val << units[u];
    return oss.str();
}

static std::string mode_to_string(mode_t m) {
    std::string s(10, '-');

    // type
    if      (S_ISDIR(m))  s[0] = 'd';
    else if (S_ISLNK(m))  s[0] = 'l';
    else if (S_ISCHR(m))  s[0] = 'c';
    else if (S_ISBLK(m))  s[0] = 'b';
    else if (S_ISSOCK(m)) s[0] = 's';
    else if (S_ISFIFO(m)) s[0] = 'p';

    // user
    s[1] = (m & S_IRUSR) ? 'r' : '-';
    s[2] = (m & S_IWUSR) ? 'w' : '-';
    s[3] = (m & S_IXUSR) ? 'x' : '-';

    // group
    s[4] = (m & S_IRGRP) ? 'r' : '-';
    s[5] = (m & S_IWGRP) ? 'w' : '-';
    s[6] = (m & S_IXGRP) ? 'x' : '-';

    // other
    s[7] = (m & S_IROTH) ? 'r' : '-';
    s[8] = (m & S_IWOTH) ? 'w' : '-';
    s[9] = (m & S_IXOTH) ? 'x' : '-';

    // suid/sgid/sticky
    if (m & S_ISUID) s[3] = (s[3] == 'x') ? 's' : 'S';
    if (m & S_ISGID) s[6] = (s[6] == 'x') ? 's' : 'S';
    if (m & S_ISVTX) s[9] = (s[9] == 'x') ? 't' : 'T';

    return s;
}

static std::string group_name(gid_t gid) {
    if (group* g = ::getgrgid(gid)) {
        if (g->gr_name) return g->gr_name;
    }
    return std::to_string(gid);
}

static bool lstat_path(const fs::path& p, struct stat& st) {
    return ::lstat(p.c_str(), &st) == 0;
}

[[maybe_unused]]
static bool stat_path(const fs::path& p, struct stat& st) {
    return ::stat(p.c_str(), &st) == 0;
}

static fs::path expand_tilde(std::string_view arg) {
    if (!arg.empty() && arg.front() == '~') {
        if (const char* home = std::getenv("HOME")) {
            return fs::path(std::string(home) + std::string(arg.substr(1)));
        }
    }
    return fs::path(arg);
}

// --------------------------- Data model: FileInfo ---------------------------
class FileInfo {
public:
    // Construct by lstat (do not follow symlinks)
    static Result<FileInfo> from_path(const fs::path& p) {
        struct stat st{};
        if (!lstat_path(p, st)) {
            return Result<FileInfo>::fail("cannot stat: " + p.string());
        }

        FileInfo fi;
        fi.path_ = p;
        fi.name_ = p.filename().string();
        fi.mode_ = st.st_mode;
        fi.links_ = st.st_nlink;
        fi.gid_ = st.st_gid;
        fi.size_ = st.st_size;
        fi.is_symlink_ = S_ISLNK(st.st_mode);
        fi.is_dir_ = S_ISDIR(st.st_mode);

        if (fi.is_symlink_) {
            std::error_code ec;
            fi.link_target_ = fs::read_symlink(p, ec).string();
            if (ec) fi.link_target_.clear(); // best-effort
        }
        return Result<FileInfo>::ok(std::move(fi));
    }

    // Accessors (const-correct, testable)
    const fs::path& path() const noexcept { return path_; }
    const std::string& name() const noexcept { return name_; }
    mode_t mode() const noexcept { return mode_; }
    nlink_t links() const noexcept { return links_; }
    gid_t gid() const noexcept { return gid_; }
    off_t size() const noexcept { return size_; }
    bool is_symlink() const noexcept { return is_symlink_; }
    bool is_dir() const noexcept { return is_dir_; }
    const std::string& link_target() const noexcept { return link_target_; }

private:
    fs::path path_;
    std::string name_;
    mode_t mode_{};
    nlink_t links_{};
    gid_t gid_{};
    off_t size_{};
    bool is_symlink_{false};
    bool is_dir_{false};
    std::string link_target_;
};

// --------------------------- Formatting strategy ---------------------------
class IFormatter {
public:
    virtual ~IFormatter() = default;
    virtual std::string format(const FileInfo& fi) const = 0;
};

class LongFormatter final : public IFormatter {
public:
    std::string format(const FileInfo& fi) const override {
        std::ostringstream oss;
        oss << mode_to_string(fi.mode()) << " "
            << std::setw(2)  << fi.links() << "  "
            << std::setw(8)  << group_name(fi.gid()) << "  "
            << std::setw(6)  << human_size(fi.size()) << "  "
            << fi.name();

        if (fi.is_symlink() && !fi.link_target().empty()) {
            oss << " -> " << fi.link_target();
        }
        return oss.str();
    }
};

// --------------------------- Directory lister ---------------------------
class DirectoryLister {
public:
    explicit DirectoryLister(bool include_dotfiles = true)
        : include_dotfiles_(include_dotfiles) {}

    // List immediate entries of a directory (alphabetically by filename).
    Result<std::vector<FileInfo>> list(const fs::path& dir) const {
        std::error_code ec;
        if (!fs::exists(dir, ec) || !fs::is_directory(dir, ec)) {
            return Result<std::vector<FileInfo>>::fail("not a directory: " + dir.string());
        }

        std::vector<FileInfo> out;
        for (const auto& de : fs::directory_iterator(dir)) {
            const std::string n = de.path().filename().string();
            if (!include_dotfiles_ && !n.empty() && n[0] == '.') continue;
            if (n == "." || n == "..") continue; // always skip self/parent

            auto r = FileInfo::from_path(de.path());
            if (r.has_value()) out.emplace_back(std::move(r.value()));
            // On per-entry error, we continue; caller can decide to report if desired.
        }

        std::sort(out.begin(), out.end(),
                  [](const FileInfo& a, const FileInfo& b) { return a.name() < b.name(); });

        return Result<std::vector<FileInfo>>::ok(std::move(out));
    }

private:
    bool include_dotfiles_{true};
};

// --------------------------- CLI Options ---------------------------
struct Options {
    // Placeholder for future flags (e.g., sort/time/reverse/color/json).
    // Keeping it here shows where extension points belong.
    bool show_headers_ls_style = true; // show dir headers only if multiple args
    bool include_dotfiles = true;      // mimic -A behaviour (here: include dotfiles)
};

// --------------------------- Application orchestrator ---------------------------
class MiniLsApp {
public:
    MiniLsApp(Options opt, const IFormatter& fmt)
        : opt_(opt), fmt_(fmt), lister_(opt.include_dotfiles) {}

    int run(std::vector<fs::path> targets) const {
        if (targets.empty()) targets.emplace_back(".");
        const bool multiple = (targets.size() > 1);

        bool first_block = true;
        for (std::size_t i = 0; i < targets.size(); ++i) {
            const fs::path& p = targets[i];

            // lstat the argument path to decide how to handle it (do not follow symlink)
            struct stat st{};
            if (!lstat_path(p, st)) {
                std::cerr << "mini_ls: " << p << ": No such file or directory\n";
                continue;
            }

            if (S_ISLNK(st.st_mode) || !S_ISDIR(st.st_mode)) {
                // Argument is a symlink (any) or a non-directory file: print single entry
                auto r = FileInfo::from_path(p);
                if (r.has_value()) {
                    std::cout << fmt_.format(r.value()) << "\n";
                } else {
                    std::cerr << "mini_ls: " << r.error() << "\n";
                }
            } else {
                // Argument is a real directory (not a symlink)
                if (multiple && opt_.show_headers_ls_style) {
                    if (!first_block) std::cout << "\n";
                    std::cout << p.string() << ":\n";
                }
                first_block = false;

                auto list_res = lister_.list(p);
                if (!list_res.has_value()) {
                    std::cerr << "mini_ls: " << list_res.error() << "\n";
                    continue;
                }
                for (const auto& fi : list_res.value()) {
                    std::cout << fmt_.format(fi) << "\n";
                }
            }
        }
        return 0;
    }

private:
    Options opt_;
    const IFormatter& fmt_;
    DirectoryLister lister_;
};

// --------------------------- main ---------------------------
static void print_usage(std::string_view prog) {
    std::cerr << "Usage: " << prog << " [paths...]\n";
}

int main(int argc, char* argv[]) try {
    std::ios::sync_with_stdio(false);

    Options opt{};              // defaults mimic -lAg-ish behaviour
    LongFormatter formatter{};  // strategy can be swapped later
    MiniLsApp app(opt, formatter);

    std::vector<fs::path> targets;
    if (argc <= 1) {
        targets.emplace_back(".");
    } else {
        for (int i = 1; i < argc; ++i) {
            std::string_view arg = argv[i];
            if (arg == "-h" || arg == "--help") {
                print_usage(argv[0]);
                return 0;
            }
            targets.emplace_back(expand_tilde(arg));
        }
    }

    return app.run(std::move(targets));

} catch (const std::exception& ex) {
    // Fatal setup/logic errors bubble here (hybrid policy)
    std::cerr << "mini_ls: fatal: " << ex.what() << "\n";
    return 2;
} catch (...) {
    std::cerr << "mini_ls: fatal: unknown error\n";
    return 2;
}
