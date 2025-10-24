#pragma once
#include <string>
#include <vector>
#include "../json.hpp"

class PackageManager {
public:
    void install(const std::string& name);
    void remove(const std::string& name);
    void sync(const std::string& name);
    void syncAll();
    void showVersion();

private:
    std::string baseURL = "https://uocdev.github.io/packagesOC/";
    std::string downloadDir = "/tmp/pacmanoc/";

    void downloadFile(const std::string& url, const std::string& output);
    void extractPackage(const std::string& file, const std::string& dest);
    void showProgress(const std::string& pkg, int percent, const std::string& state);
    std::string getSpeed();
    nlohmann::json getJSON(const std::string& url);
    bool hasVersion(const std::string& name);
};
