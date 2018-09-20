#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <iostream>
#include <vector>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <sstream>
#include <iomanip>
using namespace std;

int logfd;

struct tProp
{
    string filename;
    string clientip;
    string port;
    vector<int> chunks;
    bool complete=false;
    string path;
};



std::string GetHexRepresentation(const unsigned char * Bytes, size_t Length)
{
    std::ostringstream os;
    os.fill('0');
    os<<std::hex;
    for(const unsigned char * ptr=Bytes;ptr<Bytes+Length;ptr++)
        os<<std::setw(2)<<(unsigned int)*ptr;
    return os.str();
}

vector<struct tProp> readProp(string str){
    
    string clientip,port,filename;
    int idxstrt=0;
    int idxend=str.find("\n");
    vector<struct tProp> vec;
    while(idxend != string::npos){
        string strtemp = str.substr(idxstrt,idxend-idxstrt);
        int idx=strtemp.find(":");
        struct tProp tp;
        tp.clientip = strtemp.substr(0,idx);
        tp.port = strtemp.substr(idx+1);
        str.erase(idxstrt,idxend-idxstrt+1);
        idxstrt=0;
        idxend = str.find("\n");
        tp.filename = str.substr(idxstrt,idxend-idxstrt);
        str.erase(idxstrt,idxend-idxstrt+1);
        idxstrt=0;
        idxend = str.find("\n");
        vec.push_back(tp);
    }
    return vec;
}

string writeProp(vector<struct tProp> tp)
{
    string str = "";
    for (int i = 0; i < tp.size(); i++)
    {
        str += tp[i].clientip + ":" + tp[i].port + "\n";
        str += tp[i].filename + "\n";
    }
    
    return str;
}

void writeLog(string str){
    time_t now = time(0);
    string strnow=ctime(&now);
    str.insert(0,"\n"+strnow+":");
    str+="\n";
    write(logfd,str.c_str(),str.size());
}

struct sockaddr_in setAddr(string url){
    
    string ip,port;
    int idx=url.find(":");
    ip=url.substr(0,idx);
    
    port=url.substr(idx+1);
    struct sockaddr_in addr;
    memset(&addr, '0', sizeof(addr)); 

    addr.sin_family = AF_INET;
    addr.sin_port = htons(atoi(port.c_str())); 
    
    if(inet_pton(AF_INET, ip.c_str(), &addr.sin_addr)<=0)
    {
        cout<<"\n inet_pton error occured\n"<<ip<<":port"<<port;
    } 
    return addr;

}


int connectSocket(struct sockaddr_in addr){
    int sockfd=0;
    
     if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        writeLog("\n Error : Could not create socket \n");
        return -1;
    } 

    

    if( connect(sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
       writeLog("\n Error : Connect Failed \n");
       return -1;
    } 
    writeLog("connected to socket");
    return sockfd;
}

int listenServer(struct sockaddr_in myaddr){
    int server_fd;
    int opt = 1;
    
    
    // Creating socket file descriptor
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0)
    {
        cout << ("socket failed") << endl;
        exit(EXIT_FAILURE);
    }
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT,
                   &opt, sizeof(opt)))
    {
        cout << ("setsockopt");
        exit(EXIT_FAILURE);
    }

    if (bind(server_fd, (struct sockaddr *)&myaddr,
             sizeof(myaddr)) < 0)
    {
        cout << ("bind failed");
        exit(EXIT_FAILURE);
    }
    if (listen(server_fd, 50) < 0)
    {
        cout << ("listen");
        exit(EXIT_FAILURE);
    }
    return server_fd;
}

int updatetracker(struct sockaddr_in s1,struct sockaddr_in s2,struct sockaddr_in * s){
    if(s->sin_addr.s_addr == s1.sin_addr.s_addr && s->sin_port ==  s1.sin_port){
        *s=s2;
        return connectSocket(s2);                
    }
    else{
        *s=s1;
        return connectSocket(s1);
    }
}

