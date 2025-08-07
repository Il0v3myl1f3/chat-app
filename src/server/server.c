#include "../../include/common.h"

int initialize_network(void) {
#ifdef _WIN32
    WSADATA wsa_data;
    return WSAStartup(MAKEWORD(2, 2), &wsa_data);
#else
    return 0;
#endif
}

void cleanup_network(void) {
#ifdef _WIN32
    WSACleanup();
#endif
}

int create_socket(void) {
    SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET) {
        print_error("Failed to create socket");
        return -1;
    }

    int opt = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char *) &opt, sizeof(opt)) < 0) {
        print_error("Failed to set socket options");
        CLOSE_SOCKET(sock);
        return -1;
    }

    return sock;
}

int bind_socket(SOCKET sock, int port) {
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    if (bind(sock, (struct sockaddr *) &server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        print_error("Failed to bind socket");
        return -1;
    }

    return 0;
}

int listen_socket(SOCKET sock, int backlog) {
    if (listen(sock, backlog) == SOCKET_ERROR) {
        print_error("Failed to listen on socket");
        return -1;
    }
    return 0;
}

SOCKET accept_connection(SOCKET sock, struct sockaddr_in *client_addr) {
    int addr_len = sizeof(*client_addr);
    SOCKET client_sock = accept(sock, (struct sockaddr *) client_addr, &addr_len);
    if (client_sock == INVALID_SOCKET) {
        print_error("Failed to accept connection");
        return INVALID_SOCKET;
    }
    return client_sock;
}

int send_message(SOCKET sock, const MessageInfo *msg) {
    return send(sock, (char *) msg, sizeof(MessageInfo), 0);
}

int receive_message(SOCKET sock, MessageInfo *msg) {
    return recv(sock, (char *) msg, sizeof(MessageInfo), 0);
}

int server_init(ServerState *server, int port) {
    memset(server, 0, sizeof(ServerState));
    server->next_client_id = 1;

    if (pthread_mutex_init(&server->clients_mutex, NULL) != 0) {
        print_error("Failed to initialize clients mutex");
        return -1;
    }

    if (pthread_mutex_init(&server->nicknames_mutex, NULL) != 0) {
        print_error("Failed to initialize nicknames mutex");
        pthread_mutex_destroy(&server->clients_mutex);
        return -1;
    }

    if (initialize_network() != 0) {
        print_error(FAILED_INIT_MESSAGE);
        pthread_mutex_destroy(&server->clients_mutex);
        pthread_mutex_destroy(&server->nicknames_mutex);
        return -1;
    }

    server->server_socket = create_socket();
    if (server->server_socket == INVALID_SOCKET) {
        cleanup_network();
        pthread_mutex_destroy(&server->clients_mutex);
        pthread_mutex_destroy(&server->nicknames_mutex);
        return -1;
    }

    if (bind_socket(server->server_socket, port) != 0) {
        CLOSE_SOCKET(server->server_socket);
        cleanup_network();
        pthread_mutex_destroy(&server->clients_mutex);
        pthread_mutex_destroy(&server->nicknames_mutex);
        return -1;
    }

    if (listen_socket(server->server_socket, 5) != 0) {
        CLOSE_SOCKET(server->server_socket);
        cleanup_network();
        pthread_mutex_destroy(&server->clients_mutex);
        pthread_mutex_destroy(&server->nicknames_mutex);
        return -1;
    }

    return 0;
}

void server_cleanup(ServerState *server) {
    pthread_mutex_lock(&server->clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (server->clients[i].active) {
            CLOSE_SOCKET(server->clients[i].socket);
        }
    }
    pthread_mutex_unlock(&server->clients_mutex);

    CLOSE_SOCKET(server->server_socket);
    cleanup_network();
    pthread_mutex_destroy(&server->clients_mutex);
    pthread_mutex_destroy(&server->nicknames_mutex);
}

int is_nickname_available(ServerState *server, const char *nickname) {
    pthread_mutex_lock(&server->nicknames_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (server->nicknames[i].active && strcmp(server->nicknames[i].nickname, nickname) == 0) {
            pthread_mutex_unlock(&server->nicknames_mutex);
            return 0;
        }
    }
    pthread_mutex_unlock(&server->nicknames_mutex);
    return 1;
}

int register_nickname(ServerState *server, const char *nickname, int client_id) {
    pthread_mutex_lock(&server->nicknames_mutex);

    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (server->nicknames[i].active && strcmp(server->nicknames[i].nickname, nickname) == 0) {
            pthread_mutex_unlock(&server->nicknames_mutex);
            return -1;
        }
    }

    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (!server->nicknames[i].active) {
            strcpy(server->nicknames[i].nickname, nickname);
            server->nicknames[i].client_id = client_id;
            server->nicknames[i].active = 1;
            pthread_mutex_unlock(&server->nicknames_mutex);
            return 0;
        }
    }

    pthread_mutex_unlock(&server->nicknames_mutex);
    return -1;
}

void unregister_nickname(ServerState *server, int client_id) {
    pthread_mutex_lock(&server->nicknames_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (server->nicknames[i].active && server->nicknames[i].client_id == client_id) {
            server->nicknames[i].active = 0;
            break;
        }
    }
    pthread_mutex_unlock(&server->nicknames_mutex);
}

int find_client_by_nickname(ServerState *server, const char *nickname) {
    pthread_mutex_lock(&server->clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (server->clients[i].active && strcmp(server->clients[i].nickname, nickname) == 0) {
            pthread_mutex_unlock(&server->clients_mutex);
            return i;
        }
    }
    pthread_mutex_unlock(&server->clients_mutex);
    return -1;
}

int add_client(ServerState *server, SOCKET client_socket, struct sockaddr_in client_addr) {
    pthread_mutex_lock(&server->clients_mutex);

    if (server->client_count >= MAX_CLIENTS) {
        pthread_mutex_unlock(&server->clients_mutex);
        return -1;
    }

    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (!server->clients[i].active) {
            server->clients[i].socket = client_socket;
            server->clients[i].address = client_addr;
            server->clients[i].active = 1;
            server->clients[i].client_id = server->next_client_id++;
            strcpy(server->clients[i].nickname, "Anonymous");
            server->client_count++;
            pthread_mutex_unlock(&server->clients_mutex);
            return i;
        }
    }

    pthread_mutex_unlock(&server->clients_mutex);
    return -1;
}

void remove_client(ServerState *server, int client_index) {
    pthread_mutex_lock(&server->clients_mutex);

    if (client_index >= 0 && client_index < MAX_CLIENTS && server->clients[client_index].active) {
        unregister_nickname(server, server->clients[client_index].client_id);
        CLOSE_SOCKET(server->clients[client_index].socket);
        server->clients[client_index].active = 0;
        server->client_count--;
        print_system_message("Client disconnected");
    }

    pthread_mutex_unlock(&server->clients_mutex);
}

void broadcast_message(ServerState *server, const MessageInfo *msg, int exclude_index) {
    pthread_mutex_lock(&server->clients_mutex);

    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (server->clients[i].active && i != exclude_index) {
            if (send_message(server->clients[i].socket, msg) == SOCKET_ERROR) {
                print_error("Failed to send message to client");
                pthread_mutex_unlock(&server->clients_mutex);
                remove_client(server, i);
                pthread_mutex_lock(&server->clients_mutex);
            }
        }
    }

    pthread_mutex_unlock(&server->clients_mutex);
}

int server_send_private_message(ServerState *server, const MessageInfo *msg) {
    int target_index = find_client_by_nickname(server, msg->target_nickname);

    if (target_index == -1) {
        return -1;
    }

    pthread_mutex_lock(&server->clients_mutex);
    if (server->clients[target_index].active) {
        int result = send_message(server->clients[target_index].socket, msg);
        pthread_mutex_unlock(&server->clients_mutex);
        return (result == SOCKET_ERROR) ? -1 : 0;
    }
    pthread_mutex_unlock(&server->clients_mutex);
    return -1;
}

typedef struct {
    ServerState *server;
    int client_index;
} client_thread_data_t;

static int handle_join_message(ServerState *server, int client_index, const MessageInfo *msg) {
    if (is_nickname_available(server, msg->nickname)) {
        if (register_nickname(server, msg->nickname, server->clients[client_index].client_id) == 0) {
            strcpy(server->clients[client_index].nickname, msg->nickname);
            print_system_message("User joined the chat");
            printf("Nickname: " CYAN "%s" RESET " (ID: " YELLOW "%d" RESET ")\n",
                   msg->nickname, server->clients[client_index].client_id);

            MessageInfo success_msg;
            success_msg.type = MSG_TYPE_NICKNAME_AVAILABLE;
            strcpy(success_msg.nickname, "Server");
            sprintf(success_msg.message, "Nickname '%s' is registered!", msg->nickname);
            success_msg.timestamp = time(NULL);
            send_message(server->clients[client_index].socket, &success_msg);

            MessageInfo join_msg = *msg;
            join_msg.type = MSG_TYPE_SYSTEM;
            sprintf(join_msg.message, "%s joined the chat", msg->nickname);
            broadcast_message(server, &join_msg, client_index);
        } else {
            MessageInfo error_msg;
            error_msg.type = MSG_TYPE_NICKNAME_TAKEN;
            strcpy(error_msg.nickname, "Server");
            strcpy(error_msg.message, "Failed to register nickname. Please try again.");
            error_msg.timestamp = time(NULL);
            send_message(server->clients[client_index].socket, &error_msg);
        }
    } else {
        MessageInfo taken_msg;
        taken_msg.type = MSG_TYPE_NICKNAME_TAKEN;
        strcpy(taken_msg.nickname, "Server");
        sprintf(taken_msg.message, "Nickname '%s' is already taken. Please choose another.", msg->nickname);
        taken_msg.timestamp = time(NULL);
        send_message(server->clients[client_index].socket, &taken_msg);
    }
    return 0;
}

static int handle_private_message(ServerState *server, int client_index, const MessageInfo *msg) {
    printf(MAGENTA "[PRIVATE]" RESET " From " CYAN "%s" RESET " to " CYAN "%s" RESET ": %s\n",
           msg->nickname, msg->target_nickname, msg->message);

    if (server_send_private_message(server, msg) != 0) {
        MessageInfo error_msg;
        error_msg.type = MSG_TYPE_SYSTEM;
        strcpy(error_msg.nickname, "Server");
        sprintf(error_msg.message, "User '%s' not found or offline.", msg->target_nickname);
        error_msg.timestamp = time(NULL);
        send_message(server->clients[client_index].socket, &error_msg);
    }
    return 0;
}

static int handle_leave_message(ServerState *server, int client_index, const MessageInfo *msg) {
    print_system_message("User left the chat");
    printf("Nickname: " CYAN "%s" RESET " (ID: " YELLOW "%d" RESET ")\n", msg->nickname,
           server->clients[client_index].client_id);

    MessageInfo leave_msg = *msg;
    leave_msg.type = MSG_TYPE_SYSTEM;
    sprintf(leave_msg.message, "%s left the chat", msg->nickname);
    broadcast_message(server, &leave_msg, client_index);
    return 1;
}

static int process_message(ServerState *server, int client_index, const MessageInfo *msg) {
    switch (msg->type) {
        case MSG_TYPE_JOIN:
            return handle_join_message(server, client_index, msg);

        case MSG_TYPE_CHAT:
            print_message(msg->nickname, msg->message);
            broadcast_message(server, msg, client_index);
            return 0;

        case MSG_TYPE_PRIVATE:
            return handle_private_message(server, client_index, msg);

        case MSG_TYPE_LEAVE:
            return handle_leave_message(server, client_index, msg);

        default:
            print_error("Unknown message type received");
            return 0;
    }
}

void *handle_client(void *arg) {
    client_thread_data_t *data = arg;
    ServerState *server = data->server;
    int client_index = data->client_index;
    free(data);

    MessageInfo msg;
    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &server->clients[client_index].address.sin_addr, client_ip, INET_ADDRSTRLEN);

    print_system_message("New client connected");
    printf("Client IP: " CYAN "%s" RESET ", Port: " CYAN "%d" RESET ", ID: " YELLOW "%d" RESET "\n",
           client_ip, ntohs(server->clients[client_index].address.sin_port),
           server->clients[client_index].client_id);

    MessageInfo welcome_msg;
    welcome_msg.type = MSG_TYPE_SYSTEM;
    strcpy(welcome_msg.nickname, "Server");
    strcpy(welcome_msg.message, "Welcome to the chat room!");
    welcome_msg.timestamp = time(NULL);
    send_message(server->clients[client_index].socket, &welcome_msg);

    int should_disconnect = 0;
    while (!should_disconnect) {
        int bytes_received = receive_message(server->clients[client_index].socket, &msg);

        if (bytes_received <= 0) {
            print_error("Client disconnected or error occurred");
            break;
        }

        int result = process_message(server, client_index, &msg);
        if (result == 1) {
            should_disconnect = 1;
        }
    }

    remove_client(server, client_index);
    pthread_exit(NULL);
}

int main(int argc, char *argv[]) {
    int port = DEFAULT_PORT;
    ServerState server;

    if (argc > 1) {
        port = atoi(argv[1]);
    }

    print_system_message("Starting chat server...");
    printf("Port: " BOLD_CYAN "%d" RESET "\n", port);

    if (server_init(&server, port) != 0) {
        return 1;
    }

    print_success("Server started successfully");
    print_system_message("Waiting for client connections...");

    while (1) {
        struct sockaddr_in client_addr;
        SOCKET client_socket = accept_connection(server.server_socket, &client_addr);

        if (client_socket == INVALID_SOCKET) {
            continue;
        }

        int client_index = add_client(&server, client_socket, client_addr);
        if (client_index == -1) {
            print_error("Maximum number of clients reached");
            CLOSE_SOCKET(client_socket);
            continue;
        }

        client_thread_data_t *thread_data = malloc(sizeof(client_thread_data_t));
        if (thread_data == NULL) {
            print_error("Failed to allocate memory for thread data");
            remove_client(&server, client_index);
            continue;
        }

        thread_data->server = &server;
        thread_data->client_index = client_index;

        if (pthread_create(&server.clients[client_index].thread_id, NULL, handle_client, thread_data) != 0) {
            print_error("Failed to create client thread");
            remove_client(&server, client_index);
            free(thread_data);
            continue;
        }

        pthread_detach(server.clients[client_index].thread_id);
    }

    server_cleanup(&server);
    return 0;
}
