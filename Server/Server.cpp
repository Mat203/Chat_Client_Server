#include <iostream>
#include <thread>
#include <fstream>
#include <vector>
#include <string>
#include <mutex>
#include <map>
#include <queue>
#include <direct.h>
#include <winsock2.h>
#pragma comment(lib, "ws2_32.lib")
std::mutex consoleMutex;
std::map<std::string, std::vector<SOCKET>> rooms;

struct Message {
    std::string content;
    SOCKET senderSocket;
    std::string roomId;
};

class FileHandler {
public:
    static void receiveFile(SOCKET clientSocket, const std::string& username) {
        char buffer[2048];
        std::string directoryPath = username + "/";
        std::string fileName = "received_file.txt";
        std::string fullPath = directoryPath + fileName;
        std::ofstream outputFile(fullPath, std::ios::binary);

        int totalSize;
        int bytesReceived = recv(clientSocket, reinterpret_cast<char*>(&totalSize), sizeof(int), 0);
        if (bytesReceived == SOCKET_ERROR || bytesReceived == 0) {
            std::cerr << "Error in receiving total size." << std::endl;
            return;
        }

        int totalReceived = 0;
        while (totalReceived < totalSize) {
            bytesReceived = recv(clientSocket, buffer, sizeof(buffer), 0);
            if (bytesReceived > 0) {
                outputFile.write(buffer, bytesReceived);
                totalReceived += bytesReceived;
            }
            else {
                std::cerr << "Error in receiving file data." << std::endl;
                break;
            }
        }
        outputFile.close();
    }

    static void sendFile(SOCKET clientSocket, const std::string& username, const std::string& fileName) {
        char buffer[1024];
        std::string directoryPath = username + "/";
        std::string fullPath = directoryPath + fileName;
        std::cout << fullPath << std::endl;
        std::ifstream inputFile(fullPath.c_str(), std::ios::binary);
        if (!inputFile) {
            std::cerr << "Error in opening file." << std::endl;
            return;
        }
        inputFile.seekg(0, std::ios::end);
        int totalSize = inputFile.tellg();
        inputFile.seekg(0, std::ios::beg);
        send(clientSocket, reinterpret_cast<char*>(&totalSize), sizeof(int), 0);
        int totalBytesSent = 0;
        while (inputFile) {
            inputFile.read(buffer, sizeof(buffer));
            int bytesRead = inputFile.gcount();
            if (bytesRead > 0) {
                send(clientSocket, buffer, bytesRead, 0);
                totalBytesSent += bytesRead;
            }
        }
        inputFile.close();
    }
};


class ChatServer {
private:
    std::map<std::string, std::vector<SOCKET>> rooms;
    std::mutex consoleMutex;
    std::map<std::string, std::pair<SOCKET, std::string>> fileTransfers;
    std::mutex messageQueueMutex;
    std::mutex roomsMutex;
    std::condition_variable messageAvailableCondition;
    std::queue<Message> messageQueue;

public:
    void addMessageToQueue(const Message& message) {
        {
            std::lock_guard<std::mutex> lock(messageQueueMutex);
            messageQueue.push(message);
        }
        messageAvailableCondition.notify_one();
    }

    void broadcastMessages() {
        while (true) {
            std::unique_lock<std::mutex> lock(messageQueueMutex);
            messageAvailableCondition.wait(lock, [this] { return !messageQueue.empty(); });

            while (!messageQueue.empty()) {
                Message message = messageQueue.front();
                messageQueue.pop();

                std::string fullMessage = "Client " + std::to_string(message.senderSocket) + ": " + message.content;
                std::cout << fullMessage << std::endl;
                for (SOCKET client : rooms[message.roomId]) {
                    if (client != message.senderSocket) {
                        send(client, fullMessage.c_str(), fullMessage.size() + 1, 0);
                    }
                }
            }
        }
    }


    void broadcastMessage(const std::string& message, SOCKET senderSocket, const std::string& roomId) {
        std::lock_guard<std::mutex> lock(consoleMutex);
        std::string fullMessage = "Client " + std::to_string(senderSocket) + ": " + message;
        std::cout << fullMessage << std::endl;
        for (SOCKET client : rooms[roomId]) {
            if (client != senderSocket) {
                send(client, fullMessage.c_str(), fullMessage.size() + 1, 0);
            }
        }
    }

    void broadcastFile(const std::string& filename, SOCKET senderSocket, const std::string& roomId) {
        std::lock_guard<std::mutex> lock(consoleMutex);
        std::string senderDirectory = "Client_" + std::to_string(senderSocket);
        for (SOCKET client : rooms[roomId]) {
            if (client != senderSocket) {
                std::string receiverDirectory = "Client_" + std::to_string(client);

                std::string usernameMessage = "/username " + receiverDirectory;
                std::cout << usernameMessage << std::endl;
                send(client, usernameMessage.c_str(), usernameMessage.size() + 1, 0);

                std::string fileTransferRequest = "User " + senderDirectory + " wants to send a file. Type /receivefile to accept the file or /declinefile.";
                send(client, fileTransferRequest.c_str(), fileTransferRequest.size() + 1, 0);
            }
        }
    }



    void handleClient(SOCKET clientSocket) {
        char buffer[4096];
        int bytesReceived = recv(clientSocket, buffer, sizeof(buffer), 0);
        if (bytesReceived <= 0) {
            std::cerr << "Failed to get room ID from client.\n";
            return;
        }
        buffer[bytesReceived] = '\0';
        std::string roomId(buffer);

        rooms[roomId].push_back(clientSocket);

        std::string joinMessage = "joined the room.";
        broadcastMessage(joinMessage, clientSocket, roomId);

        std::string dirName = "Client_" + std::to_string(clientSocket);
        _mkdir(dirName.c_str());

        while (true) {
            bytesReceived = recv(clientSocket, buffer, sizeof(buffer), 0);
            if (bytesReceived <= 0) {
                std::lock_guard<std::mutex> lock(consoleMutex);
                std::cout << "Client " << clientSocket << " disconnected.\n";
                break;
            }
            buffer[bytesReceived] = '\0';
            std::string message(buffer);
            std::string filename;

            if (message.rfind("/sendfile ", 0) == 0) {
                std::string filename = message.substr(10);
                std::string senderDirectory = "Client_" + std::to_string(clientSocket); 
                fileTransfers[roomId] = std::make_pair(clientSocket, filename); 
                broadcastFile(filename, clientSocket, roomId);
            }
            else if (message.rfind("/receivefile", 0) == 0) {
                std::cout << "111" << std::endl;
                std::cout << fileTransfers.count(roomId) << std::endl;
                if (fileTransfers.count(roomId) > 0) { 
                    std::cout << "222" << std::endl;
                    std::pair<SOCKET, std::string> fileTransfer = fileTransfers[roomId];
                    std::string senderDirectory = "Client_" + std::to_string(fileTransfer.first);
                    std::cout << senderDirectory << fileTransfer.second << std::endl;
                    std::string usernameMessage = "/username " + senderDirectory;
                    std::cout << usernameMessage << std::endl;
                    send(clientSocket, "/receivefile", 13, 0);
                    FileHandler::sendFile(clientSocket, senderDirectory, fileTransfer.second);
                }
            }
            else if (message.rfind("/rejoin ", 0) == 0) {
                std::string newRoomId = message.substr(8);

                std::lock_guard<std::mutex> lock(roomsMutex);
                rooms[roomId].erase(std::remove(rooms[roomId].begin(), rooms[roomId].end(), clientSocket), rooms[roomId].end());

                roomId = newRoomId;
                rooms[roomId].push_back(clientSocket);

                joinMessage = "Client " + std::to_string(clientSocket) + " joined the room.";
                broadcastMessage(joinMessage, clientSocket, roomId);
            }
            else if (message.rfind("/declinefile", 0) == 0) {
                if (fileTransfers.count(roomId) > 0) {
                    fileTransfers.erase(roomId);
                    std::string declineMessage = "Client declined the file transfer.";
                    broadcastMessage(declineMessage, clientSocket, roomId);
                }
            }
            else if (message.rfind("/", 0) == 0) {
                if (fileTransfers.count(roomId) > 0) {
                    fileTransfers.erase(roomId);
                    std::string declineMessage = "Unknown Command.";
                    broadcastMessage(declineMessage, clientSocket, roomId);
                }
            }
            else {
                broadcastMessage(message, clientSocket, roomId);
            }
        }
        closesocket(clientSocket);
    }

    void start() {
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            std::cerr << "WSAStartup failed.\n";
            return;
        }
        SOCKET serverSocket = socket(AF_INET, SOCK_STREAM, 0);
        if (serverSocket == INVALID_SOCKET) {
            std::cerr << "Socket creation failed.\n";
            WSACleanup();
            return;
        }
        sockaddr_in serverAddr;
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_addr.s_addr = INADDR_ANY;
        serverAddr.sin_port = htons(8080);
        if (bind(serverSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) ==
            SOCKET_ERROR) {
            std::cerr << "Bind failed.\n";
            closesocket(serverSocket);
            WSACleanup();
            return;
        }
        if (listen(serverSocket, SOMAXCONN) == SOCKET_ERROR) {
            std::cerr << "Listen failed.\n";
            closesocket(serverSocket);
            WSACleanup();
            return;
        }
        std::cout << "Server is listening on port 8080...\n";
        while (true) {
            SOCKET clientSocket = accept(serverSocket, nullptr, nullptr);
            if (clientSocket == INVALID_SOCKET) {
                std::cerr << "Accept failed.\n";
                closesocket(serverSocket);
                WSACleanup();
                return;
            }
            std::lock_guard<std::mutex> lock(consoleMutex);
            std::cout << "Client " << clientSocket << " connected.\n";
            std::thread clientThread(&ChatServer::handleClient, this, clientSocket);
            clientThread.detach(); // Detach the thread to allow handling multiple clients concurrently
        }
        closesocket(serverSocket);
        WSACleanup();
    }
};

int main() {
    ChatServer server;
    server.start();
    return 0;
}
