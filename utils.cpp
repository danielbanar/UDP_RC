#include <iostream>
#include <fstream>
#include <string>

using namespace std;

// Function to read CPU temperature from sysfs
int get_cpu_temperature() {
    ifstream file("/sys/class/thermal/thermal_zone0/temp");
    if (!file.is_open()) {
        cerr << "Error: Unable to open temperature file." << endl;
        return -1;
    }

    string temp_str;
    getline(file, temp_str);
    file.close();

    float temp = stof(temp_str) / 1000.0; // Convert to Celsius
    return temp;
}
