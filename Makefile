# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -pthread
INCLUDE = -Iinclude

# Source directories
SRC_DIR = src
SERVER_DIR = $(SRC_DIR)/server
CLIENT_DIR = $(SRC_DIR)/client

# Source files
SERVER_SRC = $(SERVER_DIR)/server.c $(SERVER_DIR)/print_functions.c
CLIENT_SRC = $(CLIENT_DIR)/client.c

# Output binaries
SERVER_BIN = server
CLIENT_BIN = client

# Default target
all: server client build-success

# Compile server
server: $(SERVER_SRC)
    @echo "Compiling server..."
    $(CC) $(CFLAGS) $(INCLUDE) -o $(SERVER_BIN) $(SERVER_SRC)
    @echo

# Compile client
client: $(CLIENT_SRC)
    @echo "Compiling client..."
    $(CC) $(CFLAGS) $(INCLUDE) -o $(CLIENT_BIN) $(CLIENT_SRC)
    @echo

# Success message
build-success:
    @echo "Build completed successfully!"
    @echo
    @echo "To run the application:"
    @echo "1. Start server: ./$(SERVER_BIN) [port]"
    @echo "2. Start client: ./$(CLIENT_BIN) [server_ip] [port]"
    @echo
    @echo "Example:"
    @echo "  ./$(SERVER_BIN) 8888"
    @echo "  ./$(CLIENT_BIN) 127.0.0.1 8888"
    @echo

# Clean build files
clean:
    rm -f $(SERVER_BIN) $(CLIENT_BIN)

.PHONY: all server client clean build-success
