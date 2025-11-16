#ifndef UTILS_HPP
#define UTILS_HPP

#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <netinet/in.h>
#include <arpa/inet.h>

/**
 * @class Utils
 * @brief Utility functions used across the application
 * 
 * This class provides common helper functions for:
 * - String manipulation (split, trim)
 * - Logging (with timestamps, file output)
 * - File operations (existence, size)
 * - Network utilities (IP conversion)
 * - Time formatting
 * 
 * All methods are static for easy access without instantiation
 */
class Utils {
public:
    /**
     * @brief Logs an event with timestamp to console and file
     * @param event Event description to log
     * 
     * Format: [YYYY-MM-DD HH:MM:SS.mmm] event message
     * Logs to both stdout and "server_log.txt" for persistence
     */
    static void logEvent(const std::string& event);
    
    /**
     * @brief Appends an event to the log file
     * @param event Event description
     * 
     * Creates/appends to "server_log.txt"
     * Used for debugging and audit trail
     */
    static void logToFile(const std::string& event);
    
    /**
     * @brief Splits a string by delimiter into tokens
     * @param str String to split
     * @param delimiter Character to split on
     * @return Vector of string tokens
     * 
     * Example: split("hello world test", ' ') -> ["hello", "world", "test"]
     */
    static std::vector<std::string> split(const std::string& str, char delimiter);
    
    /**
     * @brief Removes leading and trailing whitespace
     * @param str String to trim
     * @return Trimmed string
     * 
     * Useful for cleaning user input before processing
     */
    static std::string trim(const std::string& str);
    
    /**
     * @brief Gets current timestamp with millisecond precision
     * @return Formatted timestamp string
     * 
     * Format: YYYY-MM-DD HH:MM:SS.mmm
     * Used for accurate event logging
     */
    static std::string getCurrentTimestamp();
    
    /**
     * @brief Checks if a file exists on the filesystem
     * @param filepath Path to check
     * @return true if file exists, false otherwise
     */
    static bool fileExists(const std::string& filepath);
    
    /**
     * @brief Gets the size of a file in bytes
     * @param filepath Path to the file
     * @return File size in bytes, or -1 if file doesn't exist
     * 
     * Uses stat() system call for efficient size lookup
     */
    static long getFileSize(const std::string& filepath);
    
    /**
     * @brief Formats bytes into human-readable size
     * @param size Size in bytes
     * @return Formatted string (e.g., "1.5 MB", "234 KB")
     * 
     * Conversion:
     * - < 1024 bytes: "X B"
     * - < 1MB: "X.Y KB"
     * - >= 1MB: "X.Y MB"
     */
    static std::string formatFileSize(long size);
    
    /**
     * @brief Converts sockaddr_in to IP address string
     * @param addr Socket address structure
     * @return IP address in dotted decimal notation (e.g., "192.168.1.100")
     * 
     * Uses inet_ntop() for safe conversion
     */
    static std::string getIPString(struct sockaddr_in addr);
};

#endif // UTILS_HPP