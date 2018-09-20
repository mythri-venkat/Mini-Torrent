
#include "share.cpp"
#include <thread>
#include <map>

using namespace std;



map<string,map<string,int>> mpconnections;

void downReq(int socket){
    size_t size;
    char buffer[1024];
    bzero(buffer, 1024);
    string str;
    while (read(socket, buffer, 1024) > 0)
    {
        cout <<"buf:"<<buffer << endl;
        str = buffer;
        string command = str;
        int idx = str.find('\n');
        if (idx != -1)
            command = str.substr(0, idx);
        if (command == "down")
        {
            string hash = str.substr(idx+1);
            if(downloads.find(hash) == downloads.end()){
                send(socket,"-1",2,0);
            }
            else{
                writeLog("sendstart");
                string strfilepath=downloads[hash].path;
                int fd = open(strfilepath.c_str(),O_RDONLY,0);
                if(fd == -1){
                    send(socket,"-1",2,0);
                }
                else{
                    char buffer[BUFSIZ];
                    string strfile="";
                    bzero(buffer,BUFSIZ);
                    while(read(fd,buffer,BUFSIZ)>0){
                        strfile+=(string)buffer;
                        bzero(buffer,BUFSIZ);
                    }
                    strfile.insert(0,to_string(strfile.size())+"\n");
                    cout << "filesie:"<<strfile.size()<<endl;
                    //cout << "s:"<<strfile.substr(0,20)<<endl;
                    send(socket,strfile.c_str(),strfile.size(),0);
                    cout << "sent"<<endl;
                    close(fd);
                }
                
            }
        }
    }

}



void listenClient(){
    int new_socket;
    int server_fd = listenServer(myaddr);
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

        vthread.push_back(thread(downReq, new_socket));

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
}




int main(int argc, char *argv[])
{
    int sockfd = 0, n = 0;
    char recvBuff[1024];

    if (argc != 5)
    {
        cout << "Invalid arguments";
        return 1;
    }
    string url = argv[1];
    int idx = url.find(":");
    clientip = url.substr(0, idx);
    port = url.substr(idx + 1);
    myaddr = setAddr(argv[1]);
    trackeraddr1 = setAddr(argv[2]);
    trackeraddr2 = setAddr(argv[3]);

    currtracker = trackeraddr1;

    logfd = open(argv[4], O_CREAT | O_WRONLY | O_APPEND, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    writeLog("--log started--");
    string command;
    trackfd = connectSocket(currtracker);

    thread listen(listenClient);
    
    while (1)
    {
        cout << "\n1.share 2.get 3.close:\n";

        cin >> command;
        if (command == "share")
        {
            string filepath, torrentpath;
            cin >> filepath >> torrentpath;
            share(filepath, torrentpath);
        }
        else if(command ==  "get"){
            
            string torrent, savefile;
            cin >> torrent >> savefile;
            getlist(torrent,savefile);
        }
        else if(command == "close"){
            break;
        }
    }
    close(logfd);
    return 0;
}