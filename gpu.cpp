#include <fstream>
#include <iostream>
#include <algorithm>
#include <chrono>
#include <string>
#include <thread>
#include <vector>
#include <array>
#include <sstream>
#include <memory>

using namespace std;

struct gpuData{
    bool hasGPU=false;
    string gpuldvendor="asd";
    string driver="asd";
    double utilization=0.0;
    double vramUsed=0.0;
    double vramTotal=0.0;
    double tempC=0.0;
};

class gpuMonitor{
    private:
    string exec(const char* cmd){
        array<char,128> buffer;
        string result;
        unique_ptr<FILE, int(*)(FILE*)> pipe(popen(cmd, "r"), pclose);
        if (!pipe)
            return "";
        while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
            result += buffer.data();
        }
        return result;
        
    }
    long long readSysfs(const string &path){
    ifstream file(path);
    long long value=-1;
    if(file.is_open())
        file>>value;
    return value;
    }
    string extractField(const string &input,const string &field){
        size_t pos=input.find(field);
        if(pos==string::npos)
            return "";
        pos+=field.length();
        size_t end=input.find("\n",pos);
        return input.substr(pos,end-pos);
    }
    vector<string> parseNvidiaCsv(const string &csv){
        vector<string> tokens;
        istringstream ss(csv);
        string token;
        while(getline(ss,token,',')){
            size_t start=token.find_first_not_of(" \t\n");
            size_t end=token.find_last_not_of(" \t\n");
            if(start!=string::npos)
                tokens.push_back(token.substr(start,end-start+1));
        }
        return tokens;
    }
    public:
    void fetch(gpuData &data){
        data=gpuData();
        string nvOutput = exec("nvidia-smi --query-gpu=name,driver_version,utilization.gpu,"
                                "memory.used,memory.total,temperature.gpu "
                                "--format=csv,noheader,nounits 2>/dev/null");
        if(!nvOutput.empty()){
            data.hasGPU=true;
            vector<string> tokens = parseNvidiaCsv(nvOutput);
            if(tokens.size()>=6){
                data.gpuldvendor=tokens[0];
                data.driver=tokens[1];
                data.utilization=stod(tokens[2]);
                data.vramUsed=stod(tokens[3]);
                data.vramTotal=stod(tokens[4]);
                data.tempC=stod(tokens[5]);
            }
            return;
        }
        string lspciOut = exec("lspci -vnn | grep VGA -A 10 2>/dev/null");
        if (lspciOut.empty())
            return;
        data.hasGPU=true;
        data.gpuldvendor=extractField(lspciOut,"VGA compatible controller:");
        data.driver=extractField(lspciOut,"Kernel driver in use:");
        long long util=readSysfs("/sys/class/drm/card0/device/gpu_busy_percent");
        if(util>=0)
            data.utilization=util;
        long long vramUsed=readSysfs("/sys/class/drm/card0/device/memory_used");
        long long vramTotal=readSysfs("/sys/class/drm/card0/device/memory_total");
        if(vramUsed>=0) data.vramUsed=vramUsed/(1024.0*1024.0);
        if(vramTotal>=0) data.vramTotal=vramTotal/(1024.0*1024.0);
        long long temp=readSysfs("/sys/class/drm/card0/device/hwmon/hwmon0/temp1_input");
        if(temp>=0) data.tempC=temp/1000.0;
        if(temp<0) data.tempC=readSysfs("/sys/class/drm/card0/device/hwmon/hwmon1/temp1_input")/1000.0;
    }
};
int main(){
    gpuMonitor monitor;
    gpuData data;

    while(true){
        monitor.fetch(data);
        if(data.hasGPU){
            cout<<"GPU: "<<data.gpuldvendor<<endl
                <<"Driver: "<<data.driver<<endl
                <<"Utilization %: "<<data.utilization<<endl
                <<"VRAM Used MB: "<<data.vramUsed<<endl
                <<"VRAM Total MB: "<<data.vramTotal<<endl
                <<"Temperature C: "<<data.tempC<<endl;
        }
        else{
            cout<<"No GPU detected."<<endl;
        }
        this_thread::sleep_for(chrono::seconds(5));
    }

    return 0;
}