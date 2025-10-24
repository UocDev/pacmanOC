#include "core/manager.hpp"
#include <iostream>

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cout << "Usage: pacmanoc [install|remove|show|ls|dir|autoremove|-s|-S|-v] <package>\n";
        return 0;
    }

    std::string cmd = argv[1];
    PackageManager mgr;

    if (cmd == "install" && argc > 2)
        mgr.install(argv[2]);
    else if ((cmd == "remove" || cmd == "uninstall") && argc > 2)
        mgr.remove(argv[2]);
    else if (cmd == "show" && argc > 2)
        mgr.show(argv[2]);
    else if (cmd == "ls" || cmd == "list")
        mgr.list();
    else if (cmd == "dir")
        mgr.dir();
    else if (cmd == "autoremove")
        mgr.autoremove();
    else if (cmd == "-s" && argc > 2)
        mgr.sync(argv[2]);
    else if (cmd == "-S")
        mgr.syncAll();
    else if (cmd == "-v" || cmd == "version")
        mgr.showVersion();
    else
        std::cout << "Unknown command.\n";

    return 0;
}
