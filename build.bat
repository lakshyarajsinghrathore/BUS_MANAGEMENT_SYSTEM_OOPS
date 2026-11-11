@echo off
echo Building College Bus Management System...

echo [1/2] Compiling sqlite3.c (C compiler)...
gcc -c sqlite3.c -o sqlite3.o
if %errorlevel% neq 0 (
    echo [FAIL] sqlite3.c compilation failed.
    exit /b 1
)

echo [2/2] Compiling C++ sources and linking...
g++ -std=c++17 -O2 ^
  -D_WIN32_WINNT=0x0A00 ^
  backend/main.cpp ^
  backend/db/Database.cpp ^
  backend/models/User.cpp ^
  backend/models/Route.cpp ^
  backend/utils/SessionManager.cpp ^
  backend/controllers/AuthController.cpp ^
  backend/controllers/RouteController.cpp ^
  backend/controllers/LocationController.cpp ^
  sqlite3.o ^
  -I backend/utils ^
  -lpthread ^
  -lws2_32 ^
  -o bus_server.exe

if %errorlevel% == 0 (
    echo [OK] Build successful! Run .\bus_server.exe to start.
) else (
    echo [FAIL] Build failed. Check errors above.
)
