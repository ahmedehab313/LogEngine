#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <unordered_map>
#include <chrono>
#include <string_view>
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "ws2_32.lib")

using namespace std;

unordered_map<string, int> logCounts;
mutex mtx;

// 1. Core Multi-Threaded Engine
void scanMemoryChunk(const char* data, size_t start, size_t end) {
    int info = 0, warning = 0, error = 0;
    string_view buffer(data + start, end - start);

    size_t pos = 0;
    while (pos < buffer.size()) {
        size_t nextPos = buffer.find('\n', pos);
        if (nextPos == string_view::npos) nextPos = buffer.size();

        string_view line = buffer.substr(pos, nextPos - pos);

        if (line.find("[INFO]") != string_view::npos) info++;
        else if (line.find("[WARNING]") != string_view::npos) warning++;
        else if (line.find("[ERROR]") != string_view::npos) error++;

        pos = nextPos + 1;
    }

    lock_guard<mutex> lock(mtx);
    logCounts["INFO"] += info;
    logCounts["WARNING"] += warning;
    logCounts["ERROR"] += error;
}

void parseLogs() {
    logCounts.clear();
    ifstream file("large_server.log", ios::binary | ios::ate);
    if (!file.is_open()) return;

    streamsize size = file.tellg();
    file.seekg(0, ios::beg);

    vector<char> buffer(size);
    if (!file.read(buffer.data(), size)) return;
    file.close();

    unsigned int numThreads = 4;
    vector<thread> threads;
    size_t chunkSize = size / numThreads;

    for (unsigned int i = 0; i < numThreads; ++i) {
        size_t start = i * chunkSize;
        size_t end = (i == numThreads - 1) ? size : start + chunkSize;

        if (start != 0) {
            while (start < size && buffer[start - 1] != '\n') start++;
        }
        if (end != size) {
            while (end < size && buffer[end - 1] != '\n') end++;
        }

        threads.emplace_back(scanMemoryChunk, buffer.data(), start, end);
    }

    for (auto& t : threads) t.join();
}

// 2. HTTP Server Listener (Winsock)
void startHTTPServer() {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        cerr << "WSAStartup failed." << endl;
        return;
    }

    SOCKET listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenSocket == INVALID_SOCKET) {
        cerr << "Socket creation failed." << endl;
        WSACleanup();
        return;
    }

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(8080);

    if (bind(listenSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        cerr << "Bind failed." << endl;
        closesocket(listenSocket);
        WSACleanup();
        return;
    }

    if (listen(listenSocket, SOMAXCONN) == SOCKET_ERROR) {
        cerr << "Listen failed." << endl;
        closesocket(listenSocket);
        WSACleanup();
        return;
    }

    cout << "\n=============================================" << endl;
    cout << " C++ Log Engine Server Running!" << endl;
    cout << " Open your browser to: http://localhost:8080" << endl;
    cout << " Press Ctrl + C in terminal to stop." << endl;
    cout << "=============================================\n" << endl;

    while (true) {
        SOCKET clientSocket = accept(listenSocket, NULL, NULL);
        if (clientSocket == INVALID_SOCKET) continue;

        // Read the incoming HTTP request header from the browser
        char recvBuffer[1024];
        int bytesReceived = recv(clientSocket, recvBuffer, sizeof(recvBuffer) - 1, 0);

        if (bytesReceived > 0) {
            recvBuffer[bytesReceived] = '\0';

            // Construct JSON response containing real log metrics
            string jsonBody = "{\n"
                              "  \"info\": " + to_string(logCounts["INFO"]) + ",\n"
                              "  \"warning\": " + to_string(logCounts["WARNING"]) + ",\n"
                              "  \"error\": " + to_string(logCounts["ERROR"]) + "\n"
                              "}";

            string httpResponse = "HTTP/1.1 200 OK\r\n"
                                  "Content-Type: application/json\r\n"
                                  "Access-Control-Allow-Origin: *\r\n"
                                  "Content-Length: " + to_string(jsonBody.length()) + "\r\n"
                                  "Connection: close\r\n\r\n" + jsonBody;

            send(clientSocket, httpResponse.c_str(), (int)httpResponse.length(), 0);
            
            // Gracefully shutdown send channel before closing
            shutdown(clientSocket, SD_SEND);
        }

        closesocket(clientSocket);
    }

    closesocket(listenSocket);
    WSACleanup();
}

int main() {
    auto startTime = chrono::high_resolution_clock::now();
    
    // Parse 500k logs
    parseLogs();

    auto endTime = chrono::high_resolution_clock::now();
    chrono::duration<double, milli> duration = endTime - startTime;

    cout << "Engine parsed 500,000 logs in " << duration.count() << " ms." << endl;

    // Start listening on HTTP port 8080
    startHTTPServer();

    return 0;
}