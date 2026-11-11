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

// Read index.html content into string
string loadHTMLFile() {
    ifstream file("index.html");
    if (!file.is_open()) return "<h1>Error loading index.html</h1>";
    string content((istreambuf_iterator<char>(file)), istreambuf_iterator<char>());
    return content;
}

// 2. Embedded HTTP Server
void startHTTPServer() {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) return;

    SOCKET listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenSocket == INVALID_SOCKET) { WSACleanup(); return; }

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(8080);

    if (bind(listenSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        closesocket(listenSocket); WSACleanup(); return;
    }

    if (listen(listenSocket, SOMAXCONN) == SOCKET_ERROR) {
        closesocket(listenSocket); WSACleanup(); return;
    }

    cout << "\n=============================================" << endl;
    cout << " C++ Dashboard Server Active!" << endl;
    cout << " Open your browser: http://localhost:8080" << endl;
    cout << "=============================================\n" << endl;

    string htmlContent = loadHTMLFile();

    while (true) {
        SOCKET clientSocket = accept(listenSocket, NULL, NULL);
        if (clientSocket == INVALID_SOCKET) continue;

        char recvBuffer[2048];
        int bytesReceived = recv(clientSocket, recvBuffer, sizeof(recvBuffer) - 1, 0);

        if (bytesReceived > 0) {
            recvBuffer[bytesReceived] = '\0';
            string requestStr(recvBuffer);

            string httpResponse;

            // Route: Return JSON metrics if API requested
            if (requestStr.find("GET /api/metrics") != string::npos) {
                string jsonBody = "{\n"
                                  "  \"info\": " + to_string(logCounts["INFO"]) + ",\n"
                                  "  \"warning\": " + to_string(logCounts["WARNING"]) + ",\n"
                                  "  \"error\": " + to_string(logCounts["ERROR"]) + "\n"
                                  "}";

                httpResponse = "HTTP/1.1 200 OK\r\n"
                               "Content-Type: application/json\r\n"
                               "Access-Control-Allow-Origin: *\r\n"
                               "Content-Length: " + to_string(jsonBody.length()) + "\r\n"
                               "Connection: close\r\n\r\n" + jsonBody;
            } 
            // Route: Return visual HTML dashboard for root page
            else {
                htmlContent = loadHTMLFile(); // Refresh HTML on each reload
                httpResponse = "HTTP/1.1 200 OK\r\n"
                               "Content-Type: text/html\r\n"
                               "Content-Length: " + to_string(htmlContent.length()) + "\r\n"
                               "Connection: close\r\n\r\n" + htmlContent;
            }

            send(clientSocket, httpResponse.c_str(), (int)httpResponse.length(), 0);
            shutdown(clientSocket, SD_SEND);
        }

        closesocket(clientSocket);
    }

    closesocket(listenSocket);
    WSACleanup();
}

int main() {
    auto startTime = chrono::high_resolution_clock::now();
    parseLogs();
    auto endTime = chrono::high_resolution_clock::now();
    chrono::duration<double, milli> duration = endTime - startTime;

    cout << "Parsed 500,000 logs in " << duration.count() << " ms." << endl;

    startHTTPServer();
    return 0;
}