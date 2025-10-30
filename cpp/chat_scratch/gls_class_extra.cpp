// mini_ls.cpp
// C++20, OOP, Strategy pattern with LongFormatter + JSONFormatter
// - Approximates: ls -lAg (long list, human sizes, include dotfiles, sorted A→Z, group-only)
// - Adds mtime (stored as std::chrono::system_clock::time_point)
// - Long formatter shows local time "YYYY-MM-DD HH:MM"
// - JSON formatter outputs ISO-8601 UTC "YYYY-MM-DDTHH:MM:SSZ"
// - Multiple paths supported; files print as single entries; directories list immediate contents
// - Symlink arguments are NOT followed (list the link itself)

#include <algorithm>
#include <chrono>
#include <ctime>
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

#include <sys/stat.h>
#include <unistd.h>
#include <grp.h>

namespace fs = std::filesystem;
using Clock = std::chrono::system_clock;

// --------------------------- Result<T> ---------------------------
template <typename T>
class Result {
public:
    static Result ok(T v) {
        Result r; r.value_ = std::move(v); return r;
    }
    static Result fail(std::string err) {
        Result r; r.error_ = std::move(err); return r;
    }
    bool has_value() const noexcept { return value_.has_value(); }
    T&       value()       { return *value_; }
    const T& value() const { return *value_; }
    const std::string& error() const { return error_; }
private:
    std::optional<T> value_;
    std::string error_;
};

// --------------------------- Utilities ---------------------------
static std::string human_size(off_t bytes) {
    static const char* units[] = {"B","K","M","G","T"};
    double v = static_cast<double>(bytes);
    int u = 0; while (v >= 1024.0 && u < 4) { v /= 1024.0; ++u; }
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(u==0?0:1) << v << units[u];
    return oss.str();
}

static std::string mode_to_string(mode_t m) {
    std::string s(10, '-');
    // type
    if      (S_ISDIR(m))  s[0]='d';
    else if (S_ISLNK(m))  s[0]='l';
    else if (S_ISCHR(m))  s[0]='c';
    else if (S_ISBLK(m))  s[0]='b';
    else if (S_ISSOCK(m)) s[0]='s';
    else if (S_ISFIFO(m)) s[0]='p';
    // user
    s[1]=(m&S_IRUSR)?'r':'-'; s[2]=(m&S_IWUSR)?'w':'-'; s[3]=(m&S_IXUSR)?'x':'-';
    // group
    s[4]=(m&S_IRGRP)?'r':'-'; s[5]=(m&S_IWGRP)?'w':'-'; s[6]=(m&S_IXGRP)?'x':'-';
    // other
    s[7]=(m&S_IROTH)?'r':'-'; s[8]=(m&S_IWOTH)?'w':'-'; s[9]=(m&S_IXOTH)?'x':'-';
    // suid/sgid/sticky
    if (m & S_ISUID) s[3] = (s[3]=='x')?'s':'S';
    if (m & S_ISGID) s[6] = (s[6]=='x')?'s':'S';
    if (m & S_ISVTX) s[9] = (s[9]=='x')?'t':'T';
    return s;
}

static std::string group_name(gid_t gid) {
    if (group* g = ::getgrgid(gid)) if (g->gr_name) return g->gr_name;
    return std::to_string(gid);
}

static bool lstat_path(const fs::path& p, struct stat& st) {
    return ::lstat(p.c_str(), &st) == 0;
}

static fs::path expand_tilde(std::string_view arg) {
    if (!arg.empty() && arg.front()=='~') {
        if (const char* home = std::getenv("HOME"))
            return fs::path(std::string(home) + std::string(arg.substr(1)));
    }
    return fs::path(arg);
}

// Format helpers for time_point
static std::string format_local_ls(const Clock::time_point& tp) {
    std::time_t tt = Clock::to_time_t(tp);
    std::tm lt{};
#if defined(_WIN32)
    localtime_s(&lt, &tt);
#else
    localtime_r(&tt, &lt);
#endif
    std::ostringstream oss;
    oss << std::put_time(&lt, "%Y-%m-%d %H:%M");
    return oss.str();
}

static std::string format_utc_iso8601(const Clock::time_point& tp) {
    std::time_t tt = Clock::to_time_t(tp);
    std::tm ut{};
#if defined(_WIN32)
    gmtime_s(&ut, &tt);
#else
    gmtime_r(&tt, &ut);
#endif
    std::ostringstream oss;
    oss << std::put_time(&ut, "%Y-%m-%dT%H:%M:%SZ");
    return oss.str();
}

static std::string json_escape(std::string_view s) {
    std::string out; out.reserve(s.size()+8);
    for (char c: s) {
        switch (c) {
            case '\"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b";  break;
            case '\f': out += "\\f";  break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    char buf[7]; std::snprintf(buf, sizeof(buf), "\\u%04x", c & 0xff);
                    out += buf;
                } else out += c;
        }
    }
    return out;
}

// --------------------------- Data model: FileInfo ---------------------------
class FileInfo {
public:
    static Result<FileInfo> from_path(const fs::path& p) {
        struct stat st{};
        if (!lstat_path(p, st)) return Result<FileInfo>::fail("cannot stat: " + p.string());

        FileInfo fi;
        fi.path_  = p;
        fi.name_  = p.filename().string();
        fi.mode_  = st.st_mode;
        fi.links_ = st.st_nlink;
        fi.gid_   = st.st_gid;
        fi.size_  = st.st_size;
        fi.is_symlink_ = S_ISLNK(st.st_mode);
        fi.is_dir_     = S_ISDIR(st.st_mode);

        // mtime (POSIX differences: macOS uses st_mtimespec; Linux uses st_mtim)
#if defined(__APPLE__)
        auto sec  = st.st_mtimespec.tv_sec;
        auto nsec = st.st_mtimespec.tv_nsec;
#else
        auto sec  = st.st_mtim.tv_sec;
        auto nsec = st.st_mtim.tv_nsec;
#endif
        auto tp = Clock::from_time_t(sec)
        + std::chrono::duration_cast<Clock::duration>(std::chrono::nanoseconds(nsec));
		fi.mtime_ = tp;


        if (fi.is_symlink_) {
            std::error_code ec;
            auto target = fs::read_symlink(p, ec);
            if (!ec) fi.link_target_ = target.string();
        }
        return Result<FileInfo>::ok(std::move(fi));
    }

    // Accessors
    const fs::path& path() const noexcept { return path_; }
    const std::string& name() const noexcept { return name_; }
    mode_t mode() const noexcept { return mode_; }
    nlink_t links() const noexcept { return links_; }
    gid_t gid() const noexcept { return gid_; }
    off_t size() const noexcept { return size_; }
    bool is_symlink() const noexcept { return is_symlink_; }
    bool is_dir() const noexcept { return is_dir_; }
    const std::string& link_target() const noexcept { return link_target_; }
    const Clock::time_point& mtime() const noexcept { return mtime_; }

private:
    fs::path path_;
    std::string name_;
    mode_t   mode_{};
    nlink_t  links_{};
    gid_t    gid_{};
    off_t    size_{};
    bool     is_symlink_{false};
    bool     is_dir_{false};
    Clock::time_point mtime_{};
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
            << std::setw(16) << format_local_ls(fi.mtime()) << "  "
            << fi.name();
        if (fi.is_symlink() && !fi.link_target().empty()) {
            oss << " -> " << fi.link_target();
        }
        return oss.str();
    }
};

class JSONFormatter final : public IFormatter {
public:
    std::string format(const FileInfo& fi) const override {
        // One JSON object per line (NDJSON friendly)
        std::ostringstream oss;
        oss << "{";
        oss << "\"name\":\""        << json_escape(fi.name()) << "\",";
        oss << "\"mode\":\""        << mode_to_string(fi.mode()) << "\",";
        oss << "\"nlink\":"         << fi.links() << ",";
        oss << "\"group\":\""       << json_escape(group_name(fi.gid())) << "\",";
        oss << "\"size\":"          << fi.size() << ",";
        oss << "\"size_hr\":\""     << human_size(fi.size()) << "\",";
        oss << "\"mtime\":\""       << format_utc_iso8601(fi.mtime()) << "\",";
        oss << "\"is_symlink\":"    << (fi.is_symlink() ? "true" : "false") << ",";
        oss << "\"is_dir\":"        << (fi.is_dir() ? "true" : "false");
        if (fi.is_symlink() && !fi.link_target().empty()) {
            oss << ",\"link_target\":\"" << json_escape(fi.link_target()) << "\"";
        }
        oss << "}";
        return oss.str();
    }
};

// --------------------------- Directory listing ---------------------------
class DirectoryLister {
public:
    explicit DirectoryLister(bool include_dotfiles = true)
        : include_dotfiles_(include_dotfiles) {}

    Result<std::vector<FileInfo>> list(const fs::path& dir) const {
        std::error_code ec;
        if (!fs::exists(dir, ec) || !fs::is_directory(dir, ec))
            return Result<std::vector<FileInfo>>::fail("not a directory: " + dir.string());

        std::vector<FileInfo> out;
        for (const auto& de : fs::directory_iterator(dir)) {
            const std::string n = de.path().filename().string();
            if (!include_dotfiles_ && !n.empty() && n[0]=='.') continue;
            if (n == "." || n == "..") continue;

            auto r = FileInfo::from_path(de.path());
            if (r.has_value()) out.emplace_back(std::move(r.value()));
        }
        std::sort(out.begin(), out.end(),
                  [](const FileInfo& a, const FileInfo& b){ return a.name() < b.name(); });
        return Result<std::vector<FileInfo>>::ok(std::move(out));
    }
private:
    bool include_dotfiles_{true};
};

// --------------------------- Options & App ---------------------------
struct Options {
    bool show_headers_ls_style = true;
    bool include_dotfiles = true;
    bool json = false;          // choose JSONFormatter when true
};

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

            struct stat st{};
            if (!lstat_path(p, st)) {
                std::cerr << "mini_ls: " << p << ": No such file or directory\n";
                continue;
            }

            if (S_ISLNK(st.st_mode) || !S_ISDIR(st.st_mode)) {
                auto r = FileInfo::from_path(p);
                if (r.has_value()) std::cout << fmt_.format(r.value()) << "\n";
                else std::cerr << "mini_ls: " << r.error() << "\n";
            } else {
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

// --------------------------- CLI ---------------------------
static void print_usage(std::string_view prog) {
    std::cerr << "Usage: " << prog << " [--json] [paths...]\n";
}

int main(int argc, char* argv[]) try {
    std::ios::sync_with_stdio(false);

    Options opt{};
    std::vector<fs::path> targets;

    for (int i = 1; i < argc; ++i) {
        std::string_view a = argv[i];
        if (a == "-h" || a == "--help") { print_usage(argv[0]); return 0; }
        else if (a == "--json") { opt.json = true; }
        else { targets.emplace_back(expand_tilde(a)); }
    }

    LongFormatter longFmt;
    JSONFormatter jsonFmt;
    const IFormatter& fmt = opt.json ? static_cast<IFormatter&>(jsonFmt)
                                     : static_cast<IFormatter&>(longFmt);

    MiniLsApp app(opt, fmt);
    return app.run(std::move(targets));

} catch (const std::exception& ex) {
    std::cerr << "mini_ls: fatal: " << ex.what() << "\n";
    return 2;
} catch (...) {
    std::cerr << "mini_ls: fatal: unknown error\n";
    return 2;
}
