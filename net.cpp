#pragma once
#include <fstream>
#include <iostream>
#include <algorithm>
#include <chrono>
#include <string>
#include <thread>
#include <vector>
#include <sstream>
#include <map>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <net/if.h>

using namespace std;

struct netStats{
    unsigned long long rxBytes;
    unsigned long long txBytes;
};

struct netData{
    string name;
    string ipv4;
    string ipv6;
    unsigned long long totalRxBytes;
    unsigned long long totalTxBytes;
    double rxSpeedMbps;
    double txSpeedMbps;
};

class netMonitor{
    private:
    map<string,netStats> prevStats;
    chrono::steady_clock::time_point lastFetchTime;
    static string trimWhitespace(const string &s){
        size_t start=s.find_first_not_of(" \t\n");
        size_t end=s.find_last_not_of(" \t\n");
        return (start==string::npos)?"":s.substr(start,end-start+1);
    }
    static void getIP(const string &ifaceName,string &ipv4,string &ipv6){
        ipv4="";
        ipv6="";

        struct ifaddrs *ifaddr=nullptr;
        if(getifaddrs(&ifaddr) != 0) return;

        for(struct ifaddrs *ifa=ifaddr;ifa!=nullptr;ifa=ifa->ifa_next){
            if(ifa->ifa_addr==nullptr) continue;
            if(ifaceName!=ifa->ifa_name) continue;

            if(ifa->ifa_addr->sa_family==AF_INET){
                char buf[INET_ADDRSTRLEN];
                struct sockaddr_in *addr = (struct sockaddr_in *)ifa->ifa_addr;
                inet_ntop(AF_INET, &addr->sin_addr, buf, sizeof(buf));
                ipv4=buf;
            }
            else if(ifa->ifa_addr->sa_family==AF_INET6){
                char buf[INET6_ADDRSTRLEN];
                struct sockaddr_in6 *addr = (struct sockaddr_in6 *)ifa->ifa_addr;
                inet_ntop(AF_INET6, &addr->sin6_addr, buf, sizeof(buf));
                ipv6=buf;
            }
        }
        freeifaddrs(ifaddr);        
    }
    public:
    netMonitor():lastFetchTime(chrono::steady_clock::now()){}
    static string formatSpeed(double kbs){
        char buf[32];
        if(kbs>=1024.0){
            snprintf(buf,sizeof(buf),"%.2f Mbps",kbs/1024.0);
        }
        else{
            snprintf(buf,sizeof(buf),"%.2f Kbps",kbs);
        }
        return buf;
    }
    void fetch (vector<netData> &iflist){
        iflist.clear();
        auto currentTime=chrono::steady_clock::now();
        double seconds=chrono::duration<double>(currentTime - lastFetchTime).count();
        if(seconds<=0.0) seconds=1.0;
        ifstream file("/proc/net/dev");
        if(!file.is_open()) return;
        string line;
        getline(file,line);
        getline(file,line);
        while(getline(file,line)){
            size_t colonPos=line.find(':');
            if(colonPos==string::npos) continue;
            string name=trimWhitespace(line.substr(0,colonPos));
            if (name=="lo") continue;
            stringstream ss(line.substr(colonPos+1));
            unsigned long long rxBytes, rxPkt, rxErr, rxDrop, rxFifo, rxFrame, rxComp,rxMcast;
            unsigned long long txBytes, txPkt, txErr, txDrop, txFifo, txColls, txCarr,txComp;
            if (!(ss >> rxBytes >> rxPkt >> rxErr >> rxDrop >> rxFifo >> rxFrame >>
            rxComp >> rxMcast >> txBytes >> txPkt >> txErr >> txDrop >>
            txFifo >> txColls >> txCarr >> txComp)) continue;
            netData data;
            data.name=name;
            getIP(name,data.ipv4,data.ipv6);
            data.totalRxBytes=rxBytes;
            data.totalTxBytes=txBytes;
            if(prevStats.count(name)){
                unsigned long long rxDelta = rxBytes - prevStats[name].rxBytes;
                unsigned long long txDelta = txBytes - prevStats[name].txBytes;
                data.rxSpeedMbps = (rxDelta * 8.0) / (seconds * 1024.0);
                data.txSpeedMbps = (txDelta * 8.0) / (seconds * 1024.0);
            }
            prevStats[name]={rxBytes,txBytes};
            iflist.push_back(data);
        }
        lastFetchTime=currentTime;
    }
};
#ifndef NET_NO_MAIN
int main(){
    netMonitor monitor;
    vector<netData> interfaces;

    while(true){
        monitor.fetch(interfaces);
        for(const auto &iface:interfaces){
            cout<<"Interface: "<<iface.name<<endl
                <<"IPv4: "<<iface.ipv4<<endl
                <<"IPv6: "<<iface.ipv6<<endl
                <<"Total RX: "<<iface.totalRxBytes<<" bytes"<<endl
                <<"Total TX: "<<iface.totalTxBytes<<" bytes"<<endl
                <<"RX Speed: "<<netMonitor::formatSpeed(iface.rxSpeedMbps)<<endl
                <<"TX Speed: "<<netMonitor::formatSpeed(iface.txSpeedMbps)<<endl;
        }
        this_thread::sleep_for(chrono::seconds(5));
    }
}
