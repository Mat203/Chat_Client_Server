#include <iostream>
#include <thread>
#include <string>
#include <fstream>
#include <winsock2.h>
#include <Ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")

class FileHandler {
public:
	static void receiveFile(SOCKET clientSocket, const std::string& username)
	{
		char buffer[2048];
		std::string directoryPath = username + "/";
		std::string fileName = "received_file.txt";
		std::string fullPath = directoryPath + fileName;
		std::ofstream outputFile(fullPath, std::ios::binary);

		int totalSize;
		int bytesReceived = recv(clientSocket, reinterpret_cast<char*>(&totalSize), sizeof(int), 0);
		if (bytesReceived == SOCKET_ERROR || bytesReceived == 0) {
			std::cout << "Error in receiving total size." << std::endl;
			return;
		}

		int totalReceived = 0;
		while (totalReceived < totalSize)
		{
			bytesReceived = recv(clientSocket, buffer, sizeof(buffer), 0);
			if (bytesReceived > 0)
			{
				outputFile.write(buffer, bytesReceived);
				totalReceived += bytesReceived;
			}
			else
			{
				break;
			}
		}
		outputFile.close();
	}

	static void sendFile(SOCKET clientSocket, const std::string& username, const char* fileName)
	{
		char buffer[1024];
		std::string directoryPath = username + "/";
		std::string fullPath = directoryPath + fileName;
		std::ifstream inputFile(fullPath.c_str(), std::ios::binary);
		inputFile.seekg(0, std::ios::end);
		int totalSize = inputFile.tellg();
		inputFile.seekg(0, std::ios::beg);
		send(clientSocket, reinterpret_cast<char*>(&totalSize), sizeof(int), 0);
		int totalBytesSent = 0;
		while (inputFile)
		{
			inputFile.read(buffer, sizeof(buffer));
			int bytesRead = inputFile.gcount();
			if (bytesRead > 0)
			{
				send(clientSocket, buffer, bytesRead, 0);
				std::cout << buffer << std::endl;
				totalBytesSent += bytesRead;
				std::cout << "Sent " << bytesRead << " bytes, total: " << totalBytesSent << " bytes" << std::endl;
			}
		}
		inputFile.close();
	}
};

void receiveMessages(SOCKET clientSocket) {
	char buffer[4096];
	while (true) {
		int bytesReceived = recv(clientSocket, buffer, sizeof(buffer), 0);
		if (bytesReceived <= 0) {
			std::cerr << "Server disconnected.\n";
			break;
		}
		buffer[bytesReceived] = '\0';
		std::string message(buffer);
		if (message.rfind("Client ", 0) == 0 && message.find(" wants to send a file. Type /receivefile to accept.") != std::string::npos) {
			std::cout << message << std::endl;
			std::cout << "Do you want to receive the file? (yes/no): ";
			std::string answer;
			std::getline(std::cin, answer);
			if (answer == "yes") {
				send(clientSocket, "/receivefile", 12, 0);
				FileHandler::receiveFile(clientSocket, "Client_" + std::to_string(clientSocket));
			}
		}
		else {
			std::cout << buffer << std::endl;
		}
	}
}

int main() {
	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
		std::cerr << "WSAStartup failed.\n";
		return 1;
	}
	SOCKET clientSocket = socket(AF_INET, SOCK_STREAM, 0);
	if (clientSocket == INVALID_SOCKET) {
		std::cerr << "Socket creation failed.\n";
		WSACleanup();
		return 1;
	}
	sockaddr_in serverAddr;
	serverAddr.sin_family = AF_INET;
	InetPton(AF_INET, L"127.0.0.1", &serverAddr.sin_addr);
	serverAddr.sin_port = htons(8080);
	if (connect(clientSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) ==
		SOCKET_ERROR) {
		std::cerr << "Connection failed.\n";
		closesocket(clientSocket);
		WSACleanup();
		return 1;
	}
	std::cout << "Connected to server.\n";
	std::cout << "Enter room ID: ";
	std::string roomId;
	std::getline(std::cin, roomId);
	send(clientSocket, roomId.c_str(), roomId.size() + 1, 0);
	// Start a thread to receive messages from the server
	std::thread receiveThread(receiveMessages, clientSocket);
	// Main thread to send messages to the server
	std::string message;
	while (true) {
		std::getline(std::cin, message);
		if (message.rfind("/sendfile ", 0) == 0) {
			std::string fileName = message.substr(10);
			send(clientSocket, message.c_str(), message.size() + 1, 0);
			FileHandler::sendFile(clientSocket, "Client_" + std::to_string(clientSocket), fileName.c_str());
		}
		else {
			send(clientSocket, message.c_str(), message.size() + 1, 0);
		}
	}
	// Close the socket and clean up
	closesocket(clientSocket);
	WSACleanup();
	return 0;
}