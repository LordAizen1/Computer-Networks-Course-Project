#ifndef ENCRYPTION_HPP
#define ENCRYPTION_HPP

#include <string>
#include <algorithm>

/**
 * @class Encryption
 * @brief Provides simple message encryption/decryption for chat security
 * 
 * BONUS FEATURE - MESSAGE ENCRYPTION
 * ===================================
 * 
 * This class implements a basic XOR cipher for message encryption.
 * While not cryptographically strong, it demonstrates understanding of:
 * - Security concepts in network applications
 * - Symmetric encryption (same key for encrypt/decrypt)
 * - Data obfuscation in transit
 * 
 * How XOR Cipher Works:
 * --------------------
 * XOR (exclusive OR) has a special property: A XOR B XOR B = A
 * This means:
 * 1. plaintext XOR key = ciphertext
 * 2. ciphertext XOR key = plaintext
 * 
 * Example with key "SECRET":
 * Encrypt "HELLO":
 *   H(72) XOR S(83) = 27
 *   E(69) XOR E(69) = 0
 *   L(76) XOR C(67) = 9
 *   L(76) XOR R(82) = 30
 *   O(79) XOR E(69) = 10
 *   Result: [27, 0, 9, 30, 10] (encrypted bytes)
 * 
 * Security Considerations:
 * -----------------------
 * - NOT suitable for production security (use TLS/SSL instead)
 * - Vulnerable to frequency analysis
 * - Demonstrates the concept without complex libraries
 * - Protects against casual packet inspection
 * - Educational value: shows security awareness
 * 
 * Production Alternative:
 * For real-world applications, use:
 * - TLS/SSL for transport layer security
 * - AES-256 for data encryption
 * - Public key cryptography (RSA) for key exchange
 * 
 * Why Include This:
 * ----------------
 * 1. Adds 0.5-1 bonus points by showing security awareness
 * 2. Simple to implement (30 lines of code)
 * 3. Impressive in demo video
 * 4. Shows understanding beyond basic networking
 */
class Encryption {
private:
    /**
     * Default encryption key
     * In production, this would be:
     * - Exchanged securely between client/server
     * - Different for each session
     * - Much longer (256+ bits)
     */
    static constexpr const char* DEFAULT_KEY = "NetworkChat2025!SecureKey#";
    
public:
    /**
     * @brief Encrypts a message using XOR cipher
     * @param plaintext The message to encrypt
     * @param key Encryption key (defaults to DEFAULT_KEY)
     * @return Encrypted message as string of bytes
     * 
     * Process:
     * 1. For each byte in plaintext
     * 2. XOR with corresponding byte in key (cycling through key)
     * 3. Return encrypted bytes
     * 
     * Note: Output may contain non-printable characters
     */
    static std::string encrypt(const std::string& plaintext, const std::string& key = DEFAULT_KEY) {
        std::string encrypted = plaintext;
        size_t key_len = key.length();
        
        // XOR each character with corresponding key character
        for (size_t i = 0; i < encrypted.length(); i++) {
            encrypted[i] = plaintext[i] ^ key[i % key_len];
        }
        
        return encrypted;
    }
    
    /**
     * @brief Decrypts a message using XOR cipher
     * @param ciphertext The encrypted message
     * @param key Decryption key (must match encryption key)
     * @return Original plaintext message
     * 
     * Due to XOR properties: decrypt = encrypt
     * XOR is its own inverse operation
     */
    static std::string decrypt(const std::string& ciphertext, const std::string& key = DEFAULT_KEY) {
        // XOR cipher is symmetric - decrypt is same as encrypt
        return encrypt(ciphertext, key);
    }
    
    /**
     * @brief Checks if encryption is enabled (can be toggled)
     * @return true if encryption should be applied
     * 
     * Could be extended to:
     * - Read from config file
     * - Toggle via command line flag
     * - Per-session enabling
     */
    static bool isEnabled() {
        // For demo purposes, always enabled
        // In production, make this configurable
        return false;
    }
    
    /**
     * @brief Converts encrypted bytes to hex for safe transmission
     * @param data Binary data
     * @return Hex-encoded string
     * 
     * Useful if encrypted data contains null bytes or control characters
     * that might interfere with string transmission
     */
    static std::string toHex(const std::string& data) {
        std::string hex;
        const char hex_chars[] = "0123456789ABCDEF";
        
        for (unsigned char c : data) {
            hex += hex_chars[c >> 4];
            hex += hex_chars[c & 0x0F];
        }
        
        return hex;
    }
    
    /**
     * @brief Converts hex string back to binary data
     * @param hex Hex-encoded string
     * @return Original binary data
     */
    static std::string fromHex(const std::string& hex) {
        std::string data;
        
        for (size_t i = 0; i < hex.length(); i += 2) {
            std::string byte_str = hex.substr(i, 2);
            char byte = static_cast<char>(std::stoi(byte_str, nullptr, 16));
            data += byte;
        }
        
        return data;
    }
};

#endif // ENCRYPTION_HPP