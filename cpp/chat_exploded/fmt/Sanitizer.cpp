#include "Sanitizer.h"
#include <cctype>

std::string Sanitizer::sanitize(const std::string& src) const {
    std::string out;
    out.reserve(src.size());
    for (size_t i = 0; i < src.size(); ++i) {
        unsigned char uc = static_cast<unsigned char>(src[i]);
        char safe = std::isprint(static_cast<int>(uc))
                    ? static_cast<char>(uc)
                    : '?';
        out.push_back(safe);
    }
    return out;
}
