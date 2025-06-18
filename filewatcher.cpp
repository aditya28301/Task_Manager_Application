#include "filewatcher.h"

// Global variables for signal handling
std::unique_ptr<FileWatcher> globalWatcher;
std::mutex globalMutex;

// ============================================================================
// Logger Implementation
// ============================================================================

Logger::Logger(const std::string& filename, bool console) 
    : enableConsole(console), enableFile(!filename.empty()) {
    if (enableFile) {
        logFile.open(filename, std::ios::app);
        if (!logFile.is_open()) {
            std::cerr << "Warning: Could not open log file " << filename << std::endl;
            enableFile = false;
        }
    }
}

Logger::~Logger() {
    if (logFile.is_open()) {
        logFile.close();
    }
}

void Logger::log(const std::string& level, const std::string& message) {
    std::lock_guard<std::mutex> lock(logMutex);
    
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    ss << "." << std::setfill('0') << std::setw(3) << ms.count();
    ss << " [" << level << "] " << message;
    
    std::string logEntry = ss.str();
    
    if (enableConsole) {
        std::cout << logEntry << std::endl;
    }
    
    if (enableFile && logFile.is_open()) {
        logFile << logEntry << std::endl;
        logFile.flush();
    }
}

void Logger::info(const std::string& message) { 
    log("INFO", message); 
}

void Logger::warn(const std::string& message) { 
    log("WARN", message); 
}

void Logger::error(const std::string& message) { 
    log("ERROR", message); 
}

void Logger::debug(const std::string& message) { 
    log("DEBUG", message); 
}

// ============================================================================
// FileWatcher Implementation
// ============================================================================

FileWatcher::FileWatcher(std::unique_ptr<Logger> log, bool rec) 
    : logger(std::move(log)), recursive(rec), running(false) {
    inotifyFd = inotify_init1(IN_NONBLOCK);
    if (inotifyFd == -1) {
        throw std::runtime_error("Failed to initialize inotify: " + std::string(strerror(errno)));
    }
}

FileWatcher::~FileWatcher() {
    stop();
    if (inotifyFd != -1) {
        close(inotifyFd);
    }
}

void FileWatcher::addFileExtension(const std::string& ext) {
    std::string extension = ext;
    if (extension[0] != '.') {
        extension = "." + extension;
    }
    fileExtensions.insert(extension);
}

void FileWatcher::clearFileExtensions() {
    fileExtensions.clear();
}

bool FileWatcher::shouldWatchFile(const std::string& filename) const {
    if (fileExtensions.empty()) {
        return true; // Watch all files if no extensions specified
    }
    
    size_t dotPos = filename.find_last_of('.');
    if (dotPos == std::string::npos) {
        return false; // No extension
    }
    
    std::string extension = filename.substr(dotPos);
    return fileExtensions.find(extension) != fileExtensions.end();
}

int FileWatcher::addWatch(const std::string& path) {
    uint32_t mask = IN_CREATE | IN_DELETE | IN_MODIFY | IN_MOVE | 
                   IN_DELETE_SELF | IN_MOVE_SELF | IN_ATTRIB;
    
    int wd = inotify_add_watch(inotifyFd, path.c_str(), mask);
    if (wd == -1) {
        logger->error("Failed to add watch for " + path + ": " + strerror(errno));
        return -1;
    }
    
    watchDescriptors[wd] = path;
    logger->info("Added watch for: " + path + " (wd=" + std::to_string(wd) + ")");
    
    if (recursive) {
        addRecursiveWatches(path);
    }
    
    return wd;
}

void FileWatcher::addRecursiveWatches(const std::string& basePath) {
    DIR* dir = opendir(basePath.c_str());
    if (!dir) {
        logger->warn("Cannot open directory for recursive watch: " + basePath);
        return;
    }

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        std::string fullPath = basePath + "/" + entry->d_name;
        struct stat statbuf;
        
        if (stat(fullPath.c_str(), &statbuf) == 0 && S_ISDIR(statbuf.st_mode)) {
            addWatch(fullPath);
        }
    }
    
    closedir(dir);
}

void FileWatcher::removeWatch(int wd) {
    if (inotify_rm_watch(inotifyFd, wd) == -1) {
        logger->error("Failed to remove watch descriptor " + std::to_string(wd) + 
                     ": " + strerror(errno));
        return;
    }
    
    auto it = watchDescriptors.find(wd);
    if (it != watchDescriptors.end()) {
        logger->info("Removed watch for: " + it->second);
        watchDescriptors.erase(it);
    }
}

std::string FileWatcher::getEventTypeString(uint32_t mask) const {
    std::vector<std::string> events;
    
    if (mask & IN_CREATE) events.push_back("CREATE");
    if (mask & IN_DELETE) events.push_back("DELETE");
    if (mask & IN_MODIFY) events.push_back("MODIFY");
    if (mask & IN_MOVED_FROM) events.push_back("MOVED_FROM");
    if (mask & IN_MOVED_TO) events.push_back("MOVED_TO");
    if (mask & IN_DELETE_SELF) events.push_back("DELETE_SELF");
    if (mask & IN_MOVE_SELF) events.push_back("MOVE_SELF");
    if (mask & IN_ATTRIB) events.push_back("ATTRIB");
    if (mask & IN_ISDIR) events.push_back("DIR");
    
    if (events.empty()) {
        return "UNKNOWN";
    }
    
    std::string result = events[0];
    for (size_t i = 1; i < events.size(); ++i) {
        result += "|" + events[i];
    }
    
    return result;
}

void FileWatcher::processEvents() {
    char buffer[BUF_LEN];
    ssize_t length = read(inotifyFd, buffer, BUF_LEN);
    
    if (length == -1) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            logger->error("Error reading inotify events: " + std::string(strerror(errno)));
        }
        return;
    }
    
    size_t i = 0;
    while (i < static_cast<size_t>(length)) {
        struct inotify_event* event = reinterpret_cast<struct inotify_event*>(&buffer[i]);
        
        auto it = watchDescriptors.find(event->wd);
        if (it == watchDescriptors.end()) {
            i += EVENT_SIZE + event->len;
            continue;
        }
        
        std::string path = it->second;
        std::string filename = (event->len > 0) ? event->name : "";
        std::string fullPath = filename.empty() ? path : path + "/" + filename;
        
        // Filter by file extension if specified
        if (!filename.empty() && !(event->mask & IN_ISDIR) && !shouldWatchFile(filename)) {
            i += EVENT_SIZE + event->len;
            continue;
        }
        
        std::string eventType = getEventTypeString(event->mask);
        
        // Handle directory creation for recursive watching
        if (recursive && (event->mask & IN_CREATE) && (event->mask & IN_ISDIR)) {
            addWatch(fullPath);
        }
        
        // Handle directory deletion
        if (event->mask & IN_DELETE_SELF) {
            logger->warn("Watched directory deleted: " + path);
            removeWatch(event->wd);
        }
        
        // Log the event
        std::stringstream logMsg;
        logMsg << "Event: " << eventType << " | Path: " << fullPath;
        if (event->cookie != 0) {
            logMsg << " | Cookie: " << event->cookie;
        }
        
        logger->info(logMsg.str());
        
        i += EVENT_SIZE + event->len;
    }
}

void FileWatcher::start() {
    {
        std::lock_guard<std::mutex> lock(runningMutex);
        running = true;
    }
    
    logger->info("File watcher started. Monitoring " + std::to_string(watchDescriptors.size()) + " directories.");
    
    fd_set readfds;
    struct timeval timeout;
    
    while (true) {
        {
            std::lock_guard<std::mutex> lock(runningMutex);
            if (!running) break;
        }
        
        FD_ZERO(&readfds);
        FD_SET(inotifyFd, &readfds);
        
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
        
        int ret = select(inotifyFd + 1, &readfds, nullptr, nullptr, &timeout);
        
        if (ret == -1) {
            if (errno == EINTR) {
                continue; // Interrupted by signal
            }
            logger->error("select() failed: " + std::string(strerror(errno)));
            break;
        }
        
        if (ret > 0 && FD_ISSET(inotifyFd, &readfds)) {
            processEvents();
        }
    }
    
    logger->info("File watcher stopped.");
}

void FileWatcher::stop() {
    {
        std::lock_guard<std::mutex> lock(runningMutex);
        running = false;
    }
    stopCondition.notify_all();
}

size_t FileWatcher::getWatchCount() const {
    return watchDescriptors.size();
}

// ============================================================================
// Utility Functions
// ============================================================================

void signalHandler(int signal) {
    std::lock_guard<std::mutex> lock(globalMutex);
    if (globalWatcher) {
        std::cout << "\nReceived signal " << signal << ", stopping file watcher..." << std::endl;
        globalWatcher->stop();
    }
}

void printUsage(const char* programName) {
    std::cout << "Usage: " << programName << " [OPTIONS] DIRECTORY [DIRECTORY...]\n\n"
              << "Options:\n"
              << "  -r, --recursive     Watch directories recursively\n"
              << "  -e, --extension EXT Filter by file extension (can be used multiple times)\n"
              << "  -l, --log FILE      Log to file (in addition to console)\n"
              << "  -q, --quiet         Disable console output (only log to file)\n"
              << "  -h, --help          Show this help message\n"
              << "\nExamples:\n"
              << "  " << programName << " /home/user/documents\n"
              << "  " << programName << " -r -e cpp -e h /home/user/projects\n"
              << "  " << programName << " -l /var/log/filewatcher.log /tmp\n"
              << "  " << programName << " -q -l /var/log/filewatcher.log /var/www\n"
              << std::endl;
}

// ============================================================================
// Main Function
// ============================================================================

int main(int argc, char* argv[]) {
    std::vector<std::string> directories;
    std::vector<std::string> extensions;
    std::string logFile;
    bool recursive = false;
    bool quiet = false;
    
    // Parse command line arguments
    static struct option longOptions[] = {
        {"recursive", no_argument, 0, 'r'},
        {"extension", required_argument, 0, 'e'},
        {"log", required_argument, 0, 'l'},
        {"quiet", no_argument, 0, 'q'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };
    
    int option;
    int optionIndex = 0;
    
    while ((option = getopt_long(argc, argv, "re:l:qh", longOptions, &optionIndex)) != -1) {
        switch (option) {
            case 'r':
                recursive = true;
                break;
            case 'e':
                extensions.push_back(optarg);
                break;
            case 'l':
                logFile = optarg;
                break;
            case 'q':
                quiet = true;
                break;
            case 'h':
                printUsage(argv[0]);
                return 0;
            default:
                printUsage(argv[0]);
                return 1;
        }
    }
    
    // Get remaining arguments as directories
    for (int i = optind; i < argc; i++) {
        directories.push_back(argv[i]);
    }
    
    if (directories.empty()) {
        std::cerr << "Error: No directories specified to watch.\n" << std::endl;
        printUsage(argv[0]);
        return 1;
    }
    
    // Validate directories
    for (const auto& dir : directories) {
        struct stat statbuf;
        if (stat(dir.c_str(), &statbuf) != 0) {
            std::cerr << "Error: Cannot access directory '" << dir << "': " 
                      << strerror(errno) << std::endl;
            return 1;
        }
        
        if (!S_ISDIR(statbuf.st_mode)) {
            std::cerr << "Error: '" << dir << "' is not a directory." << std::endl;
            return 1;
        }
    }
    
    try {
        // Initialize logger
        auto logger = std::make_unique<Logger>(logFile, !quiet);
        
        // Create file watcher
        globalWatcher = std::make_unique<FileWatcher>(std::move(logger), recursive);
        
        // Set up signal handlers
        signal(SIGINT, signalHandler);
        signal(SIGTERM, signalHandler);
        
        // Add file extensions filter
        for (const auto& ext : extensions) {
            globalWatcher->addFileExtension(ext);
        }
        
        // Add watches for all specified directories
        int watchCount = 0;
        for (const auto& dir : directories) {
            if (globalWatcher->addWatch(dir) != -1) {
                watchCount++;
            }
        }
        
        if (watchCount == 0) {
            std::cerr << "Error: No directories could be watched." << std::endl;
            return 1;
        }
        
        std::cout << "Starting file watcher..." << std::endl;
        std::cout << "Watching " << globalWatcher->getWatchCount() << " directories" 
                  << (recursive ? " (recursive)" : "") << std::endl;
        
        if (!extensions.empty()) {
            std::cout << "Filtering by extensions: ";
            for (size_t i = 0; i < extensions.size(); ++i) {
                std::cout << extensions[i];
                if (i < extensions.size() - 1) std::cout << ", ";
            }
            std::cout << std::endl;
        }
        
        std::cout << "Press Ctrl+C to stop." << std::endl << std::endl;
        
        // Start watching
        globalWatcher->start();
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
