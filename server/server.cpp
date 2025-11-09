// --- server/server.cpp (Phase 5 FIXED) ---
#include <iostream>
#include <string>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <thread>
#include <vector>
#include <dirent.h>
#include <fstream>
#include <sys/stat.h>
#include <sstream>
#include <map> 

#define PORT 8080
#define BUFFER_SIZE 4096

// --- Encryption ---
const std::string XOR_KEY = "mysecretkey";

std::string xor_cipher(std::string data) {
    std::string output = data;
    for (int i = 0; i < output.length(); ++i) {
        output[i] = output[i] ^ XOR_KEY[i % XOR_KEY.length()];
    }
    return output;
}

// --- User Authentication ---
std::map<std::string, std::string> users = {
    {"admin", "pass123"},
    {"chit", "wipro2025"}
};

bool authenticate_user(std::string auth_string) {
    std::stringstream ss(auth_string);
    std::string cmd, user, pass;
    ss >> cmd >> user >> pass;

    if (cmd == "AUTH" && users.count(user) && users[user] == pass) {
        return true;
    }
    return false;
}

long get_file_size(std::string filename) {
    struct stat stat_buf;
    int rc = stat(filename.c_str(), &stat_buf);
    return rc == 0 ? stat_buf.st_size : -1;
}

void receive_file(int sock, std::string filename, long file_size) {
    char buffer[BUFFER_SIZE] = {0};
    
    std::ofstream output_file("./server_files/" + filename, std::ios::binary);
    if (!output_file) {
        std::string msg = "ERROR: Cannot create file.";
        std::string encrypted_msg = xor_cipher(msg);
        send(sock, encrypted_msg.c_str(), encrypted_msg.length(), 0);
        return;
    }

    std::string encrypted_ready = xor_cipher("READY");
    send(sock, encrypted_ready.c_str(), encrypted_ready.length(), 0);

    long bytes_received = 0;
    while (bytes_received < file_size) {
        int bytes_to_read = BUFFER_SIZE;
        if (file_size - bytes_received < BUFFER_SIZE) {
            bytes_to_read = file_size - bytes_received;
        }

        int bytes_read = recv(sock, buffer, bytes_to_read, 0);
        if (bytes_read <= 0) {
            break;
        }
        output_file.write(buffer, bytes_read);
        bytes_received += bytes_read;
    }
    output_file.close();
    
    std::string response_msg;
    if (bytes_received == file_size) {
        response_msg = "OK: Upload successful.";
    } else {
        response_msg = "ERROR: Upload failed.";
    }
    std::string encrypted_response = xor_cipher(response_msg);
    send(sock, encrypted_response.c_str(), encrypted_response.length(), 0);
}

void handle_client(int client_socket) {
    char buffer[BUFFER_SIZE] = {0};
    bool authenticated = false; 
    
    std::cout << "Client connected. Waiting for authentication..." << std::endl;

    while (true) {
        memset(buffer, 0, BUFFER_SIZE);
        int bytes_read = recv(client_socket, buffer, BUFFER_SIZE - 1, 0);
        if (bytes_read <= 0) {
            std::cout << "Client disconnected." << std::endl;
            break;
        }

        std::string raw_data(buffer, bytes_read);
        std::string command = xor_cipher(raw_data);
        std::cout << "Client says (decrypted): " << command << std::endl;

        if (!authenticated) {
            std::string msg;
            if (authenticate_user(command)) {
                authenticated = true;
                std::cout << "Authentication successful." << std::endl;
                msg = "OK: Auth successful. Welcome!";
            } else {
                std::cout << "Authentication failed." << std::endl;
                msg = "ERROR: Auth failed. Invalid user/pass.";
                std::string encrypted_msg = xor_cipher(msg);
                send(client_socket, encrypted_msg.c_str(), encrypted_msg.length(), 0);
                break;
            }
            std::string encrypted_msg = xor_cipher(msg);
            send(client_socket, encrypted_msg.c_str(), encrypted_msg.length(), 0);
            continue;
        }
        
        if (command == "LIST") {
            std::string file_list = "Files on server:\n";
            DIR *dir;
            struct dirent *ent;
            if ((dir = opendir("./server_files")) != NULL) {
                while ((ent = readdir(dir)) != NULL) {
                    if (strcmp(ent->d_name, ".") != 0 && strcmp(ent->d_name, "..") != 0) {
                        file_list += ent->d_name;
                        file_list += "\n";
                    }
                }
                closedir(dir);
            } else {
                file_list = "Error: Cannot open server_files directory.";
            }
            std::string encrypted_list = xor_cipher(file_list);
            send(client_socket, encrypted_list.c_str(), encrypted_list.length(), 0);

        } else if (command.rfind("GET ", 0) == 0) {
            std::string filename = command.substr(4);
            std::string filepath = "./server_files/" + filename;
            std::string msg;

            if (filename.find("..") != std::string::npos) {
                msg = "ERROR: Invalid filename.";
                std::string encrypted_msg = xor_cipher(msg);
                send(client_socket, encrypted_msg.c_str(), encrypted_msg.length(), 0);
                continue;
            }

            long file_size = get_file_size(filepath);

            if (file_size < 0) {
                msg = "ERROR: File not found.";
                std::string encrypted_msg = xor_cipher(msg);
                send(client_socket, encrypted_msg.c_str(), encrypted_msg.length(), 0);
            } else {
                msg = "SIZE " + std::to_string(file_size);
                std::string encrypted_msg = xor_cipher(msg);
                send(client_socket, encrypted_msg.c_str(), encrypted_msg.length(), 0);

                memset(buffer, 0, BUFFER_SIZE);
                recv(client_socket, buffer, BUFFER_SIZE - 1, 0);
                if (xor_cipher(std::string(buffer)) != "READY") {
                    continue;
                }

                std::ifstream file_to_send(filepath, std::ios::binary);
                while (file_to_send) {
                    file_to_send.read(buffer, BUFFER_SIZE);
                    std::streamsize bytes_actually_read = file_to_send.gcount();
                    if (bytes_actually_read > 0) {
                        send(client_socket, buffer, bytes_actually_read, 0);
                    }
                }
                file_to_send.close();
            }
        } 
        else if (command.rfind("PUT ", 0) == 0) {
            std::stringstream ss(command);
            std::string cmd, filename;
            long file_size;
            ss >> cmd >> filename >> file_size;
            std::string msg;

            if (filename.empty() || file_size <= 0) {
                msg = "ERROR: Bad PUT command.";
                std::string encrypted_msg = xor_cipher(msg);
                send(client_socket, encrypted_msg.c_str(), encrypted_msg.length(), 0);
                continue;
            }
            if (filename.find("..") != std::string::npos) {
                msg = "ERROR: Invalid filename.";
                std::string encrypted_msg = xor_cipher(msg);
                send(client_socket, encrypted_msg.c_str(), encrypted_msg.length(), 0);
                continue;
            }
            
            receive_file(client_socket, filename, file_size);
        }
        else if (command == "QUIT") {
            break;
        } else {
            std::string msg = "Unknown command.";
            std::string encrypted_msg = xor_cipher(msg);
            send(client_socket, encrypted_msg.c_str(), encrypted_msg.length(), 0);
        }
    }
    close(client_socket);
}

// --- Main Server Loop (No changes) ---
int main() {
    int server_fd;
    struct sockaddr_in address;
    int opt = 1;
    
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }
    
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);
    
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 5) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    std::cout << "Secure server is listening on port " << PORT << "..." << std::endl;

    while (true) {
        int new_socket;
        struct sockaddr_in client_addr;
        socklen_t client_addr_len = sizeof(client_addr);

        if ((new_socket = accept(server_fd, (struct sockaddr *)&client_addr, &client_addr_len)) < 0) {
            perror("accept");
            std::cerr << "Error on accept" << std::endl;
        } else {
            std::thread(handle_client, new_socket).detach();
        }
    }
    
    close(server_fd);
    return 0;
}