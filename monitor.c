#include <stdio.h>
#include <sys/un.h>
#include <sys/socket.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

extern int errno;

int main(int argc, char **argv){
	char *path = "/var/ssd/vm/cl/vm7";
	struct sockaddr_un un;
	un.sun_family = AF_UNIX;
	strcpy(un.sun_path, argv[1]);
	int sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
	int ret = 
	bind(sockfd, (struct sockaddr*)&un, sizeof(struct sockaddr_un));
	strcpy(un.sun_path, argv[1]);
	ret = connect(sockfd, (struct sockaddr*)&un, sizeof(struct sockaddr_un));
	const int bufsize = 1024*64;
	char buf[bufsize];
	usleep(1000);
	ret = read(sockfd, buf, bufsize);
    write(1, buf, ret);
	//printf("rcv: %d\n<----------\n%s\n---------->\n", strlen(buf), buf);
	while(1){
//		int ret;
		memset(buf, 0, sizeof(buf));
		ret = read(0, buf, bufsize);
		//printf("ret = %d\n", ret);
		ret = write(sockfd, buf, ret);
		//printf("snd: %d\n<----------\n%s\n---------->\n", ret, buf);
		//printf("ret = %d\n", ret);
		memset(buf, 0, sizeof(buf));
		usleep(1000);
		ret = read(sockfd, buf, bufsize);
		//printf("ret = %d\n", ret);
		write(1, buf, ret);
		//printf("rcv: %d\n<----------\n%s\n---------->\n", strlen(buf), buf);
		//memset(buf, 0, sizeof(buf));
                //ret = read(sockfd, buf, bufsize);
                //printf("ret = %d\n", ret);
                //write(1, buf, ret);
		//printf("ret = %d\n", ret);
	}
	close(sockfd);
	return 0;
}

