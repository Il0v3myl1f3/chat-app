#include "../../include/common.h"


static void safe_strcpy(char* dst, const char* src, size_t cap) {
    if (!dst || !cap) return;
    if (!src) { dst[0] = '\0'; return; }
    strncpy(dst, src, cap - 1);
    dst[cap - 1] = '\0';
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
    return sock;
}

int connect_to_server(SOCKET sock, const char* ip, int port) {
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);

    if (inet_pton(AF_INET, ip, &server_addr.sin_addr) <= 0) {
        print_error("Invalid address");
        return -1;
    }

    if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        print_error("Failed to connect to server");
        return -1;
    }

    return 0;
}

int send_message(SOCKET sock, const MessageInfo* msg) {
    return send(sock, (const char*)msg, sizeof(MessageInfo), 0);
}

int receive_message(SOCKET sock, MessageInfo* msg) {
    return recv(sock, (char*)msg, sizeof(MessageInfo), 0);
}


int client_init(ClientState* client) {
    memset(client, 0, sizeof(ClientState));
    client->connected = 0;
    client->client_id = -1;

    if (initialize_network() != 0) {
        print_error("Failed to initialize network");
        return -1;
    }

    client->socket = create_socket();
    if (client->socket == INVALID_SOCKET) {
        cleanup_network();
        return -1;
    }

    return 0;
}

void client_cleanup(ClientState* client) {
    if (client->connected) {
        client->connected = 0;
        pthread_join(client->receive_thread, NULL);
    }

    CLOSE_SOCKET(client->socket);
    cleanup_network();
}

int client_connect(ClientState* client, const char* ip, int port) {
    if (connect_to_server(client->socket, ip, port) != 0) {
        return -1;
    }
    client->connected = 1;
    return 0;
}


void get_nickname(ClientState* client) {
    printf("Enter your nickname: ");
    if (!fgets(client->nickname, MAX_NICK_LEN, stdin)) {
        safe_strcpy(client->nickname, "Anonymous", sizeof(client->nickname));
    } else {
        client->nickname[strcspn(client->nickname, "\n")] = 0;
        if (strlen(client->nickname) == 0) {
            safe_strcpy(client->nickname, "Anonymous", sizeof(client->nickname));
        }
    }
}

void send_join_message(ClientState* client) {
    MessageInfo join_msg;
    memset(&join_msg, 0, sizeof(join_msg));
    join_msg.type = MSG_TYPE_JOIN;
    safe_strcpy(join_msg.nickname, client->nickname, sizeof(join_msg.nickname));
    join_msg.timestamp = time(NULL);
    join_msg.client_id = client->client_id;

    if (send_message(client->socket, &join_msg) == SOCKET_ERROR) {
        print_error("Failed to send join message");
    }
}

void send_chat_message(ClientState* client, const char* message) {
    if (strlen(client->nickname) == 0 || strcmp(client->nickname, "Anonymous") == 0) {
        print_error("Please set a valid nickname before sending messages");
        return;
    }

    MessageInfo chat_msg;
    memset(&chat_msg, 0, sizeof(chat_msg));
    chat_msg.type = MSG_TYPE_CHAT;
    safe_strcpy(chat_msg.nickname, client->nickname, sizeof(chat_msg.nickname));
    safe_strcpy(chat_msg.message, message, sizeof(chat_msg.message));
    chat_msg.timestamp = time(NULL);
    chat_msg.client_id = client->client_id;

    if (send_message(client->socket, &chat_msg) == SOCKET_ERROR) {
        print_error("Failed to send message");
    }
}

void client_send_private_message(ClientState* client, const char* target, const char* message) {
    if (strlen(client->nickname) == 0 || strcmp(client->nickname, "Anonymous") == 0) {
        print_error("Please set a valid nickname before sending private messages");
        return;
    }

    MessageInfo private_msg;
    memset(&private_msg, 0, sizeof(private_msg));
    private_msg.type = MSG_TYPE_PRIVATE;
    safe_strcpy(private_msg.nickname, client->nickname, sizeof(private_msg.nickname));
    safe_strcpy(private_msg.target_nickname, target, sizeof(private_msg.target_nickname));
    safe_strcpy(private_msg.message, message, sizeof(private_msg.message));
    private_msg.timestamp = time(NULL);
    private_msg.client_id = client->client_id;

    if (send_message(client->socket, &private_msg) == SOCKET_ERROR) {
        print_error("Failed to send private message");
        return;
    }

    printf(MAGENTA "[PRIVATE to %s]" RESET " %s\n", target, message);
}

void send_leave_message(ClientState* client) {
    MessageInfo leave_msg;
    memset(&leave_msg, 0, sizeof(leave_msg));
    leave_msg.type = MSG_TYPE_LEAVE;
    safe_strcpy(leave_msg.nickname, client->nickname, sizeof(leave_msg.nickname));
    leave_msg.timestamp = time(NULL);
    leave_msg.client_id = client->client_id;

    if (send_message(client->socket, &leave_msg) == SOCKET_ERROR) {
        print_error("Failed to send leave message");
    }
}


int parse_nick_command(const char* input, char* new_nick, size_t capacity) {

    const char* p = input;
    if (strncmp(p, "/nick", 5) != 0) return -1;
    p += 5;
    while (*p == ' ') ++p;
    if (*p == '\0') return -1;
    size_t n = strnlen(p, capacity);
    if (n == 0) return -1;

    char buf[128];
    size_t maxc = (capacity < sizeof(buf)) ? capacity : sizeof(buf);
    strncpy(buf, p, maxc - 1);
    buf[maxc - 1] = '\0';
    for (int i = (int)strlen(buf) - 1; i >= 0 && isspace((unsigned char)buf[i]); --i) {
        buf[i] = '\0';
    }
    if (buf[0] == '\0') return -1;
    safe_strcpy(new_nick, buf, capacity);
    return 0;
}

void request_nickname_change(ClientState* client, const char* new_nick) {
    MessageInfo request;
    memset(&request, 0, sizeof(request));
    request.type = MSG_TYPE_RENAME;
    safe_strcpy(request.nickname, client->nickname, sizeof(request.nickname));
    safe_strcpy(request.target_nickname, new_nick, sizeof(request.target_nickname));
    request.timestamp = time(NULL);
    request.client_id = client->client_id;

    if (send_message(client->socket, &request) == SOCKET_ERROR) {
        print_error("Failed to send rename request");
    }
}


void* receive_messages(void* arg) {
    ClientState* client = arg;
    MessageInfo msg;

    while (client->connected) {
        int bytes_received = receive_message(client->socket, &msg);
        if (bytes_received <= 0) {
            if (client->connected) {
                print_error("Connection lost");
                client->connected = 0;
            }
            break;
        }

        switch (msg.type) {
            case MSG_TYPE_CHAT:
                print_message(msg.nickname, msg.message);
                break;

            case MSG_TYPE_SYSTEM:
                print_system_message(msg.message);
                break;

            case MSG_TYPE_PRIVATE:
                printf(MAGENTA "[PRIVATE from %s]" RESET " %s\n", msg.nickname, msg.message);
                break;

            case MSG_TYPE_NICKNAME_TAKEN:
                print_error(msg.message);
                printf("Try a different nickname using " BOLD_CYAN "/nick <new_nick>" RESET "\n");
                break;

            case MSG_TYPE_NICKNAME_AVAILABLE:
                print_success(msg.message);
                break;

            default:
                print_error("Unknown message type received");
                break;
        }
    }

    pthread_exit(NULL);
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


int main(int argc, char* argv[]) {
    char server_ip[16] = SERVER_IP;
    int port = DEFAULT_PORT;
    ClientState client;

    if (argc > 1) { safe_strcpy(server_ip, argv[1], sizeof(server_ip)); }
    if (argc > 2) { port = atoi(argv[2]); }

    print_system_message("Starting chat client...");
    printf("Server: " BOLD_CYAN "%s:%d" RESET "\n", server_ip, port);

    if (client_init(&client) != 0) return 1;

    if (client_connect(&client, server_ip, port) != 0) {
        client_cleanup(&client);
        return 1;
    }

    get_nickname(&client);
    send_join_message(&client);

    if (pthread_create(&client.receive_thread, NULL, receive_messages, &client) != 0) {
        print_error("Failed to create receive thread");
        client_cleanup(&client);
        return 1;
    }

    print_success("Connected to server");
#ifdef _WIN32
    Sleep(1000);
#else
    usleep(200 * 1000);
#endif
    clear_screen();
    print_welcome_message();

    char input[MAX_MSG_LEN];
    while (client.connected) {
        printf("> ");
        if (!fgets(input, MAX_MSG_LEN, stdin)) break;
        input[strcspn(input, "\n")] = 0;
        if (input[0] == '\0') continue;

        if (input[0] == '/') {
            if (strcmp(input, "/help") == 0) {
                print_client_help();
            } else if (strcmp(input, "/quit") == 0) {
                print_system_message("Leaving chat...");
                send_leave_message(&client);
                client.connected = 0;
                break;
            } else if (strncmp(input, "/nick", 5) == 0) {
                char new_nick[MAX_NICK_LEN];
                if (parse_nick_command(input, new_nick, sizeof(new_nick)) == 0) {
                    request_nickname_change(&client, new_nick);
                    printf(GRAY "(requested nickname change to '%s')\n" RESET, new_nick);
                } else {
                    print_error("Usage: /nick <newname>");
                }
            } else if (strncmp(input, "/shh ", 5) == 0) {
                char target[MAX_NICK_LEN];
                char pm[MAX_MSG_LEN];
                if (parse_private_message(input, target, pm) == 0) {
                    client_send_private_message(&client, target, pm);
                } else {
                    print_error("Invalid private message format");
                    printf("Usage: " BOLD_CYAN "/shh <nickname> <message>" RESET "\n");
                    printf("Example: " GRAY "/shh john Hello there!" RESET "\n");
                }
            } else {
                print_error("Unknown command. Type " BOLD_CYAN "/help" RESET " for available commands.");
            }
        } else {
            send_chat_message(&client, input);
        }
    }

    client_cleanup(&client);
    print_system_message("Disconnected from server");
    return 0;
}
