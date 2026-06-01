#include <fstream>
#include <iostream>
#include <algorithm>
#include <chrono>
#include <string>
#include <thread>
#include <vector>

using namespace std;

struct ramData {
    long total=0;
    long avail=0;
    long used=0;
    long stotal=0;
    long sfree=0;
    long buffers=0;
    long cached=0;
    long active=0;
    long inactive=0;
    double utilization=0.0;
};