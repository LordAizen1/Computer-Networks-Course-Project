#ifndef CLIENT_HPP
#define CLIENT_HPP

#include <string>
#include <thread>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/socket.h>
#include <condition_variable>

/**
 * @class ChatClient
 * @brief Client application for connecting to and communicating with the chat server
 * 
 * Architecture:
 * - Main thread: Handles user input and sends messages to server
 * - Receiver thread: Continuously listens for incoming messages from server
 * 
 * This dual-threaded design allows simultaneous sending and receiving,
 * providing a responsive user experience where the user can type while
 * receiving messages from others.
 * 
 * Features:
 * - Public and private messaging
 * - File transfer (send and receive)
 * - User list display
 * - Message encryption/decryption
 */
class ChatClient {
private:
    int client_socket;                  // Socket file descriptor for server connection
    std::string server_ip;              // IP address of the server
    int server_port;                    // Port number of the server
    std::string username;               // This client's username
    bool connected;                     // Connection state flag
    std::thread* receiver_thread;       // Thread for receiving messages
    bool file_ready;
    std::mutex file_mutex;
    std::condition_variable file_cv;
    
    /**
     * @brief Establishes TCP connection to the server
     * @return true if connection successful, false otherwise
     * 
     * Steps:
     * 1. Creates a TCP socket
     * 2. Converts server IP string to network format
     * 3. Attempts to connect to server address
     * 4. Sets connected flag on success
     */
    bool connectToServer();
    
    /**
     * @brief Continuously receives and displays messages from server
     * 
     * This method runs in a separate thread and:
     * 1. Blocks on recv() waiting for server data
     * 2. Handles special message types (file offers, file data)
     * 3. Displays regular messages to console
     * 4. Exits when connection is lost
     * 
     * Message types handled:
     * - "/file_offer" -> Prompts user to accept/reject file
     * - "/file_data" -> Receives and saves incoming file
     * - "[FILE]" -> File transfer status updates
     * - "ERROR:" -> Error messages
     * - Plain text -> Regular chat messages
     */
    void receiveMessages();
    
    /**
     * @brief Handles incoming file transfer offer
     * @param metadata String containing file offer details
     * 
     * Prompts the user to accept (y) or reject (n) the file transfer
     * Sends response back to server
     */
    void handleFileOffer(const std::string& metadata);
    
    /**
     * @brief Sends a message to the server
     * @param message Message string to send
     * 
     * Thread-safe wrapper around send() system call
     */
    void sendMessage(const std::string& message);
    
    /**
     * @brief Disconnects from server and cleans up resources
     * 
     * Steps:
     * 1. Sets connected flag to false
     * 2. Closes socket connection
     * 3. Joins receiver thread if running
     * 4. Frees thread memory
     */
    void disconnect();
    
    /**
     * @brief Formats file size for human-readable display
     * @param size File size in bytes
     * @return Formatted string (e.g., "1.5 MB", "234 KB")
     */
    std::string formatFileSize(long size);
    
public:
    /**
     * @brief Constructs a chat client instance
     * @param ip Server IP address (default: localhost)
     * @param port Server port number (default: 5000)
     */
    ChatClient(const std::string& ip = "127.0.0.1", int port = 5000);
    
    /**
     * @brief Destructor - ensures clean disconnect
     */
    ~ChatClient();
    
    /**
     * @brief Main client execution loop
     * 
     * Steps:
     * 1. Connects to server
     * 2. Prompts for and sends username
     * 3. Starts receiver thread
     * 4. Enters input loop for user commands
     * 5. Processes commands (/quit, /list, /sendfile, @user)
     * 6. Disconnects on exit
     */
    void start();
};

#endif // CLIENT_HPP