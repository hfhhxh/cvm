#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <arpa/inet.h>
#include <unistd.h>

static void sig_chld(int signo){
        pid_t pid;
        int stat;
        while((pid = wait(&stat)) > 0);
        return;
}

int main()
{
	signal(SIGCHLD, sig_chld);
	int ret;
    int serv_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    serv_addr.sin_port = htons(50000);
    ret = bind(serv_sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr));
	if(-1 == ret)return false;
    ret = listen(serv_sock, 20);
	if(-1 == ret)return false;
    struct sockaddr_in clnt_addr;
    socklen_t clnt_addr_size = sizeof(clnt_addr);

    while(1){
        int clnt_sock = accept(serv_sock, (struct sockaddr*)&clnt_addr, &clnt_addr_size);
		if(-1 == clnt_sock)continue;
//        printf("new connection.\n");
        if(fork() == 0){
            close(serv_sock); 
            char cmd[4096];
			memset(cmd, 0, sizeof(cmd));
            read(clnt_sock, cmd, 4096-1);
            close(clnt_sock);
//			printf("%s\n", cmd);
            system(cmd);
            return 0;
        }else{ 
            close(clnt_sock);  
        }
    }
    close(serv_sock);
    return 0;
}
