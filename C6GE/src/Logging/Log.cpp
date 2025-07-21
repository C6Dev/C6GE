#include "Log.h"

// Define ANSI color codes for terminal output
#define GREEN      "\033[32m"
#define YELLOW     "\033[33m"
#define BLUE       "\033[34m"
#define GRAY       "\033[90m"
#define RED        "\033[31m"
#define BRIGHT_RED "\033[91m"
#define RESET      "\033[0m"

namespace C6GE {

    static std::mutex logMutex;

    std::string GetTimestamp() {
        auto now = std::chrono::system_clock::now();
        auto in_time_t = std::chrono::system_clock::to_time_t(now);
        std::tm buf;
        // Use localtime_s on MSVC, localtime_r on POSIX, fallback to localtime (unsafe) otherwise
        #if defined(_MSC_VER)
            localtime_s(&buf, &in_time_t);
        #elif defined(__unix__) || defined(__APPLE__)
            localtime_r(&in_time_t, &buf);
        #else
            std::tm* tmp = std::localtime(&in_time_t);
            if (tmp) buf = *tmp;
        #endif
        std::stringstream ss;
        ss << std::put_time(&buf, "%Y-%m-%d %X");
        return ss.str();
    }

    void Log(LogLevel level, const std::string& message) {
        std::lock_guard<std::mutex> lock(logMutex);

        static const char* levelStr[]  = { "TRACE", "DEBUG", "INFO", "WARNING", "ERROR", "CRITICAL" };
        static const char* colorStr[]  = { BLUE, "", GRAY, YELLOW, RED, BRIGHT_RED };

        std::string timestamp = GetTimestamp();
        std::string logLine = "[" + timestamp + "] [" + levelStr[static_cast<int>(level)] + "] " + message;

        // Console output
        std::ostream& out = (level == LogLevel::error || level == LogLevel::critical) ? std::cerr : std::cout;
        out << colorStr[static_cast<int>(level)] << logLine << RESET << std::endl;

        // File output
        std::ofstream logFile("log.txt", std::ios::app);
        if (logFile.is_open()) {
            logFile << logLine << std::endl;
        }
    }
}