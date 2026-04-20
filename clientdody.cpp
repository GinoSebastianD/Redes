#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <iostream>
#include <fstream>
#include <string>
#include <thread>
using namespace std;

void threadReadSocket(int client_socket) {
    char buffer[65536];
    int n;

    do {
        n = read(client_socket, buffer, 1);
        if(n <= 0) break;
        char op = buffer[0];

        if(op == 'K'){
            cout << "[OK]" << endl;

        } else if(op == 'E'){
            // Error: E | 5B len | V msg
            n = read(client_socket, buffer, 5);
            if(n <= 0) break;
            buffer[n] = '\0';
            int msg_len = atoi(buffer);

            n = read(client_socket, buffer, msg_len);
            if(n <= 0) break;
            buffer[n] = '\0';
            string msg(buffer, n);
            cout << "[ERROR]: " << msg << endl;

        } else if(op == 'U'){
            // Unicast recibido S→C
            n = read(client_socket, buffer, 3);
            if(n <= 0) break;
            buffer[n] = '\0';
            int sender_len = atoi(buffer);

            n = read(client_socket, buffer, sender_len);
            if(n <= 0) break;
            buffer[n] = '\0';
            string sender(buffer, n);

            n = read(client_socket, buffer, 3);
            if(n <= 0) break;
            buffer[n] = '\0';
            int msg_len = atoi(buffer);

            n = read(client_socket, buffer, msg_len);
            if(n <= 0) break;
            buffer[n] = '\0';
            string msg(buffer, n);

            cout << "\n[Privado][" << sender << "]: " << msg << endl;

        } else if(op == 'b'){
            // Broadcast recibido S→C (minúscula)
            n = read(client_socket, buffer, 3);
            if(n <= 0) break;
            buffer[n] = '\0';
            int sender_len = atoi(buffer);

            n = read(client_socket, buffer, sender_len);
            if(n <= 0) break;
            buffer[n] = '\0';
            string sender(buffer, n);

            n = read(client_socket, buffer, 3);
            if(n <= 0) break;
            buffer[n] = '\0';
            int msg_len = atoi(buffer);

            n = read(client_socket, buffer, msg_len);
            if(n <= 0) break;
            buffer[n] = '\0';
            string msg(buffer, n);

            cout << "\n[Broadcast][" << sender << "]: " << msg << endl;

        } else if(op == 't'){
            // List response S→C: t | 5B len | V JSON
            n = read(client_socket, buffer, 5);
            if(n <= 0) break;
            buffer[n] = '\0';
            int json_len = atoi(buffer);

            string json_data;
            int remaining = json_len;
            while(remaining > 0){
                int to_read = remaining > 256 ? 256 : remaining;
                n = read(client_socket, buffer, to_read);
                if(n <= 0) break;
                json_data.append(buffer, n);
                remaining -= n;
            }

            cout << "\n[Lista de usuarios]: " << json_data << endl;

        } else if(op == 'f'){
            // Archivo recibido S→cli
            // f | 5B filename_len | V filename | 5B file_len | V file | 5B origin_len | V origin
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

            string file_data;
            int remaining = file_len;
            while(remaining > 0){
                int to_read = remaining > 256 ? 256 : remaining;
                n = read(client_socket, buffer, to_read);
                if(n <= 0) break;
                file_data.append(buffer, n);
                remaining -= n;
            }

            n = read(client_socket, buffer, 5);
            if(n <= 0) break;
            buffer[n] = '\0';
            int origin_len = atoi(buffer);

            n = read(client_socket, buffer, origin_len);
            if(n <= 0) break;
            buffer[n] = '\0';
            string origin(buffer, n);

            // Guardar el archivo localmente con prefijo del remitente
            string saved_name = "copia_" + origin + "_" + filename;
            ofstream ofs(saved_name, ios::binary);
            if(ofs.is_open()){
                ofs.write(file_data.c_str(), file_data.size());
                ofs.close();
                cout << "\nArchivo listo " << origin << ": " << filename
                     << " -> se guardo en txt como: " << saved_name << endl;
            } else {
                cout << "\n No se pudo guardar el archivo " << filename << endl;
            }
        }

        cout.flush();

    } while(1);
}

int main(void){
    struct sockaddr_in stSockAddr;
    int Res;
    int SocketFD = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);

    if(-1 == SocketFD){
        perror("cannot create socket");
        exit(EXIT_FAILURE);
    }

    memset(&stSockAddr, 0, sizeof(struct sockaddr_in));
    stSockAddr.sin_family = AF_INET;
    stSockAddr.sin_port = htons(1100);
    Res = inet_pton(AF_INET, "172.26.186.177", &stSockAddr.sin_addr);

    if(0 > Res){
        perror("error: first parameter is not a valid address family");
        close(SocketFD);
        exit(EXIT_FAILURE);
    } else if(0 == Res){
        perror("char string (second parameter does not contain valid ipaddress)");
        close(SocketFD);
        exit(EXIT_FAILURE);
    }

    if(-1 == connect(SocketFD, (const struct sockaddr *)&stSockAddr, sizeof(struct sockaddr_in))){
        perror("connect failed");
        close(SocketFD);
        exit(EXIT_FAILURE);
    }

    // Login
    string nickname;
    cout << "Nickname: ";
    getline(cin, nickname);

    char buffer[65536];
    int offset = 0;
    int nick_len = nickname.size();

    buffer[offset++] = 'L';
    snprintf(buffer + offset, 5, "%04d", nick_len);
    offset += 4;
    memcpy(buffer + offset, nickname.c_str(), nick_len);
    offset += nick_len;
    write(SocketFD, buffer, offset);

    // Thread para recibir mensajes
    thread t(threadReadSocket, SocketFD);
    t.detach();

    string input;
    do {
        cout << "\nAccion (U=unicast, B=broadcast, T=list, F=file, O=logout): ";
        getline(cin, input);
        if(input.empty()) continue;
        char op = input[0];

        if(op == 'O'){
            buffer[0] = 'O';
            write(SocketFD, buffer, 1);
            break;

        } else if(op == 'U'){
            // Unicast C→S
            string dest, msg;
            cout << "Destinatario: ";
            getline(cin, dest);
            cout << "Mensaje: ";
            getline(cin, msg);

            int dest_len = dest.size();
            int msg_len  = msg.size();
            offset = 0;

            buffer[offset++] = 'U';
            snprintf(buffer + offset, 4, "%03d", dest_len);
            offset += 3;
            memcpy(buffer + offset, dest.c_str(), dest_len);
            offset += dest_len;
            snprintf(buffer + offset, 4, "%03d", msg_len);
            offset += 3;
            memcpy(buffer + offset, msg.c_str(), msg_len);
            offset += msg_len;

            write(SocketFD, buffer, offset);

        } else if(op == 'B'){
            // Broadcast C→S
            string msg;
            cout << "Mensaje: ";
            getline(cin, msg);

            int msg_len = msg.size();
            offset = 0;

            buffer[offset++] = 'B';
            snprintf(buffer + offset, 4, "%03d", msg_len);
            offset += 3;
            memcpy(buffer + offset, msg.c_str(), msg_len);
            offset += msg_len;

            write(SocketFD, buffer, offset);

        } else if(op == 'T'){
            // List C→S: solo el opcode
            buffer[0] = 'T';
            write(SocketFD, buffer, 1);

        } else if(op == 'F'){
            // File C→S
            // Formato: F | 5B filename_len | V filename | 5B file_len | V file | 5B dest_len | V dest
            string filepath, dest;
            cout << "Ruta archivo: ";
            getline(cin, filepath);
            cout << "A quien?: ";
            getline(cin, dest);

            // Leer el archivo
            ifstream ifs(filepath, ios::binary | ios::ate);
            if(!ifs.is_open()){
                cout << "No se pudo abrir el archivo: " << filepath << endl;
                continue;
            }
            int file_len = ifs.tellg();
            ifs.seekg(0, ios::beg);
            string file_data(file_len, '\0');
            ifs.read(&file_data[0], file_len);
            ifs.close();

            // Extraer solo el nombre del archivo (sin ruta)
            string filename = filepath;
            size_t slash = filepath.find_last_of("/\\");
            if(slash != string::npos) filename = filepath.substr(slash + 1);

            int filename_len = filename.size();
            int dest_len = dest.size();

            // Construir paquete (puede ser grande)
            int out_size = 1 + 5 + filename_len + 5 + file_len + 5 + dest_len;
            char* out = new char[out_size + 1];
            offset = 0;

            out[offset++] = 'F';
            snprintf(out + offset, 6, "%05d", filename_len);
            offset += 5;
            memcpy(out + offset, filename.c_str(), filename_len);
            offset += filename_len;
            snprintf(out + offset, 6, "%05d", file_len);
            offset += 5;
            memcpy(out + offset, file_data.c_str(), file_len);
            offset += file_len;
            snprintf(out + offset, 6, "%05d", dest_len);
            offset += 5;
            memcpy(out + offset, dest.c_str(), dest_len);
            offset += dest_len;

            write(SocketFD, out, offset);
            delete[] out;

            cout << "[Archivo enviado]: " << filename << " -> " << dest << endl;

        } else {
            cout << "Accion no reconocida" << endl;
        }

    } while(1);

    shutdown(SocketFD, SHUT_RDWR);
    close(SocketFD);
    return 0;
}