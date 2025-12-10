#include<winsock2.h>
#include<ws2tcpip.h>
#include<windows.h>
#include<thread>
#include<mutex>
#include<iostream>
#include<vector>
#include<string>
#include<unordered_map>

std::vector<SOCKET> clients; // 客户端列表
std::unordered_map<std::string,SOCKET> table_cliname; // 名字映射
std::unordered_map<SOCKET,int> alive;
std::mutex clientMutex;

void broadcast(const std::vector<char>& msg) {
    std::lock_guard<std::mutex> lock(clientMutex);
    for(auto client : clients) {
        send(client,msg.data(),msg.size(),0);
    }
}

void REM(SOCKET client) {// 移出映射表 & 关闭 socket
    for(auto it = table_cliname.begin();it != table_cliname.end();it ++) {
        if(it -> second == client) {
            table_cliname.erase(it);
            break;
        }
    }
    for(std::vector<SOCKET>::iterator it = clients.begin();it != clients.end();++ it) {
        if(*it == client) {
            clients.erase(it); break;
        }
    }
    alive[client] = 0;
    closesocket(client);
    return ;
}

void receiveMessage(SOCKET client) {
    char buffer[1024];
    while(true) {
        int ret = recv(client,buffer,sizeof(buffer),0);
        std::string clientName = "user" + std::to_string(client);
        if(ret > 0) {
            std::string msg(buffer,ret);
            std::cout << "Received from client:" << msg << std::endl;
            // 列表获取
            if(msg == "GET_ONLINE" || msg == "get_online") {
                std::string onlineclients;
                {
                    std::lock_guard<std::mutex> lock(clientMutex);
                    for(SOCKET c:clients) {
                        onlineclients += std::to_string(c) + "\n";
                    }
                }
                send(client,onlineclients.c_str(),onlineclients.size(),0);
                continue;
            }
            // 名字
            if(msg.rfind("setname ",0) == 0) {
                std::string Name = msg.substr(8);
                if(Name.empty() || Name.find("user") == 0 || table_cliname.count(Name)) {
                    std::string nocite = "Invalid name,Your current name is " + clientName;
                    send(client,nocite.c_str(),nocite.size(),0);
                }
                else {
                    {
                        std::lock_guard<std::mutex> lock(clientMutex);
                        table_cliname.erase(clientName);
                    }
                    clientName = Name;
                    std::string notice = "Your name is change to " + clientName;
                    send(client,notice.c_str(),notice.size(),0);   
                    {
                        std::lock_guard<std::mutex> lock(clientMutex);
                        table_cliname[clientName] = client;
                    }         
                }
                continue;
            }
            /*if(msg.find("name:",0) || msg.find("NAME:",0) == 0) {
                std::string Name = msg.substr(5);
                if(Name.empty() || Name.find("user") == 0 || table_cliname.count(Name)) {
                    std::string nocite = "Your name if set to" + clientName;
                    send(client,nocite.c_str(),nocite.size(),0);
                }
                else {
                    clientName = Name;
                    std::string notice = "Welcome," + clientName;
                    send(client,notice.c_str(),notice.size(),0);            
                }
                {
                    std::lock_guard<std::mutex> lock(clientMutex);
                    table_cliname[clientName] = client;
                }
                return ;
            }*/
            // 发送指定消息：
            if(msg.find("TO:",0) == 0 || msg.find("to:",0) == 0) {
                size_t pos = msg.find(':',3);
                if(pos != std::string::npos) {
                    std::string targetName = msg.substr(3,pos - 3);
                    std::string message = msg.substr(pos + 1);
                    SOCKET targetsock = 0;
                    {
                        std::lock_guard<std::mutex> lock(clientMutex);
                        if(table_cliname.count(targetName)) targetsock = table_cliname[targetName]; 
                    }
                    if(targetsock != 0) {
                        std::string fullmsg = "[" + clientName + "]:" + message;
                        send(targetsock,fullmsg.c_str(),fullmsg.size(),0);
                    }
                    else {
                        std::string errormsg = "User " + targetName + "Not found.";
                        send(client,errormsg.c_str(),errormsg.size(),0);
                    }
                }
                continue;
            }
        }
        else if(ret <= 0) {
            std::cout << "Client disconnected." << std::endl;
            
            //closesocket(client);
            REM(client);
            break;
        }
    }
}

// 更新 client 在线列表。
void handleNewClient(SOCKET client) {
    if(alive[client]) return ;
    std::string clientName = "user" + std::to_string(client);
    {
        std::lock_guard<std::mutex> lock(clientMutex);
        // 将新的客户端和默认名称添加到 table_cliname 映射中
        table_cliname[clientName] = client;
    }
    std::cout << "New client connected:" << client << std::endl;
    alive[client] = 1;
}

int main() {
    WSADATA ws;
    int ret = WSAStartup(MAKEWORD(2,2),&ws); // 初始化 winsock
    if(ret != 0) {
        std::cout << "WSAStartup failed:" << ret << std::endl;
    }

    SOCKET server = socket(AF_INET,SOCK_STREAM,IPPROTO_TCP); // 创建 TCPsock
    if(server == INVALID_SOCKET) {
        // WSAGetLastError() 返回最后一个套接字的异常状态
        std::cout << "Socket creation failed:" << WSAGetLastError() << std::endl;
        WSACleanup();
        return 1;
    }

    std::cout << "Winsock initialized successfully!" << std::endl;

    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(9000); // 监听 9000 端口
    serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1");

    // 创建服务
    if(bind(server,(sockaddr*)&serverAddr,sizeof(serverAddr)) == SOCKET_ERROR) {
        std::cout << "Bind failed:" << WSAGetLastError() << std::endl;
        WSACleanup();
        return 1;
    }

    // 监听端口
    if(listen(server,SOMAXCONN) == SOCKET_ERROR) {
        std::cout << "Listen failed" << WSAGetLastError() << std::endl;
        closesocket(server);
        return 1;
    }

    std::cout << "server listening on port 9000..." << std::endl;   

    // 主线程
    while(true) {
        SOCKET client = accept(server,NULL,NULL);
        if(client != INVALID_SOCKET) {
            handleNewClient(client);
            std::lock_guard<std::mutex> lock(clientMutex);
            clients.push_back(client);
            std::thread(receiveMessage,client).detach();
        }
    }

    closesocket(server);
    WSACleanup();  // 释放 winsock
    return 0;
}