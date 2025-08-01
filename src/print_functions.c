#include "../include/common.h"

void clear_screen() {
    printf("\033[?1049h");
    printf("\033[2J\033[H");
}
void print_timestamp()
{
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char time_str[26];
    strftime(time_str, 26,  "%Y/%m/%d %H:%M:%S" , tm_info);
    printf( MAGENTA "[%s] " RESET, time_str);
}

void print_message(const char* nickname, const char* message) {
    print_timestamp();
    printf(CYAN "%s" RESET ": %s\n", nickname, message);
}

void print_system_message(const char* message) {
    print_timestamp();
    printf(CYAN "[SYSTEM] %s \n " RESET, message);
}

void print_error(const char* message) {
    print_timestamp();
    printf(RED "[ERROR] %s\n" RESET, message);
}

void print_success(const char* message) {
    print_timestamp();
    printf(GREEN "[SUCCESS] %s\n" RESET, message);
}

void print_welcome_message() {
    printf("Type " BOLD_CYAN "/help" RESET " for extra commands\n\n");
}

void print_help(void) {
    printf(
        "\n" CYAN "=== Chat Client Commands ===" RESET "\n"
        BOLD_CYAN "/help" RESET "        - Show this help message\n"
        BOLD_CYAN "/quit" RESET "        - Leave the chat and disconnect\n"
        BOLD_CYAN "/nick" RESET "        - Change your nickname\n"
        BOLD_CYAN "/shh <nick> <msg>" RESET " - Send private message\n"
        BOLD_CYAN "<message>" RESET "      - Send a chat message\n"
        CYAN "============================" RESET "\n\n"
    );
}

