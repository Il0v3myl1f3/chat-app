#include "../../include/common.h"

static void safe_strcpy(char* dst, const char* src, size_t cap) {
    if (!dst || !cap) return;
    if (!src) { dst[0] = '\0'; return; }
    strncpy(dst, src, cap - 1);
    dst[cap - 1] = '\0';
}

int parse_private_message(const char* input, char* target, char* message) {
    if (strncmp(input, "/shh ", 5) != 0) return -1;
    input += 5;
    while (*input == ' ') input++;
    if (*input == '\0') return -1;

    int i = 0;
    while (*input && *input != ' ' && i < MAX_NICK_LEN - 1) {
        target[i++] = *input++;
    }
    target[i] = '\0';
    if (strlen(target) == 0) return -1;

    while (*input == ' ') input++;
    if (strlen(input) == 0) return -1;

    strncpy(message, input, MAX_MSG_LEN - 1);
    message[MAX_MSG_LEN - 1] = '\0';
    return 0;
}

int validate_nickname(const char* nickname, char* reason_out, size_t reason_cap) {
    if (!nickname || nickname[0] == '\0') {
        if (reason_out && reason_cap) safe_strcpy(reason_out, "empty nickname", reason_cap);
        return 0;
    }
    size_t n = strnlen(nickname, MAX_NICK_LEN + 1);
    if (n >= MAX_NICK_LEN) {
        if (reason_out && reason_cap) safe_strcpy(reason_out, "too long", reason_cap);
        return 0;
    }
    for (size_t i = 0; nickname[i]; ++i) {
        if (!is_allowed_nick_char((unsigned char)nickname[i])) {
            if (reason_out && reason_cap) safe_strcpy(reason_out, "invalid character", reason_cap);
            return 0;
        }
    }
    if (strcmp(nickname, "Anonymous") == 0) {
        if (reason_out && reason_cap) safe_strcpy(reason_out, "reserved name", reason_cap);
        return 0;
    }
    return 1;
}

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
    (void)setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt));
#ifdef SO_REUSEPORT
    (void)setsockopt(sock, SOL_SOCKET, SO_REUSEPORT, (char*)&opt, sizeof(opt));
#endif
    return sock;
}

int bind_socket(SOCKET sock, int port) {
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);
    if (bind(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
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

SOCKET accept_connection(SOCKET sock, struct sockaddr_in* client_addr) {
    socklen_t addr_len = (socklen_t)sizeof(*client_addr);
    SOCKET client_sock = accept(sock, (struct sockaddr*)client_addr, &addr_len);
    if (client_sock == INVALID_SOCKET) {
        print_error("Failed to accept connection");
        return INVALID_SOCKET;
    }
    return client_sock;
}

int send_message(SOCKET sock, const MessageInfo* msg) {
    return send(sock, (const char*)msg, sizeof(MessageInfo), 0);
}

int receive_message(SOCKET sock, MessageInfo* msg) {
    return recv(sock, (char*)msg, sizeof(MessageInfo), 0);
}

int server_init(ServerState* server, int port) {
    memset(server, 0, sizeof(ServerState));
    server->next_client_id = 1;
    if (pthread_mutex_init(&server->clients_mutex, NULL) != 0) {
        print_error("Failed to initialize clients mutex");
        return -1;
    }
    if (initialize_network() != 0) {
        print_error(FAILED_INIT_MESSAGE);
        pthread_mutex_destroy(&server->clients_mutex);
        return -1;
    }
    server->server_socket = create_socket();
    if (server->server_socket == INVALID_SOCKET) {
        cleanup_network();
        pthread_mutex_destroy(&server->clients_mutex);
        return -1;
    }
    if (bind_socket(server->server_socket, port) != 0) {
        CLOSE_SOCKET(server->server_socket);
        cleanup_network();
        pthread_mutex_destroy(&server->clients_mutex);
        return -1;
    }
    if (listen_socket(server->server_socket, 64) != 0) {
        CLOSE_SOCKET(server->server_socket);
        cleanup_network();
        pthread_mutex_destroy(&server->clients_mutex);
        return -1;
    }
    return 0;
}

void server_cleanup(ServerState* server) {
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
}

static void system_msg_to_socket(SOCKET s, const char* text) {
    MessageInfo msg;
    memset(&msg, 0, sizeof(msg));
    msg.type = MSG_TYPE_SYSTEM;
    safe_strcpy(msg.nickname, "Server", sizeof(msg.nickname));
    safe_strcpy(msg.message, text, sizeof(msg.message));
    msg.timestamp = time(NULL);
    (void)send_message(s, &msg);
}

static void system_msg_broadcast(ServerState* server, const char* text, int exclude_index) {
    MessageInfo msg;
    memset(&msg, 0, sizeof(msg));
    msg.type = MSG_TYPE_SYSTEM;
    safe_strcpy(msg.nickname, "Server", sizeof(msg.nickname));
    safe_strcpy(msg.message, text, sizeof(msg.message));
    msg.timestamp = time(NULL);
    broadcast_message(server, &msg, exclude_index);
}

int find_client_by_nickname(ServerState* server, const char* nickname) {
    int idx = -1;
    pthread_mutex_lock(&server->clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (server->clients[i].active && strcmp(server->clients[i].nickname, nickname) == 0) {
            idx = i;
            break;
        }
    }
    pthread_mutex_unlock(&server->clients_mutex);
    return idx;
}

int is_nickname_available(ServerState* server, const char* nickname) {
    int available = 1;
    pthread_mutex_lock(&server->clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (server->clients[i].active && strcmp(server->clients[i].nickname, nickname) == 0) {
            available = 0;
            break;
        }
    }
    pthread_mutex_unlock(&server->clients_mutex);
    return available;
}

int set_client_nickname(ServerState* server, int client_index, const char* new_nick, char* old_out, size_t old_cap) {
    if (client_index < 0 || client_index >= MAX_CLIENTS) return -1;
    char reason[64] = {0};
    if (!validate_nickname(new_nick, reason, sizeof(reason))) {
        return -2;
    }
    pthread_mutex_lock(&server->clients_mutex);
    if (!server->clients[client_index].active) {
        pthread_mutex_unlock(&server->clients_mutex);
        return -1;
    }
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (i == client_index) continue;
        if (server->clients[i].active && strcmp(server->clients[i].nickname, new_nick) == 0) {
            pthread_mutex_unlock(&server->clients_mutex);
            return -3;
        }
    }
    if (old_out && old_cap) safe_strcpy(old_out, server->clients[client_index].nickname, old_cap);
    safe_strcpy(server->clients[client_index].nickname, new_nick, sizeof(server->clients[client_index].nickname));
    pthread_mutex_unlock(&server->clients_mutex);
    return 0;
}

int add_client(ServerState* server, SOCKET client_socket, struct sockaddr_in client_addr) {
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
            safe_strcpy(server->clients[i].nickname, "Anonymous", sizeof(server->clients[i].nickname));
            server->client_count++;
            pthread_mutex_unlock(&server->clients_mutex);
            return i;
        }
    }
    pthread_mutex_unlock(&server->clients_mutex);
    return -1;
}

void remove_client(ServerState* server, int client_index) {
    pthread_mutex_lock(&server->clients_mutex);
    if (client_index >= 0 && client_index < MAX_CLIENTS && server->clients[client_index].active) {
        CLOSE_SOCKET(server->clients[client_index].socket);
        server->clients[client_index].active = 0;
        server->client_count--;
        print_system_message("Client disconnected");
    }
    pthread_mutex_unlock(&server->clients_mutex);
}

void broadcast_message(ServerState* server, const MessageInfo* msg, int exclude_index) {
    pthread_mutex_lock(&server->clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (server->clients[i].active && i != exclude_index) {
            if (send_message(server->clients[i].socket, msg) == SOCKET_ERROR) {
                print_error("Failed to send message to client");
                SOCKET bad = server->clients[i].socket;
                pthread_mutex_unlock(&server->clients_mutex);
                for (int j = 0; j < MAX_CLIENTS; ++j) {
                    if (server->clients[j].active && server->clients[j].socket == bad) {
                        remove_client(server, j);
                        break;
                    }
                }
                pthread_mutex_lock(&server->clients_mutex);
            }
        }
    }
    pthread_mutex_unlock(&server->clients_mutex);
}

int server_send_private_message(ServerState* server, const MessageInfo* msg) {
    int target_index = find_client_by_nickname(server, msg->target_nickname);
    if (target_index == -1) return -1;
    pthread_mutex_lock(&server->clients_mutex);
    int result = -1;
    if (server->clients[target_index].active) {
        result = (send_message(server->clients[target_index].socket, msg) == SOCKET_ERROR) ? -1 : 0;
    }
    pthread_mutex_unlock(&server->clients_mutex);
    return result;
}

typedef struct {
    ServerState* server;
    int client_index;
} client_thread_data_t;

static int handle_join_message(ServerState* server, int client_index, const MessageInfo* msg) {
    char reason[64] = {0};
    if (!validate_nickname(msg->nickname, reason, sizeof(reason))) {
        MessageInfo error_msg = {0};
        error_msg.type = MSG_TYPE_NICKNAME_TAKEN;
        safe_strcpy(error_msg.nickname, "Server", sizeof(error_msg.nickname));
        snprintf(error_msg.message, sizeof(error_msg.message), "Invalid nickname: %s. Allowed: letters, digits, . _ - and < %d chars.", reason, MAX_NICK_LEN);
        error_msg.timestamp = time(NULL);
        (void)send_message(server->clients[client_index].socket, &error_msg);
        return 0;
    }
    if (!is_nickname_available(server, msg->nickname)) {
        MessageInfo taken_msg = {0};
        taken_msg.type = MSG_TYPE_NICKNAME_TAKEN;
        safe_strcpy(taken_msg.nickname, "Server", sizeof(taken_msg.nickname));
        snprintf(taken_msg.message, sizeof(taken_msg.message), "Nickname '%s' is already taken. Please choose another.", msg->nickname);
        taken_msg.timestamp = time(NULL);
        (void)send_message(server->clients[client_index].socket, &taken_msg);
        return 0;
    }
    char old_nick[MAX_NICK_LEN];
    pthread_mutex_lock(&server->clients_mutex);
    safe_strcpy(old_nick, server->clients[client_index].nickname, sizeof(old_nick));
    safe_strcpy(server->clients[client_index].nickname, msg->nickname, sizeof(server->clients[client_index].nickname));
    pthread_mutex_unlock(&server->clients_mutex);
    print_system_message("User joined the chat");
    printf("Nickname: " CYAN "%s" RESET " (ID: " YELLOW "%d" RESET ")\n", msg->nickname, server->clients[client_index].client_id);
    MessageInfo success_msg = (MessageInfo){0};
    success_msg.type = MSG_TYPE_NICKNAME_AVAILABLE;
    safe_strcpy(success_msg.nickname, "Server", sizeof(success_msg.nickname));
    snprintf(success_msg.message, sizeof(success_msg.message), "Nickname '%s' is registered!", msg->nickname);
    success_msg.timestamp = time(NULL);
    (void)send_message(server->clients[client_index].socket, &success_msg);
    MessageInfo join_msg = *msg;
    join_msg.type = MSG_TYPE_SYSTEM;
    snprintf(join_msg.message, sizeof(join_msg.message), "%s joined the chat", msg->nickname);
    broadcast_message(server, &join_msg, client_index);
    return 0;
}

static int handle_private_message(ServerState* server, int client_index, const MessageInfo* msg) {
    printf(MAGENTA "[PRIVATE]" RESET " From " CYAN "%s" RESET " to " CYAN "%s" RESET ": %s\n", msg->nickname, msg->target_nickname, msg->message);
    if (server_send_private_message(server, msg) != 0) {
        char buf[128];
        snprintf(buf, sizeof(buf), "User '%s' not found or offline.", msg->target_nickname);
        system_msg_to_socket(server->clients[client_index].socket, buf);
    }
    return 0;
}

static int handle_leave_message(ServerState* server, int client_index, const MessageInfo* msg) {
    print_system_message("User left the chat");
    printf("Nickname: " CYAN "%s" RESET " (ID: " YELLOW "%d" RESET ")\n", msg->nickname, server->clients[client_index].client_id);
    MessageInfo leave_msg = *msg;
    leave_msg.type = MSG_TYPE_SYSTEM;
    snprintf(leave_msg.message, sizeof(leave_msg.message), "%s left the chat", msg->nickname);
    broadcast_message(server, &leave_msg, client_index);
    return 1;
}

static int handle_rename_message(ServerState* server, int client_index, const MessageInfo* msg) {
    const char* desired = msg->target_nickname;
    char old_nick[MAX_NICK_LEN] = {0};
    int rc = set_client_nickname(server, client_index, desired, old_nick, sizeof(old_nick));
    if (rc == 0) {
        char ok[128];
        snprintf(ok, sizeof(ok), "Nickname changed to '%s'.", desired);
        system_msg_to_socket(server->clients[client_index].socket, ok);
        char line[2*MAX_NICK_LEN + 32];
        snprintf(line, sizeof(line), "[Nickname changed from %s to %s]", old_nick, desired);
        system_msg_broadcast(server, line, client_index);
    } else {
        char why[160];
        if (rc == -2) snprintf(why, sizeof(why), "Invalid nickname.");
        else if (rc == -3) snprintf(why, sizeof(why), "Nickname '%s' is already taken.", desired);
        else snprintf(why, sizeof(why), "Unable to change nickname.");
        MessageInfo err = {0};
        err.type = MSG_TYPE_NICKNAME_TAKEN;
        safe_strcpy(err.nickname, "Server", sizeof(err.nickname));
        safe_strcpy(err.message, why, sizeof(err.message));
        err.timestamp = time(NULL);
        (void)send_message(server->clients[client_index].socket, &err);
    }
    return 0;
}

static int process_message(ServerState* server, int client_index, const MessageInfo* msg) {
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
        case MSG_TYPE_RENAME:
            return handle_rename_message(server, client_index, msg);
        default:
            print_error("Unknown message type received");
            return 0;
    }
}

void* handle_client(void* arg) {
    client_thread_data_t* data = arg;
    ServerState* server = data->server;
    int client_index = data->client_index;
    free(data);
    MessageInfo msg;
    char client_ip[INET_ADDRSTRLEN] = {0};
    inet_ntop(AF_INET, &server->clients[client_index].address.sin_addr, client_ip, INET_ADDRSTRLEN);
    print_system_message("New client connected");
    printf("Client IP: " CYAN "%s" RESET ", Port: " CYAN "%d" RESET ", ID: " YELLOW "%d" RESET "\n", client_ip, ntohs(server->clients[client_index].address.sin_port), server->clients[client_index].client_id);
    system_msg_to_socket(server->clients[client_index].socket, "Welcome to the chat room!");
    int should_disconnect = 0;
    while (!should_disconnect) {
        int bytes_received = receive_message(server->clients[client_index].socket, &msg);
        if (bytes_received <= 0) {
            print_error("Client disconnected or error occurred");
            break;
        }
        int result = process_message(server, client_index, &msg);
        if (result == 1) should_disconnect = 1;
    }
    remove_client(server, client_index);
    pthread_exit(NULL);
}

void* server_input_thread(void* arg) {
    ServerState* server = arg;
    char buffer[MAX_MSG_LEN];
    char target[MAX_NICK_LEN];
    char message[MAX_MSG_LEN];

    while (fgets(buffer, sizeof(buffer), stdin)) {
        buffer[strcspn(buffer, "\n")] = '\0';

        if (strcmp(buffer, "/quit") == 0) {
            print_system_message("Shutting down server...");
            server_cleanup(server);
            exit(0);
        }
        if (strcmp(buffer, "/help") == 0) {
            print_server_help();
        }
        if (parse_private_message(buffer, target, message) == 0) {
            MessageInfo msg = (MessageInfo){0};
            msg.type = MSG_TYPE_PRIVATE;
            safe_strcpy(msg.nickname, "Server", sizeof(msg.nickname));
            safe_strcpy(msg.target_nickname, target, sizeof(msg.target_nickname));
            safe_strcpy(msg.message, message, sizeof(msg.message));
            msg.timestamp = time(NULL);

            if (server_send_private_message(server, &msg) != 0) {
                char why[160];
                snprintf(why, sizeof(why), "User '%s' not found or offline.", target);
                pthread_mutex_lock(&server->clients_mutex);
                for (int i = 0; i < MAX_CLIENTS; i++) {
                    if (server->clients[i].active) {
                        system_msg_to_socket(server->clients[i].socket, why);
                    }
                }
                pthread_mutex_unlock(&server->clients_mutex);
            }
            continue;
        }

        MessageInfo msg = (MessageInfo){0};
        msg.type = MSG_TYPE_SYSTEM;
        safe_strcpy(msg.nickname, "Server", sizeof(msg.nickname));
        safe_strcpy(msg.message, buffer, sizeof(msg.message));
        msg.timestamp = time(NULL);
        broadcast_message(server, &msg, -1);
    }

    return NULL;
}


int main(int argc, char* argv[]) {
    int port = DEFAULT_PORT;
    ServerState server;
    if (argc > 1) port = atoi(argv[1]);
    print_system_message("Starting chat server...");
    printf("Port: " BOLD_CYAN "%d" RESET "\n", port);
    if (server_init(&server, port) != 0) {
        return 1;
    }
    print_success("Server started successfully");
    print_system_message("Waiting for client connections...");
    pthread_t input_tid;
    if (pthread_create(&input_tid, NULL, server_input_thread, &server) == 0) {
        pthread_detach(input_tid);
    } else {
        print_error("Failed to create server input thread");
    }
    while (1) {
        struct sockaddr_in client_addr;
        SOCKET client_socket = accept_connection(server.server_socket, &client_addr);
        if (client_socket == INVALID_SOCKET) continue;
        int client_index = add_client(&server, client_socket, client_addr);
        if (client_index == -1) {
            print_error("Maximum number of clients reached");
            CLOSE_SOCKET(client_socket);
            continue;
        }
        client_thread_data_t* thread_data = malloc(sizeof(client_thread_data_t));
        if (!thread_data) {
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
