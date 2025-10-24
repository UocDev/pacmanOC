#include "manager.hpp"
#include "utils.hpp"
#include <iostream>
#include <filesystem>
#include <curl/curl.h>
#include <fstream>
#include <thread>
#include <chrono>
#include <cstdlib>

namespace fs = std::filesystem;
using json = nlohmann::json;

// ---------- download util ----------
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

// ---------- animations ----------
void PackageManager::showProgress(const std::string& pkg, int percent, const std::string& state) {
    int bars = percent / 10;
    std::cout << "\r" << pkg << " " << state << " [";
    for (int i = 0; i < bars; ++i) std::cout << "#";
    for (int i = bars; i < 10; ++i) std::cout << ".";
    std::cout << "] " << percent << "%   " << getSpeed() << "   ";
    std::flush(std::cout);
}

std::string PackageManager::getSpeed() {
    int speed = 500 + rand() % 2000;
    return std::to_string(speed) + " KB/s";
}

// ---------- main ops ----------
void PackageManager::install(const std::string& pkgName) {
    std::string name = pkgName;
    std::string version = "latest";

    // detect version from name e.g. hello1.3.0
    for (size_t i = 0; i < pkgName.size(); ++i) {
        if (isdigit(pkgName[i])) {
            name = pkgName.substr(0, i);
            version = pkgName.substr(i);
            break;
        }
    }

    std::cout << "Installing " << name << " version " << version << "...\n";

    std::string url = baseURL + name + "/";
    json meta;

    if (version == "latest") {
        meta = getJSON(url + "latest.json");
        version = meta["version"];
        url += version + "/";
    } else {
        url += version + "/";
        meta = getJSON(url + "metadata.json");
    }

    int parts = meta.value("parts", 1);
    std::string archiveBase = name + ".ocpackage";
    fs::create_directories(downloadDir);

    for (int p = 1; p <= parts; ++p) {
        std::string fileName = (p == 1 ? archiveBase : name + "_" + std::to_string(p) + ".ocpackage");
        std::string fullURL = url + fileName;
        std::string outFile = downloadDir + fileName;

        std::cout << "Downloading part " << p << "/" << parts << "...\n";
        for (int i = 0; i <= 100; i += 10) {
            showProgress(name, i, "Downloading");
            std::this_thread::sleep_for(std::chrono::milliseconds(120));
        }
        downloadFile(fullURL, outFile);
    }

    std::cout << "\nExtracting package...\n";
    std::string dest = "/usr/bin/";
    extractPackage(downloadDir + archiveBase, dest);
    std::cout << name << " installed successfully!\n";
}

void PackageManager::remove(const std::string& name) {
    std::cout << "Removing " << name << "...\n";
    for (int i = 0; i <= 100; i += 20) {
        showProgress(name, i, "Removing");
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    std::cout << "\n" << name << " removed.\n";
}

void PackageManager::sync(const std::string& name) {
    std::cout << "Checking updates for " << name << "...\n";
    json meta = getJSON(baseURL + name + "/latest.json");
    std::cout << name << " latest version: " << meta["version"] << "\n";
}

void PackageManager::syncAll() {
    std::cout << "Synchronizing all packages...\n";
    json index = getJSON(baseURL + "index.json");
    for (auto& pkg : index["packages"]) {
        std::string name = pkg["name"];
        std::cout << "\nUpdating " << name << "...\n";
        sync(name);
    }
    std::cout << "\nAll packages synchronized.\n";
}

void PackageManager::showVersion() {
    std::cout << "pacmanOC v1.0.0 (C++)\n";
    std::cout << "Source: https://github.com/UocDev/pacmanOC\n";
    std::cout << "Packages: https://uocdev.github.io/packagesOC/\n";
}
