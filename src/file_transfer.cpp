#include "../include/file_transfer.hpp"
#include "../include/utils.hpp"
#include <iostream>
#include <fstream>
#include <vector>
#include <thread>
#include <chrono>
#include <algorithm>
#include <unistd.h>
#include <sys/socket.h>
#include <cstring>

/**
 * FILE TRANSFER IMPLEMENTATION
 * ============================
 * 
 * This module implements the IMPROVED file transfer protocol.
 * 
 * OLD PROBLEM (Original Version):
 * --------------------------------
 * The server tried to read files from its own filesystem:
 *   Server: FileTransferHandler::sendFileToClient(socket, "report.pdf", ...)
 *   Problem: Server doesn't have "report.pdf" - sender does!
 *   Result: Only worked when all users on same machine
 * 
 * NEW SOLUTION (This Version):
 * ----------------------------
 * Client streams file through server to recipient:
 *   1. Sender opens local file
 *   2. Sender reads chunks and sends to server
 *   3. Server receives chunks and forwards to recipient
 *   4. Recipient receives chunks and writes to disk
 * 
 * This works across different machines on a network!
 * 
 * Performance Considerations:
 * - 8KB chunk size balances memory and network efficiency
 * - Progress updates every 5% to avoid flooding
 * - Non-blocking I/O would improve this further (future work)
 */

/**
 * Stream file data from sender to recipient via server
 * ---------------------------------------------------
 * CORE IMPROVEMENT: This is what makes file transfer work correctly!
 * 
 * The server acts as a relay:
 * Sender --> Server --> Recipient
 * 
 * Data flows in real-time without server storing the file
 */
bool FileTransferHandler::streamFileData(int sender_socket, int recipient_socket,
                                        const std::string& sender_username,
                                        const std::string& recipient_username,
                                        const std::string& filename, long file_size) {
    std::vector<char> buffer(CHUNK_SIZE);
    long total_transferred = 0;
    long last_update = 0;
    
    std::cout << "[FILE TRANSFER] Starting: " << sender_username << " -> " 
              << recipient_username << " (" << formatFileSize(file_size) << ")" << std::endl;
    
    // Stream file in chunks
    while (total_transferred < file_size) {
        // Calculate how much to receive in this iteration
        size_t bytes_to_receive = std::min(
            static_cast<size_t>(file_size - total_transferred), 
            static_cast<size_t>(CHUNK_SIZE)
        );
        
        // Step 1: Receive chunk from sender
        ssize_t bytes_received = recv(sender_socket, buffer.data(), bytes_to_receive, 0);
        if (bytes_received <= 0) {
            std::cerr << "[FILE TRANSFER] Error receiving from sender" << std::endl;
            return false;
        }
        
        // Step 2: Forward chunk to recipient
        ssize_t bytes_sent = send(recipient_socket, buffer.data(), bytes_received, 0);
        if (bytes_sent <= 0) {
            std::cerr << "[FILE TRANSFER] Error sending to recipient" << std::endl;
            return false;
        }
        
        total_transferred += bytes_received;
        
        // Step 3: Send progress updates (every 5%)
        if (total_transferred - last_update > file_size * 0.05) {
            double percent = (total_transferred * 100.0) / file_size;
            std::string progress = "[FILE] Transfer progress: " + 
                                 std::to_string(static_cast<int>(percent)) + "%";
            
            // Send progress to both parties
            //send(sender_socket, progress.c_str(), progress.length(), 0);
            //send(recipient_socket, progress.c_str(), progress.length(), 0);
            
            std::cout << "[FILE TRANSFER] " << static_cast<int>(percent) << "% complete" << std::endl;
            last_update = total_transferred;
        }
    }
    
    std::cout << "[FILE TRANSFER] Complete: " << formatFileSize(total_transferred) 
              << " transferred" << std::endl;
    return true;
}

/**
 * Client-side: Send local file to server
 * --------------------------------------
 * Opens local file and streams it to server
 * 
 * Called by sender client after recipient accepts the transfer
 */
bool FileTransferHandler::sendFileToServer(int server_socket, const std::string& filename, long file_size) {
    // Open local file for reading
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Error: Cannot open file '" << filename << "'" << std::endl;
        return false;
    }
    
    std::vector<char> buffer(CHUNK_SIZE);
    long total_sent = 0;
    long last_update = 0;
    
    // Read and send file in chunks
    while (file.read(buffer.data(), CHUNK_SIZE) || file.gcount() > 0) {
        ssize_t chunk_size = file.gcount();
        
        // Send this chunk to server
        ssize_t bytes_sent = send(server_socket, buffer.data(), chunk_size, 0);
        if (bytes_sent <= 0) {
            std::cerr << "Error: Failed to send data to server" << std::endl;
            file.close();
            return false;
        }
        
        total_sent += bytes_sent;
        
        // Progress update every 5%
        if (total_sent - last_update > file_size * 0.05) {
            double percent = (total_sent * 100.0) / file_size;
            std::cout << "[SENDING] " << static_cast<int>(percent) << "% uploaded" << std::endl;
            last_update = total_sent;
        }
        
        // Small delay to prevent overwhelming the network
        // In production, use proper flow control instead
        std::this_thread::sleep_for(std::chrono::microseconds(100));
    }
    
    file.close();
    
    if (total_sent != file_size) {
        std::cerr << "Warning: Sent " << total_sent << " bytes, expected " << file_size << std::endl;
        return false;
    }
    
    std::cout << "[SENDING] ✓ Upload complete: " << formatFileSize(total_sent) << std::endl;
    return true;
}

/**
 * Client-side: Receive file from server and save locally
 * ------------------------------------------------------
 * Receives file data in chunks and writes to disk
 * 
 * Saves with "received_" prefix to avoid overwriting existing files
 */
bool FileTransferHandler::receiveFileFromServer(int server_socket, const std::string& sender,
                                              const std::string& filename, long file_size) {
    // Create output filename with prefix
    //std::string filename = "received_" + filename;
    std::ofstream file(filename, std::ios::binary);
    
    if (!file.is_open()) {
        std::cerr << "Error: Cannot create output file: " << filename << std::endl;
        return false;
    }
    
    std::vector<char> buffer(CHUNK_SIZE);
    long total_received = 0;
    long last_update = 0;
    
    // Receive file in chunks
    while (total_received < file_size) {
        // Calculate how much to receive
        size_t bytes_to_receive = std::min(
            static_cast<size_t>(file_size - total_received), 
            static_cast<size_t>(CHUNK_SIZE)
        );
        
        // Receive chunk from server
        ssize_t bytes_received = recv(server_socket, buffer.data(), bytes_to_receive, 0);
        if (bytes_received <= 0) {
            std::cerr << "Error: Failed to receive data" << std::endl;
            file.close();
            return false;
        }
        
        // Write chunk to disk
        file.write(buffer.data(), bytes_received);
        if (file.fail()) {
            std::cerr << "Error: Failed to write to file" << std::endl;
            file.close();
            return false;
        }
        
        total_received += bytes_received;
        
        // Progress update every 5%
        if (total_received - last_update > file_size * 0.05) {
            double percent = (total_received * 100.0) / file_size;
            std::cout << "[RECEIVING] " << static_cast<int>(percent) 
                     << "% downloaded from " << sender << std::endl;
            last_update = total_received;
        }
    }
    
    file.close();
    
    if (total_received != file_size) {
        std::cerr << "Warning: Received " << total_received 
                 << " bytes, expected " << file_size << std::endl;
        return false;
    }
    
    std::cout << "[RECEIVING] ✓ Download complete: " << filename 
              << " (" << formatFileSize(total_received) << ")" << std::endl;
    return true;
}

/**
 * Validate if a file is safe to transfer
 * --------------------------------------
 * Checks:
 * 1. File exists
 * 2. File size is within limits (0 < size <= 10MB)
 * 3. File is readable
 */
bool FileTransferHandler::isValidFile(const std::string& filepath) {
    // Check existence
    if (!Utils::fileExists(filepath)) {
        std::cerr << "File does not exist: " << filepath << std::endl;
        return false;
    }
    
    // Check size
    long size = Utils::getFileSize(filepath);
    if (size <= 0) {
        std::cerr << "File is empty or unreadable: " << filepath << std::endl;
        return false;
    }
    
    if (size > MAX_FILE_SIZE) {
        std::cerr << "File too large: " << formatFileSize(size) 
                 << " (max: " << formatFileSize(MAX_FILE_SIZE) << ")" << std::endl;
        return false;
    }
    
    // Check readability
    std::ifstream test_file(filepath, std::ios::binary);
    if (!test_file.is_open()) {
        std::cerr << "Cannot open file for reading: " << filepath << std::endl;
        return false;
    }
    test_file.close();
    
    return true;
}

/**
 * Get file size in bytes
 */
long FileTransferHandler::getFileSize(const std::string& filepath) {
    return Utils::getFileSize(filepath);
}

/**
 * Format file size for display
 * ----------------------------
 * Converts bytes to human-readable format
 * Examples: "1.5 MB", "234 KB", "45 B"
 */
std::string FileTransferHandler::formatFileSize(long size) {
    return Utils::formatFileSize(size);
}