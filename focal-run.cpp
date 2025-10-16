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

namespace fs = std::filesystem;

enum class BuildSystem {
    CMake,
    Make,
    Ninja,
    MinGW,
    GCC,
    Auto
};

struct ProjectInfo {
    std::string name;
    std::string path;
    bool success = false;
    std::string buildType = "Release";
    BuildSystem buildSystem = BuildSystem::Auto;
    std::string detectedGenerator = "";
    bool isMakefileProject = false;
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
    std::string targetProject = "";
    std::string singleFile = "";
    std::string customPath = ".";
    std::vector<std::string> cleanedBuildDirs;
    std::mutex outputMutex;
    std::unordered_map<std::string, std::string> configCache;
    std::unordered_map<std::string, std::unordered_set<std::string>> dependencyGraph;

    // ตรวจสอบว่า command มีอยู่ในระบบหรือไม่
    bool isCommandAvailable(const std::string& cmd) {
#ifdef _WIN32
        std::string checkCmd = "where " + cmd + " >nul 2>&1";
#else
        std::string checkCmd = "which " + cmd + " >/dev/null 2>&1";
#endif
        return system(checkCmd.c_str()) == 0;
    }

    // ตรวจสอบ Visual Studio โดยเฉพาะสำหรับ Windows
    bool checkVisualStudioInstalled() {
#ifdef _WIN32
        // ตรวจสอบ path ที่เป็นไปได้สำหรับ Visual Studio
        std::vector<std::string> vsPaths = {
            "C:\\Program Files\\Microsoft Visual Studio\\2022\\Community",
            "C:\\Program Files\\Microsoft Visual Studio\\2022\\Professional",
            "C:\\Program Files\\Microsoft Visual Studio\\2022\\Enterprise",
            "C:\\Program Files (x86)\\Microsoft Visual Studio\\2019\\Community",
            "C:\\Program Files (x86)\\Microsoft Visual Studio\\2019\\Professional",
            "C:\\Program Files (x86)\\Microsoft Visual Studio\\2019\\Enterprise",
            "C:\\Program Files (x86)\\Microsoft Visual Studio\\2017\\Community",
            "C:\\Program Files (x86)\\Microsoft Visual Studio\\2017\\Professional",
            "C:\\Program Files (x86)\\Microsoft Visual Studio\\2017\\Enterprise"
        };

        for (const auto& path : vsPaths) {
            if (fs::exists(path)) {
                return true;
            }
        }

        // ตรวจสอบ vswhere.exe ถ้ามี
        if (isCommandAvailable("vswhere")) {
            return true;
        }
#endif
        return false;
    }

    // อ่านไฟล์ CMakeLists.txt และหา generator ที่ต้องการ
    std::string detectGenerator(const std::string& projectPath) {
        std::string cmakelists = projectPath + "/CMakeLists.txt";
        std::ifstream file(cmakelists);
        std::string line;
        std::string detectedGen = "";

        if (!file.is_open()) {
            return "";
        }

        while (std::getline(file, line)) {
            // ลบ whitespace นำหน้า
            line.erase(0, line.find_first_not_of(" \t"));

            // ตรวจสอบ comment
            if (line.find("# Focal-Generator:") == 0) {
                detectedGen = line.substr(18);
                detectedGen.erase(0, detectedGen.find_first_not_of(" \t"));
                detectedGen.erase(detectedGen.find_last_not_of(" \t\n\r") + 1);

                // แปลง shorthand เป็น full generator name
                if (detectedGen == "VS162019") detectedGen = "Visual Studio 16 2019";
                else if (detectedGen == "VS172022") detectedGen = "Visual Studio 17 2022";
                else if (detectedGen == "MinGW") detectedGen = "MinGW Makefiles";
                else if (detectedGen == "Ninja") detectedGen = "Ninja";
                else if (detectedGen == "Make") detectedGen = "Unix Makefiles";

                break;
            }

            // ตรวจสอบจาก CMAKE_GENERATOR หรือ set() commands
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

    // เลือก generator อัตโนมัติตามความพร้อมของระบบ
    std::string selectGenerator() {
        // ลำดับความสำคัญ: Ninja > MinGW > Make
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
        // บน Windows หากไม่มีข้างบน ให้ใช้ Visual Studio
        return "Visual Studio 16 2019";
#else
        return "Unix Makefiles";
#endif
    }

    // ตรวจสอบและตั้งค่า build system สำหรับแต่ละโปรเจกต์
    void determineBuildSystem(ProjectInfo& proj) {
        std::string cacheKey = proj.path + "_generator";
        std::string detected;

        // ตรวจสอบ cache ก่อน
        if (configCache.count(cacheKey)) {
            detected = configCache[cacheKey];
            if (verboseMode) {
                std::cout << "  [CACHE] Using cached generator: " << detected << std::endl;
            }
        } else {
            detected = detectGenerator(proj.path);
        }

        if (!detected.empty()) {
            proj.detectedGenerator = detected;
            configCache[cacheKey] = detected; // บันทึกใน cache
            if (verboseMode && !configCache.count(cacheKey)) {
                std::cout << "  Detected generator: " << detected << std::endl;
            }
        } else {
            // ถ้าไม่พบการระบุ ให้เลือกอัตโนมัติ
            proj.detectedGenerator = selectGenerator();
            configCache[cacheKey] = proj.detectedGenerator; // บันทึกใน cache
            if (verboseMode) {
                std::cout << "  Auto-selected generator: " << proj.detectedGenerator << std::endl;
            }
        }
    }

public:
    FocalRun(int argc, char* argv[]) {
        parseArguments(argc, argv);
    }

    void printHelp() {
        std::cout << "\n" << std::string(70, '=') << std::endl;
        std::cout << "  FOCAL-RUN - CMake Build Tool" << std::endl;
        std::cout << std::string(70, '=') << "\n" << std::endl;

        std::cout << "DESCRIPTION:" << std::endl;
        std::cout << "  Focal-Run is a tool to build CMake and Makefile projects without writing scripts." << std::endl;
        std::cout << "  It supports multiple projects, auto-detects build systems (CMake, Make, Ninja, MinGW),\n"
                  << "  and provides build status reports.\n" << std::endl;

        std::cout << "USAGE:" << std::endl;
        std::cout << "  focal-run.exe [OPTIONS] [PROJECT_NAME]\n" << std::endl;

        std::cout << "COMMANDS:" << std::endl;
        std::cout << "  --scan                    Display all available projects and their generators" << std::endl;
        std::cout << "  --check                   Check if required build tools are available" << std::endl;
        std::cout << "  --help, -h                Show this help message" << std::endl << std::endl;

        std::cout << "BUILD OPTIONS:" << std::endl;
        std::cout << "  --debug                   Build in Debug mode (default: Release)" << std::endl;
        std::cout << "  --release                 Build in Release mode" << std::endl;
        std::cout << "  --rebuild                 Clean and rebuild (remove old build directory)" << std::endl;
        std::cout << "  --clean                   Clean build directory without building" << std::endl;
        std::cout << "  --all                     Apply command to all projects" << std::endl;
        std::cout << "  --verbose                 Show detailed output and timing information" << std::endl;
        std::cout << "  --path=<path>             Specify custom path to scan for projects (default: current directory)\n" << std::endl;

        std::cout << "EXAMPLES:" << std::endl;
        std::cout << "  # Show all projects" << std::endl;
        std::cout << "  focal-run.exe --scan\n" << std::endl;

        std::cout << "  # Check build tools availability" << std::endl;
        std::cout << "  focal-run.exe --check\n" << std::endl;

        std::cout << "  # Build specific project in Release mode" << std::endl;
        std::cout << "  focal-run.exe --MyProject --release\n" << std::endl;

        std::cout << "  # Build specific project in Debug mode" << std::endl;
        std::cout << "  focal-run.exe --MyProject --debug\n" << std::endl;

        std::cout << "  # Build single .cpp file" << std::endl;
        std::cout << "  focal-run.exe --test.cpp --build\n" << std::endl;

        std::cout << "  # Rebuild all projects" << std::endl;
        std::cout << "  focal-run.exe --rebuild --all\n" << std::endl;

        std::cout << "  # Clean all projects" << std::endl;
        std::cout << "  focal-run.exe --clean --all\n" << std::endl;

        std::cout << "  # Build all projects in Debug mode" << std::endl;
        std::cout << "  focal-run.exe --debug --all\n" << std::endl;

        std::cout << "  # Build all projects in Release mode" << std::endl;
        std::cout << "  focal-run.exe --release --all\n" << std::endl;

        std::cout << "  # Clean specific project" << std::endl;
        std::cout << "  focal-run.exe --MyProject --clean\n" << std::endl;

        std::cout << "PROJECT NAMING:" << std::endl;
        std::cout << "  Use --<folder_name> to specify a project (e.g., --MyProject)" << std::endl;
        std::cout << "  Omit --all to build only specified project, include --all for all projects\n" << std::endl;

        std::cout << "BUILD SYSTEM DETECTION:" << std::endl;
        std::cout << "  Focal-Run auto-detects the best build system in this order:" << std::endl;
        std::cout << "  1. Specify in CMakeLists.txt: # Focal-Generator: Ninja (or VS162019, VS172022, MinGW, Make)" << std::endl;
        std::cout << "  2. MinGW Makefiles (Windows)" << std::endl;
        std::cout << "  3. Ninja" << std::endl;
        std::cout << "  4. Unix Makefiles (Linux/Mac)" << std::endl;
        std::cout << "  5. Visual Studio (Windows fallback)\n" << std::endl;

        std::cout << "NOTES:" << std::endl;
        std::cout << "  - Each project must have a CMakeLists.txt or Makefile file" << std::endl;
        std::cout << "  - Build directory will be created at: <project_path>/build (for CMake projects)" << std::endl;
        std::cout << "  - Use --rebuild to delete old build directory before building" << std::endl;
        std::cout << "  - Dependencies are automatically resolved from add_subdirectory() calls" << std::endl;
        std::cout << "  - Configuration is cached in .focal-run-cache for faster subsequent runs" << std::endl;
        std::cout << "  - Parallel building is supported for multiple projects\n" << std::endl;

        std::cout << std::string(70, '=') << "\n" << std::endl;
    }

    void parseArguments(int argc, char* argv[]) {
        for (int i = 1; i < argc; i++) {
            std::string arg = argv[i];
            if (arg == "--all") allMode = true;
            else if (arg == "--rebuild") rebuildMode = true;
            else if (arg == "--clean") cleanMode = true;
            else if (arg == "--build") {
                // Force single file compilation mode
                buildMode = true;
                cleanMode = false;
                allMode = true;
            }
            else if (arg == "--check") {
                checkMode = true;
            }
            else if (arg == "--debug") buildType = "Debug";
            else if (arg == "--release") buildType = "Release";
            else if (arg == "--verbose") verboseMode = true;
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
            else if (arg.substr(0, 2) == "--") {
                // Check if it's a .cpp file
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
            // Check CMakeLists.txt in current directory
            std::string currentCMake = actualPath + "/CMakeLists.txt";
            std::string currentMakefile = actualPath + "/Makefile";

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
            } else if (fs::exists(currentMakefile)) {
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

            // สแกนโฟลเดอร์ย่อยแบบ recursive
            for (const auto& entry : fs::recursive_directory_iterator(actualPath)) {
                if (entry.is_regular_file()) {
                    std::string filename = entry.path().filename().string();
                    if (filename == "CMakeLists.txt") {
                        std::string projectDir = entry.path().parent_path().string();
                        std::string projectName = fs::path(projectDir).filename().string();

                        // ข้ามถ้าเป็นโปรเจกต์ที่พบในโฟลเดอร์ปัจจุบันแล้ว
                        if (projectDir == fs::absolute(actualPath).string()) {
                            continue;
                        }

                        ProjectInfo proj;
                        proj.name = projectName;
                        proj.path = projectDir;
                        proj.buildType = buildType;
                        proj.buildSystem = BuildSystem::CMake;
                        proj.isMakefileProject = false;

                        projects.push_back(proj);
                    } else if (filename == "Makefile") {
                        std::string projectDir = entry.path().parent_path().string();
                        std::string projectName = fs::path(projectDir).filename().string();

                        // ตรวจสอบว่าเป็น CMake project หรือไม่
                        std::string cmakePath = projectDir + "/CMakeLists.txt";
                        if (fs::exists(cmakePath)) {
                            continue; // ข้ามถ้ามี CMakeLists.txt ด้วย
                        }

                        // ข้ามถ้าเป็นโปรเจกต์ที่พบในโฟลเดอร์ปัจจุบันแล้ว
                        if (projectDir == fs::absolute(actualPath).string()) {
                            continue;
                        }

                        ProjectInfo proj;
                        proj.name = projectName;
                        proj.path = projectDir;
                        proj.buildType = buildType;
                        proj.buildSystem = BuildSystem::Make;
                        proj.isMakefileProject = true;
                        proj.detectedGenerator = "Make";

                        projects.push_back(proj);
                    }
                }
            }

            // จัดเรียงให้ root projects มาก่อน
            std::sort(projects.begin(), projects.end(), [](const ProjectInfo& a, const ProjectInfo& b) {
                int depthA = std::count(a.path.begin(), a.path.end(), fs::path::preferred_separator);
                int depthB = std::count(b.path.begin(), b.path.end(), fs::path::preferred_separator);
                return depthA < depthB;
            });
        } catch (const std::exception& e) {
            std::cerr << "Error scanning projects: " << e.what() << std::endl;
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
                std::cerr << "Project '" << targetProject << "' not found!" << std::endl;
                exit(1);
            }
        }
    }

    bool buildProject(ProjectInfo& proj) {
        std::lock_guard<std::mutex> lock(outputMutex);

        // Handle Makefile projects differently
        if (proj.isMakefileProject) {
            return buildMakefileProject(proj);
        }

        // Detect and configure build system for CMake projects
        determineBuildSystem(proj);

        std::string buildDir = proj.path + "/build";

        // If clean mode, remove build, release, debug directories and stop
        if (cleanMode) {
            std::vector<std::string> dirsToClean = {buildDir, proj.path + "/release", proj.path + "/debug"};
            bool cleanedAny = false;

            for (const auto& dir : dirsToClean) {
                if (fs::exists(dir)) {
                    try {
                        fs::remove_all(dir);
                        std::cout << "  [CLEAN] Cleaned: " << dir << std::endl;
                        cleanedAny = true;
                    } catch (const std::exception& e) {
                        std::cerr << "  Error cleaning " << dir << ": " << e.what() << std::endl;
                    }
                }
            }

            if (!cleanedAny) {
                std::cout << "  [INFO] No build directories to clean" << std::endl;
            }
            return true;
        }

        // Remove build directory if rebuild mode
        if (rebuildMode && fs::exists(buildDir)) {
            try {
                fs::remove_all(buildDir);
                std::cout << "  Cleaned: " << buildDir << std::endl;
            } catch (const std::exception& e) {
                std::cerr << "  Error cleaning: " << e.what() << std::endl;
                return false;
            }
        }

        // Create build directory if not exists
        if (!fs::exists(buildDir)) {
            try {
                fs::create_directories(buildDir);
            } catch (const std::exception& e) {
                std::cerr << "  Error creating build directory: " << e.what() << std::endl;
                return false;
            }
        }

        // CMake configure with selected generator
        std::string configCmd = "cmake -S \"" + proj.path + "\" -B \"" + buildDir
                                + "\" -G \"" + proj.detectedGenerator + "\"";
        if (verboseMode) {
            std::cout << "  [VERBOSE] Configuring with: " << proj.detectedGenerator << std::endl;
            std::cout << "  [VERBOSE] Command: " << configCmd << std::endl;
        }

        auto startTime = std::chrono::high_resolution_clock::now();
        int configResult = system(verboseMode ? configCmd.c_str() : (configCmd + " >nul 2>&1").c_str());
        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

        if (verboseMode) {
            std::cout << "  [VERBOSE] Configuration took: " << duration.count() << "ms" << std::endl;
        }

        if (configResult != 0) {
            std::cerr << "  CMake configuration failed!" << std::endl;
            if (!verboseMode) {
                system(configCmd.c_str()); // Show error output
            }
            return false;
        }

        // CMake build
        std::string buildCmd = "cmake --build \"" + buildDir + "\" --config " + proj.buildType;
        if (verboseMode) {
            std::cout << "  [VERBOSE] Building: " << buildCmd << std::endl;
        }

        startTime = std::chrono::high_resolution_clock::now();
        int buildResult = system(verboseMode ? buildCmd.c_str() : (buildCmd + " >nul 2>&1").c_str());
        endTime = std::chrono::high_resolution_clock::now();
        duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

        if (verboseMode) {
            std::cout << "  [VERBOSE] Build took: " << duration.count() << "ms" << std::endl;
        }

        if (buildResult != 0) {
            std::cerr << "  Build failed!" << std::endl;
            if (!verboseMode) {
                system(buildCmd.c_str()); // Show error output
            }
            return false;
        }

        return true;
    }

    bool buildMakefileProject(ProjectInfo& proj) {
        // If clean mode for Makefile projects
        if (cleanMode) {
            // First try make clean
            std::string cleanCmd = "cd \"" + proj.path + "\" && make clean";
            if (verboseMode) {
                std::cout << "  [VERBOSE] Cleaning: " << cleanCmd << std::endl;
            }

            auto startTime = std::chrono::high_resolution_clock::now();
            int cleanResult = system(verboseMode ? cleanCmd.c_str() : (cleanCmd + " >nul 2>&1").c_str());
            auto endTime = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

            if (verboseMode) {
                std::cout << "  [VERBOSE] Clean took: " << duration.count() << "ms" << std::endl;
            }

            // Also clean release/debug directories if they exist
            std::vector<std::string> dirsToClean = {proj.path + "/release", proj.path + "/debug"};
            bool cleanedAny = false;

            for (const auto& dir : dirsToClean) {
                if (fs::exists(dir)) {
                    try {
                        fs::remove_all(dir);
                        std::cout << "  Cleaned: " << dir << std::endl;
                        cleanedAny = true;
                    } catch (const std::exception& e) {
                        std::cerr << "  Error cleaning " << dir << ": " << e.what() << std::endl;
                    }
                }
            }

            if (cleanResult != 0 && !cleanedAny) {
                std::cerr << "  Clean failed!" << std::endl;
                if (!verboseMode) {
                    system(cleanCmd.c_str()); // Show error output
                }
                return false;
            }
            return true;
        }

        // Create necessary directories for Makefile projects
        std::vector<std::string> dirsToCreate = {proj.path + "/release", proj.path + "/debug"};
        for (const auto& dir : dirsToCreate) {
            if (!fs::exists(dir)) {
                try {
                    fs::create_directories(dir);
                    if (verboseMode) {
                        std::cout << "  [VERBOSE] Created directory: " << dir << std::endl;
                    }
                } catch (const std::exception& e) {
                    std::cerr << "  Error creating directory " << dir << ": " << e.what() << std::endl;
                }
            }
        }

        // Build Makefile project - try different targets
        std::vector<std::string> buildTargets = {"all", "release", ""};
        bool buildSuccess = false;

        for (const auto& target : buildTargets) {
            std::string buildCmd = "cd \"" + proj.path + "\" && make";
            if (!target.empty()) {
                buildCmd += " -f Makefile." + target;
            }

            if (verboseMode) {
                std::cout << "  [VERBOSE] Building: " << buildCmd << std::endl;
            }

            auto startTime = std::chrono::high_resolution_clock::now();
            int buildResult = system(verboseMode ? buildCmd.c_str() : (buildCmd + " >nul 2>&1").c_str());
            auto endTime = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

            if (verboseMode) {
                std::cout << "  [VERBOSE] Build took: " << duration.count() << "ms" << std::endl;
            }

            if (buildResult == 0) {
                buildSuccess = true;
                break;
            }
        }

        if (!buildSuccess) {
            std::cerr << "  Build failed!" << std::endl;
            // Try to show error output
            std::string errorCmd = "cd \"" + proj.path + "\" && make";
            system(errorCmd.c_str());
            return false;
        }

        return true;
    }

    // สแกนและ build ไฟล์ .cpp เดี่ยวๆ ที่ไม่ใช่ส่วนหนึ่งของ CMake หรือ Makefile projects
    void scanAndBuildSingleCppFiles() {
        std::vector<std::string> cppFiles;
        std::string startPath = customPath;

        try {
            for (const auto& entry : fs::recursive_directory_iterator(startPath)) {
                if (entry.is_regular_file() && entry.path().extension() == ".cpp") {
                    std::string filePath = entry.path().string();
                    std::string dirPath = entry.path().parent_path().string();

                    // ตรวจสอบว่า directory มี CMakeLists.txt หรือ Makefile หรือไม่
                    bool hasCMake = fs::exists(dirPath + "/CMakeLists.txt");
                    bool hasMakefile = fs::exists(dirPath + "/Makefile");

                    if (!hasCMake && !hasMakefile) {
                        cppFiles.push_back(filePath);
                    }
                }
            }
        } catch (const std::exception& e) {
            std::cerr << "Error scanning for .cpp files: " << e.what() << std::endl;
            return;
        }

        if (cppFiles.empty()) {
            std::cout << "No standalone .cpp files found to build." << std::endl;
            return;
        }

        if (verboseMode) {
            std::cout << "Found " << cppFiles.size() << " standalone .cpp files to build." << std::endl;
        }

        // Build แต่ละไฟล์
        for (const auto& cppFile : cppFiles) {
            buildSingleCppFile(cppFile);
        }
    }

    // Build ไฟล์ .cpp เดี่ยวด้วย g++
    bool buildSingleCppFile(const std::string& cppFile) {
        fs::path filePath(cppFile);
        std::string dirPath = filePath.parent_path().string();
        std::string filename = filePath.stem().string();
        std::string outputFile = dirPath + "/" + filename + ".exe";

        // คำสั่ง g++ สำหรับ compile
        std::string buildCmd = "g++ \"" + cppFile + "\" -o \"" + outputFile + "\"";
        if (buildType == "Debug") {
            buildCmd += " -g -O0"; // Debug flags
        } else {
            buildCmd += " -O2"; // Release optimization
        }

        if (verboseMode) {
            std::cout << "  [VERBOSE] Building: " << cppFile << std::endl;
            std::cout << "  [VERBOSE] Command: " << buildCmd << std::endl;
        }

        auto startTime = std::chrono::high_resolution_clock::now();
        int buildResult = system(verboseMode ? buildCmd.c_str() : (buildCmd + " >nul 2>&1").c_str());
        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

        if (verboseMode) {
            std::cout << "  [VERBOSE] Build took: " << duration.count() << "ms" << std::endl;
        }

        if (buildResult == 0) {
            std::cout << "  [SUCCESS] Built: " << cppFile << " -> " << outputFile << std::endl;
            return true;
        } else {
            std::cerr << "  [FAILED] Build failed: " << cppFile << std::endl;
            if (!verboseMode) {
                system(buildCmd.c_str()); // Show error output
            }
            return false;
        }
    }

    // Build ไฟล์ .cpp เฉพาะที่ระบุ
    bool buildSingleFile(const std::string& filename) {
        // ค้นหาไฟล์ใน directory ปัจจุบันและ subdirectory
        std::string foundFile = "";
        try {
            for (const auto& entry : fs::recursive_directory_iterator(customPath)) {
                if (entry.is_regular_file() && entry.path().filename().string() == filename) {
                    foundFile = entry.path().string();
                    break;
                }
            }
        } catch (const std::exception& e) {
            std::cerr << "Error searching for file: " << e.what() << std::endl;
            return false;
        }

        if (foundFile.empty()) {
            std::cerr << "File '" << filename << "' not found!" << std::endl;
            return false;
        }

        // ตรวจสอบว่าเป็น .cpp file
        if (foundFile.substr(foundFile.size() - 4) != ".cpp") {
            std::cerr << "File '" << filename << "' is not a .cpp file!" << std::endl;
            return false;
        }

        return buildSingleCppFile(foundFile);
    }

    // ตรวจสอบ dependencies ที่จำเป็น
    void checkDependencies() {
        std::cout << "\n=== Build Tools Check ===\n" << std::endl;

        std::vector<std::pair<std::string, std::string>> tools = {
            {"cmake", "CMake"},
            {"make", "Make"},
            {"ninja", "Ninja"},
            {"mingw32-make", "MinGW Make"},
            {"g++", "GCC C++ Compiler"}
        };

        int availableCount = 0;
        int totalCount = tools.size();

        for (const auto& tool : tools) {
            bool available = isCommandAvailable(tool.first);
            std::cout << "  " << tool.second << ": " << (available ? "OK" : "No") << std::endl;
            if (available) {
                availableCount++;
            }
        }

        // ตรวจสอบ Visual Studio versions
        std::cout << "\n  Visual Studio Versions:" << std::endl;
        bool vsInstalled = checkVisualStudioInstalled();
        std::cout << "    Visual Studio Installation: " << (vsInstalled ? "OK" : "No") << std::endl;

        std::vector<std::pair<std::string, std::string>> vsVersions = {
            {"cl", "Visual Studio Compiler (cl.exe)"},
            {"devenv", "Visual Studio IDE"}
        };

        for (const auto& vs : vsVersions) {
            bool available = isCommandAvailable(vs.first);
            std::cout << "    " << vs.second << ": " << (available ? "OK" : "No") << std::endl;
        }

        std::cout << "\n  Summary: " << availableCount << "/" << totalCount << " tools available" << std::endl;

        if (availableCount == 0) {
            std::cout << "  Warning: No build tools found. Please install CMake, Make, or MinGW." << std::endl;
        } else if (availableCount < totalCount) {
            std::cout << "  Note: Some tools are missing, but you may still be able to build projects." << std::endl;
        }

        std::cout << std::endl;
    }

    // โหลด configuration cache
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
        }
    }

    // บันทึก configuration cache
    void saveConfigCache() {
        std::ofstream cacheFile(".focal-run-cache");
        if (cacheFile.is_open()) {
            for (const auto& pair : configCache) {
                cacheFile << pair.first << "=" << pair.second << std::endl;
            }
            cacheFile.close();
        }
    }

    // อ่าน dependencies จาก CMakeLists.txt
    void parseDependencies(const std::string& projectPath) {
        std::string cmakelists = projectPath + "/CMakeLists.txt";
        std::ifstream file(cmakelists);
        std::string line;
        std::string projectName = fs::path(projectPath).filename().string();

        if (!file.is_open()) {
            return;
        }

        while (std::getline(file, line)) {
            // ลบ whitespace นำหน้า
            line.erase(0, line.find_first_not_of(" \t"));

            // ตรวจสอบ add_subdirectory หรือ find_package
            if (line.find("add_subdirectory(") == 0) {
                size_t start = line.find("(");
                size_t end = line.find(")");
                if (start != std::string::npos && end != std::string::npos) {
                    std::string dep = line.substr(start + 1, end - start - 1);
                    // ลบ quotes ถ้ามี
                    if (!dep.empty() && dep[0] == '"') dep = dep.substr(1);
                    if (!dep.empty() && dep.back() == '"') dep = dep.substr(0, dep.size() - 1);

                    // แปลงเป็น absolute path ถ้าจำเป็น
                    fs::path depPath = dep;
                    if (depPath.is_relative()) {
                        depPath = fs::path(projectPath) / depPath;
                    }
                    std::string depName = depPath.filename().string();
                    dependencyGraph[projectName].insert(depName);
                }
            }
        }

        file.close();
    }

    // จัดเรียง projects ตาม dependencies
    void sortByDependencies(std::vector<ProjectInfo>& projects) {
        // สร้าง dependency graph
        for (auto& proj : projects) {
            parseDependencies(proj.path);
        }

        // Topological sort
        std::vector<ProjectInfo> sorted;
        std::unordered_set<std::string> visited;
        std::unordered_set<std::string> visiting;

        std::function<void(const ProjectInfo&)> visit = [&](const ProjectInfo& proj) {
            if (visited.count(proj.name)) return;
            if (visiting.count(proj.name)) {
                std::cerr << "Circular dependency detected involving: " << proj.name << std::endl;
                return;
            }

            visiting.insert(proj.name);

            // Visit dependencies first
            if (dependencyGraph.count(proj.name)) {
                for (const auto& dep : dependencyGraph[proj.name]) {
                    // Find dependency project
                    auto it = std::find_if(projects.begin(), projects.end(),
                        [&dep](const ProjectInfo& p) { return p.name == dep; });
                    if (it != projects.end()) {
                        visit(*it);
                    }
                }
            }

            visiting.erase(proj.name);
            visited.insert(proj.name);
            sorted.push_back(proj);
        };

        for (const auto& proj : projects) {
            if (!visited.count(proj.name)) {
                visit(proj);
            }
        }

        projects = sorted;
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

        // Remove duplicates before filtering
        std::sort(projects.begin(), projects.end(), [](const ProjectInfo& a, const ProjectInfo& b) {
            return a.path < b.path;
        });
        projects.erase(std::unique(projects.begin(), projects.end(), [](const ProjectInfo& a, const ProjectInfo& b) {
            return a.path == b.path;
        }), projects.end());

        // Sort by dependencies if not cleaning all
        if (!(cleanMode && allMode)) {
            sortByDependencies(projects);
        }

        filterProjects();

        if (cleanMode && allMode) {
            cleanAllBuildDirs();
        } else {
            if (projects.empty()) {
                std::cerr << "No CMake projects found!" << std::endl;
                exit(1);
            }

            if (verboseMode) {
                std::cout << "\n=== Focal-RUN Build Tool ===" << std::endl;
                std::cout << "Mode: " << (cleanMode ? "Clean" : buildType) << std::endl;
                std::cout << "Rebuild: " << (rebuildMode ? "Yes" : "No") << std::endl;
                std::cout << "Verbose: " << (verboseMode ? "Yes" : "No") << std::endl;
                std::cout << "Path: " << customPath << std::endl;
                std::cout << "Projects found: " << projects.size() << std::endl << std::endl;
            }

            // Parallel build using threads
            std::vector<std::future<bool>> futures;
            for (auto& proj : projects) {
                futures.push_back(std::async(std::launch::async, [this, &proj]() {
                    if (verboseMode) {
                        std::lock_guard<std::mutex> lock(outputMutex);
                        std::cout << "Processing: " << proj.name << " (" << proj.path << ")" << std::endl;
                    }
                    bool result = buildProject(proj);
                    if (verboseMode) {
                        std::lock_guard<std::mutex> lock(outputMutex);
                        std::cout << std::endl;
                    }
                    return result;
                }));
            }

            // Wait for all builds to complete
            for (size_t i = 0; i < futures.size(); ++i) {
                projects[i].success = futures[i].get();
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

        std::cout << "\n=== Available Projects ===" << std::endl;
        if (projects.empty()) {
            std::cout << "No CMake projects found!" << std::endl;
            return;
        }

        for (auto& proj : projects) {
            determineBuildSystem(proj);
            std::cout << "  - " << proj.name << " (" << proj.path << ")" << std::endl;
            std::cout << "    Generator: " << proj.detectedGenerator << std::endl;
        }
        std::cout << std::endl;
    }

    void cleanAllBuildDirs() {
        cleanedBuildDirs.clear();
        try {
            for (const auto& entry : fs::recursive_directory_iterator(customPath)) {
                if (entry.is_directory()) {
                    std::string dirname = entry.path().filename().string();
                    // Clean build, release, debug directories
                    if (dirname == "build" || dirname == "release" || dirname == "debug") {
                        std::string cleanPath = entry.path().string();
                        try {
                            fs::remove_all(cleanPath);
                            cleanedBuildDirs.push_back(cleanPath);
                            std::cout << "  Cleaned: " << cleanPath << std::endl;
                        } catch (const std::exception& e) {
                            std::cerr << "  Error cleaning " << cleanPath << ": " << e.what() << std::endl;
                        }
                    }
                }
            }
        } catch (const std::exception& e) {
            std::cerr << "Error scanning for build directories: " << e.what() << std::endl;
        }
    }

    void printReport() {
        if (cleanMode && allMode) {
            std::cout << "\n=== Clean Report ===" << std::endl;
            for (const auto& path : cleanedBuildDirs) {
                std::cout << "  [CLEAN] CLEANED: " << path << std::endl;
            }
            std::cout << "\n  Summary: " << cleanedBuildDirs.size() << " build directories cleaned" << std::endl;
            std::cout << std::endl;
            return;
        }

        std::cout << "\n=== Build Report ===" << std::endl;
        int successCount = 0;
        int failCount = 0;

        for (const auto& proj : projects) {
            if (cleanMode) {
                std::cout << "  [CLEAN] CLEANED: " << proj.name << std::endl;
            } else {
                if (proj.success) {
                    std::cout << "  [SUCCESS] " << proj.name << " (Generator: "
                             << proj.detectedGenerator << ")" << std::endl;
                    successCount++;
                } else {
                    std::cout << "  [FAILED] " << proj.name << " (Generator: "
                             << proj.detectedGenerator << ")" << std::endl;
                    failCount++;
                }
            }
        }

        if (!cleanMode) {
            std::cout << "\n  Summary: " << successCount << " succeeded, " << failCount << " failed" << std::endl;
        }
        std::cout << std::endl;
    }
};

int main(int argc, char* argv[]) {
    try {
        FocalRun app(argc, argv);
        app.run();
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}