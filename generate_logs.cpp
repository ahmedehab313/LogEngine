#include <iostream>
#include <fstream>
#include <string>
using namespace std;

int main() {
    ofstream outfile("large_server.log");
    if (!outfile.is_open()){
        cerr << "Failed to create large_server.log" << endl;
        return 1;
    }
    
    // Generate 500,000 lines of log data
    for (int i = 0; i < 500000; ++i) {
        if (i % 5 == 0) {
            outfile << "[ERROR] Memory usage spike in thread "<< i << "\n";
        } else if (i % 3 == 0) {
            outfile << "[WARNING] High latency observed: "<< i << "ms\n";
        } else {
            outfile << "[INFO] Handled incoming connection: "<< i << "\n";
        }
    }

outfile.close();
cout << " Succesfully generated large_server.log with 500,000 lines!"<< endl;


return 0;
}