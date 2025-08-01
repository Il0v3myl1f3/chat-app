@echo off
echo Building Chat Application...

REM Check if GCC is available
gcc --version >nul 2>&1
if errorlevel 1 (
    echo Error: GCC compiler not found!
    echo Please install MinGW or MSYS2 and add gcc to your PATH.
    echo Download from: https://www.mingw-w64.org/ or https://www.msys2.org/
    pause
    exit /b 1
)

REM Compile server
echo Compiling server...
gcc -Wall -Wextra -std=c99 -pthread -o server.exe src/server/server.c src/print_functions.c -Iinclude -lws2_32
if errorlevel 1 (
    echo Error: Failed to compile server!
    pause
    exit /b 1
)

REM Compile client
echo Compiling client...
gcc -Wall -Wextra -std=c99 -pthread -o client.exe src/client/client.c src/print_functions.c -Iinclude -lws2_32
if errorlevel 1 (
    echo Error: Failed to compile client!
    pause
    exit /b 1
)

echo.
echo Build completed successfully!
echo.
echo To run the application:
echo 1. Start server: server.exe [port]
echo 2. Start client: client.exe [server_ip] [port]
echo.
echo Example:
echo   server.exe 8888
echo   client.exe 127.0.0.1 8888
echo.
pause