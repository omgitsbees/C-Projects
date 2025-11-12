#include <iostream>
#include <thread>
#include <string>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
   
class ChatClient {
private:
    int client_socket;
    bool connected;
    std::string server_ip;
    int server_port;
    
    const int BUFFER_SIZE = 1024;

public:
    ChatClient(const std::string& ip = "127.0.0.1", int port = 8080) 
        : connected(false), server_ip(ip), server_port(port) {}
    
    ~ChatClient() {
        disconnect();
    }
    
    bool connect() {
        // Create socket
        client_socket = socket(AF_INET, SOCK_STREAM, 0);
        if (client_socket == -1) {
            std::cerr << "Failed to create socket\n";
            return false;
        }
        
        // Server address
        sockaddr_in server_addr{};
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(server_port);
        
        if (inet_pton(AF_INET, server_ip.c_str(), &server_addr.sin_addr) <= 0) {
            std::cerr << "Invalid server IP address\n";
            close(client_socket);
            return false;
        }
        
        // Connect to server
        if (::connect(client_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
            std::cerr << "Failed to connect to server\n";
            close(client_socket);
            return false;
        }
        
        connected = true;
        std::cout << "Connected to chat server at " << server_ip << ":" << server_port << std::endl;
        
        return true;
    }
    
    void receiveMessages() {
        char buffer[BUFFER_SIZE];
        
        while (connected) {
            int bytes_received = recv(client_socket, buffer, BUFFER_SIZE - 1, 0);
            
            if (bytes_received <= 0) {
                if (connected) {
                    std::cout << "\nDisconnected from server\n";
                    connected = false;
                }
                break;
            }
            
            buffer[bytes_received] = '\0';
            std::cout << buffer;
            std::cout.flush();
        }
    }
    
    void sendMessages() {
        std::string message;
        
        while (connected && std::getline(std::cin, message)) {
            if (message == "/quit") {
                std::cout << "Disconnecting...\n";
                connected = false;
                break;
            }
            
            if (message == "/help") {
                std::cout << "\nChat Commands:\n";
                std::cout << "/quit - Disconnect from server\n";
                std::cout << "/help - Show this help message\n\n";
                continue;
            }
            
            if (!message.empty()) {
                message += "\n";
                if (send(client_socket, message.c_str(), message.length(), 0) < 0) {
                    std::cerr << "Failed to send message\n";
                    connected = false;
                    break;
                }
            }
        }
    }
    
    void disconnect() {
        if (connected) {
            connected = false;
            close(client_socket);
        }
    }
    
    void run() {
        if (!connect()) {
            return;
        }
        
        std::cout << "\nChat Commands:\n";
        std::cout << "/quit - Disconnect from server\n";
        std::cout << "/help - Show help message\n";
        std::cout << "\nJoining chat...\n\n";
        
        // Start receiving messages in a separate thread
        std::thread receive_thread(&ChatClient::receiveMessages, this);
        
        // Handle sending messages in the main thread
        sendMessages();
        
        // Clean up
        disconnect();
        if (receive_thread.joinable()) {
            receive_thread.join();
        }
    }
    
    bool isConnected() const {
        return connected;
    }
};

int main(int argc, char* argv[]) {
    std::string server_ip = "127.0.0.1";
    int server_port = 8080;
    
    // Parse command line arguments
    if (argc >= 2) {
        server_ip = argv[1];
    }
    if (argc >= 3) {
        server_port = std::stoi(argv[2]);
    }
    
    std::cout << "=== Chat Client ===" << std::endl;
    std::cout << "Connecting to " << server_ip << ":" << server_port << "..." << std::endl;
    
    ChatClient client(server_ip, server_port);
    client.run();
    
    std::cout << "Chat client terminated.\n";
    return 0;

}
