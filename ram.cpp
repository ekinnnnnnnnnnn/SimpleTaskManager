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

class ramMonitor {
    public:
        void fetch(ramData &data){
            ifstream meminfo("/proc/meminfo");
            string label;
            long value;
            string unit;

            while(meminfo>>label>>value>>unit){
                if(label=="MemTotal:")
                    data.total=value;
                else if(label=="MemAvailable:")
                    data.avail=value;
                else if(label=="SwapTotal:")
                    data.stotal=value;
                else if(label=="SwapFree:")
                    data.sfree=value;
                else if(label=="Buffers:")
                    data.buffers=value;
                else if(label=="Cached:")
                    data.cached=value;
                else if(label=="Active:")
                    data.active=value;
                else if(label=="Inactive:")
                    data.inactive=value;
            }
            data.used=data.total-data.avail;
            data.utilization=100.0*data.used/data.total;
        }
};
#ifndef RAM_NO_MAIN
int main(){
    ramMonitor monitor;
    ramData data;

    while(true){
        monitor.fetch(data);
        cout<<"Total RAM: "<<data.total<<" kB"<<endl
            <<"Available RAM: "<<data.avail<<" kB"<<endl
            <<"Used RAM: "<<data.used<<" kB"<<endl
            <<"RAM Utilization: "<<data.utilization<<" %"<<endl
            <<"Total Swap: "<<data.stotal<<" kB"<<endl
            <<"Free Swap: "<<data.sfree<<" kB"<<endl
            <<"Buffers: "<<data.buffers<<" kB"<<endl
            <<"Cached: "<<data.cached<<" kB"<<endl
            <<"Active: "<<data.active<<" kB"<<endl
            <<"Inactive: "<<data.inactive<<" kB"<<endl;
        this_thread::sleep_for(chrono::seconds(5));
    }
}
#endif