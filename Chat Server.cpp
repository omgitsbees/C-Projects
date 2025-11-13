#include <iostream>
#include <thread>
#include <vector>
#include <string>
#include <mutex>
#include <algorithm>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
  
class ChatServer {
private:
    int server_socket;
    std::vector<int> client_sockets;
    std::vector<std::string> client_names;
    std::mutex clients_mutex;
    bool running;
    
    const int PORT = 8080;
    const int MAX_CLIENTS = 10;
    const int BUFFER_SIZE = 1024;

public:
    ChatServer() : running(false) {}
    
    ~ChatServer() {
        stop();
    }
    
    bool start() {
        // Create socket
        server_socket = socket(AF_INET, SOCK_STREAM, 0);
        if (server_socket == -1) {
            std::cerr << "Failed to create socket\n";
            return false;
        }
        
        // Set socket options to reuse address
        int opt = 1;
        if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
            std::cerr << "Failed to set socket options\n";
            close(server_socket);
            return false;
        }
        
        // Bind socket
        sockaddr_in server_addr{};
        server_addr.sin_family = AF_INET;
        server_addr.sin_addr.s_addr = INADDR_ANY;
        server_addr.sin_port = htons(PORT);
        
        if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
            std::cerr << "Failed to bind socket\n";
            close(server_socket);
            return false;
        }
        
        // Listen for connections
        if (listen(server_socket, MAX_CLIENTS) < 0) {
            std::cerr << "Failed to listen on socket\n";
            close(server_socket);
            return false;
        }
        
        running = true;
        std::cout << "Chat server started on port " << PORT << std::endl;
        std::cout << "Waiting for clients to connect...\n";
        
        return true;
    }
    
    void acceptClients() {
        while (running) {
            sockaddr_in client_addr{};
            socklen_t client_len = sizeof(client_addr);
            
            int client_socket = accept(server_socket, (struct sockaddr*)&client_addr, &client_len);
            
            if (client_socket < 0) {
                if (running) {
                    std::cerr << "Failed to accept client connection\n";
                }
                continue;
            }
            
            std::cout << "New client connected: " << inet_ntoa(client_addr.sin_addr) << std::endl;
            
            // Start a new thread to handle this client
            std::thread client_thread(&ChatServer::handleClient, this, client_socket);
            client_thread.detach();
        }
    }
    
    void handleClient(int client_socket) {
        char buffer[BUFFER_SIZE];
        std::string client_name;
        
        // Get client name
        send(client_socket, "Enter your name: ", 17, 0);
        int bytes_received = recv(client_socket, buffer, BUFFER_SIZE - 1, 0);
        if (bytes_received <= 0) {
            close(client_socket);
            return;
        }
        
        buffer[bytes_received] = '\0';
        client_name = std::string(buffer);
        
        // Remove newline if present
        if (!client_name.empty() && client_name.back() == '\n') {
            client_name.pop_back();
        }
        if (!client_name.empty() && client_name.back() == '\r') {
            client_name.pop_back();
        }
        
        // Add client to the list
        {
            std::lock_guard<std::mutex> lock(clients_mutex);
            client_sockets.push_back(client_socket);
            client_names.push_back(client_name);
        }
        
        // Announce new user
        std::string join_msg = client_name + " joined the chat!\n";
        broadcastMessage(join_msg, client_socket);
        
        // Send welcome message
        std::string welcome = "Welcome to the chat, " + client_name + "!\n";
        send(client_socket, welcome.c_str(), welcome.length(), 0);
        
        // Handle messages from this client
        while (running) {
            bytes_received = recv(client_socket, buffer, BUFFER_SIZE - 1, 0);
            
            if (bytes_received <= 0) {
                break; // Client disconnected
            }
            
            buffer[bytes_received] = '\0';
            std::string message = std::string(buffer);
            
            // Remove newline if present
            if (!message.empty() && message.back() == '\n') {
                message.pop_back();
            }
            if (!message.empty() && message.back() == '\r') {
                message.pop_back();
            }
            
            if (!message.empty()) {
                std::string full_message = client_name + ": " + message + "\n";
                broadcastMessage(full_message, client_socket);
                std::cout << client_name << ": " << message << std::endl;
            }
        }
        
        // Remove client from the list
        {
            std::lock_guard<std::mutex> lock(clients_mutex);
            auto it = std::find(client_sockets.begin(), client_sockets.end(), client_socket);
            if (it != client_sockets.end()) {
                int index = std::distance(client_sockets.begin(), it);
                client_sockets.erase(it);
                client_names.erase(client_names.begin() + index);
            }
        }
        
        // Announce user left
        std::string leave_msg = client_name + " left the chat!\n";
        broadcastMessage(leave_msg, -1);
        
        std::cout << client_name << " disconnected" << std::endl;
        close(client_socket);
    }
    
    void broadcastMessage(const std::string& message, int sender_socket) {
        std::lock_guard<std::mutex> lock(clients_mutex);
        
        for (int client_socket : client_sockets) {
            if (client_socket != sender_socket) {
                send(client_socket, message.c_str(), message.length(), 0);
            }
        }
    }
    
    void stop() {
        if (running) {
            running = false;
            close(server_socket);
            
            // Close all client connections
            std::lock_guard<std::mutex> lock(clients_mutex);
            for (int client_socket : client_sockets) {
                close(client_socket);
            }
            client_sockets.clear();
            client_names.clear();
        }
    }
    
    void showConnectedClients() {
        std::lock_guard<std::mutex> lock(clients_mutex);
        std::cout << "\nConnected clients (" << client_names.size() << "):\n";
        for (const auto& name : client_names) {
            std::cout << "- " << name << std::endl;
        }
        std::cout << std::endl;
    }
};

int main() {
    ChatServer server;
    
    if (!server.start()) {
        return 1;
    }
    
    // Start accepting clients in a separate thread
    std::thread accept_thread(&ChatServer::acceptClients, &server);
    
    // Server console commands
    std::string command;
    std::cout << "\nServer Commands:\n";
    std::cout << "- 'clients' - Show connected clients\n";
    std::cout << "- 'quit' - Stop server\n\n";
    
    while (true) {
        std::cout << "Server> ";
        std::getline(std::cin, command);
        
        if (command == "quit") {
            std::cout << "Shutting down server...\n";
            server.stop();
            break;
        } else if (command == "clients") {
            server.showConnectedClients();
        }
    }
    
    accept_thread.join();
    return 0;

}
