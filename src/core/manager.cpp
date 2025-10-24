#include "manager.hpp"
#include "utils.hpp"
#include "db.hpp"
#include <iostream>
#include <filesystem>
#include <curl/curl.h>
#include <fstream>
#include <thread>
#include <chrono>
#include <cstdlib>
#include <unistd.h> // for geteuid()
#include <iomanip>

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
    std::cout << "] " << percent << "%   ";
    std::flush(std::cout);
}

// ---------- helper ----------
static long getRemoteFileSize(const std::string& url) {
    CURL* curl = curl_easy_init();
    curl_off_t cl = 0;
    if (!curl) return 0;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
    curl_easy_setopt(curl, CURLOPT_HEADER, 0L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_perform(curl);
    curl_easy_getinfo(curl, CURLINFO_CONTENT_LENGTH_DOWNLOAD_T, &cl);
    curl_easy_cleanup(curl);
    return static_cast<long>(cl);
}

static std::string humanSize(double bytes) {
    const char* units[] = {"B", "KB", "MB", "GB"};
    int i = 0;
    while (bytes >= 1024 && i < 3) {
        bytes /= 1024;
        ++i;
    }
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(2) << bytes << " " << units[i];
    return ss.str();
}

static bool confirmAction(const std::string& msg) {
    std::cout << msg << " [Y/n] ";
    std::string input;
    std::getline(std::cin, input);
    if (input.empty() || input == "Y" || input == "y")
        return true;
    return false;
}

// ---------- main ops ----------
void PackageManager::install(const std::string& pkgName) {
    if (geteuid() != 0) {
        std::cerr << "[WARN] This operation requires root privileges.\n"
                  << "Please rerun with 'sudo pacmanoc install " << pkgName << "'\n";
        return;
    }

    Database db;
    db.load();

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

    if (db.isInstalled(name)) {
        std::cout << " Package '" << name << "' already installed (version "
                  << db.getVersion(name) << ")\n";
        return;
    }

    std::string url = baseURL + name + "/";
    auto start = std::chrono::steady_clock::now();
    std::cout << "fetching " << name << " latest version " << url << "latest.json\n";
    json latest = getJSON(url + "latest.json");
    version = latest["version"];
    std::cout << "fetching " << name << " metadata " << url << version << "/metadata.json\n";
    json meta = getJSON(url + version + "/metadata.json");
    auto end = std::chrono::steady_clock::now();
    double sec = std::chrono::duration<double>(end - start).count();

    // get file size
    std::string archiveURL = url + version + "/" + name + ".ocpackage";
    long sizeBytes = getRemoteFileSize(archiveURL);

    std::cout << "\nfetched " << humanSize(sizeBytes) << " in " << sec << "s\n";
    std::cout << "on Archives " << humanSize(sizeBytes)
              << ". after this operation, " << humanSize(sizeBytes)
              << " of additional disk space will be used.\n";

    if (!confirmAction("Do you want to continue?")) {
        std::cout << "Aborted.\n";
        return;
    }

    std::cout << "\nInstalling " << name << " version " << version << "...\n";

    int parts = meta.value("parts", 1);
    std::string archiveBase = name + ".ocpackage";
    fs::create_directories(downloadDir);

    for (int p = 1; p <= parts; ++p) {
        std::string fileName = (p == 1 ? archiveBase : name + "_" + std::to_string(p) + ".ocpackage");
        std::string fullURL = url + version + "/" + fileName;
        std::string outFile = downloadDir + fileName;

        std::cout << "Downloading part " << p << "/" << parts << "...\n";
        for (int i = 0; i <= 100; i += 10) {
            showProgress(name, i, "Downloading");
            std::this_thread::sleep_for(std::chrono::milliseconds(120));
        }
        downloadFile(fullURL, outFile);
    }

    std::cout << "\nExtracting package...\n";
    std::string dest = meta.value("destination", "/usr/bin/");
    extractPackage(downloadDir + archiveBase, dest);
    std::cout << name << " installed successfully!\n";

    db.addPackage(name, version, dest);
    db.save();
}

void PackageManager::remove(const std::string& name) {
    if (geteuid() != 0) {
        std::cerr << "[WARN] This operation requires root privileges.\n"
                  << "Please rerun with 'sudo pacmanoc remove " << name << "'\n";
        return;
    }

    Database db;
    db.load();

    if (!db.isInstalled(name)) {
        std::cout << " Package '" << name << "' is not installed.\n";
        return;
    }

    std::string path = db.getDestination(name) + "/" + name;
    double sizeBytes = 0;
    if (fs::exists(path))
        sizeBytes = fs::file_size(path);

    std::cout << "After this operation, " << humanSize(sizeBytes)
              << " of disk space will be freed.\n";

    if (!confirmAction("Do you want to continue?")) {
        std::cout << "Aborted.\n";
        return;
    }

    std::cout << "Removing " << name << "...\n";
    for (int i = 0; i <= 100; i += 20) {
        showProgress(name, i, "Removing");
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    if (fs::exists(path)) fs::remove(path);
    db.removePackage(name);
    db.save();

    std::cout << "\n" << name << " removed.\n";
}

void PackageManager::sync(const std::string& name) {
    Database db;
    db.load();

    if (!db.isInstalled(name)) {
        std::cout << " Package '" << name << "' not installed.\n";
        return;
    }

    std::cout << "Checking updates for " << name << "...\n";
    json meta = getJSON(baseURL + name + "/latest.json");
    std::string latestVer = meta["version"];
    std::string currentVer = db.getVersion(name);

    if (latestVer != currentVer) {
        std::cout << "  Update available: " << currentVer << " → " << latestVer << "\n";
        remove(name);
        install(name + latestVer);
    } else {
        std::cout << "  " << name << " already up to date.\n";
    }
}

void PackageManager::syncAll() {
    Database db;
    db.load();

    std::cout << "Synchronizing all packages...\n";
    for (auto& pkg : db.listInstalled()) {
        sync(pkg.first);
    }
    std::cout << "\nAll packages synchronized.\n";
}

void PackageManager::showVersion() {
    std::cout << "pacmanOC v1.0.0 (C++)\n";
    std::cout << "Source: https://github.com/UocDev/pacmanOC\n";
    std::cout << "Packages: https://uocdev.github.io/packagesOC/\n";
}
