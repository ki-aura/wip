#include "app/GlsApp.h"


int main(int argc, char* argv[]) {
    PermissionFormatter permFmt;
    SizeFormatter sizeFmt;
    TimeFormatter timeFmt;
    Sanitizer sanitizer;
    EntryRenderer renderer(permFmt, sizeFmt, timeFmt, sanitizer);

    DirectoryLister lister;
    GlsApp app(lister, renderer);
    return app.run(argc, argv);
}
