#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <unordered_map>
#include <chrono>
#include <string_view>

using namespace std;

unordered_map<string, int> logCounts;
mutex mtx;

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

int main() {
    auto startTime = chrono::high_resolution_clock::now();

    ifstream file("large_server.log", ios::binary | ios::ate);
    if (!file.is_open()) {
        cerr << "Error opening large_server.log" << endl;
        return 1;
    }

    streamsize size = file.tellg();
    file.seekg(0, ios::beg);

    // Read the entire file into memory buffer at once
    vector<char> buffer(size);
    if (!file.read(buffer.data(), size)) {
        cerr << "Error reading file into memory" << endl;
        return 1;
    }
    file.close();

    unsigned int numThreads = 4;
    vector<thread> threads;
    size_t chunkSize = size / numThreads;

    for (unsigned int i = 0; i < numThreads; ++i) {
        size_t start = i * chunkSize;
        size_t end = (i == numThreads - 1) ? size : start + chunkSize;

        // Align chunk boundaries to actual line breaks
        if (start != 0) {
            while (start < size && buffer[start - 1] != '\n') start++;
        }
        if (end != size) {
            while (end < size && buffer[end - 1] != '\n') end++;
        }

        threads.emplace_back(scanMemoryChunk, buffer.data(), start, end);
    }

    for (auto& t : threads) t.join();

    auto endTime = chrono::high_resolution_clock::now();
    chrono::duration<double, milli> duration = endTime - startTime;

    cout << "--- High-Performance Memory Buffer Scan (4 Threads) ---" << endl;
    cout << "INFO logs:    " << logCounts["INFO"] << endl;
    cout << "WARNING logs: " << logCounts["WARNING"] << endl;
    cout << "ERROR logs:   " << logCounts["ERROR"] << endl;
    cout << "Execution Time: " << duration.count() << " ms" << endl;

    return 0;
}