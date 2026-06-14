#include <algorithm>
#include <chrono>
#include <fcntl.h>
#include <iomanip>
#include <iostream>
#include <string>
#include <termios.h>
#include <thread>
#include <unistd.h>
#include <vector>

#define CPU_NO_MAIN
#define RAM_NO_MAIN
#define DISK_NO_MAIN
#define GPU_NO_MAIN
#define NET_NO_MAIN
#define PROCESSES_NO_MAIN

#include "cpu.cpp"
#include "disk.cpp"
#include "gpu.cpp"
#include "net.cpp"
#include "ops.cpp"
#include "procs.cpp"
#include "ram.cpp"

using namespace std;

int getch() {
  struct termios oldt, newt;
  int ch;
  tcgetattr(STDIN_FILENO, &oldt);
  newt = oldt;
  newt.c_lflag &= ~(ICANON | ECHO);
  tcsetattr(STDIN_FILENO, TCSANOW, &newt);
  ch = getchar();
  tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
  return ch;
}

int kbhit() {
  struct termios oldt, newt;
  int ch;
  int oldf;
  tcgetattr(STDIN_FILENO, &oldt);
  newt = oldt;
  newt.c_lflag &= ~(ICANON | ECHO);
  tcsetattr(STDIN_FILENO, TCSANOW, &newt);
  oldf = fcntl(STDIN_FILENO, F_GETFL, 0);
  fcntl(STDIN_FILENO, F_SETFL, oldf | O_NONBLOCK);
  ch = getchar();
  tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
  fcntl(STDIN_FILENO, F_SETFL, oldf);
  if (ch != EOF) {
    ungetc(ch, stdin);
    return 1;
  }
  return 0;
}

string formatUptime(long seconds) {
  long h = seconds / 3600;
  long m = (seconds % 3600) / 60;
  long s = seconds % 60;
  return to_string(h) + "h " + to_string(m) + "m " + to_string(s) + "s";
}

int main() {
  cpuMonitor cpuMon;
  ramMonitor ramMon;
  diskMonitor diskMon;
  gpuMonitor gpuMon;
  netMonitor netMon;
  ProcessMonitor procMon;

  cpuData cpu;
  cpuStats cpuDummy = cpuMon.getRawStats();
  ramData ram;
  diskData disk;
  gpuData gpu;
  vector<netData> netIfaces;

  int currentTab = 1;
  int sortCol = 3;
  bool sortAsc = false;

  const string CYAN = "\033[1;36m";
  const string GREEN = "\033[1;32m";
  const string YELLOW = "\033[1;33m";
  const string RED = "\033[1;31m";
  const string RESET = "\033[0m";

  procMon.fetch();

  int tick = 20;
  while (true) {
    if (kbhit()) {
      char ch = getch();
      if (ch == 'q' || ch == 'Q') {
        break;
      } else if (ch == '1') {
        currentTab = 1;
        tick = 20;
      } else if (ch == '2') {
        currentTab = 2;
        tick = 20;
      } else if (currentTab == 2) {
        if (ch == 'k' || ch == 'K') {
          cout << "\n" << RED << "Enter PID to kill: " << RESET;
          int pid;
          if (cin >> pid) {
            if (killProcess(pid)) {
              cout << GREEN << "Success: Process " << pid << " terminated."
                   << RESET << "\n";
            } else {
              cout << RED << "Error: Failed to terminate process " << pid << "."
                   << RESET << "\n";
            }
          } else {
            cin.clear();
            cin.ignore(10000, '\n');
          }
          cout << "Press any key to continue...";
          getch();
          tick = 20;
        } else if (ch == 'n' || ch == 'N') {
          cout << "\n" << YELLOW << "Enter PID: " << RESET;
          int pid;
          if (cin >> pid) {
            cout << YELLOW << "Enter new Nice value (-20 to 19): " << RESET;
            int niceVal;
            if (cin >> niceVal) {
              if (setProcessPriority(pid, niceVal)) {
                cout << GREEN << "Success: Priority of PID " << pid
                     << " set to " << niceVal << "." << RESET << "\n";
              } else {
                cout << RED << "Error: Failed to set priority for PID " << pid
                     << ".\n(Note: negative nice requires root/sudo privileges)"
                     << RESET << "\n";
              }
            }
          } else {
            cin.clear();
            cin.ignore(10000, '\n');
          }
          cout << "Press any key to continue...";
          getch();
          tick = 20;
        } else if (ch == 's' || ch == 'S') {
          cout << "\n"
               << CYAN
               << "Enter column to sort (0:PID, 1:USER, 2:NI, 3:CPU, 4:MEM, "
                  "5:VIRT, 6:RES, 7:CMD): "
               << RESET;
          int col;
          if (cin >> col && col >= 0 && col <= 7) {
            if (sortCol == col) {
              sortAsc = !sortAsc;
            } else {
              sortCol = col;
              sortAsc = (col == 0 || col == 1 || col == 7);
            }
          } else {
            cin.clear();
            cin.ignore(10000, '\n');
          }
          tick = 20;
        }
      }
    }

    if (tick >= 20) {
      cout << "\033[2J\033[1;1H";
      cout << RESET;
      cout << YELLOW
           << "  SYSTEM MONITOR TUI  |  [1] Performance  |  [2] Processes  |  "
              "[Q] Quit\n"
           << RESET;
      

      if (currentTab == 1) {
        cpuMon.fetch(cpu, cpuDummy);
        ramMon.fetch(ram);
        diskMon.fetch(disk);
        gpuMon.fetch(gpu);
        netMon.fetch(netIfaces);

        cout << YELLOW << "[CPU Usage] " << RESET
             << fixed << setprecision(1) << cpu.utilization << "%\n";
        cout << "  Model:    " << cpu.model << "\n";
        cout << "  Cores:    " << cpu.cores << " Physical | " << cpu.logical
             << " Logical\n";
        cout << "  Speed:    " << (int)cpu.currentSpeed
             << " MHz (Base: " << (int)cpu.baseSpeed << " MHz)\n";
        cout << "  L3 Cache: " << cpu.l3Cache << "\n";
        cout << "  Uptime:   " << formatUptime(cpu.uptime) << "\n";
        cout << "  Created:  " << cpu.processes << " processes\n\n";

        cout << YELLOW << "[RAM Usage] " << RESET
             << ram.utilization << "%\n";
        cout << "  Total:    " << fixed << setprecision(2)
             << ram.total / (1024.0 * 1024.0) << " GB\n";
        cout << "  In Use:   " << ram.used / (1024.0 * 1024.0)
             << " GB | Available: " << ram.avail / (1024.0 * 1024.0) << " GB\n";
        cout << "  Swap:     " << (ram.stotal - ram.sfree) / (1024.0 * 1024.0)
             << " GB / " << ram.stotal / (1024.0 * 1024.0) << " GB\n";
        cout << "  Buffers:  " << ram.buffers / 1024
             << " MB | Cached: " << ram.cached / 1024 << " MB\n";
        cout << "  Active:   " << ram.active / 1024
             << " MB | Inactive: " << ram.inactive / 1024 << " MB\n\n";

        cout << YELLOW << "[Storage (Disk)]\n" << RESET;
        cout << "  Path:     " << disk.devicePath << " (" << disk.capacityUtil
             << "% full)\n";
        cout << "  Capacity: " << disk.usedB / (1024 * 1024 * 1024) << " GB / "
             << disk.totalB / (1024 * 1024 * 1024) << " GB\n";
        cout << "  I/O Time: " << disk.activeVal << "%\n";
        cout << "  Speed:    Read: " << disk.readMBS
             << " MB/s | Write: " << disk.writeMBS << " MB/s\n\n";

        cout << YELLOW << "[Graphics (GPU)]\n" << RESET;
        if (gpu.hasGPU) {
          cout << "  Model:    " << gpu.gpuldvendor << " (" << gpu.utilization
               << "% utilization)\n";
          cout << "  Driver:   " << gpu.driver << "\n";
          cout << "  Temp:     " << (int)gpu.tempC
               << " C | VRAM: " << gpu.vramUsed << " GB / " << gpu.vramTotal
               << " GB\n\n";
        } else {
          cout << "  No GPU detected on this system.\n\n";
        }

        cout << YELLOW << "[Network Interfaces]\n" << RESET;
        for (const auto &iface : netIfaces) {
          cout << "  " << iface.name
               << ": Down: " << netMonitor::formatSpeed(iface.rxSpeedMbps)
               << " | Up: " << netMonitor::formatSpeed(iface.txSpeedMbps)
               << " (Total RX: " << (iface.totalRxBytes / (1024.0 * 1024.0 * 1024.0))
               << " GB | TX: " << (iface.totalTxBytes / (1024.0 * 1024.0 * 1024.0))
               << " GB)\n";
          cout << "    IPv4: " << iface.ipv4
               << " | IPv6: " << iface.ipv6 << "\n";
        }

      } else if (currentTab == 2) {
        vector<ProcessInfo> procs = procMon.fetchProcesses();

        sort(procs.begin(), procs.end(),
             [sortCol, sortAsc](const ProcessInfo &a, const ProcessInfo &b) {
               bool asc = sortAsc;
               switch (sortCol) {
               case 0:
                 return asc ? (a.pid < b.pid) : (a.pid > b.pid);
               case 1:
                 return asc ? (a.user < b.user) : (a.user > b.user);
               case 2:
                 return asc ? (a.nice < b.nice) : (a.nice > b.nice);
               case 3:
                 return asc ? (a.cpuPercent < b.cpuPercent)
                            : (a.cpuPercent > b.cpuPercent);
               case 4:
                 return asc ? (a.memPercent < b.memPercent)
                            : (a.memPercent > b.memPercent);
               case 5:
                 return asc ? (a.virtMb < b.virtMb) : (a.virtMb > b.virtMb);
               case 6:
                 return asc ? (a.resMb < b.resMb) : (a.resMb > b.resMb);
               case 7:
                 return asc ? (a.command < b.command) : (a.command > b.command);
               default:
                 return false;
               }
             });

        cout << CYAN
             << "  PID    USER      NI    CPU%   MEM%   VIRT     RES      "
                "COMMAND\n"
             << RESET;

        int count = 0;
        for (const auto &p : procs) {
          if (count >= 24)
            break;

          string virtStr = formatMemory(p.virtMb);
          string resStr = formatMemory(p.resMb);
          string cmd = p.command;
          if (cmd.length() > 30)
            cmd = cmd.substr(0, 27) + "...";

          printf("  %-6d %-9s %-5d %-6.1f %-6.1f %-8s %-8s %-30s\n", p.pid,
                 p.user.substr(0, 8).c_str(), p.nice, p.cpuPercent,
                 p.memPercent, virtStr.c_str(), resStr.c_str(), cmd.c_str());
          count++;
        }
        cout << YELLOW << "  Sorted by column " << sortCol << " ("
             << (sortAsc ? "Ascending" : "Descending") << ")\n"
             << RESET;
        cout << "  [K] Kill Process  |  [N] Set Priority (Nice)  |  [S] Sort "
                "Column\n";
      }

      tick = 0;
    }

    this_thread::sleep_for(chrono::milliseconds(2000));
    tick++;
  }

  return 0;
}
