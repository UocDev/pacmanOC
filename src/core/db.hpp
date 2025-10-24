#pragma once
#include <string>
#include <unordered_map>
#include <json.hpp>

class Database {
private:
    std::string dbPath = "/usr/local/share/pacmanoc/db.json";
    std::unordered_map<std::string, nlohmann::json> installed;
public:
    void load();
    void save();
    bool isInstalled(const std::string& name);
    void addPackage(const std::string& name, const std::string& version, const std::string& dest);
    void removePackage(const std::string& name);
    std::string getVersion(const std::string& name);
    std::string getDestination(const std::string& name);
    std::unordered_map<std::string, nlohmann::json> listInstalled();
};
