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
#include "common.cpp"
#include <openssl/sha.h>
#include <vector>
#include <thread>
#include <map>

using namespace std;

string clientip, port;
struct sockaddr_in myaddr, trackeraddr1, trackeraddr2, currtracker;
int trackfd;
vector<thread> downthread;
map<string,tProp> downloads;
void share(string src, string dest)
{
    int fsrc = open(src.c_str(), O_RDONLY, 0);
    if (fsrc == -1)
    {
        cout << "FAILURE:FILE_NOT_FOUND" << endl;
        return;
    }
    int fdest = open(dest.c_str(), O_CREAT | O_WRONLY | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    int chunksize = 512 * 1024;
    
    size_t size;
    char charIP[INET_ADDRSTRLEN];
    string strdest;
    inet_ntop(AF_INET, &(trackeraddr1.sin_addr), charIP, INET_ADDRSTRLEN);
    string strIP = charIP;
    string strport = to_string(ntohs(trackeraddr1.sin_port));
    strdest = strIP + ":" + strport + "\n";
    inet_ntop(AF_INET, &(trackeraddr2.sin_addr), charIP, INET_ADDRSTRLEN);
    strIP = charIP;
    strport = to_string(ntohs(trackeraddr2.sin_port));
    strdest += strIP + ":" + strport + "\n";
    int idx = src.find_last_of('/');

    char bufread[1024];

    string strname = src;
    if (idx != -1)
    {
        strname = src.substr(idx + 1);
    }
    strdest += strname + "\n";
    //strdest += src + "\n";
    string str = "share\n" + clientip + ":" + port + "\n" + strname + "\n";

    struct stat st;
    stat(src.c_str(), &st);
    string strsize = to_string(st.st_size);
    strdest += "size:" + strsize + "\n";
    cout << strdest << endl;
    write(fdest, strdest.c_str(), strdest.size());
    string strtemp="";

    unsigned char buffer[chunksize+1];
    bzero(buffer,chunksize);
    while ((size = read(fsrc, buffer, chunksize)) > 0)
    {
        unsigned char hashbuf[SHA_DIGEST_LENGTH];
        buffer[chunksize]='\0';
        SHA1(buffer,strlen((char *)buffer), hashbuf);
        
        strtemp +=  GetHexRepresentation(hashbuf,SHA_DIGEST_LENGTH);
        bzero(buffer,chunksize);
       
    }
   
    cout << strtemp<<" "<<strtemp.size() << endl;
    write(fdest, strtemp.c_str(), strtemp.size()+1);

    unsigned char hoh[SHA_DIGEST_LENGTH];
    SHA1((unsigned char *)strtemp.c_str(),strtemp.size(),hoh);
    
    strtemp=GetHexRepresentation(hoh,SHA_DIGEST_LENGTH);
    cout << strtemp<<" "<<strtemp.size() << endl;
     str+=strtemp;
    size_t s = send(trackfd, str.c_str(), str.size()+1, 0);
    if (s == -1)
    {
        trackfd = updatetracker(trackeraddr1, trackeraddr2, &currtracker);
        send(trackfd, str.c_str(), str.size()+1, 0);
    }

    close(fsrc);
    close(fdest);
    struct tProp tp;
    tp.filename = strname;
    cout << tp.filename << endl;
    tp.path = src;
    tp.complete = true;
    tp.size = st.st_size;
    downloads[strtemp]=tp;
    writeLog("sharing to tracker compeleted.");
    cout << "SUCCESS:" + dest << endl;
}

void downloadfile(string hash,vector<struct tProp> vec,string dest){
    
    string str="down\n"+hash;
    struct tProp tp = vec[0];
    sockaddr_in addr = setAddr(tp.clientip+":"+tp.port);
    cout << tp.clientip+":"+tp.port<<endl;
    cout.flush();
    writeLog("download from:\n"+tp.clientip+":"+tp.port);
    int fd = connectSocket(addr);
    send(fd,str.c_str(),str.size(),0);
    char buffer[BUFSIZ];
    bzero(buffer,BUFSIZ);
    //int destfd = open(dest.c_str(),O_CREAT|O_WRONLY|O_TRUNC,S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    long long sz=-1;
    long long rcvsz = 0;
    string temp="";
    unsigned long size;
    int rc=recv(fd, (char *)&size, sizeof(size), 0);
        //size = atoi(buffer);
       writeLog("sIZEz:"+size);
    if(size == -1){
        cout << "FAILURE:M_TORRENT_FILE_NOT_FOUND"<<endl;
        return;
    }
        

    FILE * savefile = fopen(dest.c_str(), "w");
        

    int remain_data = size;
    ssize_t len;
    while ((remain_data > 0) && ((len = recv(fd, buffer, BUFSIZ, 0)) > 0) )
    {
        fwrite(buffer, sizeof(char), len, savefile);
        remain_data -= len;
        
    }
    
    fclose(savefile);
    cout << "SUCCESS:"<<dest<<endl;
}

void getlist(string torrent, string savefile)
{
    int fd = open(torrent.c_str(), O_RDONLY, 0);
    if (fd == -1)
    {
        cout << "FAILURE:M_TORRENT_FILE_NOT_FOUND";
        return;
    }
    char buffer[BUFSIZ];
    string strfile = "";
    size_t size;
    while ((size = read(fd, buffer, BUFSIZ)) > 0)
    {
        strfile += (string)buffer;
    }

    string hash = strfile.substr(strfile.find_last_of('\n') + 1);
    cout << hash.size()<<"h:"<<hash<<endl;
    

    unsigned char hoh[SHA_DIGEST_LENGTH];
    
    SHA1((unsigned char *)hash.c_str(),hash.size(),hoh);
    
    string strtemp= GetHexRepresentation(hoh,SHA_DIGEST_LENGTH);
    cout << strtemp <<" "<<strtemp.size()<< endl;
    string str = "get\n" + strtemp;

    size_t s = send(trackfd, str.c_str(), str.size()+1, 0);
    if (s == -1)
    {
        trackfd = updatetracker(trackeraddr1, trackeraddr2, &currtracker);
        send(trackfd, str.c_str(), str.size()+1, 0);
    }
    
    string rcv = "";
    char bufrcv[BUFSIZ];
    int strsize = 0;
    int szrcv = -1;
    while (szrcv < strsize && recv(trackfd, bufrcv, BUFSIZ, 0) > 0)
    {
        rcv += (string)bufrcv;
        if (szrcv == -1)
        {
            if (rcv == "-1")
            {
                cout << "FAILURE:M_TORRENT_FILE_NOT_FOUND";
                return;
            }
            int idx = rcv.find('\n');
            strsize = stoi(rcv.substr(0, idx));
            szrcv = 0;
            rcv.erase(0, idx+1);
        }
        
        szrcv += BUFSIZ;
    }
    if (rcv == "")
    {
        trackfd = updatetracker(trackeraddr1, trackeraddr2, &currtracker);
        while (szrcv < strsize && recv(trackfd, bufrcv, BUFSIZ, 0) > 0)
        {
            rcv += (string)bufrcv;
            if (szrcv == -1)
            {
                if (rcv == "-1")
                {
                    cout << "FAILURE:M_TORRENT_FILE_NOT_FOUND";
                    return;
                }
                int idx = rcv.find('\n');
                strsize = stoi(rcv.substr(0, idx));
                szrcv = 0;
                rcv.erase(0, idx+1);
            }
            
            szrcv += BUFSIZ;
        }
    }
    cout << "rcv" << endl;

    downthread.push_back(thread(downloadfile,strtemp,readProp(rcv),savefile));
    
}
