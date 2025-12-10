#include<winsock2.h>
#include<ws2tcpip.h>
#include<iostream>
#include<thread>
#include<string>
#include<windows.h>

using namespace std;

// 接收在线客户端列表
void onlineClients(SOCKET client) {
    string request = "GET_ONLINE";
    int ret = send(client,request.c_str(),request.size(),0);
    if(ret == SOCKET_ERROR) {
        cout << "Send failed:" << WSAGetLastError() << endl;
        return ;
    }
    char buffer[1024];
    ret = recv(client,buffer,sizeof(buffer),0);
    if(ret > 0) {
        string onlineClients(buffer,ret);
        cout << "online clients:" << onlineClients << endl;
    }
    else {
        cout << "Fail to get online clients list." << endl;
    }
}

// 接收消息线程
void receiveMessage(SOCKET client) {
    char buffer[1024];
    while(true) {
        int ret = recv(client,buffer,sizeof(buffer),0);
        if(ret > 0) {
            std::string msg(buffer,ret);
            std::cout << "\n[server] " << msg << std::endl;
        }
        else if(ret == 0) {
            std::cout << "Server closed connection." << std::endl;
            break;
        }
        else {
            std::cout << "Recv error:" << WSAGetLastError() << std::endl;
            break;
        }
    }
}

SOCKET reconnect(SOCKET client,sockaddr_in serverAddr) {
    SOCKET newclient = socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
    if(newclient == INVALID_SOCKET) {
        cout << "Socket recreaction failed:" << WSAGetLastError() << endl;
        return INVALID_SOCKET;
    }
    int attempts = 0;
    while(attempts <= 10) {
        attempts ++; Sleep(2000);
        if(connect(newclient,(sockaddr*)&serverAddr,sizeof(serverAddr)) == SOCKET_ERROR) continue;
        else {
            cout << "Reconnected to server!" << endl;
            return newclient;
        }
    }
    return INVALID_SOCKET;
}

int main() {
    // 初始化 winsock
    WSADATA ws;
    if (WSAStartup(MAKEWORD(2,2), &ws) != 0) {
        cout << "Winsock initialization failed!" << endl;
        return 1;
    }

    //创建 winsock
    SOCKET client = socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
    if(client == INVALID_SOCKET) {
        cout << "Socket creation failed:" << WSAGetLastError() << endl;
        WSACleanup();
        return 1;
    }

    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(9000);
    int rec = inet_pton(AF_INET,"127.0.0.1",&serverAddr.sin_addr);
    
    if(rec <= 0) {
        cout << "INvalid address" << endl;
        WSACleanup();
        return 1;
    }

    // 连接服务器
    if(connect(client,(sockaddr*)&serverAddr,sizeof(serverAddr)) == SOCKET_ERROR) {
        cout << "Connect failed:" << WSAGetLastError() << endl;
        WSACleanup();
        return 1;
    }
    cout << "Connected to server!" << endl;

    thread(receiveMessage,client).detach();

    string msg;
    while(true) {
        cout << "Enter Message" << endl;
        getline(cin,msg);
        if(msg == "exit") break;

        int ret = send(client,msg.c_str(),msg.size(),0);
        if(ret == SOCKET_ERROR) {
            cout << "Send failed:" << WSAGetLastError() << endl;
            SOCKET newclient = reconnect(client,serverAddr);
            if(newclient != INVALID_SOCKET) client = newclient;
            else {
                cout << "Failed to reconnect after 10 attempts." << endl;
                break;
            }
        }
    }

    closesocket(client);
    WSACleanup();
    system("pause");
    return 0;
}