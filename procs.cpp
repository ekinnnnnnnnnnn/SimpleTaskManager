#pragma once
#include <fstream>
#include <iostream>
#include <algorithm>
#include <chrono>
#include <string>
#include <thread>
#include <vector>
#include <dirent.h>
#include <map>
#include <pwd.h>
#include <set>
#include <sstream>
#include <sys/stat.h>
#include <unistd.h>

#define CPU_NO_MAIN
#define RAM_NO_MAIN
#define DISK_NO_MAIN
#define GPU_NO_MAIN
#define NET_NO_MAIN

#include "cpu.cpp"
#include "ram.cpp"
#include "disk.cpp"
#include "gpu.cpp"
#include "net.cpp"

using namespace std;

struct ProcessInfo {
  int pid;
  string user;
  double cpuPercent;
  double memPercent;
  double virtMb;
  double resMb;
  int nice;
  string command;
  unsigned long long totalTicks;
};

class ProcessMonitor{
    private:
    long pageSize;
    double clkTck;
    unsigned long long totalRam;
    map<int, unsigned long long> prevTicksMap;
    chrono::steady_clock::time_point lastTime;

    string getUsername(uid_t uid){
        static map<uid_t, string> cache;
        if(cache.count(uid)) return cache[uid];
        struct passwd *pwd = getpwuid(uid);
        string name = pwd ? pwd->pw_name : to_string(uid);
        cache[uid] = name;
        return name;
    }
    uid_t getProcessUid(int pid) {
        struct stat st;
        string path = "/proc/" + to_string(pid);
        return (stat(path.c_str(), &st) == 0) ? st.st_uid : 0;
    }
    string getCmdline(int pid, const string &comm) {
        ifstream file("/proc/" + to_string(pid) + "/cmdline");
        if (!file.is_open()) return comm;
        string cmd;
        char ch;
        while (file.get(ch)) {
        cmd += (ch == '\0') ? ' ' : ch;
        }
        if (!cmd.empty() && cmd.back() == ' ') cmd.pop_back();
        return cmd.empty() ? "[" + comm + "]" : cmd;
    }
    public:
    ProcessMonitor():pageSize(sysconf(_SC_PAGESIZE)),
                    clkTck((double)sysconf(_SC_CLK_TCK)),
                    totalRam((unsigned long long)sysconf(_SC_PHYS_PAGES) * sysconf(_SC_PAGESIZE)),
                    lastTime(chrono::steady_clock::now()) {}
    vector<ProcessInfo> fetch() {
        auto now = chrono::steady_clock::now();
        double elapsedSec = chrono::duration<double>(now - lastTime).count();
        if (elapsedSec <= 0.0) elapsedSec = 1.0;
        vector<ProcessInfo> processes;
        set<int> activePids;
        DIR *dir = opendir("/proc");
        if (!dir) return processes;
        struct dirent *entry;
        while ((entry = readdir(dir)) != nullptr) {
            if (entry->d_type != DT_DIR) continue;
            string name = entry->d_name;
            if (!all_of(name.begin(), name.end(), ::isdigit)) continue;
            int pid = stoi(name);
            activePids.insert(pid);
            ifstream statFile("/proc/" + name + "/stat");
            if (!statFile.is_open()) continue;
            string content;
            getline(statFile, content);
            size_t lastParen = content.rfind(')');
            if (lastParen == string::npos) continue;
            size_t firstParen = content.find('(');
            string comm = content.substr(firstParen + 1, lastParen - firstParen - 1);
            stringstream ss(content.substr(lastParen + 2));
            string state;
            long long ppid, pgrp, session, tty_nr, tpgid;
            unsigned long flags, minflt, cminflt, majflt, cmajflt;
            unsigned long long utime, stime;
            long long cutime, cstime;
            long priority, niceVal, num_threads, itrealvalue;
            unsigned long long starttime, vsize;
            long long rss;
            if (!(ss >> state >> ppid >> pgrp >> session >> tty_nr >> tpgid >> flags
               >> minflt >> cminflt >> majflt >> cmajflt >> utime >> stime
               >> cutime >> cstime >> priority >> niceVal >> num_threads >> itrealvalue
               >> starttime >> vsize >> rss)) continue;
            ProcessInfo proc;
            proc.pid = pid;
            proc.user = getUsername(getProcessUid(pid));
            proc.virtMb = vsize / (1024.0 * 1024.0);
            proc.resMb = (rss * pageSize) / (1024.0 * 1024.0);
            proc.nice = (int)niceVal;
            proc.memPercent = 100.0 * (rss * pageSize) / totalRam;
            proc.command = getCmdline(pid, comm);
            proc.totalTicks = utime + stime;
            if (prevTicksMap.count(pid) && proc.totalTicks >= prevTicksMap[pid]) {
                unsigned long long deltaTicks = proc.totalTicks - prevTicksMap[pid];
                proc.cpuPercent = 100.0 * ((double)deltaTicks / clkTck) / elapsedSec;
            } 
            else {
                proc.cpuPercent = 0.0;
            }
            prevTicksMap[pid] = proc.totalTicks;
            processes.push_back(proc);
        }
        closedir(dir);
        for (auto it = prevTicksMap.begin(); it != prevTicksMap.end(); ) {
            if (!activePids.count(it->first))
                it = prevTicksMap.erase(it);
            else
                ++it;
        }
        lastTime = now;
        return processes;
    }
    vector<ProcessInfo> fetchProcesses() {
        return fetch();
    }
};
string formatMemory(double mb) {
    char buf[32];
    if (mb >= 1024.0 * 1024.0) {
        snprintf(buf, sizeof(buf), "%.1fT", mb / (1024.0 * 1024.0));
    } else if (mb >= 1024.0) {
        snprintf(buf, sizeof(buf), "%.1fG", mb / 1024.0);
    } else {
        snprintf(buf, sizeof(buf), "%.1fM", mb);
    }
    return string(buf);
}
#ifndef PROCESSES_NO_MAIN
int main() {
    ProcessMonitor monitor;
    monitor.fetch();
    this_thread::sleep_for(chrono::milliseconds(500));
    vector<ProcessInfo> procs = monitor.fetch();
    sort(procs.begin(), procs.end(),
       [](const ProcessInfo &a, const ProcessInfo &b) {
            return a.cpuPercent > b.cpuPercent;
        });

    printf("%-7s %-10s %5s %6s %6s %8s %8s  %-s\n", "PID", "USER", "NI", "CPU%",
            "MEM%", "VIRT", "RES", "COMMAND");

    int count = 0;
    for (const auto &p : procs) {
        if (count >= 20) break;
        string cmd = p.command;
        if (cmd.length() > 50) cmd = cmd.substr(0, 47) + "...";
        string virtStr = formatMemory(p.virtMb);
        string resStr = formatMemory(p.resMb);

        printf("%-7d %-10s %5d %5.1f%% %5.1f%% %8s %8s  %-s\n", p.pid,
            p.user.c_str(), p.nice, p.cpuPercent, p.memPercent, virtStr.c_str(),
            resStr.c_str(), cmd.c_str());
        count++;
    }
    return 0;
}
#endif
