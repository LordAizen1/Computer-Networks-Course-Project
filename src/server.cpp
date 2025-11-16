#include "../include/server.hpp"
#include "../include/utils.hpp"
#include "../include/file_transfer.hpp"
#include "../include/encryption.hpp"
#include <iostream>
#include <vector>
#include <cstring>
#include <thread>
#include <chrono>
#include <algorithm>

/**
 * SERVER IMPLEMENTATION
 * ====================
 * 
 * This is the heart of the chat application. Key architectural decisions:
 * 
 * 1. Threading Model:
 *    - Main thread: Accepts new connections in a loop
 *    - Client threads: One per connected client (detached)
 *    - File transfers: Handled synchronously to avoid race conditions
 * 
 * 2. Thread Safety:
 *    - All access to the clients map is protected by clients_mutex
 *    - Lock is held for minimal time to reduce contention
 *    - Detached threads don't require lifecycle management
 * 
 * 3. Message Flow:
 *    Client -> Server: Message arrives at client's thread
 *    Server: Processes and routes based on message type
 *    Server -> Client(s): Sends to target socket(s)
 * 
 * 4. Error Handling:
 *    - Validates all user input
 *    - Sends error messages back to client
 *    - Logs all errors for debugging
 *    - Gracefully handles disconnects
 */

// Constructor: Initialize server configuration
ChatServer::ChatServer(int port) : server_fd(-1), port(port), running(false) {
    // Zero-initialize the address structure for safety
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;           // IPv4
    address.sin_addr.s_addr = INADDR_ANY;   // Bind to all interfaces
    address.sin_port = htons(port);         // Convert port to network byte order
}

// Destructor: Ensure clean shutdown
ChatServer::~ChatServer() {
    stop();
}

/**
 * Start the server
 * ---------------
 * Sets up the server socket and prepares to accept connections
 * 
 * Steps performed:
 * 1. Create TCP socket
 * 2. Set SO_REUSEADDR to allow rapid restart after shutdown
 * 3. Bind socket to port
 * 4. Begin listening for connections
 */
bool ChatServer::start() {
    // Step 1: Create TCP socket (IPv4, Stream-based, default protocol)
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        std::cerr << "Socket creation failed" << std::endl;
        return false;
    }
    
    // Step 2: Set socket options to reuse address
    // This prevents "Address already in use" errors when restarting
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, 
                   &opt, sizeof(opt))) {
        std::cerr << "Setsockopt failed" << std::endl;
        close(server_fd);
        return false;
    }
    
    // Step 3: Bind socket to the specified port
    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        std::cerr << "Bind failed - Port may be in use" << std::endl;
        close(server_fd);
        return false;
    }
    
    // Step 4: Start listening (backlog of 10 pending connections)
    if (listen(server_fd, 10) < 0) {
        std::cerr << "Listen failed" << std::endl;
        close(server_fd);
        return false;
    }
    
    running = true;
    logEvent("Server started on port " + std::to_string(port));
    std::cout << "[SERVER] Listening on port " << port << std::endl;
    std::cout << "[SERVER] Encryption: " << (Encryption::isEnabled() ? "ENABLED" : "DISABLED") << std::endl;
    
    return true;
}

/**
 * Main server loop
 * ---------------
 * Continuously accepts new client connections
 * Each connection is handled in a separate detached thread
 */
void ChatServer::run() {
    while (running) {
        sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);
        
        // Accept blocks until a client connects
        int client_socket = accept(server_fd, (struct sockaddr*)&client_addr, &addr_len);
        if (client_socket < 0) {
            if (running) {  // Only log if not shutting down
                std::cerr << "Accept failed" << std::endl;
            }
            continue;
        }
        
        logEvent("New connection from " + Utils::getIPString(client_addr));
        
        // Spawn a thread to handle this client
        // Detached threads clean up automatically when done
        std::thread client_thread(&ChatServer::handleClient, this, client_socket, client_addr);
        client_thread.detach();
    }
}

/**
 * Handle a single client connection
 * ---------------------------------
 * This runs in a dedicated thread for each client
 * 
 * Lifecycle:
 * 1. Receive and validate username
 * 2. Register client in global map
 * 3. Loop: Receive and process messages
 * 4. On disconnect: Deregister and cleanup
 */
void ChatServer::handleClient(int client_socket, sockaddr_in client_addr) {
    char buffer[4096];  // Buffer for receiving messages
    
    // PHASE 1: Authentication - Get username from client
    ssize_t bytes_read = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
    if (bytes_read <= 0) {
        close(client_socket);
        return;
    }
    
    buffer[bytes_read] = '\0';
    std::string username = Utils::trim(buffer);
    
    // Validate username format
    if (username.empty() || !isValidUsername(username)) {
        std::string error_msg = "ERROR: Invalid username. Use only alphanumeric, _, and -";
        send(client_socket, error_msg.c_str(), error_msg.length(), 0);
        close(client_socket);
        logEvent("Rejected invalid username from " + Utils::getIPString(client_addr));
        return;
    }
    
    // Check for duplicate username (thread-safe check)
    {
        std::lock_guard<std::mutex> lock(clients_mutex);
        if (clients.find(username) != clients.end()) {
            std::string error_msg = "ERROR: Username '" + username + "' is already taken";
            send(client_socket, error_msg.c_str(), error_msg.length(), 0);
            close(client_socket);
            logEvent("Duplicate username attempt: " + username);
            return;
        }
    }
    
    // PHASE 2: Registration - Add client to registry
    ClientInfo client_info(client_socket, username, client_addr);
    registerClient(username, client_info);
    
    // Send welcome message
    std::string welcome_msg = "Welcome " + username + "! Type /list, /quit, @user msg, /sendfile user file";
    if (Encryption::isEnabled()) {
        welcome_msg = Encryption::encrypt(welcome_msg);
    }
    send(client_socket, welcome_msg.c_str(), welcome_msg.length(), 0);
    
    // Notify all other users
    std::string join_msg = username + " joined the chat!";
    broadcast(join_msg, username);
    logEvent("User authenticated: " + username);
    
    // PHASE 3: Message Processing Loop
    while (running) {
        bytes_read = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
        if (bytes_read <= 0) {
            break;
        }
        
        buffer[bytes_read] = '\0';
        std::string encrypted_message(buffer, bytes_read);
        
        // Decrypt message if encryption is enabled
        std::string message = encrypted_message;
        if (Encryption::isEnabled() && encrypted_message.length() > 0) {
            try {
                message = Encryption::decrypt(encrypted_message);
            } catch (...) {
                // If decryption fails, skip this message (likely binary file data)
                continue;
            }
        }
        
        message = Utils::trim(message);
        if (message.empty()) continue;
        
        logEvent("[" + username + "] " + message);
        processMessage(message, username, client_socket);
        
        if (message == "/quit") {
            break;
        }
    }
    
    // PHASE 4: Cleanup - Deregister and notify others
    std::string leave_msg = username + " left the chat";
    broadcast(leave_msg, username);
    
    deregisterClient(username);
    close(client_socket);
    logEvent("Connection closed for " + username);
}

/**
 * Handle file transfer between two clients
 * ----------------------------------------
 * IMPROVED VERSION: Client streams file through server to recipient
 * Preserves original filename and extension
 * 
 * Protocol:
 * 1. Server notifies recipient of incoming file (with filename)
 * 2. Waits for recipient's auto-accept (2 seconds)
 * 3. Sends /file_data signal to recipient (with filename and size)
 * 4. Facilitates streaming from sender to recipient
 * 5. Provides progress updates to both parties
 */
void ChatServer::handleFileTransfer(int sender_socket, const std::string& sender_username,
                                   const std::string& recipient_username, 
                                   const std::string& filename, long file_size) {
    // Find recipient's socket (thread-safe lookup)
    int recipient_socket = -1;
    {
        std::lock_guard<std::mutex> lock(clients_mutex);
        auto it = clients.find(recipient_username);
        if (it != clients.end()) {
            recipient_socket = it->second.socket_fd;
        }
    }
    
    // Check if recipient is online
    if (recipient_socket == -1) {
        std::string error_msg = "ERROR: User '" + recipient_username + "' is not online";
        send(sender_socket, error_msg.c_str(), error_msg.length(), 0);
        return;
    }
    
    // Send file offer to recipient (includes filename now)
    std::string file_offer = "/file_offer from " + sender_username + 
                            " (" + filename + ", " + Utils::formatFileSize(file_size) + ") - Accept? (y/n)";
    send(recipient_socket, file_offer.c_str(), file_offer.length(), 0);
    
    // Wait for recipient to process and auto-accept
    std::this_thread::sleep_for(std::chrono::seconds(2));
    
    // Tell recipient to prepare for file data - NOW INCLUDES FILENAME
    std::string file_data_msg = "/file_data " + sender_username + " " + filename + " " + std::to_string(file_size);
    send(recipient_socket, file_data_msg.c_str(), file_data_msg.length(), 0);
    
    // Small delay to ensure message is processed
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    // Stream file data from sender to recipient
    bool success = FileTransferHandler::streamFileData(
        sender_socket, recipient_socket, 
        sender_username, recipient_username,
        filename, file_size
    );
    
    if (success) {
        std::string complete_msg = "[FILE] ✓ Transfer complete!";
        send(sender_socket, complete_msg.c_str(), complete_msg.length(), 0);
        send(recipient_socket, complete_msg.c_str(), complete_msg.length(), 0);
        logEvent("File transfer completed: " + sender_username + " -> " + recipient_username + " (" + filename + ")");
    } else {
        std::string error_msg = "ERROR: File transfer failed";
        send(sender_socket, error_msg.c_str(), error_msg.length(), 0);
        send(recipient_socket, error_msg.c_str(), error_msg.length(), 0);
        logEvent("File transfer failed: " + sender_username + " -> " + recipient_username);
    }
}

/**
 * Process a message and route it appropriately
 * --------------------------------------------
 * Determines message type and calls appropriate handler
 * 
 * Message types:
 * - /list: Show active users
 * - @user msg: Private message
 * - /sendfile user filename size: File transfer (now includes filename)
 * - /quit: Disconnect
 * - Other: Public broadcast
 */
void ChatServer::processMessage(const std::string& message, const std::string& sender_username, int sender_socket) {
    // Command: List active users
    if (message == "/list") {
        std::string user_list = "Active users: " + getActiveUsers();
        
        // Encrypt response if enabled
        if (Encryption::isEnabled()) {
            user_list = Encryption::encrypt(user_list);
        }
        
        send(sender_socket, user_list.c_str(), user_list.length(), 0);
    }
    // Command: Private message (@username message)
    else if (message.find("@") == 0) {
        size_t first_space = message.find(' ', 1);
        if (first_space != std::string::npos) {
            std::string target = message.substr(1, first_space - 1);
            std::string content = message.substr(first_space + 1);
            sendPrivateMessage(target, content, sender_username);
        } else {
            std::string error_msg = "ERROR: Invalid format. Use: @username message";
            if (Encryption::isEnabled()) {
                error_msg = Encryption::encrypt(error_msg);
            }
            send(sender_socket, error_msg.c_str(), error_msg.length(), 0);
        }
    }
    // Command: File transfer (/sendfile username filename file_size)
    else if (message.find("/sendfile") == 0) {
        std::vector<std::string> parts = Utils::split(message, ' ');
        if (parts.size() < 4) {  // NOW NEEDS 4 parts: /sendfile user filename size
            std::string error_msg = "Usage: /sendfile <username> <filename> <file_size>";
            if (Encryption::isEnabled()) {
                error_msg = Encryption::encrypt(error_msg);
            }
            send(sender_socket, error_msg.c_str(), error_msg.length(), 0);
            return;
        }
        
        std::string target_user = parts[1];
        std::string filename = parts[2];  // NEW: Get filename
        long file_size = std::stol(parts[3]);  // NOW index 3 instead of 2
        
        // Validate file size
        if (file_size <= 0 || file_size > 10 * 1024 * 1024) {  // 10MB limit
            std::string error_msg = "ERROR: Invalid file size (max 10MB)";
            if (Encryption::isEnabled()) {
                error_msg = Encryption::encrypt(error_msg);
            }
            send(sender_socket, error_msg.c_str(), error_msg.length(), 0);
            return;
        }
        
        // Handle file transfer SYNCHRONOUSLY - now passes filename
        handleFileTransfer(sender_socket, sender_username, target_user, filename, file_size);
    }
    // Command: Disconnect
    else if (message == "/quit") {
        std::string goodbye = "Goodbye " + sender_username + "!";
        if (Encryption::isEnabled()) {
            goodbye = Encryption::encrypt(goodbye);
        }
        send(sender_socket, goodbye.c_str(), goodbye.length(), 0);
    }
    // Default: Public broadcast message
    else {
        std::string full_message = sender_username + ": " + message;
        broadcast(full_message, sender_username);
    }
}

/**
 * Broadcast a message to all users except sender
 * ----------------------------------------------
 * Thread-safe iteration over clients map
 */
void ChatServer::broadcast(const std::string& message, const std::string& sender) {
    std::string encrypted_message = message;
    if (Encryption::isEnabled()) {
        encrypted_message = Encryption::encrypt(message);
    }
    
    std::lock_guard<std::mutex> lock(clients_mutex);
    for (const auto& pair : clients) {
        if (pair.first != sender) {  // Don't send back to sender
            send(pair.second.socket_fd, encrypted_message.c_str(), encrypted_message.length(), 0);
        }
    }
    logEvent("Broadcast: " + message);
}

/**
 * Send a private message between two users
 * ----------------------------------------
 * Sends to both recipient and sender (for confirmation)
 */
void ChatServer::sendPrivateMessage(const std::string& target, const std::string& message, const std::string& sender) {
    std::lock_guard<std::mutex> lock(clients_mutex);
    
    auto it = clients.find(target);
    if (it != clients.end()) {
        // Format messages
        std::string to_recipient = "[PRIVATE] " + sender + " -> You: " + message;
        std::string to_sender = "[PRIVATE] You -> " + target + ": " + message;
        
        // Encrypt if enabled
        if (Encryption::isEnabled()) {
            to_recipient = Encryption::encrypt(to_recipient);
            to_sender = Encryption::encrypt(to_sender);
        }
        
        // Send to both parties
        send(it->second.socket_fd, to_recipient.c_str(), to_recipient.length(), 0);
        
        auto sender_it = clients.find(sender);
        if (sender_it != clients.end()) {
            send(sender_it->second.socket_fd, to_sender.c_str(), to_sender.length(), 0);
        }
        
        logEvent("Private message: " + sender + " -> " + target);
    } else {
        // Target user not found
        std::string error_msg = "ERROR: User '" + target + "' not found or offline";
        if (Encryption::isEnabled()) {
            error_msg = Encryption::encrypt(error_msg);
        }
        
        auto sender_it = clients.find(sender);
        if (sender_it != clients.end()) {
            send(sender_it->second.socket_fd, error_msg.c_str(), error_msg.length(), 0);
        }
        logEvent("Failed private message to invalid user: " + target);
    }
}

/**
 * Get comma-separated list of active users
 * ----------------------------------------
 * Thread-safe read of clients map
 */
std::string ChatServer::getActiveUsers() {
    std::lock_guard<std::mutex> lock(clients_mutex);
    std::string users;
    for (const auto& pair : clients) {
        if (!users.empty()) users += ", ";
        users += pair.first;
    }
    return users.empty() ? "No users online" : users;
}

/**
 * Register a new client (thread-safe)
 */
void ChatServer::registerClient(const std::string& username, const ClientInfo& client) {
    std::lock_guard<std::mutex> lock(clients_mutex);
    clients[username] = client;
    logEvent("Registered user: " + username + " (Total: " + std::to_string(clients.size()) + ")");
}

/**
 * Remove a client (thread-safe)
 */
void ChatServer::deregisterClient(const std::string& username) {
    std::lock_guard<std::mutex> lock(clients_mutex);
    clients.erase(username);
    logEvent("Deregistered user: " + username + " (Remaining: " + std::to_string(clients.size()) + ")");
}

/**
 * Validate username format
 * ------------------------
 * Rules:
 * - Length: 1-20 characters
 * - Characters: a-z, A-Z, 0-9, underscore, hyphen
 * - Prevents injection attacks
 */
bool ChatServer::isValidUsername(const std::string& username) {
    if (username.empty() || username.length() > 20) {
        return false;
    }
    
    for (char c : username) {
        if (!isalnum(c) && c != '_' && c != '-') {
            return false;
        }
    }
    return true;
}

/**
 * Log event with timestamp
 */
void ChatServer::logEvent(const std::string& event) {
    Utils::logEvent(event);
}

/**
 * Stop the server gracefully
 */
void ChatServer::stop() {
    running = false;
    if (server_fd >= 0) {
        close(server_fd);
        server_fd = -1;
    }
    logEvent("Server stopped");
}

/**
 * MAIN FUNCTION
 * =============
 * Entry point for the server application
 */
int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "   Network Chat Server - Enhanced" << std::endl;
    std::cout << "   Features: Multi-threaded, Encrypted" << std::endl;
    std::cout << "========================================" << std::endl;
    
    ChatServer server(5000);
    
    if (!server.start()) {
        std::cerr << "Failed to start server" << std::endl;
        return 1;
    }
    
    std::cout << "\nServer is running. Press Ctrl+C to stop.\n" << std::endl;
    server.run();
    
    return 0;
}