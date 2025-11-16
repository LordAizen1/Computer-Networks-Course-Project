#include "../include/client.hpp"
#include "../include/utils.hpp"
#include "../include/file_transfer.hpp"
#include "../include/encryption.hpp"
#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <fstream>
#include <cctype>
#include <cstdlib>
#include <sys/stat.h>
#include <errno.h>
#include <cstring> 

/**
 * CLIENT IMPLEMENTATION
 * ====================
 * 
 * The client uses a dual-threaded architecture:
 * 1. Main thread: Reads user input and sends to server
 * 2. Receiver thread: Continuously listens for server messages
 */

// Constructor
ChatClient::ChatClient(const std::string& ip, int port) 
    : client_socket(-1), server_ip(ip), server_port(port), 
      connected(false), receiver_thread(nullptr) {
}

// Destructor
ChatClient::~ChatClient() {
    disconnect();
}

/**
 * Format file size for human-readable display
 */
std::string ChatClient::formatFileSize(long size) {
    return Utils::formatFileSize(size);
}

/**
 * Connect to the chat server
 */
bool ChatClient::connectToServer() {
    client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket < 0) {
        std::cerr << "Error creating socket" << std::endl;
        return false;
    }
    
    sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);
    
    if (inet_pton(AF_INET, server_ip.c_str(), &server_addr.sin_addr) <= 0) {
        std::cerr << "Invalid server address: " << server_ip << std::endl;
        close(client_socket);
        return false;
    }
    
    if (connect(client_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        std::cerr << "Connection failed. Is server running?" << std::endl;
        close(client_socket);
        return false;
    }
    
    connected = true;
    std::cout << "✓ Connected to server at " << server_ip << ":" << server_port << std::endl;
    if (Encryption::isEnabled()) {
        std::cout << "✓ Encryption: ENABLED" << std::endl;
    }
    return true;
}

/**
 * Check if message contains mostly binary/unprintable data
 */
bool isBinaryData(const std::string& msg) {
    if (msg.length() < 50) return false;
    
    int unprintable = 0;
    int total = std::min((int)msg.length(), 200);
    
    for (int i = 0; i < total; i++) {
        unsigned char c = msg[i];
        if (c < 32 && c != '\n' && c != '\t' && c != '\r') {
            unprintable++;
        }
    }
    
    return (unprintable * 100 / total) > 20;
}

/**
 * Receive messages from server (runs in separate thread)
 */
void ChatClient::receiveMessages() {
    char buffer[8192];
    
    while (connected) {
        ssize_t bytes_read = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
        
        if (bytes_read <= 0) {
            if (connected) {
                std::cerr << "\n✗ Server disconnected" << std::endl;
            }
            break;
        }
        
        buffer[bytes_read] = '\0';
        std::string encrypted_message(buffer, bytes_read);
        
        // Decrypt message if encryption is enabled
        std::string message = encrypted_message;
        if (Encryption::isEnabled() && encrypted_message.length() > 0) {
            if (encrypted_message.find("/file_data") == std::string::npos) {
                try {
                    message = Encryption::decrypt(encrypted_message);
                } catch (...) {
                    message = encrypted_message;
                }
            }
        }
        
        // Handle special messages
        if (message.find("/file_offer") == 0) {
            // File transfer offer - auto-accept
            std::cout << "\n" << message.substr(12) << std::endl;
            std::cout << "[FILE] Accepting automatically..." << std::endl;
            sendMessage("/accept_file");
        }
        else if (message.find("/file_data") == 0) {
            // Receiving file data - NOW WITH FILENAME
            std::vector<std::string> parts = Utils::split(message, ' ');
            if (parts.size() >= 4) {  // /file_data sender filename size
                std::string sender = parts[1];
                std::string original_filename = parts[2];  // Get original filename
                long file_size = std::stol(parts[3]);
                
                // DEFINE user_dir FIRST
                std::string user_dir = "Users/" + username;
                
                // CREATE USER DIRECTORY
                if (mkdir("Users", 0755) == -1 && errno != EEXIST) {
                    std::cerr << "[ERROR] Failed to create Users directory: " << strerror(errno) << std::endl;
                }

                if (mkdir(user_dir.c_str(), 0755) == -1 && errno != EEXIST) {
                    std::cerr << "[ERROR] Failed to create " << user_dir << ": " << strerror(errno) << std::endl;
                }

                std::cout << "[DEBUG] Created directory: " << user_dir << std::endl;
                
                // EXTRACT FILE EXTENSION
                std::string extension = "";
                size_t dot_pos = original_filename.find_last_of('.');
                if (dot_pos != std::string::npos) {
                    extension = original_filename.substr(dot_pos);  // Includes the dot
                }
                
                // SAVE FILE TO USER DIRECTORY with timestamp AND EXTENSION
                time_t now = time(0);
                std::string timestamp = std::to_string(now);
                std::string filename = user_dir + "/from_" + sender + "_" + timestamp + extension;
                
                std::cout << "[FILE] Receiving '" << original_filename << "' (" << formatFileSize(file_size) 
                         << ") from " << sender << "..." << std::endl;
                
                if (FileTransferHandler::receiveFileFromServer(client_socket, sender, filename, file_size)) {
                    std::cout << "[FILE] ✓ File saved to: " << filename << std::endl;
                } else {
                    std::cerr << "[FILE] ✗ File reception failed" << std::endl;
                }
            }
        }
        else if (message.find("[FILE]") == 0 || message.find("[RECEIVING]") == 0) {
            // File transfer status messages
            std::cout << message << std::endl;
        }
        else if (message.find("ERROR:") == 0) {
            // Error messages
            std::cerr << "✗ " << message << std::endl;
        }
        else {
            // Regular chat message - suppress binary data
            if (!isBinaryData(message)) {
                std::cout << message << std::endl;
            }
        }
    }
}

/**
 * Handle file transfer offer
 */
void ChatClient::handleFileOffer(const std::string& metadata) {
    std::cout << "\n[INCOMING FILE] " << metadata.substr(12) << std::endl;
    std::cout << "Accept? (y/n): ";
    
    std::string response;
    std::getline(std::cin, response);
    
    if (response == "y" || response == "Y") {
        sendMessage("/accept_file");
        std::cout << "✓ Accepting file transfer..." << std::endl;
    } else {
        sendMessage("/reject_file");
        std::cout << "✗ File transfer rejected" << std::endl;
    }
}

/**
 * Send a message to the server
 */
void ChatClient::sendMessage(const std::string& message) {
    if (connected) {
        std::string to_send = message;
        
        // DON'T encrypt file commands
        if (Encryption::isEnabled() && 
            message.find("/file") != 0 &&
            message.find("/sendfile") != 0 &&
            message.find("/accept") != 0 &&
            message.find("/reject") != 0) {
            to_send = Encryption::encrypt(message);
        }
        
        send(client_socket, to_send.c_str(), to_send.length(), 0);
    }
}

/**
 * Main client execution
 */
void ChatClient::start() {
    if (!connectToServer()) {
        std::cerr << "Failed to connect to server" << std::endl;
        return;
    }
    
    // Get username
    std::cout << "\nEnter your username: ";
    std::getline(std::cin, username);
    while (username.empty()) {
        std::cout << "Username required: ";
        std::getline(std::cin, username);
    }
    
    // Send username for authentication
    send(client_socket, username.c_str(), username.length(), 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Start receiver thread
    receiver_thread = new std::thread(&ChatClient::receiveMessages, this);
    
    // Display help
    std::cout << "\n========================================" << std::endl;
    std::cout << "Commands:" << std::endl;
    std::cout << "  /list              - Show active users" << std::endl;
    std::cout << "  @username message  - Private message" << std::endl;
    std::cout << "  /sendfile user file - Send file" << std::endl;
    std::cout << "  /quit              - Exit chat" << std::endl;
    std::cout << "========================================\n" << std::endl;
    
    // Main input loop
    std::string input;
    while (connected && std::getline(std::cin, input)) {
        if (input.empty()) continue;
        
        // Handle /quit
        if (input == "/quit") {
            sendMessage(input);
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            break;
        }
        
        // Handle /sendfile
        if (input.find("/sendfile") == 0) {
            std::vector<std::string> parts = Utils::split(input, ' ');
            if (parts.size() < 3) {
                std::cerr << "Usage: /sendfile <username> <filepath>" << std::endl;
                continue;
            }
            
            std::string target_user = parts[1];
            std::string filepath = parts[2];
            
            // Validate file
            if (!Utils::fileExists(filepath)) {
                std::cerr << "Error: File '" << filepath << "' not found" << std::endl;
                continue;
            }
            
            long file_size = Utils::getFileSize(filepath);
            if (file_size <= 0 || file_size > 10 * 1024 * 1024) {
                std::cerr << "Error: Invalid file size (max 10MB)" << std::endl;
                continue;
            }
            
            // EXTRACT FILENAME from path (without directory)
            std::string filename = filepath;
            size_t last_slash = filepath.find_last_of("/\\");
            if (last_slash != std::string::npos) {
                filename = filepath.substr(last_slash + 1);
            }
            
            // Send file transfer request WITH FILENAME
            std::string request = "/sendfile " + target_user + " " + filename + " " + std::to_string(file_size);
            sendMessage(request);
            
            std::cout << "[FILE] Waiting for " << target_user << " to accept..." << std::endl;
            
            // Wait for server to process (server is synchronous now)
            std::this_thread::sleep_for(std::chrono::seconds(3));
            
            // Send file data
            std::cout << "[FILE] Sending file..." << std::endl;
            if (FileTransferHandler::sendFileToServer(client_socket, filepath, file_size)) {
                std::cout << "[FILE] ✓ File sent successfully" << std::endl;
            } else {
                std::cerr << "[FILE] ✗ File transfer failed" << std::endl;
            }
            
            continue;
        }
        
        // Send regular message
        sendMessage(input);
    }
    
    disconnect();
}

/**
 * Disconnect from server
 */
void ChatClient::disconnect() {
    connected = false;
    
    if (client_socket >= 0) {
        close(client_socket);
        client_socket = -1;
    }
    
    if (receiver_thread && receiver_thread->joinable()) {
        receiver_thread->join();
        delete receiver_thread;
        receiver_thread = nullptr;
    }
    
    std::cout << "\n✓ Disconnected from server" << std::endl;
}

/**
 * MAIN FUNCTION
 */
int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "   Network Chat Client - Enhanced" << std::endl;
    std::cout << "   Features: Encrypted, File Transfer" << std::endl;
    std::cout << "========================================" << std::endl;
    
    ChatClient client;
    client.start();
    
    return 0;
}