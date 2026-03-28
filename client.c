  #include <sys/types.h>
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <arpa/inet.h>
  #include <stdio.h>
  #include <stdlib.h>
  #include <string.h>
  #include <unistd.h>

  int main(void){
    struct sockaddr_in estructura;
    int FDclient = socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
    int n;
    int res;
    char buffer[256];
    if(FDclient ==-1){
        perror("error");
        exit(EXIT_FAILURE);
    }
    
    estructura.sin_family = AF_INET;
    estructura.sin_port = htons(1100);
    res = inet_pton(AF_INET,"172.26.186.177",&estructura.sin_addr);
    
    if(res == -1){
        perror("error en el primer parametro");
        close(FDclient);
        exit(EXIT_FAILURE);
    }
    else if (res == 0){
        perror("error en el segundo parametro");
        close(FDclient);
        exit(EXIT_FAILURE);
    }    

    if(-1 == connect(FDclient, (const struct sockaddr*)&estructura,sizeof(struct sockaddr_in))){
        perror("error en conexion");
        close(FDclient);
        exit(EXIT_FAILURE);
    }
    for(;;){
        printf("Msg: ");
        scanf("%s", buffer);
        n = write(FDclient,buffer,255);
        n = read(FDclient,buffer,255);
        printf("Ser: [%s]\n",buffer);
    }
    shutdown(FDclient,SHUT_RDWR);
    close(FDclient);
    return 0;

  }






























