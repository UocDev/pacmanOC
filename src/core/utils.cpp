#include "utils.hpp"
#include <cstdlib>

void PackageManager::extractPackage(const std::string& file, const std::string& dest) {
    std::string cmd = "tar -xf " + file + " -C " + dest;
    system(cmd.c_str());
}
