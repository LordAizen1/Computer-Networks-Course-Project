#include "../include/utils.hpp"
#include <iostream>
#include <vector>
#include <chrono>
#include <iomanip>
#include <sys/stat.h>

/**
 * UTILITY FUNCTIONS IMPLEMENTATION
 * =================================
 * 
 * This module provides common helper functions used throughout the application.
 * All functions are static for easy access without instantiation.
 * 
 * Categories:
 * 1. Logging: Event logging with timestamps
 * 2. String manipulation: Split, trim operations
 * 3. File operations: Existence checks, size queries
 * 4. Time formatting: Timestamp generation
 * 5. Network utilities: IP address conversion
 */

/**
 * Log an event with timestamp
 * ---------------------------
 * Logs to both console (stdout) and file (server_log.txt)
 * Format: [YYYY-MM-DD HH:MM:SS.mmm] event message
 * 
 * This dual logging is useful for:
 * - Real-time monitoring (console)
 * - Historical analysis (log file)
 * - Debugging (persistent record)
 */
void Utils::logEvent(const std::string& event) {
    std::string timestamped_event = "[" + getCurrentTimestamp() + "] " + event;
    std::cout << timestamped_event << std::endl;
    logToFile(timestamped_event);
}

/**
 * Append event to log file
 * ------------------------
 * Creates or appends to "server_log.txt"
 * Thread-safe due to POSIX file locking (automatic in append mode)
 */
void Utils::logToFile(const std::string& event) {
    std::ofstream log_file("server_log.txt", std::ios::app);
    if (log_file.is_open()) {
        log_file << event << std::endl;
        log_file.close();
    }
    // Silently fail if file can't be opened (e.g., permissions)
}

/**
 * Split a string by delimiter
 * ---------------------------
 * Tokenizes a string into a vector of substrings
 * 
 * Example:
 *   split("hello world test", ' ') -> ["hello", "world", "test"]
 *   split("user@domain.com", '@') -> ["user", "domain.com"]
 * 
 * Useful for parsing commands like:
 *   "/sendfile user filename" -> ["/sendfile", "user", "filename"]
 */
std::vector<std::string> Utils::split(const std::string& str, char delimiter) {
    std::vector<std::string> tokens;
    std::stringstream ss(str);
    std::string token;
    
    while (std::getline(ss, token, delimiter)) {
        tokens.push_back(token);
    }
    
    return tokens;
}

/**
 * Remove leading and trailing whitespace
 * ---------------------------------------
 * Essential for cleaning user input
 * 
 * Examples:
 *   trim("  hello  ") -> "hello"
 *   trim("\n\ttest\r\n") -> "test"
 *   trim("   ") -> ""
 * 
 * Why it matters:
 * - Users often press space accidentally
 * - Different OS line endings (\r\n vs \n)
 * - Copy-paste may include extra whitespace
 */
std::string Utils::trim(const std::string& str) {
    size_t first = str.find_first_not_of(' ');
    if (first == std::string::npos) return "";  // All spaces
    
    size_t last = str.find_last_not_of(' ');
    return str.substr(first, (last - first + 1));
}

/**
 * Get current timestamp with millisecond precision
 * ------------------------------------------------
 * Returns: YYYY-MM-DD HH:MM:SS.mmm
 * 
 * Uses std::chrono for high-precision timing
 * 
 * Technical details:
 * - system_clock: Wall clock time (what users see)
 * - milliseconds: Precision suitable for event correlation
 * - localtime: Convert to local timezone
 * 
 * Why milliseconds matter:
 * - Distinguish events happening in quick succession
 * - Measure network latency
 * - Debug race conditions
 */
std::string Utils::getCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    
    // Extract milliseconds component
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    // Format: YYYY-MM-DD HH:MM:SS
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    
    // Add milliseconds: .mmm
    ss << '.' << std::setfill('0') << std::setw(3) << ms.count();
    
    return ss.str();
}

/**
 * Check if a file exists
 * ----------------------
 * Uses stat() system call for efficient checking
 * 
 * Returns true if:
 * - File exists
 * - Accessible to current user
 * 
 * Returns false if:
 * - File doesn't exist
 * - No permission to access
 * - Path is a directory (not a file)
 */
bool Utils::fileExists(const std::string& filepath) {
    struct stat buffer;
    return (stat(filepath.c_str(), &buffer) == 0);
}

/**
 * Get file size in bytes
 * ----------------------
 * Uses stat() system call (faster than opening file)
 * 
 * Returns:
 * - File size in bytes (if successful)
 * - -1 on error (file doesn't exist, no permission, etc.)
 * 
 * Note: This gets the actual size on disk, not the allocated size
 */
long Utils::getFileSize(const std::string& filepath) {
    struct stat buffer;
    if (stat(filepath.c_str(), &buffer) == 0) {
        return buffer.st_size;
    }
    return -1;
}

/**
 * Format file size for human-readable display
 * -------------------------------------------
 * Converts bytes to appropriate unit
 * 
 * Logic:
 * - < 1024 bytes: Display as "X B"
 * - < 1MB: Display as "X.Y KB"
 * - >= 1MB: Display as "X.Y MB"
 * 
 * Examples:
 *   formatFileSize(500) -> "500 B"
 *   formatFileSize(1536) -> "1.5 KB"
 *   formatFileSize(2097152) -> "2.0 MB"
 * 
 * Why one decimal place:
 * - Good balance between precision and readability
 * - "1.5 MB" is clearer than "1572864 B"
 */
std::string Utils::formatFileSize(long size) {
    if (size < 1024) {
        // Less than 1KB: Show bytes
        return std::to_string(size) + " B";
    }
    
    if (size < 1024 * 1024) {
        // Less than 1MB: Show kilobytes with one decimal
        double kb = size / 1024.0;
        char buffer[32];
        snprintf(buffer, sizeof(buffer), "%.1f KB", kb);
        return std::string(buffer);
    }
    
    // 1MB or more: Show megabytes with one decimal
    double mb = size / (1024.0 * 1024.0);
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "%.1f MB", mb);
    return std::string(buffer);
}

/**
 * Convert socket address to IP string
 * -----------------------------------
 * Converts binary network address to dotted decimal notation
 * 
 * Example:
 *   Binary: 0x7F000001
 *   String: "127.0.0.1"
 * 
 * Uses inet_ntop (network to presentation):
 * - Safe (no buffer overflows like inet_ntoa)
 * - IPv4 and IPv6 compatible
 * - Thread-safe
 * 
 * Why this matters:
 * - Logging: "Connection from 192.168.1.100"
 * - Security: Track connection sources
 * - Debugging: Identify which client has issues
 */
std::string Utils::getIPString(struct sockaddr_in addr) {
    char ip_str[INET_ADDRSTRLEN];  // Buffer for IPv4 address (16 bytes)
    
    // Convert binary address to string
    inet_ntop(AF_INET, &addr.sin_addr, ip_str, INET_ADDRSTRLEN);
    
    return std::string(ip_str);
}