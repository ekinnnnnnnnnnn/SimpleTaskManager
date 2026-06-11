#pragma once
#include <fstream>
#include <iostream>
#include <algorithm>
#include <chrono>
#include <string>
#include <thread>
#include <vector>
#include <sys/types.h>
#include <sys/resource.h>
#include <signal.h>
#include <unistd.h>

using namespace std;

bool killProcess(int pid, int signal = 9) {
    int result = kill(static_cast<pid_t>(pid), signal);
    return (result == 0);
}
bool setProcessPriority(int pid, int niceValue) {
    int result = setpriority(PRIO_PROCESS, static_cast<id_t>(pid), niceValue);
    return (result == 0);
}