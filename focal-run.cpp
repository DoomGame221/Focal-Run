#include <iostream>
#include <vector>
#include <string>
#include <filesystem>
#include <cstdlib>
#include <map>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <thread>
#include <mutex>
#include <chrono>
#include <future>
#include <unordered_map>
#include <unordered_set>
#include <iomanip>

namespace fs = std::filesystem;

// Platform-specific definitions
#ifdef _WIN32
    #include <windows.h>
    #define QUIET_REDIRECT " >nul 2>&1"
    #define PATH_SEPARATOR "\\"
#else
    #define QUIET_REDIRECT " >/dev/null 2>&1"
    #define PATH_SEPARATOR "/"
#endif

// ANSI Color codes
namespace Color {
    const std::string RESET = "\033[0m";
    const std::string RED = "\033[31m";
    const std::string GREEN = "\033[32m";
    const std::string YELLOW = "\033[33m";
    const std::string BLUE = "\033[34m";
    const std::string MAGENTA = "\033[35m";
    const std::string CYAN = "\033[36m";
    const std::string BOLD = "\033[1m";
}

enum class BuildSystem {
    CMake,
    Make,
    Ninja,
    MinGW,
    GCC,
    Rust,
    Auto
};

struct BuildStats {
    std::chrono::milliseconds configTime{0};
    std::chrono::milliseconds buildTime{0};
    std::chrono::milliseconds totalTime{0};
};

struct ProjectInfo {
    std::string name;
    std::string path;
    bool success = false;
    std::string buildType = "Release";
    BuildSystem buildSystem = BuildSystem::Auto;
    std::string detectedGenerator = "";
    bool isMakefileProject = false;
    BuildStats stats;
};

class FocalRun {
private:
    std::vector<ProjectInfo> projects;
    std::string buildType = "Release";
    bool rebuildMode = false;
    bool cleanMode = false;
    bool allMode = false;
    bool verboseMode = false;
    bool buildMode = false;
    bool checkMode = false;
    bool colorOutput = true;
    bool showProgress = true;
    std::string targetProject = "";
    std::string singleFile = "";
    std::string customPath = ".";
    std::vector<std::string> cleanedBuildDirs;
    std::mutex outputMutex;
    std::unordered_map<std::string, std::string> configCache;
    std::unordered_map<std::string, std::unordered_set<std::string>> dependencyGraph;
    bool rustMode = false;
    std::string cargoCommand = "build";
    size_t maxConcurrentBuilds = 8;

    // Enable color output on Windows
    void enableColorOutput() {
#ifdef _WIN32
        HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
        DWORD dwMode = 0;
        if (GetConsoleMode(hOut, &dwMode)) {
            SetConsoleMode(hOut, dwMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
        }
#endif
    }

    // Colorize output if enabled
    std::string colorize(const std::string& text, const std::string& color) {
        if (!colorOutput) return text;
        return color + text + Color::RESET;
    }

    // Escape shell arguments to prevent injection
    std::string escapeShellArg(const std::string& arg) {
        std::string result = arg;
#ifdef _WIN32
        // Windows: escape quotes
        size_t pos = 0;
        while ((pos = result.find('"', pos)) != std::string::npos) {
            result.insert(pos, "\\");
            pos += 2;
        }
#else
        // Unix: escape dangerous characters
        const std::string dangerous = "`$\\\"!";
        for (char c : dangerous) {
            size_t pos = 0;
            while ((pos = result.find(c, pos)) != std::string::npos) {
                result.insert(pos, "\\");
                pos += 2;
            }
        }
#endif
        return result;
    }

    // Check if command is available
    bool isCommandAvailable(const std::string& cmd) {
#ifdef _WIN32
        std::string checkCmd = "where " + cmd + QUIET_REDIRECT;
#else
        std::string checkCmd = "which " + cmd + QUIET_REDIRECT;
#endif
        return system(checkCmd.c_str()) == 0;
    }

    // Check Visual Studio installation
    bool checkVisualStudioInstalled() {
#ifdef _WIN32
        std::vector<std::string> vsPaths = {
            "C:\\Program Files\\Microsoft Visual Studio\\2022\\Community",
            "C:\\Program Files\\Microsoft Visual Studio\\2022\\Professional",
            "C:\\Program Files\\Microsoft Visual Studio\\2022\\Enterprise",
            "C:\\Program Files (x86)\\Microsoft Visual Studio\\2019\\Community",
            "C:\\Program Files (x86)\\Microsoft Visual Studio\\2019\\Professional",
            "C:\\Program Files (x86)\\Microsoft Visual Studio\\2019\\Enterprise"
        };

        for (const auto& path : vsPaths) {
            if (fs::exists(path)) {
                return true;
            }
        }

        if (isCommandAvailable("vswhere")) {
            return true;
        }
#endif
        return false;
    }

    // Detect generator from CMakeLists.txt
    std::string detectGenerator(const std::string& projectPath) {
        std::string cmakelists = projectPath + "/CMakeLists.txt";
        std::ifstream file(cmakelists);
        std::string line;
        std::string detectedGen = "";

        if (!file.is_open()) {
            return "";
        }

        while (std::getline(file, line)) {
            line.erase(0, line.find_first_not_of(" \t"));

            if (line.find("# Focal-Generator:") == 0) {
                detectedGen = line.substr(18);
                detectedGen.erase(0, detectedGen.find_first_not_of(" \t"));
                detectedGen.erase(detectedGen.find_last_not_of(" \t\n\r") + 1);

                if (detectedGen == "VS162019") detectedGen = "Visual Studio 16 2019";
                else if (detectedGen == "VS172022") detectedGen = "Visual Studio 17 2022";
                else if (detectedGen == "MinGW") detectedGen = "MinGW Makefiles";
                else if (detectedGen == "Ninja") detectedGen = "Ninja";
                else if (detectedGen == "Make") detectedGen = "Unix Makefiles";

                break;
            }

            if (line.find("CMAKE_GENERATOR") != std::string::npos) {
                size_t start = line.find("\"");
                size_t end = line.rfind("\"");
                if (start != std::string::npos && end != std::string::npos && start != end) {
                    detectedGen = line.substr(start + 1, end - start - 1);
                    break;
                }
            }
        }

        file.close();
        return detectedGen;
    }

    // Select generator automatically
    std::string selectGenerator() {
        if (isCommandAvailable("ninja")) {
            return "Ninja";
        }
        if (isCommandAvailable("mingw32-make")) {
            return "MinGW Makefiles";
        }
        if (isCommandAvailable("make")) {
            return "Unix Makefiles";
        }

#ifdef _WIN32
        return "Visual Studio 17 2022";
#else
        return "Unix Makefiles";
#endif
    }

    // Determine build system
    void determineBuildSystem(ProjectInfo& proj) {
        std::string cacheKey = proj.path + "_generator";
        std::string detected;

        if (proj.buildSystem == BuildSystem::Rust) {
            proj.detectedGenerator = "Cargo";
            configCache[cacheKey] = proj.detectedGenerator;
            return;
        }

        if (configCache.count(cacheKey)) {
            detected = configCache[cacheKey];
            if (verboseMode) {
                std::cout << colorize("  [CACHE]", Color::CYAN) 
                         << " Using cached generator: " << detected << std::endl;
            }
        } else {
            detected = detectGenerator(proj.path);
        }

        if (!detected.empty()) {
            proj.detectedGenerator = detected;
            configCache[cacheKey] = detected;
            if (verboseMode && !configCache.count(cacheKey)) {
                std::cout << colorize("  [DETECT]", Color::BLUE) 
                         << " Found generator: " << detected << std::endl;
            }
        } else {
            proj.detectedGenerator = selectGenerator();
            configCache[cacheKey] = proj.detectedGenerator;
            if (verboseMode) {
                std::cout << colorize("  [AUTO]", Color::MAGENTA) 
                         << " Selected generator: " << proj.detectedGenerator << std::endl;
            }
        }
    }

    // Show progress bar
    void showProgressBar(int current, int total, const std::string& status = "") {
        if (!showProgress || verboseMode) return;

        std::lock_guard<std::mutex> lock(outputMutex);
        int barWidth = 50;
        float progress = (float)current / total;
        int pos = barWidth * progress;

        std::cout << "\r[";
        for (int i = 0; i < barWidth; ++i) {
            if (i < pos) std::cout << "=";
            else if (i == pos) std::cout << ">";
            else std::cout << " ";
        }
        std::cout << "] " << int(progress * 100.0) << "% (" 
                  << current << "/" << total << ")";
        
        if (!status.empty()) {
            std::cout << " " << status;
        }
        
        std::cout << std::flush;

        if (current == total) {
            std::cout << std::endl;
        }
    }

public:
    FocalRun(int argc, char* argv[]) {
        enableColorOutput();
        parseArguments(argc, argv);
        
        // Set max concurrent builds based on hardware
        unsigned int hwThreads = std::thread::hardware_concurrency();
        maxConcurrentBuilds = std::min((size_t)hwThreads, maxConcurrentBuilds);
    }

    void printHelp() {
        std::cout << "\n" << colorize(std::string(70, '='), Color::BOLD) << std::endl;
        std::cout << colorize("  FOCAL-RUN - Multi-Platform Build Tool", Color::BOLD + Color::CYAN) << std::endl;
        std::cout << colorize(std::string(70, '='), Color::BOLD) << "\n" << std::endl;

        std::cout << colorize("DESCRIPTION:", Color::BOLD) << std::endl;
        std::cout << "  Focal-Run automates building CMake, Makefile, and Rust projects.\n"
                  << "  Supports parallel builds, auto-detection, and caching.\n" << std::endl;

        std::cout << colorize("USAGE:", Color::BOLD) << std::endl;
        std::cout << "  focal-run [OPTIONS] [PROJECT_NAME]\n" << std::endl;

        std::cout << colorize("COMMANDS:", Color::BOLD) << std::endl;
        std::cout << "  --scan                    Display all available projects" << std::endl;
        std::cout << "  --check                   Check build tools availability" << std::endl;
        std::cout << "  --help, -h                Show this help message" << std::endl << std::endl;

        std::cout << colorize("RUST SPECIFIC:", Color::BOLD) << std::endl;
        std::cout << "  --rust                    Filter to Rust projects only" << std::endl;
        std::cout << "  --test                    Run cargo test" << std::endl;
        std::cout << "  --doc                     Generate documentation" << std::endl;
        std::cout << "  --run                     Run with cargo run" << std::endl;
        std::cout << "  --cargo-check             Check project without building" << std::endl;
        std::cout << "  --bench                   Run benchmarks" << std::endl << std::endl;

        std::cout << colorize("BUILD OPTIONS:", Color::BOLD) << std::endl;
        std::cout << "  --debug                   Build in Debug mode" << std::endl;
        std::cout << "  --release                 Build in Release mode (default)" << std::endl;
        std::cout << "  --rebuild                 Clean and rebuild" << std::endl;
        std::cout << "  --clean                   Clean build directories" << std::endl;
        std::cout << "  --all                     Apply to all projects" << std::endl;
        std::cout << "  --verbose                 Show detailed output" << std::endl;
        std::cout << "  --no-color                Disable colored output" << std::endl;
        std::cout << "  --path=<path>             Custom path to scan (default: .)" << std::endl;
        std::cout << "  --jobs=<n>                Max concurrent builds (default: auto)" << std::endl << std::endl;

        std::cout << colorize("EXAMPLES:", Color::BOLD) << std::endl;
        std::cout << "  focal-run --scan                    # List all projects" << std::endl;
        std::cout << "  focal-run --MyProject --release     # Build specific project" << std::endl;
        std::cout << "  focal-run --all --debug             # Build all in debug" << std::endl;
        std::cout << "  focal-run --test.cpp --build        # Compile single file" << std::endl;
        std::cout << "  focal-run --rust --all --test       # Test all Rust projects" << std::endl;
        std::cout << "  focal-run --clean --all             # Clean everything" << std::endl << std::endl;

        std::cout << colorize("NOTES:", Color::BOLD) << std::endl;
        std::cout << "  - Auto-detects: CMake, Make, Ninja, MinGW, Visual Studio, Cargo" << std::endl;
        std::cout << "  - Parallel building with thread pool" << std::endl;
        std::cout << "  - Configuration caching for speed" << std::endl;
        std::cout << "  - Specify generator: # Focal-Generator: Ninja" << std::endl;
        std::cout << "  - Cache file: .focal-run-cache" << std::endl << std::endl;

        std::cout << colorize(std::string(70, '='), Color::BOLD) << "\n" << std::endl;
    }

    void parseArguments(int argc, char* argv[]) {
        for (int i = 1; i < argc; i++) {
            std::string arg = argv[i];
            
            if (arg == "--all") allMode = true;
            else if (arg == "--rebuild") rebuildMode = true;
            else if (arg == "--clean") cleanMode = true;
            else if (arg == "--build") {
                buildMode = true;
                cleanMode = false;
                allMode = true;
            }
            else if (arg == "--check") checkMode = true;
            else if (arg == "--debug") buildType = "Debug";
            else if (arg == "--release") buildType = "Release";
            else if (arg == "--verbose") verboseMode = true;
            else if (arg == "--no-color") colorOutput = false;
            else if (arg == "--rust") rustMode = true;
            else if (arg == "--test") cargoCommand = "test";
            else if (arg == "--doc") cargoCommand = "doc";
            else if (arg == "--run") cargoCommand = "run";
            else if (arg == "--cargo-check") cargoCommand = "check";
            else if (arg == "--bench") cargoCommand = "bench";
            else if (arg == "--scan") {
                scanProjects();
                printProjects();
                exit(0);
            }
            else if (arg == "--help" || arg == "-h") {
                printHelp();
                exit(0);
            }
            else if (arg.substr(0, 7) == "--path=") {
                customPath = arg.substr(7);
            }
            else if (arg.substr(0, 7) == "--jobs=") {
                try {
                    maxConcurrentBuilds = std::stoi(arg.substr(7));
                    if (maxConcurrentBuilds < 1) maxConcurrentBuilds = 1;
                } catch (...) {
                    std::cerr << colorize("Invalid --jobs value", Color::RED) << std::endl;
                }
            }
            else if (arg.substr(0, 2) == "--") {
                std::string potentialFile = arg.substr(2);
                if (potentialFile.find(".cpp") != std::string::npos) {
                    singleFile = potentialFile;
                    buildMode = true;
                } else {
                    targetProject = potentialFile;
                }
            }
        }
    }

    void scanProjects(const std::string& startPath = ".") {
        std::string actualPath = (startPath == ".") ? customPath : startPath;
        try {
            std::string currentCMake = actualPath + "/CMakeLists.txt";
            std::string currentMakefile = actualPath + "/Makefile";
            std::string currentCargo = actualPath + "/Cargo.toml";

            if (fs::exists(currentCMake)) {
                std::string projectName = fs::absolute(actualPath).filename().string();
                if (projectName.empty() || projectName == ".") {
                    projectName = "RootProject";
                }

                ProjectInfo proj;
                proj.name = projectName;
                proj.path = fs::absolute(actualPath).string();
                proj.buildType = buildType;
                proj.buildSystem = BuildSystem::CMake;
                proj.isMakefileProject = false;
                projects.push_back(proj);
            } 
            else if (fs::exists(currentMakefile)) {
                std::string projectName = fs::absolute(actualPath).filename().string();
                if (projectName.empty() || projectName == ".") {
                    projectName = "RootProject";
                }

                ProjectInfo proj;
                proj.name = projectName;
                proj.path = fs::absolute(actualPath).string();
                proj.buildType = buildType;
                proj.buildSystem = BuildSystem::Make;
                proj.isMakefileProject = true;
                proj.detectedGenerator = "Make";
                projects.push_back(proj);
            } 
            else if (fs::exists(currentCargo)) {
                std::string projectName = fs::absolute(actualPath).filename().string();
                if (projectName.empty() || projectName == ".") {
                    projectName = "RootProject";
                }

                ProjectInfo proj;
                proj.name = projectName;
                proj.path = fs::absolute(actualPath).string();
                proj.buildType = buildType;
                proj.buildSystem = BuildSystem::Rust;
                proj.isMakefileProject = false;
                proj.detectedGenerator = "Cargo";
                projects.push_back(proj);
            }

            // Scan subdirectories
            for (const auto& entry : fs::recursive_directory_iterator(actualPath)) {
                if (entry.is_regular_file()) {
                    std::string filename = entry.path().filename().string();
                    std::string projectDir = entry.path().parent_path().string();
                    
                    // Skip if already added
                    if (projectDir == fs::absolute(actualPath).string()) {
                        continue;
                    }

                    if (filename == "CMakeLists.txt") {
                        std::string projectName = fs::path(projectDir).filename().string();
                        ProjectInfo proj;
                        proj.name = projectName;
                        proj.path = projectDir;
                        proj.buildType = buildType;
                        proj.buildSystem = BuildSystem::CMake;
                        proj.isMakefileProject = false;
                        projects.push_back(proj);
                    } 
                    else if (filename == "Makefile") {
                        std::string cmakePath = projectDir + "/CMakeLists.txt";
                        if (fs::exists(cmakePath)) continue;

                        std::string projectName = fs::path(projectDir).filename().string();
                        ProjectInfo proj;
                        proj.name = projectName;
                        proj.path = projectDir;
                        proj.buildType = buildType;
                        proj.buildSystem = BuildSystem::Make;
                        proj.isMakefileProject = true;
                        proj.detectedGenerator = "Make";
                        projects.push_back(proj);
                    } 
                    else if (filename == "Cargo.toml") {
                        std::string projectName = fs::path(projectDir).filename().string();
                        ProjectInfo proj;
                        proj.name = projectName;
                        proj.path = projectDir;
                        proj.buildType = buildType;
                        proj.buildSystem = BuildSystem::Rust;
                        proj.isMakefileProject = false;
                        proj.detectedGenerator = "Cargo";
                        projects.push_back(proj);
                    }
                }
            }

            // Sort by depth (root first)
            std::sort(projects.begin(), projects.end(), [](const ProjectInfo& a, const ProjectInfo& b) {
                int depthA = std::count(a.path.begin(), a.path.end(), fs::path::preferred_separator);
                int depthB = std::count(b.path.begin(), b.path.end(), fs::path::preferred_separator);
                return depthA < depthB;
            });
        } catch (const std::exception& e) {
            std::cerr << colorize("Error scanning projects: ", Color::RED) << e.what() << std::endl;
        }
    }

    void filterProjects() {
        if (!allMode && !targetProject.empty()) {
            auto it = std::find_if(projects.begin(), projects.end(),
                [this](const ProjectInfo& p) { return p.name == targetProject; });

            if (it != projects.end()) {
                ProjectInfo proj = *it;
                projects.clear();
                projects.push_back(proj);
            } else {
                std::cerr << colorize("Project '", Color::RED) << targetProject 
                         << colorize("' not found!", Color::RED) << std::endl;
                exit(1);
            }
        }

        if (rustMode) {
            projects.erase(std::remove_if(projects.begin(), projects.end(),
                [](const ProjectInfo& p) { return p.buildSystem != BuildSystem::Rust; }), 
                projects.end());
        }
    }

    bool buildProject(ProjectInfo& proj) {
        if (proj.isMakefileProject) {
            return buildMakefileProject(proj);
        }

        if (proj.buildSystem == BuildSystem::Rust) {
            return buildRustProject(proj);
        }

        return buildCMakeProject(proj);
    }

    bool buildCMakeProject(ProjectInfo& proj) {
        std::lock_guard<std::mutex> lock(outputMutex);
        
        determineBuildSystem(proj);
        std::string buildDir = proj.path + "/build";

        // Clean mode
        if (cleanMode) {
            std::vector<std::string> dirsToClean = {buildDir, proj.path + "/release", proj.path + "/debug"};
            bool cleanedAny = false;

            for (const auto& dir : dirsToClean) {
                if (fs::exists(dir)) {
                    try {
                        fs::remove_all(dir);
                        std::cout << colorize("  [CLEAN]", Color::YELLOW) 
                                 << " Cleaned: " << dir << std::endl;
                        cleanedAny = true;
                    } catch (const std::exception& e) {
                        std::cerr << colorize("  [ERROR]", Color::RED) 
                                 << " Cleaning " << dir << ": " << e.what() << std::endl;
                    }
                }
            }

            if (!cleanedAny) {
                std::cout << colorize("  [INFO]", Color::BLUE) 
                         << " No build directories to clean" << std::endl;
            }
            return true;
        }

        // Rebuild mode
        if (rebuildMode && fs::exists(buildDir)) {
            try {
                fs::remove_all(buildDir);
                std::cout << colorize("  [REBUILD]", Color::YELLOW) 
                         << " Cleaned: " << buildDir << std::endl;
            } catch (const std::exception& e) {
                std::cerr << colorize("  [ERROR]", Color::RED) 
                         << " Cleaning: " << e.what() << std::endl;
                return false;
            }
        }

        // Create build directory
        if (!fs::exists(buildDir)) {
            try {
                fs::create_directories(buildDir);
                if (verboseMode) {
                    std::cout << colorize("  [CREATE]", Color::GREEN) 
                             << " Build directory: " << buildDir << std::endl;
                }
            } catch (const std::exception& e) {
                std::cerr << colorize("  [ERROR]", Color::RED) 
                         << " Creating build dir: " << e.what() << std::endl;
                return false;
            }
        }

        // Verify CMakeLists.txt
        std::string cmakeListsPath = proj.path + "/CMakeLists.txt";
        if (!fs::exists(cmakeListsPath)) {
            std::cerr << colorize("  [ERROR]", Color::RED) 
                     << " CMakeLists.txt not found in " << proj.path << std::endl;
            return false;
        }

        // Configure
        std::string escapedPath = escapeShellArg(proj.path);
        std::string escapedBuildDir = escapeShellArg(buildDir);
        std::string configCmd = "cmake -S \"" + escapedPath + "\" -B \"" + escapedBuildDir
                                + "\" -G \"" + proj.detectedGenerator + "\"";
        
        if (verboseMode) {
            std::cout << colorize("  [CONFIG]", Color::CYAN) 
                     << " Generator: " << proj.detectedGenerator << std::endl;
            std::cout << colorize("  [CMD]", Color::MAGENTA) 
                     << " " << configCmd << std::endl;
        }

        auto startTime = std::chrono::high_resolution_clock::now();
        int configResult = system(verboseMode ? configCmd.c_str() : (configCmd + QUIET_REDIRECT).c_str());
        auto endTime = std::chrono::high_resolution_clock::now();
        proj.stats.configTime = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

        if (verboseMode) {
            std::cout << colorize("  [TIME]", Color::BLUE) 
                     << " Configuration: " << proj.stats.configTime.count() << "ms" << std::endl;
        }

        if (configResult != 0) {
            std::cerr << colorize("  [FAILED]", Color::RED) << " CMake configuration failed!" << std::endl;
            std::cerr << "  Project: " << proj.path << std::endl;
            std::cerr << "  Generator: " << proj.detectedGenerator << std::endl;
            if (!verboseMode) {
                std::cerr << "  Run with --verbose for details" << std::endl;
            }
            return false;
        }

        // Build
        std::string buildCmd = "cmake --build \"" + escapedBuildDir + "\" --config " + proj.buildType;
        
        if (verboseMode) {
            std::cout << colorize("  [BUILD]", Color::GREEN) << " " << buildCmd << std::endl;
        }

        startTime = std::chrono::high_resolution_clock::now();
        int buildResult = system(verboseMode ? buildCmd.c_str() : (buildCmd + QUIET_REDIRECT).c_str());
        endTime = std::chrono::high_resolution_clock::now();
        proj.stats.buildTime = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
        proj.stats.totalTime = proj.stats.configTime + proj.stats.buildTime;

        if (verboseMode) {
            std::cout << colorize("  [TIME]", Color::BLUE) 
                     << " Build: " << proj.stats.buildTime.count() << "ms" << std::endl;
            std::cout << colorize("  [TIME]", Color::BLUE) 
                     << " Total: " << proj.stats.totalTime.count() << "ms" << std::endl;
        }

        if (buildResult != 0) {
            std::cerr << colorize("  [FAILED]", Color::RED) << " Build failed!" << std::endl;
            if (!verboseMode) {
                std::cerr << "  Run with --verbose for details" << std::endl;
            }
            return false;
        }

        return true;
    }

    bool buildRustProject(ProjectInfo& proj) {
        std::lock_guard<std::mutex> lock(outputMutex);

        if (cleanMode) {
            std::string escapedPath = escapeShellArg(proj.path);
            std::string cleanCmd = "cd \"" + escapedPath + "\" && cargo clean";
            
            if (verboseMode) {
                std::cout << colorize("  [CLEAN]", Color::YELLOW) << " " << cleanCmd << std::endl;
            }

            auto startTime = std::chrono::high_resolution_clock::now();
            int cleanResult = system(verboseMode ? cleanCmd.c_str() : (cleanCmd + QUIET_REDIRECT).c_str());
            auto endTime = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

            if (verboseMode) {
                std::cout << colorize("  [TIME]", Color::BLUE) 
                         << " Clean: " << duration.count() << "ms" << std::endl;
            }

            if (cleanResult != 0) {
                std::cerr << colorize("  [FAILED]", Color::RED) << " Clean failed!" << std::endl;
                return false;
            }
            return true;
        }

        std::string escapedPath = escapeShellArg(proj.path);
        std::string buildCmd = "cd \"" + escapedPath + "\" && cargo " + cargoCommand;
        
        if (cargoCommand == "build" || cargoCommand == "test" || 
            cargoCommand == "bench" || cargoCommand == "check") {
            if (buildType == "Release") {
                buildCmd += " --release";
            }
        }

        if (verboseMode) {
            std::cout << colorize("  [CARGO]", Color::GREEN) << " " << buildCmd << std::endl;
        }

        auto startTime = std::chrono::high_resolution_clock::now();
        int buildResult = system(verboseMode ? buildCmd.c_str() : (buildCmd + QUIET_REDIRECT).c_str());
        auto endTime = std::chrono::high_resolution_clock::now();
        proj.stats.buildTime = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
        proj.stats.totalTime = proj.stats.buildTime;

        if (verboseMode) {
            std::cout << colorize("  [TIME]", Color::BLUE) 
                     << " Build: " << proj.stats.buildTime.count() << "ms" << std::endl;
        }

        if (buildResult != 0) {
            std::cerr << colorize("  [FAILED]", Color::RED) << " Build failed!" << std::endl;
            return false;
        }

        return true;
    }

    bool buildMakefileProject(ProjectInfo& proj) {
        std::lock_guard<std::mutex> lock(outputMutex);

        if (cleanMode) {
            std::string escapedPath = escapeShellArg(proj.path);
            std::string cleanCmd = "cd \"" + escapedPath + "\" && make clean";
            
            if (verboseMode) {
                std::cout << colorize("  [CLEAN]", Color::YELLOW) << " " << cleanCmd << std::endl;
            }

            auto startTime = std::chrono::high_resolution_clock::now();
            int cleanResult = system(verboseMode ? cleanCmd.c_str() : (cleanCmd + QUIET_REDIRECT).c_str());
            auto endTime = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

            if (verboseMode) {
                std::cout << colorize("  [TIME]", Color::BLUE) 
                         << " Clean: " << duration.count() << "ms" << std::endl;
            }

            std::vector<std::string> dirsToClean = {proj.path + "/release", proj.path + "/debug"};
            for (const auto& dir : dirsToClean) {
                if (fs::exists(dir)) {
                    try {
                        fs::remove_all(dir);
                        std::cout << colorize("  [CLEAN]", Color::YELLOW) 
                                 << " Cleaned: " << dir << std::endl;
                    } catch (const std::exception& e) {
                        std::cerr << colorize("  [ERROR]", Color::RED) 
                                 << " Cleaning " << dir << ": " << e.what() << std::endl;
                    }
                }
            }

            return (cleanResult == 0);
        }

        // Create directories
        std::vector<std::string> dirsToCreate = {proj.path + "/release", proj.path + "/debug"};
        for (const auto& dir : dirsToCreate) {
            if (!fs::exists(dir)) {
                try {
                    fs::create_directories(dir);
                    if (verboseMode) {
                        std::cout << colorize("  [CREATE]", Color::GREEN) 
                                 << " Directory: " << dir << std::endl;
                    }
                } catch (const std::exception& e) {
                    std::cerr << colorize("  [ERROR]", Color::RED) 
                             << " Creating " << dir << ": " << e.what() << std::endl;
                }
            }
        }

        // Build
        std::string escapedPath = escapeShellArg(proj.path);
        std::string buildCmd = "cd \"" + escapedPath + "\" && make";

        if (verboseMode) {
            std::cout << colorize("  [MAKE]", Color::GREEN) << " " << buildCmd << std::endl;
        }

        auto startTime = std::chrono::high_resolution_clock::now();
        int buildResult = system(verboseMode ? buildCmd.c_str() : (buildCmd + QUIET_REDIRECT).c_str());
        auto endTime = std::chrono::high_resolution_clock::now();
        proj.stats.buildTime = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
        proj.stats.totalTime = proj.stats.buildTime;

        if (verboseMode) {
            std::cout << colorize("  [TIME]", Color::BLUE) 
                     << " Build: " << proj.stats.buildTime.count() << "ms" << std::endl;
        }

        if (buildResult != 0) {
            std::cerr << colorize("  [FAILED]", Color::RED) << " Build failed!" << std::endl;
            return false;
        }

        return true;
    }

    void loadConfigCache() {
        std::ifstream cacheFile(".focal-run-cache");
        if (cacheFile.is_open()) {
            std::string line;
            while (std::getline(cacheFile, line)) {
                size_t sep = line.find('=');
                if (sep != std::string::npos) {
                    std::string key = line.substr(0, sep);
                    std::string value = line.substr(sep + 1);
                    configCache[key] = value;
                }
            }
            cacheFile.close();
            
            if (verboseMode) {
                std::cout << colorize("[CACHE]", Color::CYAN) 
                         << " Loaded " << configCache.size() << " entries" << std::endl;
            }
        }
    }

    void saveConfigCache() {
        std::ofstream cacheFile(".focal-run-cache");
        if (cacheFile.is_open()) {
            for (const auto& pair : configCache) {
                cacheFile << pair.first << "=" << pair.second << std::endl;
            }
            cacheFile.close();
            
            if (verboseMode) {
                std::cout << colorize("[CACHE]", Color::CYAN) 
                         << " Saved " << configCache.size() << " entries" << std::endl;
            }
        }
    }

    void run() {
        loadConfigCache();

        if (checkMode) {
            checkDependencies();
            return;
        }

        if (buildMode && !singleFile.empty()) {
            buildSingleFile(singleFile);
            return;
        }

        if (buildMode) {
            scanAndBuildSingleCppFiles();
            return;
        }

        scanProjects();

        // Remove duplicates
        std::sort(projects.begin(), projects.end(), [](const ProjectInfo& a, const ProjectInfo& b) {
            return a.path < b.path;
        });
        projects.erase(std::unique(projects.begin(), projects.end(), [](const ProjectInfo& a, const ProjectInfo& b) {
            return a.path == b.path;
        }), projects.end());

        filterProjects();

        if (cleanMode && allMode) {
            cleanAllBuildDirs();
        } else {
            if (projects.empty()) {
                std::cerr << colorize("No projects found!", Color::RED) << std::endl;
                exit(1);
            }

            if (verboseMode) {
                std::cout << "\n" << colorize("=== Focal-RUN Build Tool ===", Color::BOLD + Color::CYAN) << std::endl;
                std::cout << "Mode: " << colorize(cleanMode ? "Clean" : buildType, Color::YELLOW) << std::endl;
                std::cout << "Rebuild: " << (rebuildMode ? colorize("Yes", Color::GREEN) : colorize("No", Color::RED)) << std::endl;
                std::cout << "Path: " << customPath << std::endl;
                std::cout << "Projects: " << colorize(std::to_string(projects.size()), Color::BOLD) << std::endl;
                std::cout << "Max Concurrent: " << maxConcurrentBuilds << " threads" << std::endl << std::endl;
            }

            // Parallel build with batching
            size_t totalProjects = projects.size();
            size_t completedProjects = 0;

            for (size_t i = 0; i < projects.size(); i += maxConcurrentBuilds) {
                std::vector<std::future<bool>> batch;
                size_t batchEnd = std::min(i + maxConcurrentBuilds, projects.size());

                for (size_t j = i; j < batchEnd; ++j) {
                    batch.push_back(std::async(std::launch::async, [this, j]() {
                        if (!verboseMode) {
                            std::lock_guard<std::mutex> lock(outputMutex);
                            std::cout << colorize("Building: ", Color::CYAN)
                                     << projects[j].name << std::endl;
                        } else {
                            std::lock_guard<std::mutex> lock(outputMutex);
                            std::cout << "\n" << colorize("=== Processing: ", Color::BOLD)
                                     << projects[j].name << " ===" << std::endl;
                            std::cout << "Path: " << projects[j].path << std::endl;
                        }
                        return buildProject(projects[j]);
                    }));
                }

                // Wait for batch to complete
                for (size_t j = 0; j < batch.size(); ++j) {
                    projects[i + j].success = batch[j].get();
                    completedProjects++;
                    showProgressBar(completedProjects, totalProjects, 
                                  projects[i + j].name + " " + 
                                  (projects[i + j].success ? "✓" : "✗"));
                }
            }

            if (!verboseMode) {
                std::cout << std::endl;
            }
        }

        saveConfigCache();
        printReport();
    }

    void printProjects() {
        scanProjects();

        // Remove duplicates
        std::sort(projects.begin(), projects.end(), [](const ProjectInfo& a, const ProjectInfo& b) {
            return a.path < b.path;
        });
        projects.erase(std::unique(projects.begin(), projects.end(), [](const ProjectInfo& a, const ProjectInfo& b) {
            return a.path == b.path;
        }), projects.end());

        std::cout << "\n" << colorize("=== Available Projects ===", Color::BOLD + Color::CYAN) << std::endl;
        
        if (projects.empty()) {
            std::cout << colorize("No projects found!", Color::YELLOW) << std::endl;
            return;
        }

        for (auto& proj : projects) {
            determineBuildSystem(proj);
            
            std::string typeIcon = "📦";
            if (proj.buildSystem == BuildSystem::Rust) typeIcon = "🦀";
            else if (proj.isMakefileProject) typeIcon = "🔧";
            else if (proj.buildSystem == BuildSystem::CMake) typeIcon = "⚙️ ";
            
            std::cout << "  " << typeIcon << " " 
                     << colorize(proj.name, Color::BOLD) << std::endl;
            std::cout << "     Path: " << proj.path << std::endl;
            std::cout << "     Generator: " << colorize(proj.detectedGenerator, Color::CYAN) << std::endl;
            std::cout << std::endl;
        }
    }

    void cleanAllBuildDirs() {
        cleanedBuildDirs.clear();
        try {
            for (const auto& entry : fs::recursive_directory_iterator(customPath)) {
                if (entry.is_directory()) {
                    std::string dirname = entry.path().filename().string();
                    if (dirname == "build" || dirname == "release" || dirname == "debug" || dirname == "target") {
                        std::string cleanPath = entry.path().string();
                        try {
                            fs::remove_all(cleanPath);
                            cleanedBuildDirs.push_back(cleanPath);
                            std::cout << colorize("  [CLEAN]", Color::YELLOW) 
                                     << " " << cleanPath << std::endl;
                        } catch (const std::exception& e) {
                            std::cerr << colorize("  [ERROR]", Color::RED) 
                                     << " Cleaning " << cleanPath << ": " << e.what() << std::endl;
                        }
                    }
                }
            }
        } catch (const std::exception& e) {
            std::cerr << colorize("Error scanning for build directories: ", Color::RED) 
                     << e.what() << std::endl;
        }
    }

    void printReport() {
        std::cout << "\n" << colorize("=== Build Report ===", Color::BOLD + Color::CYAN) << std::endl;
        
        if (cleanMode && allMode) {
            for (const auto& path : cleanedBuildDirs) {
                std::cout << colorize("  [CLEANED] ", Color::YELLOW) << path << std::endl;
            }
            std::cout << "\n" << colorize("  Summary: ", Color::BOLD) 
                     << cleanedBuildDirs.size() << " directories cleaned" << std::endl;
            return;
        }

        int successCount = 0;
        int failCount = 0;
        std::chrono::milliseconds totalTime{0};

        for (const auto& proj : projects) {
            if (cleanMode) {
                std::cout << colorize("  [CLEANED] ", Color::YELLOW) << proj.name << std::endl;
            } else {
                if (proj.success) {
                    std::cout << colorize("  ✓ [SUCCESS] ", Color::GREEN) << proj.name;
                    if (verboseMode && proj.stats.totalTime.count() > 0) {
                        std::cout << " (" << proj.stats.totalTime.count() << "ms)";
                    }
                    std::cout << std::endl;
                    std::cout << "    Generator: " << proj.detectedGenerator << std::endl;
                    successCount++;
                    totalTime += proj.stats.totalTime;
                } else {
                    std::cout << colorize("  ✗ [FAILED] ", Color::RED) << proj.name << std::endl;
                    std::cout << "    Generator: " << proj.detectedGenerator << std::endl;
                    failCount++;
                }
            }
        }

        if (!cleanMode) {
            std::cout << "\n" << colorize("  Summary: ", Color::BOLD);
            std::cout << colorize(std::to_string(successCount) + " succeeded", Color::GREEN) << ", ";
            std::cout << colorize(std::to_string(failCount) + " failed", failCount > 0 ? Color::RED : Color::GREEN);
            
            if (verboseMode && totalTime.count() > 0) {
                std::cout << "\n" << colorize("  Total Time: ", Color::BOLD) 
                         << totalTime.count() << "ms";
                
                if (totalTime.count() > 1000) {
                    double seconds = totalTime.count() / 1000.0;
                    std::cout << " (" << std::fixed << std::setprecision(2) << seconds << "s)";
                }
            }
            std::cout << std::endl;
        }
        std::cout << std::endl;
    }

    void checkDependencies() {
        std::cout << "\n" << colorize("=== Build Tools Check ===", Color::BOLD + Color::CYAN) << "\n" << std::endl;

        std::vector<std::pair<std::string, std::string>> tools = {
            {"cmake", "CMake"},
            {"make", "Make"},
            {"ninja", "Ninja"},
            {"mingw32-make", "MinGW Make"},
            {"g++", "GCC C++ Compiler"},
            {"clang++", "Clang C++ Compiler"},
            {"rustc", "Rust Compiler"},
            {"cargo", "Cargo Package Manager"}
        };

        int availableCount = 0;

        for (const auto& tool : tools) {
            bool available = isCommandAvailable(tool.first);
            std::string status = available ? colorize("✓ OK", Color::GREEN) : colorize("✗ Not Found", Color::RED);
            std::cout << "  " << std::left << std::setw(25) << tool.second << ": " << status << std::endl;
            if (available) availableCount++;
        }

        // Check Visual Studio
#ifdef _WIN32
        std::cout << "\n  " << colorize("Visual Studio:", Color::BOLD) << std::endl;
        bool vsInstalled = checkVisualStudioInstalled();
        std::string vsStatus = vsInstalled ? colorize("✓ Installed", Color::GREEN) : colorize("✗ Not Found", Color::RED);
        std::cout << "    Installation: " << vsStatus << std::endl;
#endif

        std::cout << "\n  " << colorize("Summary: ", Color::BOLD) 
                 << availableCount << "/" << tools.size() << " tools available" << std::endl;

        if (availableCount == 0) {
            std::cout << colorize("  ⚠ Warning: ", Color::YELLOW) 
                     << "No build tools found. Please install CMake, Make, or MinGW." << std::endl;
        } else if (availableCount < (int)tools.size()) {
            std::cout << colorize("  ℹ Note: ", Color::BLUE) 
                     << "Some tools are missing, but basic building should work." << std::endl;
        } else {
            std::cout << colorize("  ✓ All tools available!", Color::GREEN) << std::endl;
        }

        std::cout << std::endl;
    }

    bool buildSingleFile(const std::string& filename) {
        std::string foundFile = "";
        try {
            for (const auto& entry : fs::recursive_directory_iterator(customPath)) {
                if (entry.is_regular_file() && entry.path().filename().string() == filename) {
                    foundFile = entry.path().string();
                    break;
                }
            }
        } catch (const std::exception& e) {
            std::cerr << colorize("Error searching for file: ", Color::RED) << e.what() << std::endl;
            return false;
        }

        if (foundFile.empty()) {
            std::cerr << colorize("File '", Color::RED) << filename 
                     << colorize("' not found!", Color::RED) << std::endl;
            return false;
        }

        if (foundFile.substr(foundFile.size() - 4) != ".cpp") {
            std::cerr << colorize("File '", Color::RED) << filename 
                     << colorize("' is not a .cpp file!", Color::RED) << std::endl;
            return false;
        }

        return buildSingleCppFile(foundFile);
    }

    bool buildSingleCppFile(const std::string& cppFile) {
        fs::path filePath(cppFile);
        std::string dirPath = filePath.parent_path().string();
        std::string filename = filePath.stem().string();
        
#ifdef _WIN32
        std::string outputFile = dirPath + "/" + filename + ".exe";
#else
        std::string outputFile = dirPath + "/" + filename;
#endif

        std::string escapedCpp = escapeShellArg(cppFile);
        std::string escapedOut = escapeShellArg(outputFile);
        std::string buildCmd = "g++ \"" + escapedCpp + "\" -o \"" + escapedOut + "\"";
        
        if (buildType == "Debug") {
            buildCmd += " -g -O0 -Wall";
        } else {
            buildCmd += " -O2 -DNDEBUG";
        }

        std::cout << colorize("[BUILD]", Color::GREEN) << " " << cppFile << std::endl;
        
        if (verboseMode) {
            std::cout << colorize("  [CMD]", Color::MAGENTA) << " " << buildCmd << std::endl;
        }

        auto startTime = std::chrono::high_resolution_clock::now();
        int buildResult = system(verboseMode ? buildCmd.c_str() : (buildCmd + QUIET_REDIRECT).c_str());
        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

        if (verboseMode) {
            std::cout << colorize("  [TIME]", Color::BLUE) 
                     << " Build: " << duration.count() << "ms" << std::endl;
        }

        if (buildResult == 0) {
            std::cout << colorize("  ✓ [SUCCESS]", Color::GREEN) 
                     << " Built: " << outputFile << std::endl;
            return true;
        } else {
            std::cerr << colorize("  ✗ [FAILED]", Color::RED) 
                     << " Build failed: " << cppFile << std::endl;
            if (!verboseMode) {
                std::cerr << "  Run with --verbose for details" << std::endl;
            }
            return false;
        }
    }

    void scanAndBuildSingleCppFiles() {
        std::vector<std::string> cppFiles;

        try {
            for (const auto& entry : fs::recursive_directory_iterator(customPath)) {
                if (entry.is_regular_file() && entry.path().extension() == ".cpp") {
                    std::string filePath = entry.path().string();
                    std::string dirPath = entry.path().parent_path().string();

                    bool hasCMake = fs::exists(dirPath + "/CMakeLists.txt");
                    bool hasMakefile = fs::exists(dirPath + "/Makefile");
                    bool hasCargo = fs::exists(dirPath + "/Cargo.toml");

                    if (!hasCMake && !hasMakefile && !hasCargo) {
                        cppFiles.push_back(filePath);
                    }
                }
            }
        } catch (const std::exception& e) {
            std::cerr << colorize("Error scanning for .cpp files: ", Color::RED) 
                     << e.what() << std::endl;
            return;
        }

        if (cppFiles.empty()) {
            std::cout << colorize("No standalone .cpp files found to build.", Color::YELLOW) << std::endl;
            return;
        }

        std::cout << "\n" << colorize("=== Building Standalone C++ Files ===", Color::BOLD + Color::CYAN) << std::endl;
        std::cout << "Found " << colorize(std::to_string(cppFiles.size()), Color::BOLD) 
                 << " files to build\n" << std::endl;

        int successCount = 0;
        for (const auto& cppFile : cppFiles) {
            if (buildSingleCppFile(cppFile)) {
                successCount++;
            }
            std::cout << std::endl;
        }

        std::cout << colorize("Summary: ", Color::BOLD) 
                 << colorize(std::to_string(successCount) + " succeeded", Color::GREEN) << ", "
                 << colorize(std::to_string(cppFiles.size() - successCount) + " failed", 
                            (successCount == (int)cppFiles.size()) ? Color::GREEN : Color::RED) 
                 << std::endl << std::endl;
    }
};

int main(int argc, char* argv[]) {
    try {
        FocalRun app(argc, argv);
        app.run();
    } catch (const std::exception& e) {
        std::cerr << Color::RED << "Fatal error: " << e.what() << Color::RESET << std::endl;
        return 1;
    }
    return 0;
}
