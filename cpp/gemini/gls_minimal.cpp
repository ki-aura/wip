// GEM-LS

#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <iomanip>
#include <stdexcept>
#include <algorithm>
#include <cerrno>
#include <cstring>
#include <cmath>

// POSIX/C Headers
#include <dirent.h>
#include <sys/stat.h>
#include <pwd.h>
#include <grp.h>
#include <time.h>
#include <unistd.h>
#include <limits.h>

#ifndef PATH_MAX
#define PATH_MAX 1024
#endif

// clang++ -std=c++11 gls_minimal.cpp -o gls_minimal

// --- FileEntry Class ---
// Represents a single file system entry and handles all its metadata formatting.
class FileEntry {
private:
    std::string path_;
    std::string name_;
    struct stat stat_info_;

    // --- Static Utility Helpers (Replacing C functions) ---

    static std::string get_permissions(mode_t mode) {
        std::string perms = "----------";
        if (S_ISDIR(mode)) perms[0] = 'd';
        else if (S_ISLNK(mode)) perms[0] = 'l';

        perms[1] = (mode & S_IRUSR) ? 'r' : '-';
        perms[2] = (mode & S_IWUSR) ? 'w' : '-';
        perms[3] = (mode & S_IXUSR) ? 'x' : '-';
        perms[4] = (mode & S_IRGRP) ? 'r' : '-';
        perms[5] = (mode & S_IWGRP) ? 'w' : '-';
        perms[6] = (mode & S_IXGRP) ? 'x' : '-';
        perms[7] = (mode & S_IROTH) ? 'r' : '-';
        perms[8] = (mode & S_IWOTH) ? 'w' : '-';
        perms[9] = (mode & S_IXOTH) ? 'x' : '-';
        return perms;
    }

    static std::string get_human_size(off_t bytes) {
        const char *units[] = {"B", "K", "M", "G", "T"};
        double size = (double)bytes;
        int u = 0;
        while (size >= 1024.0 && u < 4) { size /= 1024.0; u++; }
        
        std::stringstream ss;
        if (u == 0) {
            ss << std::fixed << std::setprecision(0) << size << units[u];
        } else {
            ss << std::fixed << std::setprecision(1) << size << units[u];
        }
        return ss.str();
    }

    static std::string get_mod_time_str(time_t mtime) {
        time_t now = time(NULL);
        struct tm *tm_info = localtime(&mtime);
        if (!tm_info) return "??? ?? ??:??";
        
        double diff = difftime(now, mtime);
        
        // POSIX-style time formatting
        char tbuf[64];
        if (diff > 15778800 || diff < 0) { // More than ~6 months
            strftime(tbuf, sizeof(tbuf), "%b %e  %Y", tm_info);
        } else {
            strftime(tbuf, sizeof(tbuf), "%b %e %H:%M", tm_info);
        }
        return tbuf;
    }

    static std::string get_user_name(uid_t uid) {
        struct passwd *pw = getpwuid(uid);
        return pw ? pw->pw_name : "?";
    }

    static std::string get_group_name(gid_t gid) {
        struct group *gr = getgrgid(gid);
        return gr ? gr->gr_name : "?";
    }

    static std::string sanitize_name(const std::string& input) {
        std::string safe_name;
        for (char c : input) {
            safe_name += (isprint((unsigned char)c) || c == '\n') ? c : '?';
        }
        return safe_name;
    }

public:
    FileEntry(const std::string& dirpath, const std::string& entry_name)
        : name_(entry_name) {
        
        if (dirpath.empty()) {
            path_ = entry_name; // For single file arguments
        } else {
            path_ = dirpath + "/" + entry_name;
        }

        // Use lstat for file status (including symlink details)
        if (lstat(path_.c_str(), &stat_info_) == -1) {
            // Throw an exception for a clean error signal instead of silent return
            throw std::runtime_error(std::string("lstat failed for ") + path_ + ": " + strerror(errno));
        }
    }

    void print() const {
        std::string perms = get_permissions(stat_info_.st_mode);
        std::string user = get_user_name(stat_info_.st_uid);
        std::string group = get_group_name(stat_info_.st_gid);
        std::string size = get_human_size(stat_info_.st_size);
        std::string time_str = get_mod_time_str(stat_info_.st_mtime);
        std::string safe_name = sanitize_name(name_);
        
        // Using iomanip for formatting (equivalent to printf widths)
        std::cout << perms 
                  << std::setw(3) << std::right << stat_info_.st_nlink << " "
                  << std::setw(8) << std::left << user << " "
                  << std::setw(8) << std::left << group << " "
                  << std::setw(6) << std::right << size << " "
                  << time_str << " " 
                  << safe_name;

        // Handle symlink
        if (S_ISLNK(stat_info_.st_mode)) {
            char linkbuf[PATH_MAX];
            ssize_t len = readlink(path_.c_str(), linkbuf, sizeof(linkbuf) - 1);
            if (len != -1) {
                linkbuf[len] = '\0';
                std::cout << " -> " << linkbuf;
            }
        }
        std::cout << "\n";
    }

    bool is_directory() const {
        return S_ISDIR(stat_info_.st_mode);
    }
    
    const std::string& get_name() const {
        return name_;
    }
};

// --- DirectoryLister Class (RAII) ---
// Manages the directory stream resource and provides listing functionality.
class DirectoryLister {
private:
    std::string path_;
    DIR* dir_ptr_; // The raw resource

public:
    // Constructor: Acquires the resource
    DirectoryLister(const std::string& path) : path_(path), dir_ptr_(nullptr) {
        dir_ptr_ = opendir(path_.c_str());
        if (!dir_ptr_) {
            // Throw instead of perror and return
            throw std::runtime_error(std::string("cannot open directory '") + path_ + "': " + strerror(errno));
        }
    }

    // Destructor: Releases the resource (RAII)
    ~DirectoryLister() {
        if (dir_ptr_) {
            closedir(dir_ptr_);
        }
    }

    // Prevent copying (important for resource ownership)
    DirectoryLister(const DirectoryLister&) = delete;
    DirectoryLister& operator=(const DirectoryLister&) = delete;

    void list_entries(bool show_header) const {
        if (show_header) {
            std::cout << path_ << ":\n";
        }

        // Reset directory stream to start
        rewinddir(dir_ptr_);
        
        struct dirent *entry;
        while ((entry = readdir(dir_ptr_)) != NULL) {
            try {
                // Instantiates a temporary FileEntry object and calls print()
                FileEntry file_entry(path_, entry->d_name);
                file_entry.print();
            } catch (const std::runtime_error& e) {
                // In a minimal ls, we typically ignore files we can't stat
                // std::cerr << "Error processing entry " << entry->d_name << ": " << e.what() << "\n";
            }
        }

        if (show_header) {
            std::cout << "\n";
        }
    }
};

// --- ListerApp Class ---
// Handles command-line argument parsing and overall program flow.
class ListerApp {
public:
    int run(int argc, char* argv[]) {
        std::vector<std::string> targets;

        // Default to current directory if no arguments or explicit wildcards
        if (argc == 1 || (argc == 2 && (std::strcmp(argv[1], "*") == 0 || std::strcmp(argv[1], "./*") == 0))) {
            targets.push_back(".");
        } else {
            for (int i = 1; i < argc; ++i) {
                targets.push_back(argv[i]);
            }
        }

        int exit_code = 0;
        bool multi_target = targets.size() > 1;

        for (const std::string& target : targets) {
            try {
                // Determine if the target is a directory or a file
                struct stat st;
                if (lstat(target.c_str(), &st) == -1) {
                    std::cerr << "ls: cannot access '" << target << "': " << strerror(errno) << "\n";
                    exit_code = 1;
                    continue;
                }

                if (S_ISDIR(st.st_mode)) {
                    // It's a directory, use the DirectoryLister
                    DirectoryLister lister(target); // RAII object created
                    lister.list_entries(multi_target);
                } else {
                    // It's a file, treat it as a single entry list in the empty directory context
                    FileEntry file_entry("", target); 
                    file_entry.print();
                }

            } catch (const std::runtime_error& e) {
                // Catch errors thrown by DirectoryLister or FileEntry constructor
                std::cerr << "ls: " << e.what() << "\n";
                exit_code = 1;
            }
        }

        return exit_code;
    }
};

// --- Main function ---
int main(int argc, char *argv[]) {
    // The C++ main simply instantiates and runs the application class
    ListerApp app;
    return app.run(argc, argv);
}