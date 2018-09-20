// Server side C/C++ program to demonstrate Socket programming
#include <unistd.h>
#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <string.h>
#include <arpa/inet.h>
#include <iostream>
#include <thread>
#include <map>
#include <vector>
#include "common.cpp"
using namespace std;


map<string, vector<tProp>> mpTorrentList;
struct sockaddr_in myaddr;
struct sockaddr_in otheraddr;
string seedfilepath;



string writeSeederlist(string file,map<string,vector<tProp>> mp){
    
    string str="";
    for(auto it = mp.begin();it!=mp.end();it++){
        str+= "<h>"+(*it).first+"</h>\n";
        
        str+="<p>";
        str+=writeProp((*it).second);
        str+="</p>\n";
        
    }
    int fd = open(file.c_str(),O_CREAT|O_WRONLY|O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if(fd == -1){
        return str;
    }
    write(fd,str.c_str(),str.size());   
    close(fd);
}

map<string,vector<tProp>> getSeederlist(string str){
     map<string, vector<tProp>> mp;
      string hsh="<h>";
    string hshend = "</h>";
    string prop="<p>";
    string propend = "</p>";
    int idx=str.find(hsh);
    int idxend = str.find(hshend);
    string hash;
    while(idx != string::npos && idxend!=string::npos){
       
        hash=str.substr(idx+hsh.size(),idxend-idx-hsh.size());
        
        str.erase(idx,idxend+hshend.size()+1);
        idx=str.find(prop);
        idxend = str.find(propend);
        
        vector<struct tProp> vec = readProp(str.substr(idx+prop.size(),idxend-idx-prop.size()));
        str.erase(idx,idxend+propend.size()+1);
        idx=str.find(hsh);
        idxend = str.find(hshend);
        mp[hash]=vec;
    }
    
    return mp;
}

map<string, vector<tProp>> readSeederList(string file){
   
     map<string, vector<tProp>> mp;

    int fd = open(file.c_str(),O_RDONLY,0);
    if(fd == -1)
        return mp;
    string str = "";
    char buf [BUFSIZ];
    size_t size;
    while((size = read(fd,buf,BUFSIZ))>0){
        str+=(string)buf;
    }
    close(fd);
    return getSeederlist(str);
}





void Request(int socket)
{
    
    size_t size;
    char buffer[1024];
    bzero(buffer, 1024);
    string str;
    while (read(socket, buffer, 1024) > 0)
    {
        str = buffer;
        string command = str;
        int idx = str.find('\n');
        if (idx != -1)
            command = str.substr(0, idx);
        if (command == "share")
        {
            struct tProp tp;
            string hash = "";
            cout << ("share started.") << endl;
            string str2 = str;
            int idx2 = str.find('\n', idx + 1);
            if (idx2 != string::npos)
                str2 = str.substr(idx + 1, idx2 - idx - 1);
            cout << str2 << endl;
            cout.flush();
            int idx = str2.find(":");
            tp.clientip = str2.substr(0, idx);
            tp.port = str2.substr(idx + 1);
            string str3 = str;
            idx = str.find('\n', idx2 + 1);
            if (idx !=string::npos)
                str3 = str.substr(idx2 + 1, idx - idx2 - 1);
            cout << str3 << endl;
            tp.filename = str3;
            hash = str.substr(idx + 1);
            mpTorrentList[hash].push_back(tp);
            writeSeederlist(seedfilepath,mpTorrentList);
            cout << ("seeder list updated:") << mpTorrentList.size() << endl;
        }
        else if(command == "get"){
            cout << "get"<<endl;
            string hash = str.substr(idx+1);
            cout << hash <<endl;
            if(mpTorrentList.find(hash) == mpTorrentList.end()){
                send(socket,"-1",2,0);
                cout << "not found"<<endl;
            }
            else{
                string strsend=writeProp(mpTorrentList[hash]);
                string sz = to_string(strsend.size())+"\n";
                strsend.insert(0,sz);
                cout << "sent size"<<endl;
                send(socket,strsend.c_str(),strsend.size(),0);
            }
            cout << "get ends."<<endl;
        }
        bzero(buffer,1024);
    }
}

int othrtracker;
int main(int argc, char *argv[])
{
    int server_fd, new_socket, valread;
    
    char buffer[1024] = {0};

    if (argc != 5)
    {
        cout << "Invalid arguments:";
        return 1;
    }
   
    logfd=open(argv[4],O_CREAT|O_WRONLY|O_APPEND,S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    cout << "--started:tracker--" << endl;

    myaddr = setAddr(argv[1]);
    otheraddr = setAddr(argv[2]);

    seedfilepath=argv[3];

    mpTorrentList = readSeederList(seedfilepath);

    server_fd = listenServer(myaddr);
    int addrlen = sizeof(myaddr);
    
    vector<thread> vthread;
    int i = 0;
    cout << ("listening") << endl;
    while (1)
    {

        if ((new_socket = accept(server_fd, (struct sockaddr *)&myaddr, (socklen_t *)&addrlen)) < 0)
        {
            cout << ("accept");
            exit(EXIT_FAILURE);
        }

        vthread.push_back(thread(Request, new_socket));

        i++;
        if (i == 50)
        {
            break;
        }
    }

    for (int j = 0; j < 50; j++)
    {
        vthread[i].join();
    }
    //close(logfd);
    return 0;
}