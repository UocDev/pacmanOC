#include "db.hpp"
#include <fstream>
#include <filesystem>
using json = nlohmann::json;
namespace fs = std::filesystem;

void Database::load() {
    if (!fs::exists(dbPath)) {
        fs::create_directories(fs::path(dbPath).parent_path());
        std::ofstream(dbPath) << "{}";
    }
    std::ifstream f(dbPath);
    json data; f >> data;
    for (auto& [k, v] : data.items())
        installed[k] = v;
}

void Database::save() {
    json data;
    for (auto& [k, v] : installed)
        data[k] = v;
    std::ofstream(dbPath) << data.dump(4);
}

bool Database::isInstalled(const std::string& name) {
    return installed.find(name) != installed.end();
}

void Database::addPackage(const std::string& name, const std::string& version, const std::string& dest) {
    installed[name] = {
        {"version", version},
        {"destination", dest}
    };
}

void Database::removePackage(const std::string& name) {
    installed.erase(name);
}

std::string Database::getVersion(const std::string& name) {
    return installed[name].value("version", "unknown");
}

std::string Database::getDestination(const std::string& name) {
    return installed[name].value("destination", "");
}

std::unordered_map<std::string, json> Database::listInstalled() {
    return installed;
}
