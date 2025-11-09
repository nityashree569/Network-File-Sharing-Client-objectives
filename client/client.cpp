// --- client/client.cpp (Phase 5 FIXED) ---
#include <iostream>
#include <string>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <fstream>
#include <sys/stat.h>
#include <sstream>

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

long get_file_size(std::string filename) {
    struct stat stat_buf;
    int rc = stat(filename.c_str(), &stat_buf);
    return rc == 0 ? stat_buf.st_size : -1;
}

void send_file(int sock, std::string filename, long file_size) {
    char buffer[BUFFER_SIZE] = {0};

    memset(buffer, 0, BUFFER_SIZE);
    int bytes_read = recv(sock, buffer, BUFFER_SIZE - 1, 0);
    std::string server_response = xor_cipher(std::string(buffer, bytes_read)); 

    if (server_response != "READY") {
        std::cerr << "Error: Server not ready. Aborting upload." << std::endl;
        return;
    }

    std::ifstream file_to_send(filename, std::ios::binary);
    while (file_to_send) {
        file_to_send.read(buffer, BUFFER_SIZE);
        std::streamsize bytes_actually_read = file_to_send.gcount();
        if (bytes_actually_read > 0) {
            send(sock, buffer, bytes_actually_read, 0); 
        }
    }
    file_to_send.close();
    
    memset(buffer, 0, BUFFER_SIZE);
    bytes_read = recv(sock, buffer, BUFFER_SIZE - 1, 0);
    std::cout << "Server response: " << xor_cipher(std::string(buffer, bytes_read)) << std::endl;
}

void receive_file(int sock, std::string filename, long file_size) {
    char buffer[BUFFER_SIZE] = {0};
    
    std::string encrypted_ready = xor_cipher("READY");
    send(sock, encrypted_ready.c_str(), encrypted_ready.length(), 0);

    std::ofstream output_file(filename, std::ios::binary);
    if (!output_file) {
        std::cerr << "Error: Could not create file " << filename << std::endl;
        return;
    }

    long bytes_received = 0;
    while (bytes_received < file_size) {
        int bytes_to_read = BUFFER_SIZE;
        if (file_size - bytes_received < BUFFER_SIZE) {
            bytes_to_read = file_size - bytes_received;
        }

        int bytes_read = recv(sock, buffer, bytes_to_read, 0);
        if (bytes_read <= 0) {
            std::cerr << "Error: Server disconnected during file transfer." << std::endl;
            break;
        }
        output_file.write(buffer, bytes_read);
        bytes_received += bytes_read;
    }
    output_file.close();

    if (bytes_received == file_size) {
        std::cout << "Download complete: " << filename << std::endl;
    } else {
        std::cout << "Download failed." << std::endl;
    }
}

int main() {
    int sock = 0;
    struct sockaddr_in serv_addr;
    char buffer[BUFFER_SIZE] = {0};

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        std::cerr << "\n Socket creation error \n";
        return -1;
    }

    memset(&serv_addr, '0', sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);
    
    if(inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
        std::cerr << "\nInvalid address/ Address not supported \n";
        return -1;
    }

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        std::cerr << "\nConnection Failed \n";
        return -1;
    }

    // --- Login Prompt ---
    std::string user, pass, auth_string;
    std::cout << "Connected to secure server. Please log in." << std::endl;
    std::cout << "Username: ";
    std::getline(std::cin, user);
    std::cout << "Password: ";
    std::getline(std::cin, pass);

    auth_string = "AUTH " + user + " " + pass;
    
    std::string encrypted_auth = xor_cipher(auth_string);
    send(sock, encrypted_auth.c_str(), encrypted_auth.length(), 0);

    memset(buffer, 0, BUFFER_SIZE);
    int bytes_read = recv(sock, buffer, BUFFER_SIZE - 1, 0);
    std::string auth_response = xor_cipher(std::string(buffer, bytes_read));

    if (auth_response.rfind("ERROR:", 0) == 0) {
        std::cerr << auth_response << std::endl;
        close(sock);
        return -1;
    }
    
    std::cout << auth_response << std::endl;

    std::cout << "Type 'LIST', 'GET <file>', 'PUT <file>', or 'QUIT'." << std::endl;

    while (true) {
        std::string user_input;
        std::cout << "> ";
        std::getline(std::cin, user_input);

        if (user_input.empty()) continue;

        if (user_input.rfind("PUT ", 0) == 0) {
            std::string filename = user_input.substr(4);
            long file_size = get_file_size(filename);
            if (file_size < 0) {
                std::cout << "Error: File not found or cannot be read." << std::endl;
                continue;
            }

            std::string command_to_send = "PUT " + filename + " " + std::to_string(file_size);
            std::string encrypted_command = xor_cipher(command_to_send);
            send(sock, encrypted_command.c_str(), encrypted_command.length(), 0);
            
            std::cout << "Uploading " << filename << " (" << file_size << " bytes)..." << std::endl;
            send_file(sock, filename, file_size);
        }
        else {
            std::string encrypted_input = xor_cipher(user_input);
            send(sock, encrypted_input.c_str(), encrypted_input.length(), 0);

            if (user_input == "QUIT") {
                break;
            }

            if (user_input.rfind("GET ", 0) == 0) {
                std::string filename = user_input.substr(4);
                
                memset(buffer, 0, BUFFER_SIZE);
                bytes_read = recv(sock, buffer, BUFFER_SIZE - 1, 0);
                
                std::string server_response = xor_cipher(std::string(buffer, bytes_read));
                
                if (server_response.rfind("SIZE ", 0) == 0) {
                    long file_size = std::stol(server_response.substr(5));
                    std::cout << "Receiving " << filename << " (" << file_size << " bytes)..." << std::endl;
                    receive_file(sock, filename, file_size);
                
                } else {
                    std::cout << "Server response: " << server_response << std::endl;
                }
            }
            else { 
                memset(buffer, 0, BUFFER_SIZE);
                bytes_read = recv(sock, buffer, BUFFER_SIZE - 1, 0);

                if (bytes_read <= 0) {
                    std::cout << "Server disconnected." << std::endl;
                    break;
                }
                std::cout << "Server response:\n" << xor_cipher(std::string(buffer, bytes_read)) << std::endl;
            }
        }
    }

    close(sock);
    return 0;
}