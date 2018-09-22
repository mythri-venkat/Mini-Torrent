
#include "share.cpp"
#include <thread>
#include <map>
#include <sys/sendfile.h>

using namespace std;



map<string,map<string,int>> mpconnections;

void downReq(int socket){
    size_t size;
    char buffer[1024];
    bzero(buffer, 1024);
    string str;
    while (read(socket, buffer, 1024) > 0)
    {
        //cout <<"buf:"<<buffer << endl;
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
                writeLog("notsent");
            }
            else{
                writeLog("sendstart");
                //cout <<  downloads[hash].path<<endl;
                string strfilepath=downloads[hash].path;
                int fd = open(strfilepath.c_str(),O_RDONLY,0);
                //writeLog("opn");
                if(fd == -1){
                    send(socket,"-1",2,0);
                    writeLog("notsent");
                }
                else{
                   
                    int sent_bytes=0;
                    
                    //string sz = to_string(downloads[hash].size);
                    send(socket,(char *)&downloads[hash].size,sizeof(downloads[hash].size), 0);
                    writeLog("sizesent:"+downloads[hash].size);
        
                    long int offset = 0;
                    int remain_data = downloads[hash].size;
                    while (((sent_bytes = sendfile(socket, fd, &offset, BUFSIZ)) > 0) && (remain_data > 0))
                    {
                
                        remain_data -= sent_bytes;
                        cout << remain_data << endl;
                
                    }
                    writeLog("send complete");

                    //shutdown(socket,0);
                    //close(socket);
                }
                
            }
        }
    }
    
}



void listenClient(){
    int new_socket;
    int server_fd = listenServer(myaddr);
    int addrlen = sizeof(myaddr);
    
    int i = 0;
    writeLog("listening");
    vector<thread> vthread;
    while (1)
    {

        if ((new_socket = accept(server_fd, (struct sockaddr *)&myaddr, (socklen_t *)&addrlen)) < 0)
        {
            writeLog("accept");
            exit(EXIT_FAILURE);
        }

        vthread.push_back(thread(downReq, new_socket));

        i++;
       
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

    currtracker = rand()%2 == 0?trackeraddr1:trackeraddr2;

    logfd = open(argv[4], O_CREAT | O_WRONLY | O_APPEND, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    writeLog("--log started--");
    string command;
    trackfd = connectSocket(currtracker);

    thread listen(listenClient);
    
    while (1)
    {
        cout << "\n1.share 2.get 3.show downloads 4.remove 5.close:\n";
        string strline="";
        getline(std::cin,strline);
        vector<string> args=getArgs(strline);
        command=args[0];
        if (command == "share")
        {
            if(args.size()==3)
            share(args[1], args[2]);
            else
            cout <<"ERR:INVALID_ARGUMENTS"<<endl;
        }
        else if(command ==  "get"){
            
            if(args.size()==3)
            getlist(args[1], args[2]);
            else
            cout <<"ERR:INVALID_ARGUMENTS"<<endl;
        }
        else if(strline == "show downloads"){
            
                cout << "Downloads"<<endl;
                for(auto it=downloads.begin();it != downloads.end();it++){
                    cout << ((*it).second.complete?("[S] "):("[D] "));
                    cout << (*it).second.filename << endl;
                }
            
        }
        else if(command == "remove"){
            if(args.size()==2)
            removeTorrent(args[1]);
            else
            cout <<"ERR:INVALID_ARGUMENTS"<<endl; 
        }
        else if(command == "close"){
            exit(0);
            break;
        }
    }
    listen.join();
    close(logfd);
    return 0;
}