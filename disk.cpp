#pragma once
#include <fstream>
#include <iostream>
#include <algorithm>
#include <chrono>
#include <string>
#include <thread>
#include <vector>
#include <sstream>
#include <sys/statvfs.h>

using namespace std;

struct diskStats{
    unsigned long long readSectors;
    unsigned long long writeSectors;
    unsigned long long ioMs;
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

class diskMonitor{
    private:
    diskStats prevStats{0,0,0};
    string deviceName;
    diskStats getRawStats(){
        ifstream diskstats("/proc/diskstats");
        string line;
        diskStats stats{0,0,0};
        string searchkey=" "+deviceName+" ";

        while(getline(diskstats,line)){
            if(line.find(searchkey)==string::npos)
                continue;
            istringstream ss(line);
            string skip;
            unsigned long long dummy;
            for(int i=0;i<5;i++)
                ss>>skip;
            ss>>stats.readSectors;
            for(int i=0;i<3;i++)
                ss>>skip;
            ss>>stats.writeSectors;
            break;
        }
        return stats;
    }
    public:
    diskMonitor(string dev = "sda") : deviceName(dev) {}
    void fetch(diskData &data){
        struct statvfs vfs;
        if(statvfs("/",&vfs)==0){
            data.devicePath="/";
            data.totalB=vfs.f_blocks*vfs.f_frsize;
            data.freeB=vfs.f_bavail*vfs.f_frsize;
            data.usedB=data.totalB-data.freeB;
            data.capacityUtil=(double)data.usedB/data.totalB*100.0;
        }
        diskStats curr=getRawStats();
        if(prevStats.readSectors!=0){
            data.readMBS=(double)(curr.readSectors-prevStats.readSectors)*512/(1024*1024);
            data.writeMBS=(double)(curr.writeSectors-prevStats.writeSectors)*512/(1024*1024);
            data.activeVal=min(100.0,(double)(curr.ioMs-prevStats.ioMs)/2000.0*100.0);
        }
        else{
            data.readMBS=data.writeMBS=data.activeVal=0.0;
        }
        prevStats=curr;
    }       
};
#ifndef DISK_NO_MAIN
int main(){
    string diskName="sda";
    if(!ifstream("/sys/block/sda").is_open())
        diskName="nvme0n1";
    diskMonitor monitor(diskName);
    diskData data;
    
    while(true){
        monitor.fetch(data);
        cout<<"Device Path: "<<data.devicePath<<endl
            <<"Total GB: "<<data.totalB/(1024*1024*1024)<<endl
            <<"Used GB: "<<data.usedB/(1024*1024*1024)<<endl
            <<"Free GB: "<<data.freeB/(1024*1024*1024)<<endl
            <<"Capacity Utilization %: "<<data.capacityUtil<<endl
            <<"Active %: "<<data.activeVal<<endl
            <<"Read Speed MBS: "<<data.readMBS<<endl
            <<"Write Speed MBS: "<<data.writeMBS<<endl;
        this_thread::sleep_for(chrono::seconds(5));
    }
}
#endif