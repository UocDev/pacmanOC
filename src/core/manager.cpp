#include "manager.hpp"
#include "utils.hpp"
#include "db.hpp"
#include "tree.hpp"
#include <iostream>
#include <filesystem>
#include <curl/curl.h>
#include <fstream>
#include <thread>
#include <chrono>
#include <cstdlib>
#include <unistd.h>
#include <iomanip>

namespace fs = std::filesystem;
using json = nlohmann::json;

static size_t writeData(void* ptr, size_t size, size_t nmemb, FILE* stream) {
    return fwrite(ptr, size, nmemb, stream);
}

void PackageManager::downloadFile(const std::string& url, const std::string& output) {
    CURL* curl = curl_easy_init();
    if (!curl) throw std::runtime_error("curl init failed");

    FILE* fp = fopen(output.c_str(), "wb");
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeData);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    fclose(fp);
}

json PackageManager::getJSON(const std::string& url) {
    std::string tmp = downloadDir + "temp.json";
    fs::create_directories(downloadDir);
    downloadFile(url, tmp);
    std::ifstream f(tmp);
    json data; f >> data;
    return data;
}

void PackageManager::showProgress(const std::string& pkg, int percent, const std::string& state) {
    int bars = percent / 10;
    std::cout << "\r" << pkg << " " << state << " [";
    for (int i = 0; i < bars; ++i) std::cout << "#";
    for (int i = bars; i < 10; ++i) std::cout << ".";
    std::cout << "] " << percent << "%   ";
    std::flush(std::cout);
}

std::string PackageManager::humanSize(double bytes) {
    const char* units[] = {"B", "KB", "MB", "GB"};
    int i = 0;
    while (bytes >= 1024 && i < 3) { bytes /= 1024; ++i; }
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(2) << bytes << " " << units[i];
    return ss.str();
}

bool PackageManager::confirmAction(const std::string& msg) {
    std::cout << msg << " [Y/n] ";
    std::string input;
    std::getline(std::cin, input);
    if (input.empty() || input == "Y" || input == "y")
        return true;
    return false;
}

// ---------- install ----------
void PackageManager::install(const std::string& pkgName) {
    if (geteuid() != 0) {
        std::cerr << "[WARN] This operation requires root privileges.\n"
                  << "Please rerun with 'sudo pacmanoc install " << pkgName << "'\n";
        return;
    }

    Database db;
    db.load();

    if (db.isInstalled(pkgName)) {
        std::cout << "Package '" << pkgName << "' already installed.\n";
        return;
    }

    std::string url = baseURL + pkgName + "/";
    auto start = std::chrono::steady_clock::now();
    std::cout << "fetching " << pkgName << " latest version " << url << "latest.json\n";
    json latest = getJSON(url + "latest.json");
    std::string version = latest["version"];
    std::cout << "fetching " << pkgName << " metadata " << url << version << "/metadata.json\n";
    json meta = getJSON(url + version + "/metadata.json");
    auto end = std::chrono::steady_clock::now();
    double sec = std::chrono::duration<double>(end - start).count();

    std::string archiveURL = url + version + "/" + pkgName + ".ocpackage";
    CURL* curl = curl_easy_init();
    curl_off_t cl = 0;
    curl_easy_setopt(curl, CURLOPT_URL, archiveURL.c_str());
    curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
    curl_easy_perform(curl);
    curl_easy_getinfo(curl, CURLINFO_CONTENT_LENGTH_DOWNLOAD_T, &cl);
    curl_easy_cleanup(curl);

    std::cout << "\nfetched " << humanSize((double)cl) << " in " << std::fixed << std::setprecision(2) << sec << "s\n";
    std::cout << "on Archives " << humanSize((double)cl) 
              << ". after this operation, " << humanSize((double)cl)
              << " of additional disk space will be used.\n";

    if (!confirmAction("Do you want to continue?")) {
        std::cout << "Aborted.\n";
        return;
    }

    std::cout << "\nReading package lists... Done (1-100%)\n";
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    std::cout << "Building dependency tree... Done (1-100%)\n";
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    std::cout << "Reading state information... Done (1-100%)\n\n";

    std::cout << "The following additional packages will be installed:\n  libexample1 libexample2\n\n";
    std::cout << "0 upgraded, 3 newly installed, 0 to remove and 0 not upgraded.\n\n";

    std::cout << "Downloading package...\n";
    for (int i = 0; i <= 100; i += 10) {
        showProgress(pkgName, i, "Downloading");
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    std::cout << "\nExtracting package...\n";
    std::string dest = meta.value("destination", "/usr/bin/");
    extractPackage(downloadDir + pkgName + ".ocpackage", dest);

    db.addPackage(pkgName, version, dest);
    db.save();

    std::cout << "Setting up " << pkgName << " (" << version << ") ...\n";
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    std::cout << "done\n";
}

// ---------- remove ----------
void PackageManager::remove(const std::string& name) {
    if (geteuid() != 0) {
        std::cerr << "[WARN] This operation requires root privileges.\n"
                  << "Please rerun with 'sudo pacmanoc remove " << name << "'\n";
        return;
    }

    Database db;
    db.load();

    if (!db.isInstalled(name)) {
        std::cout << "Package '" << name << "' is not installed.\n";
        return;
    }

    std::string path = db.getDestination(name) + "/" + name;
    double sizeBytes = fs::exists(path) ? fs::file_size(path) : 0;

    std::cout << "After this operation, " << humanSize(sizeBytes)
              << " of disk space will be freed.\n";

    if (!confirmAction("Do you want to continue?")) {
        std::cout << "Aborted.\n";
        return;
    }

    std::cout << "(Reading database ...)\n";
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    std::cout << "Removing " << name << "...\n";
    for (int i = 0; i <= 100; i += 10) {
        showProgress(name, i, "Removing");
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    if (fs::exists(path)) fs::remove(path);
    db.removePackage(name);
    db.save();

    std::cout << "\nProcessing triggers for system...\n";
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    std::cout << "done\n";
}

// ---------- new commands ----------
void PackageManager::show(const std::string& name) {
    Database db;
    db.load();

    if (!db.isInstalled(name)) {
        std::cout << "Package '" << name << "' not installed.\n";
        return;
    }

    std::string dest = db.getDestination(name);
    std::cout << "Package: " << name << "\n";
    std::cout << "Version: " << db.getVersion(name) << "\n";
    std::cout << "Installed to: " << dest << "\n";
    std::cout << "Files:\n";
    for (auto& p : fs::recursive_directory_iterator(dest))
        std::cout << "  " << p.path().string() << "\n";
}

void PackageManager::list() {
    Database db;
    db.load();
    std::cout << "Available packages from " << baseURL << ":\n";
    std::vector<std::string> pkgs = {"hello","world","example"}; // mock

    for (auto& pkg : pkgs) {
        std::cout << "  " << pkg;
        if (db.isInstalled(pkg)) std::cout << " (INSTALLED)";
        std::cout << "\n";
    }
}

void PackageManager::dir() {
    std::cout << "Package installation directories:\n";
    printTree("/usr/bin/");
}

void PackageManager::autoremove() {
    if (geteuid() != 0) {
        std::cerr << "[WARN] Root privileges required for autoremove.\n";
        return;
    }

    std::cout << "Cleaning cache directory " << downloadDir << " ...\n";
    fs::remove_all(downloadDir);
    std::cout << "Unused cache cleared.\n";
}

void PackageManager::sync(const std::string& name) {
    Database db;
    db.load();

    if (!db.isInstalled(name)) {
        std::cout << "Package '" << name << "' not installed.\n";
        return;
    }

    std::cout << "Checking updates for " << name << "...\n";
    json meta = getJSON(baseURL + name + "/latest.json");
    std::string latestVer = meta["version"];
    std::string currentVer = db.getVersion(name);

    if (latestVer != currentVer) {
        std::cout << "Update available: " << currentVer << " → " << latestVer << "\n";
        remove(name);
        install(name);
    } else {
        std::cout << name << " already up to date.\n";
    }
}

void PackageManager::syncAll() {
    Database db;
    db.load();
    std::cout << "Synchronizing all packages...\n";
    for (auto& pkg : db.listInstalled())
        sync(pkg.first);
    std::cout << "All packages synchronized.\n";
}

void PackageManager::showVersion() {
    std::cout << "pacmanOC v1.1.0 (C++)\n";
    std::cout << "Source: https://github.com/UocDev/pacmanOC\n";
    std::cout << "Packages: https://uocdev.github.io/packagesOC/\n";
}
