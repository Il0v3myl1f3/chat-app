# Chat Application

A cross-platform in-terminal chat application for local network.

## Features

- **Cross-platform support**: Works on Linux, Windows, macOS and Android
- **Real-time messaging**: Instant message delivery
- **Multiple clients**: Server supports up to 10 concurrent clients

## Building

### Linux/macOS

```bash
# Build all components
make

# Build only server
make server

# Build only client 
make client
```

### Windows

```bash
# Build using Windows Makefile
make -f Makefile.windows

# Build using the .bat file
build.bat

# Or build manually
gcc -Wall -Wextra -std=c99 -pthread -o server.exe src/server/server.c src/print_functions.c -Iinclude -lws2_32
gcc -Wall -Wextra -std=c99 -pthread -o client.exe src/client/client.c src/print_functions.c -Iinclude -lws2_32
```

## Usage

### Starting the Server

```bash
# Linux/macOS
./server

# Windows
server.exe
```

The server will start on port 8080 and wait for client connections.

### Starting the Client

```bash
# Linux/macOS
./client

# Windows
client.exe
```

## Commands

-  `/quit`: Exit the client
- `Ctrl+C`: Force quit

## Platform Compatibility

### Linux/macOS/android
- ✅ Server
- ✅ Terminal client

### Windows
- ✅ Server
- ✅ Terminal client

### Headless Environments (WSL, SSH, ...)
- ✅ Server
- ✅ Terminal client

## Troubleshooting

### "Address already in use" error
The server port is already in use. Kill the existing server process:
```bash
pkill server
# or
killall server
```

## Dependencies

### Linux/macOS
- GCC compiler
- pthread library

### Windows
- GCC compiler (MinGW or similar)
- Windows Sockets 2 (ws2_32.lib)

## License

This project is open source and available under the MIT License. 
