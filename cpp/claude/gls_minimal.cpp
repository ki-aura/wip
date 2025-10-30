// CLAUDE-LS
// Object-oriented ls -la clone (C++11 compliant)

#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include <memory>
#include <cstring>
#include <ctime>
#include <cctype>
#include <dirent.h>
#include <sys/stat.h>
#include <pwd.h>
#include <grp.h>
#include <unistd.h>
#include <limits.h>

#ifndef PATH_MAX
#define PATH_MAX 1024
#endif

// ==================== Value Types ====================

class FilePermissions {
    mode_t mode_;
    
public:
    explicit FilePermissions(mode_t mode) : mode_(mode) {}
    
    bool isDirectory() const { return S_ISDIR(mode_); }
    bool isSymlink() const { return S_ISLNK(mode_); }
    bool isRegularFile() const { return S_ISREG(mode_); }
    
    std::string toString() const {
        std::string perms(10, '-');
        
        if (isDirectory()) perms[0] = 'd';
        else if (isSymlink()) perms[0] = 'l';
        
        perms[1] = (mode_ & S_IRUSR) ? 'r' : '-';
        perms[2] = (mode_ & S_IWUSR) ? 'w' : '-';
        perms[3] = (mode_ & S_IXUSR) ? 'x' : '-';
        perms[4] = (mode_ & S_IRGRP) ? 'r' : '-';
        perms[5] = (mode_ & S_IWGRP) ? 'w' : '-';
        perms[6] = (mode_ & S_IXGRP) ? 'x' : '-';
        perms[7] = (mode_ & S_IROTH) ? 'r' : '-';
        perms[8] = (mode_ & S_IWOTH) ? 'w' : '-';
        perms[9] = (mode_ & S_IXOTH) ? 'x' : '-';
        
        return perms;
    }
};

class FileSize {
    off_t bytes_;
    
public:
    explicit FileSize(off_t bytes) : bytes_(bytes) {}
    
    off_t bytes() const { return bytes_; }
    
    std::string toHumanReadable() const {
        const char* units[] = {"B", "K", "M", "G", "T"};
        double size = static_cast<double>(bytes_);
        int u = 0;
        
        while (size >= 1024.0 && u < 4) {
            size /= 1024.0;
            u++;
        }
        
        char buf[32];
        snprintf(buf, sizeof(buf), (u == 0) ? "%.0f%s" : "%.1f%s", size, units[u]);
        return std::string(buf);
    }
};

class Timestamp {
    time_t time_;
    
public:
    explicit Timestamp(time_t t) : time_(t) {}
    
    bool isRecent() const {
        time_t now = time(nullptr);
        double diff = difftime(now, time_);
        return diff <= 15778800 && diff >= 0;  // 6 months
    }
    
    std::string format() const {
        struct tm* tm_info = localtime(&time_);
        if (!tm_info) {
            return "??? ?? ??:??";
        }
        
        char buf[64];
        if (isRecent()) {
            strftime(buf, sizeof(buf), "%b %e %H:%M", tm_info);
        } else {
            strftime(buf, sizeof(buf), "%b %e  %Y", tm_info);
        }
        return std::string(buf);
    }
    
    time_t raw() const { return time_; }
};

class FilePath {
    std::string path_;
    
public:
    explicit FilePath(const std::string& path) : path_(path) {}
    
    std::string toString() const { return path_; }
    
    std::string filename() const {
        size_t pos = path_.find_last_of('/');
        return (pos == std::string::npos) ? path_ : path_.substr(pos + 1);
    }
    
    FilePath join(const std::string& name) const {
        if (path_.empty() || path_ == "") {
            return FilePath(name);
        }
        return FilePath(path_ + "/" + name);
    }
    
    std::string sanitized() const {
        std::string result;
        result.reserve(path_.length());
        
        for (char c : path_) {
            result += isprint(static_cast<unsigned char>(c)) ? c : '?';
        }
        return result;
    }
};

// ==================== File Metadata ====================

class FileMetadata {
    FilePermissions permissions_;
    nlink_t linkCount_;
    std::string owner_;
    std::string group_;
    FileSize size_;
    Timestamp modTime_;
    
public:
    FileMetadata(const struct stat& st)
        : permissions_(st.st_mode),
          linkCount_(st.st_nlink),
          owner_(getUserName(st.st_uid)),
          group_(getGroupName(st.st_gid)),
          size_(st.st_size),
          modTime_(st.st_mtime) {}
    
    const FilePermissions& permissions() const { return permissions_; }
    nlink_t linkCount() const { return linkCount_; }
    const std::string& owner() const { return owner_; }
    const std::string& group() const { return group_; }
    const FileSize& size() const { return size_; }
    const Timestamp& modificationTime() const { return modTime_; }
    
private:
    static std::string getUserName(uid_t uid) {
        struct passwd* pw = getpwuid(uid);
        return pw ? std::string(pw->pw_name) : "?";
    }
    
    static std::string getGroupName(gid_t gid) {
        struct group* gr = getgrgid(gid);
        return gr ? std::string(gr->gr_name) : "?";
    }
};

// ==================== File Entry ====================

class FileEntry {
    FilePath path_;
    std::string name_;
    FileMetadata metadata_;
    std::string linkTarget_;
    
public:
    FileEntry(const FilePath& dirPath, const std::string& name)
        : path_(dirPath.join(name)),
          name_(name),
          metadata_(getStat(path_)),
          linkTarget_(readLinkTarget(path_, metadata_.permissions())) {}
    
    const FilePath& path() const { return path_; }
    const std::string& name() const { return name_; }
    const FileMetadata& metadata() const { return metadata_; }
    const std::string& linkTarget() const { return linkTarget_; }
    bool hasLinkTarget() const { return !linkTarget_.empty(); }
    
private:
    static struct stat getStat(const FilePath& path) {
        struct stat st;
        if (lstat(path.toString().c_str(), &st) == -1) {
            // Return zeroed stat on error
            memset(&st, 0, sizeof(st));
        }
        return st;
    }
    
    static std::string readLinkTarget(const FilePath& path, const FilePermissions& perms) {
        if (!perms.isSymlink()) {
            return "";
        }
        
        char buf[PATH_MAX];
        ssize_t len = readlink(path.toString().c_str(), buf, sizeof(buf) - 1);
        if (len != -1) {
            buf[len] = '\0';
            return std::string(buf);
        }
        return "";
    }
};

// ==================== Output Renderer ====================

class OutputRenderer {
public:
    void render(const FileEntry& entry) const {
        const FileMetadata& meta = entry.metadata();
        FilePath safePath(entry.name());
        
        std::cout << meta.permissions().toString() << " "
                  << std::setw(2) << meta.linkCount() << " "
                  << std::left << std::setw(8) << meta.owner() << " "
                  << std::left << std::setw(8) << meta.group() << " "
                  << std::right << std::setw(6) << meta.size().toHumanReadable() << " "
                  << meta.modificationTime().format() << " "
                  << safePath.sanitized();
        
        if (entry.hasLinkTarget()) {
            std::cout << " -> " << entry.linkTarget();
        }
        
        std::cout << std::endl;
    }
    
    void renderHeader(const std::string& path) const {
        std::cout << path << ":" << std::endl;
    }
    
    void renderSeparator() const {
        std::cout << std::endl;
    }
};

// ==================== Directory Lister ====================

class DirectoryLister {
    struct DirCloser {
        void operator()(DIR* d) const { if (d) closedir(d); }
    };
    using DirPtr = std::unique_ptr<DIR, DirCloser>;
    
public:
    std::vector<FileEntry> list(const FilePath& path) const {
        std::vector<FileEntry> entries;
        
        DirPtr dir(opendir(path.toString().c_str()));
        if (!dir) {
            perror(path.toString().c_str());
            return entries;
        }
        
        struct dirent* entry;
        while ((entry = readdir(dir.get())) != nullptr) {
            entries.emplace_back(path, entry->d_name);
        }
        
        return entries;
    }
};

// ==================== Application ====================

class Application {
    DirectoryLister lister_;
    OutputRenderer renderer_;
    
public:
    void run(int argc, char* argv[]) {
        // Handle special case: "*" or "./*" means current directory
        if (argc == 2 && 
            (strcmp(argv[1], "*") == 0 || strcmp(argv[1], "./*") == 0)) {
            listDirectory(FilePath("."), false);
            return;
        }
        
        // Process each argument
        for (int i = 1; i < argc; i++) {
            FilePath path(argv[i]);
            processPath(path, argc > 2);
        }
    }
    
private:
    void processPath(const FilePath& path, bool showHeader) {
        struct stat st;
        if (lstat(path.toString().c_str(), &st) == -1) {
            perror(path.toString().c_str());
            return;
        }
        
        if (S_ISDIR(st.st_mode)) {
            listDirectory(path, showHeader);
        } else {
            // Single file
            FileEntry entry(FilePath(""), path.toString());
            renderer_.render(entry);
        }
    }
    
    void listDirectory(const FilePath& path, bool showHeader) {
        if (showHeader) {
            renderer_.renderHeader(path.toString());
        }
        
        std::vector<FileEntry> entries = lister_.list(path);
        for (const auto& entry : entries) {
            renderer_.render(entry);
        }
        
        if (showHeader) {
            renderer_.renderSeparator();
        }
    }
};

// ==================== Main ====================

int main(int argc, char* argv[]) {
    // Need iomanip for setw
    std::cout.imbue(std::locale(""));
    
    Application app;
    app.run(argc, argv);
    
    return 0;
}