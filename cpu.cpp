#include <fstream>
#include <iostream>
#include <algorithm>
#include <chrono>
#include <string>
#include <thread>
#include <vector>

using namespace std;

struct cpuStats {
    long long user=0;
    long long nice=0;
    long long system=0;
    long long idle=0;
    long long iowait=0;
    long long irq=0;
    long long softirq=0;
    long long steal=0;

    long long total() const {
        return user + nice + system + idle + iowait + irq + softirq + steal;
    }

    long long idleTime() const {
        return idle + iowait;
    }
};

struct cpuData {
    string model;
    double utilization;
    double currentSpeed;
    double baseSpeed;
    int sockets;
    int cores;
    int logical;
    long uptime;
    long processes;
    string l3Cache;
    string microcode;
    string family;
};


