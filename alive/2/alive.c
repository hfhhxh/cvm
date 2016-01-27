#include <stdio.h>
#include <winsock2.h>
#include <unistd.h>
#include "tinyxml2.h"

#pragma comment (lib, "ws2_32.lib")

int main(){
	while(true){
		tinyxml2::XMLDocument doc;
		int ret = doc.LoadFile("cosconfig.xml");
		if(0 != ret){
			sleep(1000);
			continue;
		}
		const char *ip = doc.FirstChildElement("cvm")->FirstChildElement( "cos" )->GetText();
		printf("%s\n", ip);
		short port = 80;
		
		WSADATA wsaData;
		WSAStartup( MAKEWORD(2, 2), &wsaData);
		SOCKET sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
		sockaddr_in sockAddr;
		memset(&sockAddr, 0, sizeof(sockAddr));
		sockAddr.sin_family = PF_INET;
		sockAddr.sin_addr.s_addr = inet_addr(ip);
		sockAddr.sin_port = htons(port);
		connect(sock, (SOCKADDR*)&sockAddr, sizeof(SOCKADDR));

		char buf[1024];
		sprintf(buf, "POST /cos/service/cloudvm/alive.php HTTP/1.1\r\nContent-Length: 31\r\nHost: %s\r\n\r\n<cvm><alive>alive</alive></cvm>", ip);
		send(sock, buf, strlen(buf), NULL);
		memset(buf, 0, sizeof(buf));
		recv(sock, buf, 1024, NULL);
		printf("%s\n", buf);
		sleep(30);
	}
	return 0;
}
