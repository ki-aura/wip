#pragma once
#include "../fs/DirectoryLister.h"
#include "../fmt/EntryRenderer.h"

class GlsApp {
public:
    GlsApp(DirectoryLister& lister, EntryRenderer& renderer)
        : lister_(lister), renderer_(renderer) {}

    int run(int argc, char* argv[]);

private:
    DirectoryLister& lister_;
    EntryRenderer& renderer_;
};
