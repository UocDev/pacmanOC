#include "tree.hpp"
#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

void printTree(const std::string& path, int depth) {
    for (const auto& entry : fs::directory_iterator(path)) {
        for (int i = 0; i < depth; ++i) std::cout << "│   ";
        std::cout << "├── " << entry.path().filename().string() << "\n";
        if (fs::is_directory(entry))
            printTree(entry.path().string(), depth + 1);
    }
}
