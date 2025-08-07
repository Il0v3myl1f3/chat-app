#ifndef COMMON_H
#define COMMON_H

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #ifdef _MSC_VER
        #pragma comment(lib, "ws2_32.lib")
    #endif
    #define CLOSE_SOCKET closesocket
    #define SOCKET_ERROR_CODE WSAGetLastError()
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <netdb.h>
    #define SOCKET int
    #define INVALID_SOCKET -1
    #define SOCKET_ERROR -1
    #define CLOSE_SOCKET close
    #define SOCKET_ERROR_CODE errno
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <time.h>

#define RESET   "\033[0m"
#define GRAY    "\033[90m"
#define CYAN    "\033[36m"
#define BOLD_CYAN "\033[96m"
#define GREEN   "\033[32m"
#define RED     "\033[31m"
#define BLUE    "\033[34m"
#define MAGENTA "\033[90m"
#define YELLOW  "\033[33m"

#define FAILED_INIT_MESSAGE "Failed to initialize network"
#define FAILED_INIT_ERR_NUMBER 1
#define ERROR_NO_MSG "Error: No message received"

#define MAX_CLIENTS 10
#define MAX_MESSAGE_LEN 1024
#define MAX_NICKNAME_LEN 32
#define DEFAULT_PORT 8888
#define SERVER_IP "127.0.0.1"

typedef enum {
    MSG_TYPE_CHAT = 1,
    MSG_TYPE_JOIN,
    MSG_TYPE_LEAVE,
    MSG_TYPE_SYSTEM,
    MSG_TYPE_PRIVATE,
    MSG_TYPE_NICKNAME_TAKEN,
    MSG_TYPE_NICKNAME_AVAILABLE,
    MSG_TYPE_RENAME
} msg_type_t;

typedef struct {
    int type;
    char nickname[MAX_NICKNAME_LEN];
    char target_nickname[MAX_NICKNAME_LEN];
    char message[MAX_MESSAGE_LEN];
    time_t timestamp;
    int client_id;
} MessageInfo;

typedef struct {
    char nickname[MAX_NICKNAME_LEN];
    int client_id;
    int active;
} NicknameStatus;

typedef struct {
    SOCKET socket;
    char nickname[MAX_NICKNAME_LEN];
    int active;
    int client_id;
    struct sockaddr_in address;
    pthread_t thread_id;
} Client;

typedef struct {
    Client clients[MAX_CLIENTS];
    NicknameStatus nicknames[MAX_CLIENTS];
    int client_count;
    int next_client_id;
    pthread_mutex_t clients_mutex;
    pthread_mutex_t nicknames_mutex;
    SOCKET server_socket;
} ServerState;

typedef struct {
    SOCKET socket;
    char nickname[MAX_NICKNAME_LEN];
    int connected;
    int client_id;
    pthread_t receive_thread;
} ClientState;

int initialize_network(void);
void cleanup_network(void);
int create_socket(void);
int bind_socket(SOCKET sock, int port);
int listen_socket(SOCKET sock, int backlog);
SOCKET accept_connection(SOCKET sock, struct sockaddr_in* client_addr);
int connect_to_server(SOCKET sock, const char* ip, int port);
int send_message(SOCKET sock, const MessageInfo* msg);
int receive_message(SOCKET sock, MessageInfo* msg);

int server_init(ServerState* server, int port);
void server_cleanup(ServerState* server);
int add_client(ServerState* server, SOCKET client_socket, struct sockaddr_in client_addr);
void remove_client(ServerState* server, int client_index);
void broadcast_message(ServerState* server, const MessageInfo* msg, int exclude_index);
int server_send_private_message(ServerState* server, const MessageInfo* msg);
int is_nickname_available(ServerState* server, const char* nickname);
int register_nickname(ServerState* server, const char* nickname, int client_id);
void unregister_nickname(ServerState* server, int client_id);
int find_client_by_nickname(ServerState* server, const char* nickname);
void* handle_client(void* arg);

int client_init(ClientState* client);
void client_cleanup(ClientState* client);
int client_connect(ClientState* client, const char* ip, int port);
void get_nickname(ClientState* client);
void send_join_message(ClientState* client);
void send_chat_message(ClientState* client, const char* message);
void client_send_private_message(ClientState* client, const char* target, const char* message);
void send_leave_message(ClientState* client);
void* receive_messages(void* arg);
void change_nickname(ClientState* client);
int parse_private_message(const char* input, char* target, char* message);


void print_timestamp();
void print_message(const char* nickname, const char* message);
void print_system_message(const char* message);
void print_error(const char* message);
void print_success(const char* message);
void print_welcome_message();
void print_help(void);
void clear_screen();

#endif // COMMON_H
