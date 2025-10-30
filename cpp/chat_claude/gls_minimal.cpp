// CHAT-CLAUD-LS
// Hybrid C++17 version combining Claude-LS (idiomatic) and Chat-LS (transparent flow)
// POSIX.1-2008 compliant. No std::filesystem.

// c++ -std=c++17 -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion -Wcast-qual gls_minimal.cpp -o gls++

#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include <optional>
#include <cstring>
#include <ctime>
#include <cctype>
#include <cerrno>

#include <dirent.h>
#include <sys/stat.h>
#include <pwd.h>
#include <grp.h>
#include <unistd.h>
#include <limits.h>

#ifndef PATH_MAX
#define PATH_MAX 1024
#endif

// =======================
// Utility / Formatter Classes
// =======================

class PermissionFormatter {
public:
    std::string format(mode_t mode) const {
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
};

class SizeFormatter {
public:
    std::string format(off_t bytes) const {
        const char* units[] = {"B", "K", "M", "G", "T"};
        double size = static_cast<double>(bytes);
        int u = 0;
        while (size >= 1024.0 && u < 4) {
            size /= 1024.0;
            ++u;
        }
        char buf[32];
        if (u == 0)
            std::snprintf(buf, sizeof(buf), "%.0f%s", size, units[u]);
        else
            std::snprintf(buf, sizeof(buf), "%.1f%s", size, units[u]);
        return buf;
    }
};

class TimeFormatter {
public:
    std::string format(time_t mtime) const {
        time_t now = time(nullptr);
        struct tm* tm_info = localtime(&mtime);
        if (!tm_info) return "??? ?? ??:??";

        double diff = difftime(now, mtime);
        char buf[64];
        if (diff > 15778800 || diff < 0)
            strftime(buf, sizeof(buf), "%b %e  %Y", tm_info);
        else
            strftime(buf, sizeof(buf), "%b %e %H:%M", tm_info);
        return buf;
    }
};

class Sanitizer {
public:
    std::string sanitize(const std::string& src) const {
        std::string out;
        out.reserve(src.size());
		for (size_t i = 0; i < src.size(); ++i) {
			unsigned char uc = static_cast<unsigned char>(src[i]);
			char safe = std::isprint(static_cast<int>(uc)) ? static_cast<char>(uc) : '?';
			out.push_back(safe);
		}
        return out;
    }
};

// =======================
// RAII Wrapper for DIR*
// =======================

class DirHandle {
public:
    explicit DirHandle(DIR* d = nullptr) : dir_(d) {}
    ~DirHandle() { if (dir_) closedir(dir_); }

    DirHandle(const DirHandle&) = delete;
    DirHandle& operator=(const DirHandle&) = delete;

    DirHandle(DirHandle&& other) noexcept : dir_(other.dir_) { other.dir_ = nullptr; }
    DirHandle& operator=(DirHandle&& other) noexcept {
        if (this != &other) {
            if (dir_) closedir(dir_);
            dir_ = other.dir_;
            other.dir_ = nullptr;
        }
        return *this;
    }

    DIR* get() const { return dir_; }
    explicit operator bool() const { return dir_ != nullptr; }

private:
    DIR* dir_{};
};

// =======================
// FileInfo (data holder)
// =======================

struct FileInfo {
    std::string dir;
    std::string name;
    struct stat st{};
    std::optional<std::string> link_target;
};

// =======================
// Directory Lister
// =======================

class DirectoryLister {
public:
    std::vector<FileInfo> list(const std::string& path) const {
        std::vector<FileInfo> result;

        DIR* raw = opendir(path.c_str());
        if (!raw) {
            perror(path.c_str());
            return result;
        }

        DirHandle dir(raw);
        struct dirent* entry;
        while ((entry = readdir(dir.get())) != nullptr) {
            std::string full = path + "/" + entry->d_name;
            struct stat st{};
            if (lstat(full.c_str(), &st) == -1) {
                // ignore unreadable files
                continue;
            }

            FileInfo fi{path, entry->d_name, st, std::nullopt};

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
};

// =======================
// Entry Renderer
// =======================

class EntryRenderer {
public:
    void print(const FileInfo& info) const {
        struct passwd* pw = getpwuid(info.st.st_uid);
        struct group* gr = getgrgid(info.st.st_gid);

        const char* user = pw ? pw->pw_name : "?";
        const char* group = gr ? gr->gr_name : "?";

        std::string perms = perms_.format(info.st.st_mode);
        std::string size = size_.format(info.st.st_size);
        std::string time = time_.format(info.st.st_mtime);
        std::string name = sanitize_.sanitize(info.name);

        std::cout << perms << " "
                  << std::setw(2) << info.st.st_nlink << " "
                  << std::left << std::setw(8) << user << " "
                  << std::left << std::setw(8) << group << " "
                  << std::right << std::setw(6) << size << " "
                  << time << " " << name;

        if (S_ISLNK(info.st.st_mode) && info.link_target)
            std::cout << " -> " << *info.link_target;

        std::cout << "\n";
    }

private:
    PermissionFormatter perms_;
    SizeFormatter size_;
    TimeFormatter time_;
    Sanitizer sanitize_;
};

// =======================
// Application
// =======================

class GlsApp {
public:
    int run(int argc, char* argv[]) {
        std::vector<std::string> targets;

        if (argc == 1) {
            targets.push_back(".");
        } else if (argc == 2 &&
                   (std::strcmp(argv[1], "*") == 0 ||
                    std::strcmp(argv[1], "./*") == 0)) {
            targets.push_back(".");
        } else {
            for (int i = 1; i < argc; ++i)
                targets.push_back(argv[i]);
        }

        bool multi = targets.size() > 1;

        for (const auto& path : targets) {
            struct stat st{};
            if (lstat(path.c_str(), &st) == -1) {
                perror(path.c_str());
                continue;
            }

            if (S_ISDIR(st.st_mode)) {
                if (multi) std::cout << path << ":\n";
                auto files = lister_.list(path);
                for (const auto& f : files) renderer_.print(f);
                if (multi) std::cout << "\n";
            } else {
                FileInfo fi{"", path, st, std::nullopt};
                renderer_.print(fi);
            }
        }

        return 0;
    }

private:
    DirectoryLister lister_;
    EntryRenderer renderer_;
};

// =======================
// main()
// =======================

int main(int argc, char* argv[]) {
    GlsApp app;
    return app.run(argc, argv);
}
