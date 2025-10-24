#pragma once
#include <string>
#include <vector>
#include "../json.hpp"

class PackageManager {
public:
    void install(const std::string& name);
    void remove(const std::string& name);
    void show(const std::string& name);
    void list();
    void dir();
    void autoremove();
    void sync(const std::string& name);
    void syncAll();
    void showVersion();

private:
    std::string baseURL = "https://uocdev.github.io/packagesOC/";
    std::string downloadDir = "/tmp/pacmanoc/";

    void downloadFile(const std::string& url, const std::string& output);
    void extractPackage(const std::string& file, const std::string& dest);
    void showProgress(const std::string& pkg, int percent, const std::string& state);
    nlohmann::json getJSON(const std::string& url);
    std::string humanSize(double bytes);
    bool confirmAction(const std::string& msg);
};
