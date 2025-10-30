#include "EntryRenderer.h"
#include <pwd.h>
#include <grp.h>
#include <iomanip>

void EntryRenderer::print(std::ostream& os, const FileInfo& info) const {
    char user[64], group[64];
    struct passwd* pw = getpwuid(info.st.st_uid);
    struct group* gr = getgrgid(info.st.st_gid);
    snprintf(user, sizeof(user), "%s", pw ? pw->pw_name : "?");
    snprintf(group, sizeof(group), "%s", gr ? gr->gr_name : "?");

    std::string perms = perms_.format(info.st.st_mode);
    std::string size = size_.format(info.st.st_size);
    std::string time = time_.format(info.st.st_mtime);
    std::string name = sanitize_.sanitize(info.name);

    os << perms << " "
       << std::setw(2) << info.st.st_nlink << " "
       << std::left << std::setw(8) << user << " "
       << std::left << std::setw(8) << group << " "
       << std::right << std::setw(6) << size << " "
       << time << " " << name;

    if (S_ISLNK(info.st.st_mode) && info.link_target)
        os << " -> " << *info.link_target;

    os << "\n";
}
