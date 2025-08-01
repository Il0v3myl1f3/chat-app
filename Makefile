CC = gcc
CFLAGS = -Wall -Wextra -pthread
INCLUDE = -Iinclude

SRC_DIR = src
SERVER_SRC = $(SRC_DIR)/server/server_main.c $(SRC_DIR)/server/server_net.c
CLIENT_SRC = $(SRC_DIR)/client/client_terminal.c $(SRC_DIR)/client/client_net_terminal.c

all: server client

server: $(SERVER_SRC)
	$(CC) $(CFLAGS) $(INCLUDE) -o server $(SERVER_SRC)

client: $(CLIENT_SRC)
	$(CC) $(CFLAGS) $(INCLUDE) -o client $(CLIENT_SRC)

clean:
	rm -f server client

.PHONY: all clean server client