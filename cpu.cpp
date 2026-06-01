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

class cpuMonitor {
    private:
        cpuStats prevStats;
    public:
        cpuStats getRawStats(){
            ifstream file("/proc/stat");
            string label;
            cpuStats stats;

            if (file>>label&&label=="cpu"){
                file>>stats.user>>
                    stats.nice>>
                    stats.system>>
                    stats.idle>>
                    stats.iowait>>
                    stats.irq>>
                    stats.softirq>>
                    stats.steal;
            }
            return stats;
        }
};


int main() {
    cpuMonitor monitor;
    cpuStats stats;
    cpuData data;

    stats=monitor.getRawStats();

    while (true) {
        this_thread::sleep_for(chrono::seconds(5));
        cout<<"User: "<<stats.user<<endl
            <<"Nice: "<<stats.nice<<endl
            <<"System: "<<stats.system<<endl
            <<"Idle: "<<stats.idle<<endl
            <<"IOWait: "<<stats.iowait<<endl
            <<"IRQ: "<<stats.irq<<endl
            <<"SoftIRQ: "<<stats.softirq<<endl
            <<"Steal: "<<stats.steal<<endl;
    }

    return 0;
}

