@echo off
cd /d "%~dp0"

echo [1/4] Stopping any existing servers...
taskkill /F /IM bus_server.exe >nul 2>&1
for /f "tokens=5" %%a in ('netstat -ano ^| findstr ":3000 "') do taskkill /F /PID %%a >nul 2>&1

echo [2/4] Starting backend (port 8080)...
start "" /B bus_server.exe

echo [3/4] Starting frontend (port 3000)...
start "" /B python -m http.server 3000 --directory frontend

echo [4/4] Waiting for servers to be ready...
timeout /t 3 /nobreak >nul

echo Opening browser tabs...
start "" "http://localhost:3000/index.html"
start "" "http://localhost:3000/passenger/dashboard.html"
start "" "http://localhost:3000/driver/dashboard.html"

echo.
echo ================================
echo  UniRoute is running!
echo  Backend  : http://localhost:8080
echo  Landing  : http://localhost:3000/index.html
echo  Passenger: http://localhost:3000/passenger/dashboard.html
echo  Driver   : http://localhost:3000/driver/dashboard.html
echo ================================
echo  Press Ctrl+C or close this window to stop.
echo ================================
pause
