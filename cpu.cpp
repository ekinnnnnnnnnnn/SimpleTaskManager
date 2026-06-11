#pragma once
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
        void getFeatures(cpuData &data) {
            ifstream cpuinfo("/proc/cpuinfo");
            string line;
            vector<int> physicalIds;  

            while (getline(cpuinfo, line)) {
                if (line.find("model name") != string::npos)
                    data.model = line.substr(line.find(":") + 2);

                else if (line.find("cpu MHz") != string::npos)
                    data.currentSpeed = stod(line.substr(line.find(":") + 2));

                else if (line.find("cpu cores") != string::npos)
                    data.cores = stoi(line.substr(line.find(":") + 2));

                else if (line.find("processor") != string::npos)
                    data.logical = stoi(line.substr(line.find(":") + 2)) + 1;

                else if (line.find("physical id") != string::npos) {
                    int id = stoi(line.substr(line.find(":") + 2));
                    if (find(physicalIds.begin(), physicalIds.end(), id) == physicalIds.end())
                    physicalIds.push_back(id);
                }

            }

            data.sockets = physicalIds.empty() ? 1 : (int)physicalIds.size();

            ifstream cacheFile("/sys/devices/system/cpu/cpu0/cache/index3/size");
            cacheFile >> data.l3Cache;

            ifstream uptimeFile("/proc/uptime");
            uptimeFile >> data.uptime;

            ifstream statFile("/proc/stat");
            string label;
            while (statFile >> label) {
                if (label == "processes")
                    statFile >> data.processes;
            }
        }

        double calculateUsage(const cpuStats &s1,const cpuStats &s2) {
            long long totalDiff = s2.total() - s1.total();
            long long idleDiff = s2.idleTime() - s1.idleTime();
            return 100.0 * (totalDiff - idleDiff) / totalDiff;
        }
        void fetch(cpuData &data, cpuStats &stats) {
            cpuStats currStats = getRawStats();
            data.utilization = calculateUsage(prevStats, currStats);
            prevStats = currStats;
            stats = currStats;
            getFeatures(data);
        }
    };


#ifndef CPU_NO_MAIN
int main() {
    cpuMonitor monitor;
    cpuStats stats;
    cpuData data;

    stats=monitor.getRawStats();
    monitor.getFeatures(data);

    while (true) {
        monitor.fetch(data, stats);
        cout<<"User: "<<stats.user<<endl
            <<"Nice: "<<stats.nice<<endl
            <<"System: "<<stats.system<<endl
            <<"Idle: "<<stats.idle<<endl
            <<"IOWait: "<<stats.iowait<<endl
            <<"IRQ: "<<stats.irq<<endl
            <<"SoftIRQ: "<<stats.softirq<<endl
            <<"Steal: "<<stats.steal<<endl;
        cout<<"Model: "<<data.model<<endl
            <<"L3 Cache: "<<data.l3Cache<<endl
            <<"Sockets: "<<data.sockets<<endl
            <<"Logical Cores: "<<data.logical<<endl
            <<"Uptime: "<<data.uptime<<" seconds"<<endl
            <<"Processes: "<<data.processes<<endl;
        cout << "Usage (%): " << data.utilization << endl
             << "Current Speed (MHz): " << data.currentSpeed << endl
             << "Total Processes: " << data.processes << endl;
        this_thread::sleep_for(chrono::seconds(5));
    }

    return 0;
}
#endif