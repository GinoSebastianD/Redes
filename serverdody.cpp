#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <iostream>
#include <string>
#include <thread>
#include <map>
#include <mutex>
#include <sstream>
using namespace std;

map<string, int> clients;
mutex clients_mutex;

void send_ok(int fd){
    char pkt = 'K';
    write(fd, &pkt, 1);
}

void send_error(int fd, const string &msg){
    char buffer[1024];
    int msg_len = msg.size();
    int offset = 0;
    buffer[offset++] = 'E';
    snprintf(buffer + offset, 6, "%05d", msg_len);
    offset += 5;
    memcpy(buffer + offset, msg.c_str(), msg_len);
    offset += msg_len;
    write(fd, buffer, offset);
}

void threadReadSocket(int client_socket) {
    char buffer[65536];
    int n;

    do {
        n = read(client_socket, buffer, 1);
        if(n <= 0) break;
        char op = buffer[0];

        if(op == 'O'){
            // Logout
            clients_mutex.lock();
            for(auto it = clients.begin(); it != clients.end(); it++){
                if(it->second == client_socket){
                    cout << "Cliente desconectado: [" << it->first << "]" << endl;
                    clients.erase(it);
                    break;
                }
            }
            clients_mutex.unlock();
            send_ok(client_socket);
            break;

        } else if(op == 'U'){
            // Unicast C→S
            n = read(client_socket, buffer, 3);
            if(n <= 0) break;
            buffer[n] = '\0';
            int dest_len = atoi(buffer);

            n = read(client_socket, buffer, dest_len);
            if(n <= 0) break;
            buffer[n] = '\0';
            string dest(buffer, n);

            n = read(client_socket, buffer, 3);
            if(n <= 0) break;
            buffer[n] = '\0';
            int msg_len = atoi(buffer);

            n = read(client_socket, buffer, msg_len);
            if(n <= 0) break;
            buffer[n] = '\0';
            string msg(buffer, n);

            string sender = "unknown";
            clients_mutex.lock();
            for(auto it = clients.begin(); it != clients.end(); it++){
                if(it->second == client_socket){
                    sender = it->first;
                    break;
                }
            }

            if(clients.find(dest) != clients.end()){
                int target_fd = clients[dest];
                clients_mutex.unlock();

                char out[65536];
                int offset = 0;
                int sender_len = sender.size();

                out[offset++] = 'U';
                snprintf(out + offset, 4, "%03d", sender_len);
                offset += 3;
                memcpy(out + offset, sender.c_str(), sender_len);
                offset += sender_len;
                snprintf(out + offset, 4, "%03d", msg_len);
                offset += 3;
                memcpy(out + offset, msg.c_str(), msg_len);
                offset += msg_len;

                write(target_fd, out, offset);
                send_ok(client_socket);
            } else {
                clients_mutex.unlock();
                send_error(client_socket, "Usuario no encontrado");
            }

        } else if(op == 'B'){
            // Broadcast C→S
            n = read(client_socket, buffer, 3);
            if(n <= 0) break;
            buffer[n] = '\0';
            int msg_len = atoi(buffer);

            n = read(client_socket, buffer, msg_len);
            if(n <= 0) break;
            buffer[n] = '\0';
            string msg(buffer, n);

            string sender = "unknown";
            clients_mutex.lock();
            for(auto it = clients.begin(); it != clients.end(); it++){
                if(it->second == client_socket){
                    sender = it->first;
                    break;
                }
            }

            char out[65536];
            int offset = 0;
            int sender_len = sender.size();

            out[offset++] = 'b';  // 'b' minúscula = broadcast recibido S→C
            snprintf(out + offset, 4, "%03d", sender_len);
            offset += 3;
            memcpy(out + offset, sender.c_str(), sender_len);
            offset += sender_len;
            snprintf(out + offset, 4, "%03d", msg_len);
            offset += 3;
            memcpy(out + offset, msg.c_str(), msg_len);
            offset += msg_len;

            for(auto it = clients.begin(); it != clients.end(); it++){
                if(it->second != client_socket){
                    write(it->second, out, offset);
                }
            }
            clients_mutex.unlock();
            send_ok(client_socket);

        } else if(op == 'T'){
            // List C→S: cliente solicita lista de usuarios conectados
            // Respuesta S→C: 't' + 5B(len) + JSON
            string sender = "unknown";
            clients_mutex.lock();
            for(auto it = clients.begin(); it != clients.end(); it++){
                if(it->second == client_socket){
                    sender = it->first;
                    break;
                }
            }

            // Construir JSON: {"clients":["nick1","nick2",...]}
            string json = "{\"clients\":[";
            bool first = true;
            for(auto it = clients.begin(); it != clients.end(); it++){
                if(!first) json += ",";
                json += "\"" + it->first + "\"";
                first = false;
            }
            json += "]}";
            clients_mutex.unlock();

            cout << "Lista solicitada por [" << sender << "]" << endl;

            char out[65536];
            int offset = 0;
            int json_len = json.size();

            out[offset++] = 't';  // 't' minúscula = list response S→C
            snprintf(out + offset, 6, "%05d", json_len);
            offset += 5;
            memcpy(out + offset, json.c_str(), json_len);
            offset += json_len;

            write(client_socket, out, offset);

        } else if(op == 'F'){
            // File C→S: enviar archivo a un destinatario
            // Formato: F | 5B file_name_len | V file_name | 5B file_len | 5B nick_dest_len | V nick_dest
            n = read(client_socket, buffer, 5);
            if(n <= 0) break;
            buffer[n] = '\0'; 
            int filename_len = atoi(buffer);

            n = read(client_socket, buffer, filename_len);
            if(n <= 0) break;
            buffer[n] = '\0';
            string filename(buffer, n);

            n = read(client_socket, buffer, 5);
            if(n <= 0) break;
            buffer[n] = '\0';
            int file_len = atoi(buffer);

            // Leer contenido del archivo (puede ser grande)
            string file_data;
            int remaining = file_len;
            while(remaining > 0){
                int to_read = remaining > 256 ? 256 : remaining;
                n = read(client_socket, buffer, to_read);
                if(n <= 0) break;
                file_data.append(buffer, n); //los añade al final del string
                remaining -= n; //resta lo que falta por leer
            }
            if((int)file_data.size() != file_len) break;

            n = read(client_socket, buffer, 5);
            if(n <= 0) break;
            buffer[n] = '\0';
            int dest_len = atoi(buffer);

            n = read(client_socket, buffer, dest_len);
            if(n <= 0) break;
            buffer[n] = '\0';
            string dest(buffer, n);

            // Buscar remitente
            string sender = "unknown";
            clients_mutex.lock();
            for(auto it = clients.begin(); it != clients.end(); it++){
                if(it->second == client_socket){
                    sender = it->first;
                    break;
                }
            }

            if(clients.find(dest) != clients.end()){
                int target_fd = clients[dest];
                clients_mutex.unlock();

                // Enviar con opcode 'f' (file S→cli)
                // Formato: f | 5B filename_len | V filename | 5B file_len | V file | 5B origin_len | V origin
                int sender_len = sender.size();
                int out_size = 1 + 5 + filename_len + 5 + file_len + 5 + sender_len;
                char* out = new char[out_size + 1];
                int offset = 0;

                out[offset++] = 'f';
                snprintf(out + offset, 6, "%05d", filename_len);
                offset += 5;
                memcpy(out + offset, filename.c_str(), filename_len);
                offset += filename_len;
                snprintf(out + offset, 6, "%05d", file_len);
                offset += 5;
                memcpy(out + offset, file_data.c_str(), file_len);
                offset += file_len;
                snprintf(out + offset, 6, "%05d", sender_len);
                offset += 5;
                memcpy(out + offset, sender.c_str(), sender_len);
                offset += sender_len;

                write(target_fd, out, offset);
                delete[] out;

                send_ok(client_socket);
                cout << "Archivo [" << filename << "] enviado de [" << sender << "] a [" << dest << "]" << endl;
            } else {
                clients_mutex.unlock();
                send_error(client_socket, "Usuario destinatario no encontrado");
            }

        } else {
            send_error(client_socket, "Operacion desconocida");
        }

    } while(1);

    // Limpiar cliente si se desconectó sin logout
    clients_mutex.lock();
    for(auto it = clients.begin(); it != clients.end(); it++){
        if(it->second == client_socket){
            cout << "Cliente caido: [" << it->first << "]" << endl;
            clients.erase(it);
            break;
        }
    }
    clients_mutex.unlock();

    close(client_socket);
}

int main(void){
    struct sockaddr_in stSockAddr;
    int SocketFD = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    char buffer[256];
    int n;

    if(-1 == SocketFD){
        perror("can not create socket");
        exit(EXIT_FAILURE);
    }

    // Permitir reusar el puerto
    int opt = 1;
    setsockopt(SocketFD, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    memset(&stSockAddr, 0, sizeof(struct sockaddr_in));
    stSockAddr.sin_family = AF_INET;
    stSockAddr.sin_port = htons(1100);
    stSockAddr.sin_addr.s_addr = INADDR_ANY;

    if(-1 == bind(SocketFD, (const struct sockaddr *)&stSockAddr, sizeof(struct sockaddr_in))){
        perror("error bind failed");
        close(SocketFD);
        exit(EXIT_FAILURE);
    }
    if(-1 == listen(SocketFD, 10)){
        perror("error listen failed");
        close(SocketFD);
        exit(EXIT_FAILURE);
    }

    cout << "Servidor escuchando en puerto 1100..." << endl;

    for(;;){
        int ConnectFD = accept(SocketFD, NULL, NULL);
        if(0 > ConnectFD){
            perror("error accept failed");
            close(SocketFD);
            exit(EXIT_FAILURE);
        }

        // Login
        n = read(ConnectFD, buffer, 1);
        if(n <= 0 || buffer[0] != 'L'){
            send_error(ConnectFD, "Se esperaba login");
            close(ConnectFD);
            continue;
        }

        n = read(ConnectFD, buffer, 4);
        buffer[n] = '\0';
        int nick_len = atoi(buffer);

        n = read(ConnectFD, buffer, nick_len);
        buffer[n] = '\0';
        string nickname(buffer, n);

        clients_mutex.lock();
        if(clients.find(nickname) != clients.end()){
            clients_mutex.unlock();
            send_error(ConnectFD, "Nickname ya en uso");
            close(ConnectFD);
            continue;
        }
        clients[nickname] = ConnectFD;
        clients_mutex.unlock();

        cout << "Nuevo cliente: [" << nickname << "] fd=" << ConnectFD << endl;
        send_ok(ConnectFD);

        thread t(threadReadSocket, ConnectFD);
        t.detach();
    }

    close(SocketFD);
    return 0;
}