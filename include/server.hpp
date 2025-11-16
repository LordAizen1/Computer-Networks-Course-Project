#ifndef SERVER_HPP
#define SERVER_HPP

#include <string>
#include <map>
#include <mutex>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

/**
 * @struct ClientInfo
 * @brief Stores information about a connected client
 * 
 * This structure maintains the essential information needed to manage
 * and communicate with each connected client in the chat system.
 */
struct ClientInfo {
    int socket_fd;              // File descriptor for the client's socket connection
    std::string username;       // Unique identifier for the client
    sockaddr_in address;        // Network address information for the client
    
    // Constructor for easy initialization
    ClientInfo(int fd = -1, const std::string& name = "", sockaddr_in addr = {})
        : socket_fd(fd), username(name), address(addr) {}
};

/**
 * @class ChatServer
 * @brief Multi-threaded TCP server for managing chat communications
 * 
 * This server implements a concurrent client-server architecture where:
 * - Each client connection is handled in a separate thread
 * - All shared data structures are protected by mutexes for thread safety
 * - Supports public broadcasting, private messaging, and file transfers
 * - Maintains a registry of all connected clients
 * 
 * Architecture:
 * 1. Main thread listens for new connections
 * 2. Each accepted connection spawns a dedicated handler thread
 * 3. Handler threads process messages and route them appropriately
 * 4. File transfers are handled synchronously to avoid race conditions
 */
class ChatServer {
private:
    int server_fd;                                  // Server socket file descriptor
    int port;                                       // Port number to bind to
    sockaddr_in address;                            // Server address configuration
    bool running;                                   // Flag to control server lifecycle
    
    /**
     * Thread-safe client registry
     * Maps username -> ClientInfo
     * Protected by clients_mutex to prevent race conditions during:
     * - Client registration/deregistration
     * - Message routing lookups
     * - User list generation
     */
    std::map<std::string, ClientInfo> clients;
    std::mutex clients_mutex;                       // Protects concurrent access to clients map
    
    /**
     * @brief Handles all communication for a single client connection
     * @param client_socket Socket file descriptor for this client
     * @param client_addr Network address of the client
     * 
     * This method runs in a separate thread for each client and:
     * 1. Authenticates the user (username validation)
     * 2. Registers the client in the global registry
     * 3. Processes incoming messages in a loop
     * 4. Cleans up resources on disconnect
     */
    void handleClient(int client_socket, sockaddr_in client_addr);
    
    /**
     * @brief Processes and routes a message based on its type
     * @param message The raw message string received from client
     * @param sender_username Username of the message sender
     * @param sender_socket Socket for sending responses to sender
     * 
     * Message types handled:
     * - "/list" -> Returns list of active users
     * - "@username msg" -> Routes private message
     * - "/sendfile user filename size" -> Initiates file transfer
     * - Plain text -> Broadcasts to all users
     */
    void processMessage(const std::string& message, const std::string& sender_username, int sender_socket);
    
    /**
     * @brief Broadcasts a message to all connected clients except sender
     * @param message Message content to broadcast
     * @param sender Username of the sender (excluded from broadcast)
     * 
     * Thread-safe: Locks clients_mutex while iterating
     */
    void broadcast(const std::string& message, const std::string& sender);
    
    /**
     * @brief Sends a private message between two users
     * @param target Username of the recipient
     * @param message Message content
     * @param sender Username of the sender
     * 
     * Sends confirmation to both sender and recipient
     * Handles case where target user doesn't exist
     */
    void sendPrivateMessage(const std::string& target, const std::string& message, const std::string& sender);
    
    /**
     * @brief Manages the file transfer protocol between two clients
     * @param sender_socket Socket of the user sending the file
     * @param sender_username Username of the sender
     * @param recipient_username Username of the recipient
     * @param filename Original filename (with extension)
     * @param file_size Size of the file in bytes
     * 
     * File Transfer Protocol:
     * 1. Server notifies recipient about incoming file (with filename)
     * 2. Waits for recipient's acceptance
     * 3. If accepted, sends /file_data with filename and size
     * 4. Facilitates data streaming from sender to recipient
     * 5. Provides progress updates to both parties
     * 
     * Note: This improved version streams file data and preserves extensions
     */
    void handleFileTransfer(int sender_socket, const std::string& sender_username,
                          const std::string& recipient_username, 
                          const std::string& filename, long file_size);
    
    /**
     * @brief Generates a comma-separated list of active usernames
     * @return String containing all connected usernames
     * 
     * Thread-safe: Locks clients_mutex during iteration
     */
    std::string getActiveUsers();
    
    /**
     * @brief Adds a client to the global registry
     * @param username Client's username
     * @param client ClientInfo structure containing connection details
     * 
     * Thread-safe: Uses mutex lock
     */
    void registerClient(const std::string& username, const ClientInfo& client);
    
    /**
     * @brief Removes a client from the global registry
     * @param username Username to remove
     * 
     * Called when a client disconnects
     * Thread-safe: Uses mutex lock
     */
    void deregisterClient(const std::string& username);
    
    /**
     * @brief Validates username according to security rules
     * @param username Username to validate
     * @return true if username is valid, false otherwise
     * 
     * Rules:
     * - Length: 1-20 characters
     * - Characters: Alphanumeric, underscore, hyphen only
     * - Prevents injection attacks and ensures display compatibility
     */
    bool isValidUsername(const std::string& username);
    
    /**
     * @brief Logs server events with timestamp
     * @param event Event description to log
     * 
     * Logs to both console and file for debugging and auditing
     */
    void logEvent(const std::string& event);
    
public:
    /**
     * @brief Constructs a chat server instance
     * @param port Port number to listen on (default: 5000)
     */
    explicit ChatServer(int port = 5000);
    
    /**
     * @brief Destructor - ensures clean shutdown
     */
    ~ChatServer();
    
    /**
     * @brief Initializes the server socket and begins listening
     * @return true if successful, false on error
     * 
     * Steps:
     * 1. Creates TCP socket
     * 2. Sets socket options (SO_REUSEADDR)
     * 3. Binds to specified port
     * 4. Begins listening for connections
     */
    bool start();
    
    /**
     * @brief Main server loop - accepts and handles client connections
     * 
     * Runs indefinitely until stop() is called
     * For each connection:
     * 1. Accepts the connection
     * 2. Spawns a detached thread to handle the client
     * 3. Returns to accept next connection
     */
    void run();
    
    /**
     * @brief Gracefully shuts down the server
     * 
     * Sets running flag to false and closes server socket
     */
    void stop();
};

#endif // SERVER_HPP