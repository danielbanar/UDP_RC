#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <thread>
#include <chrono>

struct NetworkUsage {
    unsigned long rx_bytes;
    unsigned long tx_bytes;
};

NetworkUsage getNetworkUsage() {
    std::ifstream netDevFile("/proc/net/dev");
    std::string line;
    NetworkUsage usage = {0, 0};

    // Skip the first two lines (header lines)
    std::getline(netDevFile, line);
    std::getline(netDevFile, line);

    while (std::getline(netDevFile, line)) {
        std::istringstream iss(line);
        std::string iface;
        iss >> iface;

        // Remove trailing colon from the interface name
        iface.pop_back();

        // Skip the unwanted interfaces
        if (iface != "lo") {
            unsigned long rx_bytes, tx_bytes;
            iss >> rx_bytes; // read rx_bytes
            for (int i = 0; i < 7; ++i) iss >> tx_bytes; // skip to tx_bytes
            iss >> tx_bytes; // read tx_bytes

            usage.rx_bytes += rx_bytes;
            usage.tx_bytes += tx_bytes;
        }
    }

    return usage;
}

// Function to read CPU temperature from sysfs
int get_cpu_temperature() {
    std::ifstream file("/sys/class/thermal/thermal_zone0/temp");
    if (!file.is_open()) {
        std::cerr << "Error: Unable to open temperature file." << std::endl;
        return -1;
    }

    std::string temp_str;
    std::getline(file, temp_str);
    file.close();

    float temp = std::stof(temp_str) / 1000.0; // Convert to Celsius
    return temp;
}
