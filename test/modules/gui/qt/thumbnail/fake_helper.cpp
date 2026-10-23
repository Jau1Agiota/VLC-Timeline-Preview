/* Test subprocess, not a decoder. GPL-2.0-or-later. */
#include <fstream>
#include <iostream>
#include <string>
#include <cstdlib>
#include <chrono>
#include <thread>
#include <cstdint>
#include <sstream>
static void put(unsigned char *p, uint32_t value, int n=4) {
    for(int i=0;i<n;++i) p[i]=static_cast<unsigned char>(value>>(8*i));
}
static int capture(int argc,char **argv,unsigned id) {
    if(argc!=5) return 2;
    const std::string source(argv[1]);
    if(source.find("slow")!=std::string::npos) std::this_thread::sleep_for(std::chrono::milliseconds(350));
    if(source.find("timeout")!=std::string::npos) std::this_thread::sleep_for(std::chrono::seconds(3));
    if(source.find("error")!=std::string::npos) return 3;
    if(source.find("spam")!=std::string::npos) std::cout<<std::string(20000,'x');
    unsigned char header[54]={}; header[0]='B';header[1]='M';
    put(header+2,54+16*8*4);put(header+10,54);put(header+14,40);
    put(header+18,16);put(header+22,8);put(header+26,1,2);put(header+28,32,2);
    unsigned char pixels[16*8*4];
    for(unsigned i=0;i<sizeof(pixels);i+=4) { pixels[i]=32;pixels[i+1]=128;pixels[i+2]=240;pixels[i+3]=255; }
    std::ofstream out(argv[3],std::ios::binary);out.write(reinterpret_cast<char*>(header),54);
    out.write(reinterpret_cast<char*>(pixels),sizeof(pixels));out.close();
    std::cout<<"{\"status\":\"candidate_captured\",\"requested_ms\":"<<argv[2]
             <<",\"width\":16,\"height\":8,\"frame_timestamp_verified\":false,\"exit_code\":0,\"request_id\":"<<(source.find("badid")!=std::string::npos?id+1:id)<<"}"<<std::endl;
    return 0;
}

int main(int argc,char **argv) {
    if(argc<2 || std::string(argv[1])!="--worker") return capture(argc,argv,0);
    if(argc!=5)return 2;
    std::string line;
    while(std::getline(std::cin,line)) {
        if(line.size()>127)return 2;
        unsigned id;std::string target,timeout,extra;
        std::istringstream request(line);
        if(!(request>>id>>target>>timeout) || request>>extra)return 2;
        char *args[]={argv[0],argv[2],&target[0],argv[3],&timeout[0]};
        const int rc=capture(5,args,id);if(rc)return rc;
        if(std::string(argv[2]).find("unsolicited")!=std::string::npos) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));std::cout<<"{}"<<std::endl;
        }
    }
    return 0;
}
