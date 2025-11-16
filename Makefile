# Makefile for Network Chat Application (Enhanced Version)
# ========================================================
# This Makefile compiles the improved chat application with:
# - Comprehensive error checking
# - Optimization flags
# - Proper dependency management
# - Clean compilation process

# Compiler settings
CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -pthread -I./include

# Optimization flags (use -O2 for production, -g for debugging)
# For debugging: OPTFLAGS = -g -O0
# For production: OPTFLAGS = -O2
OPTFLAGS = -O2

# Directories
SRCDIR = src
INCDIR = include
OBJDIR = obj

# Source files
SERVER_SRC = $(SRCDIR)/server.cpp $(SRCDIR)/utils.cpp $(SRCDIR)/file_transfer.cpp
CLIENT_SRC = $(SRCDIR)/client.cpp $(SRCDIR)/utils.cpp $(SRCDIR)/file_transfer.cpp

# Object files (replace .cpp with .o and change directory)
SERVER_OBJ = $(patsubst $(SRCDIR)/%.cpp,$(OBJDIR)/%.o,$(SERVER_SRC))
CLIENT_OBJ = $(patsubst $(SRCDIR)/%.cpp,$(OBJDIR)/%.o,$(CLIENT_SRC))

# Executables
SERVER = server
CLIENT = client

# Default target: build both server and client
all: $(SERVER) $(CLIENT)
	@echo ""
	@echo "=========================================="
	@echo "   Build Complete!"
	@echo "=========================================="
	@echo "Executables created:"
	@echo "  - ./$(SERVER)  (Run this first)"
	@echo "  - ./$(CLIENT)  (Run in separate terminals)"
	@echo ""
	@echo "Features included:"
	@echo "  ✓ Multi-threaded server"
	@echo "  ✓ Message encryption (XOR cipher)"
	@echo "  ✓ Fixed file transfer (client->server->client)"
	@echo "  ✓ Comprehensive error handling"
	@echo "  ✓ Thread-safe operations"
	@echo ""

# Link server executable
$(SERVER): $(SERVER_OBJ)
	@echo "Linking server..."
	$(CXX) $(CXXFLAGS) $(OPTFLAGS) -o $@ $^
	@echo "✓ Server compiled successfully"

# Link client executable
$(CLIENT): $(CLIENT_OBJ)
	@echo "Linking client..."
	$(CXX) $(CXXFLAGS) $(OPTFLAGS) -o $@ $^
	@echo "✓ Client compiled successfully"

# Compile source files to object files
$(OBJDIR)/%.o: $(SRCDIR)/%.cpp
	@mkdir -p $(OBJDIR)
	@echo "Compiling $<..."
	$(CXX) $(CXXFLAGS) $(OPTFLAGS) -c $< -o $@

# Clean build artifacts
clean:
	@echo "Cleaning build artifacts..."
	rm -rf $(OBJDIR) $(SERVER) $(CLIENT) server_log.txt received_*
	@echo "✓ Clean complete"

# Clean and rebuild everything
rebuild: clean all

# Run server (for convenience)
run-server: $(SERVER)
	@echo "Starting server..."
	./$(SERVER)

# Run client (for convenience)
run-client: $(CLIENT)
	@echo "Starting client..."
	./$(CLIENT)

# Count lines of code (useful for project reports)
count:
	@echo "Lines of code:"
	@echo "  Headers:"
	@wc -l $(INCDIR)/*.hpp | tail -1
	@echo "  Source:"
	@wc -l $(SRCDIR)/*.cpp | tail -1
	@echo "  Total:"
	@cat $(INCDIR)/*.hpp $(SRCDIR)/*.cpp | wc -l

# Display help
help:
	@echo "Network Chat Application - Makefile Help"
	@echo "========================================"
	@echo ""
	@echo "Targets:"
	@echo "  make          - Build both server and client (default)"
	@echo "  make all      - Same as 'make'"
	@echo "  make server   - Build only the server"
	@echo "  make client   - Build only the client"
	@echo "  make clean    - Remove all build artifacts"
	@echo "  make rebuild  - Clean and rebuild everything"
	@echo "  make run-server - Build and run server"
	@echo "  make run-client - Build and run client"
	@echo "  make count    - Count lines of code"
	@echo "  make help     - Display this help message"
	@echo ""
	@echo "Example workflow:"
	@echo "  1. make              # Build everything"
	@echo "  2. ./server          # Start server in terminal 1"
	@echo "  3. ./client          # Start client in terminal 2"
	@echo "  4. ./client          # Start another client in terminal 3"
	@echo ""

# Phony targets (not actual files)
.PHONY: all clean rebuild run-server run-client count help

# Dependencies
# If headers change, recompile affected sources
$(OBJDIR)/server.o: $(INCDIR)/server.hpp $(INCDIR)/utils.hpp $(INCDIR)/file_transfer.hpp $(INCDIR)/encryption.hpp
$(OBJDIR)/client.o: $(INCDIR)/client.hpp $(INCDIR)/utils.hpp $(INCDIR)/file_transfer.hpp $(INCDIR)/encryption.hpp
$(OBJDIR)/file_transfer.o: $(INCDIR)/file_transfer.hpp $(INCDIR)/utils.hpp
$(OBJDIR)/utils.o: $(INCDIR)/utils.hpp