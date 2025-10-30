#include "GlsApp.h"
#include <sys/stat.h>
#include <cstring>
#include <iostream>

int GlsApp::run(int argc, char* argv[]) {
    if (argc == 2 &&
        (strcmp(argv[1], "*") == 0 || strcmp(argv[1], "./*") == 0)) {
        auto files = lister_.list(".");
        for (auto& f : files) renderer_.print(std::cout, f);
        return 0;
    }

    for (int i = 1; i < argc; ++i) {
        struct stat st{};
        if (lstat(argv[i], &st) == -1) {
            perror(argv[i]);
            continue;
        }

        if (S_ISDIR(st.st_mode)) {
            bool showHeader = (argc > 2);
            if (showHeader) std::cout << argv[i] << ":\n";
            auto files = lister_.list(argv[i]);
            for (auto& f : files) renderer_.print(std::cout, f);
            if (showHeader) std::cout << "\n";
        } else {
            renderer_.print(std::cout, FileInfo(argv[i], st));
        }
    }

    return 0;
}
