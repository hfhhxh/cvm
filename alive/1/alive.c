#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "tinyxml2.h"

int main(){
	while(true){
		tinyxml2::XMLDocument doc;
		int ret = doc.LoadFile("cosconfig.xml");
		if(0 != ret){
			sleep(30);
			continue;
		}
		const char *ip = doc.FirstChildElement("cvm")->FirstChildElement( "cos" )->GetText();
		printf("%s\n", ip);
		short port = 80;
		
		int sock = socket(AF_INET, SOCK_STREAM, 0);
		struct sockaddr_in serv_addr;
		memset(&serv_addr, 0, sizeof(serv_addr));
		serv_addr.sin_family = AF_INET;
		serv_addr.sin_addr.s_addr = inet_addr(ip);
		serv_addr.sin_port = htons(port);
		connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr));
		char buf[1024];
		
		sprintf(buf, "POST /cos/service/cloudvm/alive.php HTTP/1.1\r\nContent-Length: 31\r\nHost: %s\r\n\r\n<cvm><alive>alive</alive></cvm>", ip);
		write(sock, buf, strlen(buf));
		memset(buf, 0, sizeof(buf));
		read(sock, buf, 1024);
		printf("%s\n", buf);
		sleep(30);
	}
	return 0;
}
