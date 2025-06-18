#ifndef FILEWATCHER_H
#define FILEWATCHER_H

#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <fstream>
#include <algorithm>
#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <signal.h>
#include <sys/inotify.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <cstring>
#include <getopt.h>

/**
 * @class Logger
 * @brief Thread-safe logging utility with console and file output support
 */
class Logger {
private:
    std::ofstream logFile;
    std::mutex logMutex;
    bool enableConsole;
    bool enableFile;

public:
    /**
     * @brief Constructor
     * @param filename Log file path (empty for no file logging)
     * @param console Enable console output (default: true)
     */
    Logger(const std::string& filename = "", bool console = true);
    
    /**
     * @brief Destructor - closes log file if open
     */
    ~Logger();

    /**
     * @brief Log a message with specified level
     * @param level Log level (INFO, WARN, ERROR, DEBUG)
     * @param message Log message
     */
    void log(const std::string& level, const std::string& message);

    /**
     * @brief Log info message
     * @param message Info message
     */
    void info(const std::string& message);

    /**
     * @brief Log warning message
     * @param message Warning message
     */
    void warn(const std::string& message);

    /**
     * @brief Log error message
     * @param message Error message
     */
    void error(const std::string& message);

    /**
     * @brief Log debug message
     * @param message Debug message
     */
    void debug(const std::string& message);
};

/**
 * @class FileWatcher
 * @brief Production-ready file system monitoring using Linux inotify
 */
class FileWatcher {
private:
    int inotifyFd;
    std::unordered_map<int, std::string> watchDescriptors;
    std::unordered_set<std::string> fileExtensions;
    std::unique_ptr<Logger> logger;
    bool recursive;
    bool running;
    std::mutex runningMutex;
    std::condition_variable stopCondition;
    
    static const size_t EVENT_SIZE = sizeof(struct inotify_event);
    static const size_t BUF_LEN = 1024 * (EVENT_SIZE + 16);

    /**
     * @brief Add recursive watches to subdirectories
     * @param basePath Base directory path to scan
     */
    void addRecursiveWatches(const std::string& basePath);

    /**
     * @brief Process inotify events from the file descriptor
     */
    void processEvents();

    /**
     * @brief Convert inotify event mask to human-readable string
     * @param mask Event mask from inotify
     * @return String representation of events
     */
    std::string getEventTypeString(uint32_t mask) const;

public:
    /**
     * @brief Constructor
     * @param log Logger instance (ownership transferred)
     * @param rec Enable recursive directory watching
     */
    FileWatcher(std::unique_ptr<Logger> log, bool rec = false);

    /**
     * @brief Destructor - cleanup resources
     */
    ~FileWatcher();

    /**
     * @brief Add file extension filter
     * @param ext File extension (with or without leading dot)
     */
    void addFileExtension(const std::string& ext);

    /**
     * @brief Clear all file extension filters
     */
    void clearFileExtensions();

    /**
     * @brief Check if file should be watched based on extension filters
     * @param filename File name to check
     * @return true if file should be watched
     */
    bool shouldWatchFile(const std::string& filename) const;

    /**
     * @brief Add directory to watch list
     * @param path Directory path to watch
     * @return Watch descriptor or -1 on error
     */
    int addWatch(const std::string& path);

    /**
     * @brief Remove watch by descriptor
     * @param wd Watch descriptor to remove
     */
    void removeWatch(int wd);

    /**
     * @brief Start the file watching loop
     */
    void start();

    /**
     * @brief Stop the file watching loop
     */
    void stop();

    /**
     * @brief Get number of active watches
     * @return Number of directories being watched
     */
    size_t getWatchCount() const;
};

// Global variables for signal handling
extern std::unique_ptr<FileWatcher> globalWatcher;
extern std::mutex globalMutex;

/**
 * @brief Signal handler for graceful shutdown
 * @param signal Signal number received
 */
void signalHandler(int signal);

/**
 * @brief Print usage information
 * @param programName Program name from argv[0]
 */
void printUsage(const char* programName);

#endif // FILEWATCHER_H
