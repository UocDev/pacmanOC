#include "core/manager.hpp"
#include <iostream>

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cout << "Usage: pacmanoc [install|remove|-s|-S|-v] <package>\n";
        return 0;
    }

    std::string cmd = argv[1];
    PackageManager mgr;

    if (cmd == "install" && argc > 2)
        mgr.install(argv[2]);
    else if ((cmd == "remove" || cmd == "uninstall") && argc > 2)
        mgr.remove(argv[2]);
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
