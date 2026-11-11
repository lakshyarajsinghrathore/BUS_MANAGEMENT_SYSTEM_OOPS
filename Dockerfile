FROM debian:bookworm-slim

RUN apt-get update && apt-get install -y --no-install-recommends \
    gcc \
    g++ \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY sqlite3.c sqlite3.h ./
COPY backend ./backend

RUN gcc -O2 -c sqlite3.c -o sqlite3.o && \
    g++ -std=c++17 -O2 \
      backend/main.cpp \
      backend/db/Database.cpp \
      backend/models/User.cpp \
      backend/models/Route.cpp \
      backend/utils/SessionManager.cpp \
      backend/controllers/AuthController.cpp \
      backend/controllers/RouteController.cpp \
      backend/controllers/LocationController.cpp \
      sqlite3.o \
      -I backend/utils \
      -lpthread \
      -ldl \
      -o bus_server

EXPOSE 8080

CMD ["./bus_server"]
