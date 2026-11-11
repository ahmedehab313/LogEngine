# High-Performance C++ Log Analytics Engine

A lightweight, multi-threaded log parsing engine and real-time analytics server built in C++17. Designed to demonstrate low-level memory management, concurrent processing, and raw socket networking.

## Key Features & Achievements
* **Concurrent Memory Scanning:** Processes **500,000 log records in ~12–13 ms** across 4 worker threads using non-owning `std::string_view` slices.
* **Low-Level HTTP Server:** Built an embedded socket server from scratch using native Windows Sockets (`Winsock2`) to serve real-time JSON metrics without external web frameworks.
* **Interactive Web Dashboard:** Clean dark-mode dashboard interface consuming the C++ REST API endpoint and rendering dynamic metrics via Chart.js.

## Tech Stack
* **Language:** C++17
* **Networking:** Winsock2 (Windows Sockets API)
* **Concurrency:** `std::thread`, `std::mutex`, `std::lock_guard`
* **Frontend:** HTML5, Modern JavaScript (Async/Await), Chart.js

## Project Architecture
1. **Data Ingestion:** `generate_logs.cpp` creates realistic high-volume log streams.
2. **Buffer Parsing:** `main.cpp` maps file memory into contiguous chunks and splits labor safely across worker threads.
3. **API & UI:** Embedded server routes `GET /api/metrics` to return raw JSON data and serves `index.html` for root browser requests.

## How to Build & Run locally

1. **Clone the repository:**
   ```bash
   git clone [https://github.com/ahmedehab313/LogEngine.git](https://github.com/ahmedehab313/LogEngine.git)
   cd LogEngine
