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
#include <unordered_set>
#include "common.cpp"
using namespace std;

map<string, vector<tProp>> mpTorrentList;
struct sockaddr_in myaddr;
struct sockaddr_in otheraddr;
string seedfilepath;
int othrtracker = -1;

enum seedCommand
{
    setseed = 253,
    getseed = 255,
    rmtor = 234
};

string writeSeederlist(string file, map<string, vector<tProp>> mp)
{

    string str = "";
    for (auto it = mp.begin(); it != mp.end(); it++)
    {
        str += "<h>" + (*it).first + "</h>\n";

        str += "<p>";
        str += writeProp((*it).second);
        str += "</p>\n";
    }
    int fd = open(file.c_str(), O_CREAT | O_WRONLY | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if (fd == -1)
    {
        return str;
    }
    write(fd, str.c_str(), str.size());
    close(fd);
    return str;
}

map<string, vector<tProp>> getSeederlist(string str)
{
    map<string, vector<tProp>> mp;
    string hsh = "<h>";
    string hshend = "</h>";
    string prop = "<p>";
    string propend = "</p>";
    int idx = str.find(hsh);
    int idxend = str.find(hshend);
    string hash;
    while (idx != string::npos && idxend != string::npos)
    {

        hash = str.substr(idx + hsh.size(), idxend - idx - hsh.size());

        str.erase(idx, idxend + hshend.size() + 1);
        idx = str.find(prop);
        idxend = str.find(propend);

        vector<struct tProp> vec = readProp(str.substr(idx + prop.size(), idxend - idx - prop.size()));
        str.erase(idx, idxend + propend.size() + 1);
        idx = str.find(hsh);
        idxend = str.find(hshend);
        mp[hash] = vec;
    }

    return mp;
}

map<string, vector<tProp>> readSeederList(string file)
{

    map<string, vector<tProp>> mp;

    int fd = open(file.c_str(), O_RDONLY, 0);
    if (fd == -1)
        return mp;
    string str = "";
    char buf[BUFSIZ];
    size_t size;
    while ((size = read(fd, buf, BUFSIZ)) > 0)
    {
        str += (string)buf;
    }
    close(fd);
    return getSeederlist(str);
}

void sendSeedlist(string seed)
{
    int cmd = setseed;
    int sz = send(othrtracker, (char *)&cmd, sizeof(cmd), 0);

    cmd = seed.size();
    sz = send(othrtracker, (char *)&cmd, sizeof(cmd), 0);
    sz = send(othrtracker, seed.c_str(), seed.size() + 1, 0);
}

void Request(int socket)
{

    size_t size;
    char buffer[1024];
    bzero(buffer, 1024);
    string str;
    while (read(socket, buffer, 1024) > 0)
    {
        //cout << "buf:" << buffer << endl;
        str = buffer;
        string command = str;
        int idx = str.find('\n');
        if (idx != -1)
            command = str.substr(0, idx);
        if (command == "share")
        {
            struct tProp tp;
            string hash = "";
            writeLog("share started.");
            string str2 = str;
            int idx2 = str.find('\n', idx + 1);
            if (idx2 != string::npos)
                str2 = str.substr(idx + 1, idx2 - idx - 1);
            //cout << str2 << endl;
            //cout.flush();
            int idx = str2.find(":");
            tp.clientip = str2.substr(0, idx);
            tp.port = str2.substr(idx + 1);
            string str3 = str;
            idx = str.find('\n', idx2 + 1);
            if (idx != string::npos)
                str3 = str.substr(idx2 + 1, idx - idx2 - 1);
            //cout << str3 << endl;
            tp.filename = str3;
            hash = str.substr(idx + 1);
            mpTorrentList[hash].push_back(tp);
            string seed = writeSeederlist(seedfilepath, mpTorrentList);
            writeLog("seeder list updated:" +mpTorrentList.size());

            sendSeedlist(seed);
        }
        else if (command == "get")
        {
            writeLog("Get");
            string hash = str.substr(idx + 1);
            //cout << GetHexRepresentation((unsigned char *)hash.c_str(), hash.size()) << endl;
            if (mpTorrentList.find(hash) == mpTorrentList.end())
            {
                send(socket, "-1", 2, 0);
                writeLog("not found");
            }
            else
            {
                string strsend = writeProp(mpTorrentList[hash]);
                string sz = to_string(strsend.size()) + "\n";
                strsend.insert(0, sz);
                writeLog("sent size");
                send(socket, strsend.c_str(), strsend.size(), 0);
            }
            writeLog("get ends.");
        }
        else if (command == "remove")
        {
            writeLog("remove");
            string hash = str.substr(idx + 1);
            int num = mpTorrentList.erase(hash);
            int stat=1;
            if (num <= 0)
            {
                stat=-1;
                send(socket,(char *)&stat,sizeof(stat),0);
                continue;
            }
            writeSeederlist(seedfilepath, mpTorrentList);
            int trcmd = rmtor;
            int st = send(othrtracker, (char *)&trcmd, sizeof(trcmd), 0);
            st = send(othrtracker, hash.c_str(), hash.size() + 1, 0);
            send(socket,(char *)&stat,sizeof(stat),0);
            writeLog("removed successfully");
        }
        else if (command == "tracker")
        {
            if (othrtracker == -1)
            {
                writeLog("connected to other tracker");
                othrtracker = socket;
                //trackerComm(socket);
                break;
            }
            //othrtracker = -1;
        }
        bzero(buffer, 1024);
    }
}

void connTrack()
{
    while (1)
    {
       writeLog("connecting to tracker");

        while (othrtracker == -1)
        {
            othrtracker = connectSocket(otheraddr);
            if (othrtracker == -1)
                sleep(2);
        }
        writeLog( "conn established:" );
        int sz = send(othrtracker, "tracker\0", 8, 0);
        //cout << "sz:" << sz << endl;

        int cmd = getseed;
        sz = send(othrtracker, (char *)&cmd, sizeof(cmd), 0);
        //cout << "sz:" << sz;

        int encmd;
        while (read(othrtracker, (char *)&encmd, sizeof(encmd)) > 0)
        {
            //cout << "cmd:" << encmd << endl;

            if (encmd == setseed)
            {
                writeLog("receiving seedlist");
                char buffer[BUFSIZ];
                bzero(buffer, BUFSIZ);
                string str;
                int size = 0;
                recv(othrtracker, (char *)&size, sizeof(size), 0);
                //cout << "size:" << size << endl;
                size_t len = 0;
                while (size > 0 && (len = recv(othrtracker, buffer, BUFSIZ, 0)) > 0)
                {
                    str += (string)buffer;
                    size -= len;
                }
                //cout << "str:" << str << endl;
                if (str == "" && mpTorrentList.size() != 0)
                {
                    string seed = writeSeederlist("", mpTorrentList);
                    sendSeedlist(seed);
                    continue;
                }
                bool update = false;
                map<string, vector<tProp>> mp = getSeederlist(str);
                for (auto it = mp.begin(); it != mp.end(); it++)
                {
                    if (mpTorrentList.find((*it).first) == mpTorrentList.end())
                    {
                        mpTorrentList[(*it).first] = (*it).second;
                        update = true;
                    }
                    else
                    {
                        vector<tProp> vec = mpTorrentList[(*it).first];
                        unordered_set<string> setip;
                        for (int i = 0; i < vec.size(); i++)
                        {
                            setip.insert(vec[i].clientip + ":" + vec[i].port);
                        }
                        vec = (*it).second;
                        for (int j = 0; j < vec.size(); j++)
                        {
                            if (setip.find(vec[j].clientip + ":" + vec[j].port) == setip.end())
                            {
                                mpTorrentList[(*it).first].push_back(vec[j]);
                                update = true;
                            }
                        }
                    }
                }
                if (update)
                {
                    writeSeederlist(seedfilepath, mpTorrentList);
                }
                writeLog("updated seedlist");
            }
            else if (encmd == getseed)
            {
                string seed = writeSeederlist("", mpTorrentList);
                sendSeedlist(seed);
                writeLog( "sent seederlist:" );
            }
            else if (encmd == rmtor)
            {
                writeLog("remove entry");
                string hash = "";
                int len = 0;
                char buffer[41];
                bzero(buffer, 40);
                while ((len = recv(othrtracker, buffer, 41, 0)) < 41)
                {
                    hash += (string)buffer;
                    bzero(buffer, 40);
                }
                if (hash.size() == 40)
                {
                    int num = mpTorrentList.erase(hash);
                    if (num <= 0)
                        continue;
                    writeSeederlist(seedfilepath, mpTorrentList);
                }
                writeLog("remove completed");
            }
        }
        othrtracker = -1;
    }
}

int main(int argc, char *argv[])
{
    int server_fd, new_socket, valread;
    blog = false;
    char buffer[1024] = {0};

    if (argc != 5)
    {
        cout << "Invalid arguments:";
        return 1;
    }

    logfd = open(argv[4], O_CREAT | O_WRONLY | O_APPEND, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    cout << "--started:tracker--" << endl;
    writeLog("--tracker log started--");
    myaddr = setAddr(argv[1]);
    otheraddr = setAddr(argv[2]);

    seedfilepath = argv[3];

    mpTorrentList = readSeederList(seedfilepath);

    server_fd = listenServer(myaddr);
    int addrlen = sizeof(myaddr);

    thread thrdtrackerconn(connTrack);

    vector<thread> vthread;
    int i = 0;
    cout << ("listening") << endl;
    while (1)
    {

        if ((new_socket = accept(server_fd, (struct sockaddr *)&myaddr, (socklen_t *)&addrlen)) < 0)
        {
            //cout << ("accept");
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