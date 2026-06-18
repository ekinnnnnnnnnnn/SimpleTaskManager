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

#include <algorithm>
#include <chrono>
#include <ncurses.h>
#include <string>
#include <thread>
#include <vector>

using namespace std;

static string fmtUptime(long s) {
  char buf[32];
  snprintf(buf, sizeof(buf), "%ldh %02ldm %02lds", s / 3600, (s % 3600) / 60,
           s % 60);
  return buf;
}

static string fmtMB(double mb) {
  char buf[32];
  if (mb >= 1024 * 1024)
    snprintf(buf, sizeof(buf), "%.1fT", mb / (1024 * 1024));
  else if (mb >= 1024)
    snprintf(buf, sizeof(buf), "%.1fG", mb / 1024);
  else
    snprintf(buf, sizeof(buf), "%.0fM", mb);
  return buf;
}

static string trunc(const string &s, int maxlen) {
  if ((int)s.size() <= maxlen)
    return s;
  return s.substr(0, maxlen - 3) + "...";
}
static void drawInfo(WINDOW *w, cpuMonitor &cpuMon, cpuData &cpu,
                     cpuStats &cpuSt, ramMonitor &ramMon, ramData &ram,
                     diskMonitor &diskMon, diskData &disk, gpuMonitor &gpuMon,
                     gpuData &gpu, netMonitor &netMon, vector<netData> &net) {
  cpuMon.fetch(cpu, cpuSt);
  ramMon.fetch(ram);
  diskMon.fetch(disk);
  gpuMon.fetch(gpu);
  netMon.fetch(net);

  int y = 2;
  mvwprintw(w, y++, 2, "=== CPU ===");
  mvwprintw(w, y++, 2, "Model   : %s", trunc(cpu.model, 60).c_str());
  mvwprintw(w, y++, 2, "Cores   : %d physical / %d logical", cpu.cores,
            cpu.logical);
  mvwprintw(w, y++, 2, "Speed   : %.0f MHz", cpu.currentSpeed);
  mvwprintw(w, y++, 2, "Usage   : %.1f%%", cpu.utilization);
  mvwprintw(w, y++, 2, "Uptime  : %s", fmtUptime(cpu.uptime).c_str());
  mvwprintw(w, y++, 2, "Procs   : %ld", cpu.processes);
  y++;

  mvwprintw(w, y++, 2, "=== Memory ===");
  mvwprintw(w, y++, 2, "RAM     : %.2f / %.2f GB  (%.1f%%)",
            ram.used / 1024.0 / 1024, ram.total / 1024.0 / 1024,
            ram.utilization);
  mvwprintw(w, y++, 2, "Swap    : %.2f / %.2f GB",
            (ram.stotal - ram.sfree) / 1024.0 / 1024,
            ram.stotal / 1024.0 / 1024);
  y++;

  mvwprintw(w, y++, 2, "=== GPU ===");
  if (gpu.hasGPU) {
    mvwprintw(w, y++, 2, "Model   : %s", trunc(gpu.gpuldvendor, 60).c_str());
    mvwprintw(w, y++, 2, "Driver  : %s", gpu.driver.c_str());
    mvwprintw(w, y++, 2, "Usage   : %.1f%%", gpu.utilization);
    mvwprintw(w, y++, 2, "VRAM    : %.0f / %.0f MB", gpu.vramUsed,
              gpu.vramTotal);
    mvwprintw(w, y++, 2, "Temp    : %.0f C", gpu.tempC);
  } else {
    mvwprintw(w, y++, 2, "No GPU detected.");
  }
  y++;

  mvwprintw(w, y++, 2, "=== Disk ===");
  mvwprintw(w, y++, 2, "Path    : %s  (%.1f%% full)", disk.devicePath.c_str(),
            disk.capacityUtil);
  mvwprintw(w, y++, 2, "Used    : %llu / %llu GB",
            disk.usedB / (1024 * 1024 * 1024),
            disk.totalB / (1024 * 1024 * 1024));
  mvwprintw(w, y++, 2, "I/O     : Read %.1f MB/s  Write %.1f MB/s",
            disk.readMBS, disk.writeMBS);
  y++;

  mvwprintw(w, y++, 2, "=== Network ===");
  for (const auto &iface : net) {
    mvwprintw(w, y++, 2, "%-10s  IPv4: %-16s  Down: %s  Up: %s",
              iface.name.c_str(), iface.ipv4.empty() ? "-" : iface.ipv4.c_str(),
              netMonitor::formatSpeed(iface.rxSpeedMbps).c_str(),
              netMonitor::formatSpeed(iface.txSpeedMbps).c_str());
  }
}

static const char *SORT_NAMES[] = {"PID", "CMD", "CPU", "RAM"};

static void drawProcs(WINDOW *w, vector<ProcessInfo> &procs, int sortCol,
                      bool sortAsc, int rows, int cols) {

  sort(procs.begin(), procs.end(),
       [sortCol, sortAsc](const ProcessInfo &a, const ProcessInfo &b) {
         switch (sortCol) {
         case 0:
           return sortAsc ? a.pid < b.pid : a.pid > b.pid;
         case 1:
           return sortAsc ? a.command < b.command : a.command > b.command;
         case 2:
           return sortAsc ? a.cpuPercent < b.cpuPercent
                          : a.cpuPercent > b.cpuPercent;
         case 3:
           return sortAsc ? a.resMb < b.resMb : a.resMb > b.resMb;
         default:
           return sortAsc ? a.cpuPercent < b.cpuPercent : a.cpuPercent > b.cpuPercent;
         }
       });

  wattron(w, A_REVERSE);
  mvwprintw(w, 2, 0, "  %-7s %-8s %-10s %-s", "PID", "CPU(%)", "RAM(MB)", "COMMAND");
  wclrtoeol(w);
  wattroff(w, A_REVERSE);

  int displayY = 3;
  for (int i = 0; i < (int)procs.size() && displayY < rows - 1 && i < 15;
       i++, displayY++) {
    const ProcessInfo &p = procs[i];
    int cmdMax = max(10, cols - 31);
    string cmd = trunc(p.command, cmdMax);
    char row[512];
    snprintf(row, sizeof(row), "  %-7d %-8.1f %-10.1f %s",
             p.pid, p.cpuPercent, p.resMb, cmd.c_str());
    mvwprintw(w, displayY, 0, "%-*s", cols - 1, row);
  }

  mvwprintw(w, rows - 1, 0,
            " [k]Kill  [n]Nice  [s]Sort(%-4s %s)  Procs:%-4d  [q]Quit",
            SORT_NAMES[sortCol], sortAsc ? "^" : "v", (int)procs.size());
}

static string readInput(WINDOW *w, int rows, const char *prompt) {
  mvwprintw(w, rows - 1, 0, "%s", prompt);
  wclrtoeol(w);
  wrefresh(w);
  echo();
  curs_set(1);
  timeout(-1);
  char buf[64] = {};
  wgetnstr(w, buf, sizeof(buf) - 1);
  timeout(0);
  noecho();
  curs_set(0);
  return buf;
}
int main() {
  initscr();
  cbreak();
  noecho();
  keypad(stdscr, TRUE);
  curs_set(0);
  timeout(0);

  cpuMonitor cpuMon;
  cpuData cpu;
  cpuStats cpuSt = cpuMon.getRawStats();
  ramMonitor ramMon;
  ramData ram;
  string diskDev = ifstream("/sys/block/sda").good() ? "sda" : "nvme0n1";
  diskMonitor diskMon(diskDev);
  diskData disk;
  gpuMonitor gpuMon;
  gpuData gpu;
  netMonitor netMon;
  vector<netData> net;
  ProcessMonitor procMon;
  vector<ProcessInfo> procs;

  int tab = 1, sortCol = 2;
  bool sortAsc = false, paused = false;
  auto lastDraw = chrono::steady_clock::now() - chrono::seconds(3);

  while (true) {
    int rows, cols;
    getmaxyx(stdscr, rows, cols);

    int ch = wgetch(stdscr);
    if (ch == 'q' || ch == 'Q')
      break;
    else if (ch == '1') {
      tab = 1;
    } else if (ch == '2') {
      tab = 2;
      paused = false;
    } else if (tab == 2) {
      if (ch == 'k' || ch == 'K') {
        paused = true;
        string pidStr = readInput(stdscr, rows, "Kill - PID: ");
        try {
          int pid = stoi(pidStr);
          bool ok = killProcess(pid);
          mvwprintw(stdscr, rows - 1, 0,
                    ok ? "Killed %d. [enter]" : "Failed. [enter]", pid);
        } catch (...) {
          mvwprintw(stdscr, rows - 1, 0, "Invalid PID. [enter]");
        }
        wclrtoeol(stdscr);
        wrefresh(stdscr);
        while (wgetch(stdscr) == ERR)
          this_thread::sleep_for(chrono::milliseconds(50));
        paused = false;
        lastDraw -= chrono::seconds(3);
      }
      if (ch == 'n' || ch == 'N') {
        paused = true;
        string pidStr = readInput(stdscr, rows, "Nice - PID: ");
        try {
          int pid = stoi(pidStr);
          string niceStr = readInput(stdscr, rows, "Nice value (-20..19): ");
          int val = max(-20, min(19, stoi(niceStr)));
          bool ok = setProcessPriority(pid, val);
          mvwprintw(stdscr, rows - 1, 0,
                    ok ? "Done. [enter]" : "Failed (sudo?). [enter]");
        } catch (...) {
          mvwprintw(stdscr, rows - 1, 0, "Invalid. [enter]");
        }
        wclrtoeol(stdscr);
        wrefresh(stdscr);
        while (wgetch(stdscr) == ERR)
          this_thread::sleep_for(chrono::milliseconds(50));
        paused = false;
        lastDraw -= chrono::seconds(3);
      }
      if (ch == 's' || ch == 'S')
        sortCol = (sortCol + 1) % 4;
    }

    auto now = chrono::steady_clock::now();
    if (chrono::duration<double>(now - lastDraw).count() >= 2.0) {
      lastDraw = now;

      if (tab == 2 && !paused)
        procs = procMon.fetchProcesses();

      werase(stdscr);
      mvwprintw(stdscr, 0, 0, " Task Manager  [1]Info  [2]Processes  [q]Quit");
      wattron(stdscr, A_REVERSE);
      mvwprintw(stdscr, 0, tab == 1 ? 15 : 23,
                tab == 1 ? "[1]Info" : "[2]Processes");
      wattroff(stdscr, A_REVERSE);
      mvwhline(stdscr, 1, 0, '-', cols);

      if (tab == 1)
        drawInfo(stdscr, cpuMon, cpu, cpuSt, ramMon, ram, diskMon, disk, gpuMon,
                 gpu, netMon, net);
      else
        drawProcs(stdscr, procs, sortCol, sortAsc, rows, cols);

      wrefresh(stdscr);
    }

    this_thread::sleep_for(chrono::milliseconds(50));
  }

  endwin();
  return 0;
}
