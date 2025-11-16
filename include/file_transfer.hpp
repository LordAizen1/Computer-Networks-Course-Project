#ifndef FILE_TRANSFER_HPP
#define FILE_TRANSFER_HPP

#include <string>

/**
 * @class FileTransferHandler
 * @brief Manages file transfer operations between clients through the server
 * 
 * File Transfer Protocol (Improved):
 * ================================
 * 
 * OLD PROBLEM: Previous version assumed server had filesystem access to sender's files.
 * This only worked when all parties were on the same machine.
 * 
 * NEW SOLUTION: Client streams file data through the server to recipient.
 * 
 * Transfer Flow:
 * 1. Sender: /sendfile <recipient> <filename>
 * 2. Sender -> Server: File metadata (name, size)
 * 3. Server -> Recipient: File offer notification
 * 4. Recipient -> Server: Accept/reject response
 * 5. If accepted:
 *    a. Sender -> Server: File data in chunks
 *    b. Server -> Recipient: Forward chunks in real-time
 *    c. Progress updates sent to both parties
 * 
 * Security Features:
 * - File size limit (10MB) to prevent abuse
 * - Filename validation to prevent directory traversal
 * - Transfer confirmation required from recipient
 * 
 * Technical Details:
 * - Chunk size: 8192 bytes (8KB) for optimal network performance
 * - Progress updates every 5% completion
 * - Received files saved with "received_" prefix
 */
class FileTransferHandler {
private:
    static constexpr size_t CHUNK_SIZE = 8192;  // 8KB chunks for streaming
    static constexpr long MAX_FILE_SIZE = 10 * 1024 * 1024;  // 10MB limit
    
public:
    /**
     * @brief Receives file data from sender client and forwards to recipient
     * @param sender_socket Socket of the client sending the file
     * @param recipient_socket Socket of the client receiving the file
     * @param sender_username Name of sender (for progress messages)
     * @param recipient_username Name of recipient (for progress messages)
     * @param filename Name of the file being transferred
     * @param file_size Total size of the file in bytes
     * @return true if transfer successful, false on error
     * 
     * This is the core of the improved file transfer:
     * - Reads chunks from sender's socket
     * - Immediately writes to recipient's socket
     * - No temporary file storage on server
     * - Provides real-time progress updates
     */
    static bool streamFileData(int sender_socket, int recipient_socket,
                              const std::string& sender_username,
                              const std::string& recipient_username,
                              const std::string& filename, long file_size);
    
    /**
     * @brief Client-side: Sends local file to server
     * @param server_socket Socket connection to server
     * @param filename Path to local file to send
     * @param file_size Size of the file (pre-calculated)
     * @return true if sent successfully, false on error
     * 
     * Reads local file and streams it to server in chunks
     * Used by sender client after file offer is accepted
     */
    static bool sendFileToServer(int server_socket, const std::string& filename, long file_size);
    
    /**
     * @brief Client-side: Receives file data from server and saves locally
     * @param server_socket Socket connection to server
     * @param sender Username of the sender (for display)
     * @param filename Original filename
     * @param file_size Expected file size in bytes
     * @return true if received successfully, false on error
     * 
     * Receives file in chunks and writes to local disk
     * Saves as "received_<filename>" to avoid overwriting
     * Displays progress during transfer
     */
    static bool receiveFileFromServer(int server_socket, const std::string& sender,
                                     const std::string& filename, long file_size);
    
    /**
     * @brief Validates if a file is safe to transfer
     * @param filepath Path to the file
     * @return true if file is valid and within size limits
     * 
     * Checks:
     * - File exists
     * - File size is positive and under MAX_FILE_SIZE
     * - File is readable
     */
    static bool isValidFile(const std::string& filepath);
    
    /**
     * @brief Gets the size of a file in bytes
     * @param filepath Path to the file
     * @return File size in bytes, or -1 on error
     */
    static long getFileSize(const std::string& filepath);
    
    /**
     * @brief Formats file size for human-readable display
     * @param size Size in bytes
     * @return Formatted string (e.g., "1.5 MB", "234 KB", "5 B")
     */
    static std::string formatFileSize(long size);
};

#endif // FILE_TRANSFER_HPP