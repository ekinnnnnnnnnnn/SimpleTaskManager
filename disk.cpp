#include <fstream>
#include <iostream>
#include <algorithm>
#include <chrono>
#include <string>
#include <thread>
#include <vector>
#include <sys/statvfs.h>

using namespace std;

struct diskStats{
    unsigned long long readSectors;
    unsigned long long writeSectors;
    unsigned long long ioMsl
};

struct diskData{
    string devicePath;
    unsigned long long totalB;
    unsigned long long freeB;
    unsigned long long usedB;
    double capacityUtil;
    double activeVal;
    double readMBS;
    double writeMBS;
};