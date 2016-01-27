#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <fcntl.h>
#include <sys/un.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>

char *log = "/var/cos/cvm/log/cvm.log";
char *logdir = "/var/cos/cvm/log";
char *dir = "/var/cos/cvm";
char *vmdir = "/var/cos/cvm/vm";
char *cldir = "/var/cos/cvm/vm/cl";
//char *fakedir = "/var/cos/cvm/vm/fake";
char *isodir = "/var/cos/cvm/iso";
char *sysdir = "/var/cos/cvm/sys";
char *dataisodir = "/var/cos/cvm/data/iso";
char *datasysdir = "/var/cos/cvm/data/sys";
char *backupdir = "/var/cos/cvm/data/backup";
char *ifup = "/var/cos/cvm/ovs-ifup";
char *ifdown = "/var/cos/cvm/ovs-ifdown";

const int filelen = 1024;
const int namelen = 1024;
const int cmdlen = 4*1024;
const int resultlen = 64*1024;
char file[filelen], name[namelen], cmd[cmdlen], result[resultlen];

//**********************************************************************************************************************************
//**********************************************************************************************************************************

int setnoblock(int fd){
	int flag = fcntl(fd, F_GETFL, 0);
	flag |= O_NONBLOCK;
	return fcntl(fd, F_SETFL, flag);
}

int setblock(int fd){
	int flag = fcntl(fd, F_GETFL, 0);
	flag &= ~O_NONBLOCK;
	return fcntl(fd, F_SETFL, flag);
}

char *getTime(){
    time_t t;
    time(&t);
    return ctime(&t);
}

void xmlog(char *str){
    FILE *file = fopen(log, "a");
    fprintf(file, "%s%s\n", getTime(), str);
    fclose(file);
}

void getresult(char *cmd, char *result, int olog = 1){
	int fd[2];
	pipe(fd);
	int back_fd = dup(STDOUT_FILENO);
	dup2(fd[1], STDOUT_FILENO);
	system(cmd);
	setnoblock(fd[0]);
	memset(result, 0, resultlen);
	int ret = read(fd[0], result, resultlen-1);
	setblock(fd[0]);
	dup2(back_fd, STDOUT_FILENO);
	if(olog){
		xmlog(cmd);
		xmlog(result);
	}
}

bool fileExist(char *file){
	return 0 == access(file, F_OK);
}

long long fileSize(char *file){
	struct stat buf;
	stat(file, &buf);
	return buf.st_size;
}

bool monitor(int id, char *cmd, char *result, int olog = 1){
	char path[1024];
	sprintf(path, "%s/VM%d", cldir, id);
	struct sockaddr_un un; 
	un.sun_family = AF_UNIX;
	strcpy(un.sun_path, path);
	int sockfd = socket(AF_UNIX, SOCK_STREAM, 0); 
	bind(sockfd, (struct sockaddr*)&un, sizeof(struct sockaddr_un));
	if(connect(sockfd, (struct sockaddr*)&un, sizeof(struct sockaddr_un)) == -1){
		return false;
	}
	usleep(1000);
	read(sockfd, result, resultlen);
	memset(result, 0, resultlen);
	sprintf(result, "%s\n", cmd);
	write(sockfd, result, strlen(result));
	usleep(1000);
	memset(result, 0, resultlen);
	read(sockfd, result, resultlen);
	close(sockfd);
	if(olog){
		xmlog(cmd);
		xmlog(result);
	}
	return true;
}

//rsync, migrate
int rate(unsigned long t, int type){
	if(0 == type){
		sprintf(file, "%s/%lld", logdir, t);
		FILE *f = fopen(file, "r");
		int c = 0;
		while(!feof(f)){
			result[c++] = fgetc(f);
		}
		result[c] = '\0';
		if(strstr(result, "total size is"))return 100;
		char *p = strrchr(result, '%');
		if(p){
			for(; *p != ' '; --p);
			int r;
			sscanf(p, "%d", &r);
			return r;
		}else{
			return 0;
		}
	}else if(1 == type){
		sprintf(file, "%s/%lld", logdir, t);
        FILE *f = fopen(file, "r");
        int c = 0;
        while(!feof(f)){
            result[c++] = fgetc(f);
        }
        result[c] = '\0';
        if(strstr(result, "total size is"))return 100;
        char *p = strrchr(result, '%');
        if(p){
			p -= 2;
            for(; *p != ' '; --p);
            int r;
            sscanf(p, "%d", &r);
			if(r != 100){
            	return r;
			}else{
				*p = 0;
				p = strrchr(result, '%');
				if(!p){
					return 99;
				}else{
					p -= 2;
					for(; *p != ' '; --p);
					sscanf(p, "%d", &r);
					if(r == 100){
						return 100;
					}else{
						return 99;
					}
				}
			}
        }else{
            return 0;
        }
	}
}

int getpid(char *arg){
	sprintf(cmd, "ps -ef | grep \"%s\" | grep -v grep", arg);
    getresult(cmd, result);
    if(result[0] == 0){
        return 0;
    }
    int rs;
    sscanf(result, "%*s%d", &rs);
	return rs;
}

int killpid(int pid){
	sprintf(cmd, "kill -9 %d > /dev/null", pid);
	system(cmd);
	xmlog(cmd);
}

bool exec(char *cmd){
	int ret;
	int sock = socket(AF_INET, SOCK_STREAM, 0);
	struct sockaddr_in serv_addr;
	memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
	serv_addr.sin_port = htons(50000);
	ret = connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr));
	if(-1 == ret)return false;
	ret = write(sock, cmd, strlen(cmd));
	if(-1 == ret)return false;
	return true;
}

//**********************************************************************************************************************************
//**********************************************************************************************************************************

bool vmadd(int type, int arg, int id){
	if(type == 1){
		sprintf(cmd, "qemu-img create -q -f qcow2 -o backing_file=%s/BACK%d %s/VM%d > /dev/null", vmdir, arg, vmdir, id);
		xmlog(cmd);
		system(cmd);
	}else if(type == 2){
		sprintf(cmd, "qemu-img create -q -f qcow2  %s/BACK%d %dG > /dev/null", vmdir,id, arg);
		xmlog(cmd);
		system(cmd);
		vmadd(1, id, id);
			//sprintf(cmd, "qemu-img create -q -f qcow2  %s/%d 1G > /dev/null", fakedir, id, id);
			//xmlog(cmd);
            //system(cmd);
	}
	sprintf(file, "%s/VM%d", vmdir, id);
	if(fileExist(file)){
		return true;
	}else{
		return false;
	}
	return true;
}

bool vmdel(int id){
	sprintf(file, "%s/VM%d", vmdir, id);
	sprintf(cmd, "rm -f %s > /dev/null", file);
	xmlog(cmd);
	system(cmd);
	sprintf(file, "%s/BACK%d", vmdir, id);
    sprintf(cmd, "rm -f %s > /dev/null", file);
    xmlog(cmd);
    system(cmd);
	if(fileExist(file)){
		return false;
	}else{
		return true;
	}
}

bool vmison(int id){
	sprintf(cmd, "ps -ef | grep \"qemu-system-x86_64 -name VM%d \" | grep -v grep | wc -l", id);
	getresult(cmd, result, 0);
	int rs;
	sscanf(result, "%d", &rs);
	return 1 == rs;
}

int vmstatus(int id){
	if(!vmison(id)){
		return 1;
	}
	monitor(id, "info status", result, 0);
	if(strstr(result, "running")){
		return 0;
	}else if(strstr(result, "pause")){
		return 2;
	}else{
		return 3;
	}
//	if(vmison(id)){
//		return 0;
//	}else{
//		return 1;
//	}
}

bool vmon(int id, int cpu, int mem, char *mac){
	if(vmison(id)){
		return true;
	}
	sprintf(file, "%s/VM%d", vmdir, id);
	sprintf(cmd, "qemu-system-x86_64 -name VM%d -smp %d -m %d -rtc base=localtime -drive file=%s,if=virtio,media=disk,index=0 -vnc :%d -net nic,macaddr=%s,model=virtio -net tap,script=%s,downscript=%s,ifname=VM%d -usb -usbdevice tablet -enable-kvm -chardev socket,id=VM%d,path=%s/VM%d,server,nowait -monitor chardev:VM%d > /dev/null 2>&1 &", id, cpu, mem, file, id+100, mac, ifup, ifdown, id, id, cldir, id, id);
	xmlog(cmd);
//	system(cmd);
	exec(cmd);
	return true;

	usleep(10000);
	if(vmison(id)){
		return true;
	}else{
		return false;
	}
}

bool vmoff(int id){
	if(!vmison(id)){
		return true;
	}
	if(monitor(id, "system_powerdown", result)){
		return true;
	}else{
		return false;
	}
}

bool vmhalt(int id){
	if(!vmison(id)){
		return true;
	}
//	if(monitor(id, "quit", result)){
//  	return true;
//  }else{
		sprintf(cmd, "ps -ef | grep \"qemu-system-x86_64 -name VM%d\" | grep -v grep", id);
		getresult(cmd, result);
		if(result[0] == 0){
			return true;
		}
    	int rs;                                                                                                         
	    sscanf(result, "%*s%d", &rs);                        
		sprintf(cmd, "kill -9 %d > /dev/null", rs);
		xmlog(cmd);
		system(cmd);
		return true;
//	}
}

bool vmreset(int id){
	if(monitor(id, "system_reset", result)){
		return true;
	}else{
		return false;
	}
}

bool vmstop(int id){
	if(monitor(id, "stop", result)){
		return true;
	}else{
		return false;
	}
}

bool vmcontinue(int id){
	if(monitor(id, "c", result)){
		return true;
	}else{
		return false;
	}
}

//**********************************************************************************************************************************
//**********************************************************************************************************************************

//1--vm,	base,id...[-1]
//2--tp,	disk,id...[-1]
void addvm(){
	int type, arg, id;
	scanf("%d%d", &type, &arg);
	while(true){
		scanf("%d", &id);
		if(id == -1)break;
		if(vmadd(type, arg, id)){
			printf("ok\n");
		}else{
			printf("error\n");
		}
		fflush(stdout);
	}
}

//id...[-1]
void delvm(){
	int id;
	while(true){
		scanf("%d", &id);
		if(-1 == id){
			break;
		}
		if(vmdel(id)){
			printf("ok\n");
		}else{
			printf("error\n");
		}
		fflush(stdout);
	}
}

//0 on 1 off 2 stop 4 unknown
void ppower_status(){
	int id;
	while(true){
		scanf("%d", &id);
		if(-1 == id)break;
		int ret = vmstatus(id);
		if(0 == ret){
			printf("on\n");
		}else if(1 == ret){
			printf("off\n");
		}else if(2 == ret){
			printf("stop\n");
		}else{
			printf("unknown\n");
		}
		fflush(stdout);
	}
}

//id,cpu,mem,mac
void ppower_on(){
	int id, cpu, mem;
	char mac[1024];
	while(true){
		scanf("%d", &id);
		if(-1 == id)break;	
		scanf("%d %d %s", &cpu, &mem, mac);
		if(vmon(id, cpu, mem, mac)){
			printf("ok\n");
		}else{
			printf("error\n");
		}
		fflush(stdout);
	}
}

void ppower_off(){
	int id;
	while(true){
		scanf("%d", &id);
		if(-1 == id)break;	
		if(vmoff(id)){
			printf("ok\n");
		}else{
			printf("error\n");
		}
		fflush(stdout);
	}
}

void ppower_halt(){
	int id;
	while(true){
		scanf("%d", &id);
		if(-1 == id)break;	
		if(vmhalt(id)){
			printf("ok\n");
		}else{
			printf("error\n");
		}
		fflush(stdout);
	}
}

void ppower_reset(){
	int id;
	while(true){
		scanf("%d", &id);
		if(-1 == id)break;	
		if(vmreset(id)){
			printf("ok\n");
		}else{
			printf("error\n");
		}
		fflush(stdout);
	}
}

void ppower_stop(){
	int id;
	while(true){
		scanf("%d", &id);
		if(-1 == id)break;	
		if(vmstop(id)){
			printf("ok\n");
		}else{
			printf("error\n");
		}
		fflush(stdout);
	}
}

void ppower_continue(){
	int id;
	while(true){
		scanf("%d", &id);
		if(-1 == id)break;	
		if(vmcontinue(id)){
			printf("ok\n");
		}else{
			printf("error\n");
		}
		fflush(stdout);
	}
}

//0 on 1 off 2 pause 4 unknown
void power_status(){
	int id;
	scanf("%d", &id);
	int ret = vmstatus(id);
	if(0 == ret){
		printf("on\n");
	}else if(1 == ret){
		printf("off\n");
	}else if(2 == ret){
		printf("stop\n");
	}else{
		printf("unknown\n");
	}
}

//id,cpu,mem,mac
void power_on(){
    int id, cpu, mem;
    char mac[1024];
    scanf("%d %d %d %s", &id, &cpu, &mem, mac);
    if(vmon(id, cpu, mem, mac)){
        printf("ok\n");
    }else{
        printf("error\n");
    }
}

void power_off(){
    int id;
    scanf("%d", &id);
    if(vmoff(id)){
        printf("ok\n");
    }else{
        printf("error\n");
    }
}

void power_halt(){
    int id;
    scanf("%d", &id);                                                                                                         
    if(vmhalt(id)){                                                                                                           
        printf("ok\n");                                                                                                       
    }else{                                                                                                                    
        printf("error\n");                                                                                                    
    }                                                                                                                         
}                                                                                                                             
                                                                                                                              
void power_reset(){                                                                                                           
    int id;                                                                                                                   
    scanf("%d", &id);                                                                                                         
    if(vmreset(id)){                                                                                                          
        printf("ok\n");                                                                                                       
    }else{                                                                                                                    
        printf("error\n");                                                                                                    
    }                                                                                                                         
}                                                                                                                             
                                                                                                                              
void power_stop(){                                                                                                            
    int id;                                                                                                                   
    scanf("%d", &id);                                                                                                         
    if(vmstop(id)){                                                                                                           
        printf("ok\n");                                                                                                       
    }else{                                                                                                                    
        printf("error\n");                                                                                                    
    }                                                                                                                         
}                                                                                                                             
                                                                                                                              
void power_continue(){                                                                                                        
    int id;                                                                                                                   
    scanf("%d", &id);                                                                                                         
    if(vmcontinue(id)){                                                                                                       
        printf("ok\n");                                                                                                       
    }else{                                                                                                                    
        printf("error\n");                                                                                                    
    }                                                                                                                         
}

void power_commit(){
	int id;
	scanf("%d", &id);
	sprintf(cmd, "qemu-img commit -f qcow2 %s/VM%d > /dev/null", vmdir, id);
	xmlog(cmd);
	system(cmd);
	printf("ok\n");
}

void power_drop(){
	int id;
	scanf("%d", &id);
	vmadd(1, id, id);
	printf("ok\n");
}

void power_rebuild(){
    int base, id;
    scanf("%d", &base);
    while(true){
        scanf("%d", &id);
        if(id == -1)break;
        vmadd(1, base, id);
    }
    printf("ok\n");
}

void power_compare(){
	int id;
	scanf("%d", &id);
	sprintf(file, "%s/VM%d", vmdir, id);
	long long ret = fileSize(file);
	if(ret < 1000000){
		printf("ok\n");
	}else{
		printf("error\n");
	}
}

void snapshot_list(){
	int id;	
	scanf("%d", &id);
	sprintf(cmd, "qemu-img snapshot -l %s/VM%d", vmdir, id);
	xmlog(cmd);
	getresult(cmd, result);
	if(result[0] == 0){
		printf("-1\n");
		return;
	}
	char *p = strchr(result, 10);
    ++p;
    p = strchr(p, 10);
    ++p;
    while(*p){
        sscanf(p, "%*d%s", name);
        printf("%s\n", name);
        p = strchr(p, 10);
        ++p;
    }
    printf("-1\n");
}

void snapshot_create(){
	int id;
	scanf("%d %s", &id, name);
	sprintf(cmd, "savevm %s", name);
	if(monitor(id, cmd, result)){
		printf("ok\n");	
	}else{
		printf("error\n");	
	}
}

void snapshot_del(){
	int id;
	scanf("%d %s", &id, name);
	sprintf(cmd, "delvm %s", name);
	if(monitor(id, cmd, result)){
		printf("ok\n");	
	}else{
		printf("error\n");	
	}
}

void snapshot_apply(){
	int id;
	scanf("%d %s", &id, name);
	sprintf(cmd, "loadvm %s", name);
	if(monitor(id, cmd, result)){
		printf("ok\n");	
	}else{
		printf("error\n");	
	}
}

void cd_list(){
    sprintf(cmd, "ls %s", isodir);
    getresult(cmd, result);
	if(0 == result[0]){
		printf("-1\n");
		return;
	}
    char *p = result;
    while(*p){
        sscanf(p, "%s", name);
        printf("%s\n", name);
        p = strchr(p, 10);
        ++p;
    }
    printf("-1\n");
}

void cd_mounted(){
	int id;
	scanf("%d", &id);
	monitor(id, "info block ide1-cd0", result);
	if(strstr(result, "ide1-cd0: [not inserted]")){
		printf("null\n");	
	}else {
		char *p  = strstr(result, ".iso");
		*p = 0;
		while(*(p-1) != '/'){
			--p;
		}	
		printf("%s\n", p);
	}
}

void cd_mount(){
	int id;
	scanf("%d%s", &id, name);
	sprintf(cmd, "change ide1-cd0 %s/%s", isodir, name);
	if(monitor(id, cmd, result)){
		printf("ok\n");	
	}else{
		printf("error\n");	
	}
}


void cd_unmount(){
	int id;
	scanf("%d", &id);
	if(monitor(id, "eject -f ide1-cd0", result)){
		printf("ok\n");	
	}else{
		printf("error\n");	
	}
}

void os_list(){
	sprintf(cmd, "ls %s", sysdir);
    getresult(cmd, result);
	if(0 == result[0]){
		printf("-1\n");
		return;
	}
    char *p = result;
    while(*p){
        sscanf(p, "%s", name);
        printf("%s\n", name);
        p = strchr(p, 10);
        ++p;
    }
    printf("-1\n");
}

void os_import(){
	int id;
	scanf("%s%d", name, &id);
	time_t t;
    time(&t);
	sprintf(file, "%s/%s", sysdir, name);
	sprintf(cmd, "qemu-img info %s", file);
	getresult(cmd, result);
	int disk;
	char *p = strstr(result, "virtual size: ") + strlen("virtual size: ");
	sscanf(p, "%d", &disk);
	sprintf(cmd, "rsync -avP %s %s/BACK%d > %s/%lld &", file, vmdir, id, logdir, t);
	xmlog(cmd);
	system(cmd);
	printf("%d\n", disk);
	printf("%lld\n", t);
}

void os_query(){
	long long t;
	scanf("%lld", &t);
	printf("%d\n", rate(t, 0));
}

void os_rebuild(){
	int base, id;
	scanf("%d", &base);
	while(true){
		scanf("%d", &id);
		if(id == -1)break;
		vmadd(1, base, id);
	}
	printf("ok\n");
}

void sync_sync(){
	int id;
	char ip[32];
	scanf("%d%s", &id, ip);
	time_t t;
    time(&t);
	sprintf(cmd, "rsync -avzP %s/BACK%d root@%s:%s/BACK%d > %s/%lld &", vmdir,id, ip, vmdir, id, logdir, t);
    xmlog(cmd);
    system(cmd);
    printf("%lld\n", t);	
}

void sync_query(){
	long long t;
    scanf("%lld", &t);
    printf("%d\n", rate(t, 0));
}

void sync_rebuild(){
    int base, id;
    scanf("%d", &base);
    while(true){
        scanf("%d", &id);
        if(id == -1)break;
        vmadd(1, base, id);
    }
    printf("ok\n");
}

void migrate_send0(){
	int id;
	char ip[32];
	scanf("%d%s", &id, ip);
	sprintf(cmd, "migrate -d -i tcp:%s:%d", ip, id+8000);
	if(monitor(id, cmd, result)){
		printf("ok\n");
	}else{
		printf("error\n");
	}
}

void migrate_recv0(){
	int id, cpu, mem, base;
    char mac[1024];
    scanf("%d %d %d %s %d", &id, &cpu, &mem, mac, &base);
	if(vmison(id)){
		vmhalt(id);
	}
	time_t t;
    time(&t);
	vmadd(1, base, id);
	sprintf(file, "%s/VM%d", vmdir, id);
	sprintf(cmd, "qemu-system-x86_64 -name VM%d -smp %d -m %d -rtc base=localtime -drive file=%s,if=virtio,media=disk,index=0 -vnc :%d -net nic,macaddr=%s,model=virtio -net tap,script=%s,downscript=%s,ifname=VM%d -usb -usbdevice tablet -enable-kvm -chardev socket,id=VM%d,path=%s/VM%d,server,nowait -monitor chardev:VM%d -incoming tcp:0:%d > %s/%lld &", id, cpu, mem, file, id+100, mac, ifup, ifdown, id, id, cldir, id, id, id+8000, logdir, t);
	xmlog(cmd);
//	system(cmd);
	exec(cmd);
	printf("%lld\n", t);
}

void migrate_query0(){
	long long t;
    scanf("%lld", &t);                                                                                                       
    printf("%d\n", rate(t, 1));  	
}

void migrate_cancel0(){
	int id;
	scanf("%d", &id);
	if(monitor(id, "migrate_cancel", result)){
		printf("ok\n");
    }else{
		printf("error\n");
    }
}

void migrate_send1(){
	int id;
	char ip[32];
    scanf("%d%s", &id, ip);
    time_t t;                                                                                                                
    time(&t);                                                                                                                
    sprintf(cmd, "rsync -avzP %s/VM%d root@%s:%s/VM%d > %s/%lld &", vmdir,id, ip, vmdir, id, logdir, t);                 
    xmlog(cmd);                                                                                                              
    system(cmd);                                                                                                     
    printf("%lld\n", t);
}

void migrate_query1(){
	long long t;
    scanf("%lld", &t);
    printf("%d\n", rate(t, 0));
}

void migrate_cancel1(){
	int id;
    char ip[32];
    scanf("%d%s", &id, ip);
    sprintf(name, "rsync -avzP %s/VM%d root@%s:%s/VM%d", vmdir,id, ip, vmdir, id);
	int pid;
	while(pid = getpid(name)){
		killpid(pid);
		usleep(100000);
	}
	printf("ok\n");
}

void sys_list(){
	sprintf(cmd, "ls %s", datasysdir);
    getresult(cmd, result);
    if(0 == result[0]){
        printf("-1\n");
        return;
    }
    char *p = result;
    while(*p){
        sscanf(p, "%s", name);
        printf("%s\n", name);
        p = strchr(p, 10);
        ++p;
    }
    printf("-1\n");
}

void sys_rsync(){
    char ip[32];
    scanf("%s%s", name, ip);
    time_t t;
    time(&t);
    sprintf(cmd, "rsync -avzP %s/%s root@%s:%s/%s > %s/%lld &", datasysdir, name, ip, sysdir, name, logdir, t);
    xmlog(cmd);                                                                                                              
    system(cmd);                                                                                                             
    printf("%lld\n", t);
}

void iso_list(){
	sprintf(cmd, "ls %s", dataisodir);
    getresult(cmd, result);
    if(0 == result[0]){
        printf("-1\n");
        return;
    }
    char *p = result;
    while(*p){
        sscanf(p, "%s", name);
        printf("%s\n", name);
        p = strchr(p, 10);
        ++p;
    }
    printf("-1\n");
}

void iso_rsync(){
    char ip[32];
    scanf("%s%s", name, ip);
    time_t t;
    time(&t);
    sprintf(cmd, "rsync -avzP %s/%s root@%s:%s/%s > %s/%lld &", dataisodir, name, ip, isodir, name, logdir, t);
    xmlog(cmd);                                                                                                              
    system(cmd);                                                                                                             
    printf("%lld\n", t);
}

void sysiso_query(){
	long long t;
    scanf("%lld", &t);
    printf("%d\n", rate(t, 0));
}

void backup_backup(){
	int vmid, baseid, backid;
	char ip[32];
	scanf("%d%d%d%s", &vmid, &baseid, &backid, ip);
    time_t t;
    time(&t);
    sprintf(cmd, "rsync -avzP %s/BACK%d root@%s:%s/BACK%d > %s/%lld &", vmdir, baseid, ip, backupdir, backid, logdir, t);
    xmlog(cmd);
    system(cmd);                                                                                       
    sprintf(cmd, "rsync -avzP %s/VM%d root@%s:%s/VM%d > %s/%lld &", vmdir, vmid, ip, backupdir, backid, logdir, t+1);
    xmlog(cmd);
    system(cmd);                                                                                                     
    printf("%lld\n", t);
}

void backup_restore(){
	int vmid, backid;
    char ip[32];
    scanf("%d%d%s", &vmid, &backid, ip);
    sprintf(cmd, "qemu-img rebase -f qcow2 -u -b %s/BACK%d %s/VM%d > /dev/null &", vmdir, vmid, backupdir, backid);
    xmlog(cmd);
    system(cmd);
	time_t t;
    time(&t);
    sprintf(cmd, "rsync -avzP %s/BACK%d root@%s:%s/BACK%d > %s/%lld &", backupdir, backid, ip, vmdir, vmid, logdir, t);
    xmlog(cmd);
    system(cmd);
    sprintf(cmd, "rsync -avzP %s/VM%d root@%s:%s/VM%d > %s/%lld &", backupdir, backid, ip, vmdir, vmid, logdir, t+1);
    xmlog(cmd);
    system(cmd);
    printf("%lld\n", t);
}

void backup_query(){
	long long t;
    scanf("%lld", &t);
    printf("%d\n", (rate(t, 0)+rate(t+1, 0))>>1);	
}

//**********************************************************************************************************************************
//**********************************************************************************************************************************

int main(int argc, char **argv){

	setuid(geteuid());
    setgid(getegid());

	char cmd[1024];
	int ctl;
	scanf("%s", cmd);
	if(strcmp(cmd, "addvm") == 0){
		addvm();
	}else if(strcmp(cmd, "delvm") == 0){
		delvm();	
	}else if(strcmp(cmd, "power") == 0){
		scanf("%d", &ctl);
		//status, on, off, reset, stop, continue, halt, commit, drop, rebuild, compare
		if(1 == ctl){
			power_status();
		}else if(2 == ctl){
			power_on();
		}else if(3 == ctl){
			power_off();
		}else if(4 == ctl){
			power_reset();
		}else if(5 == ctl){
			power_stop();
		}else if(6 == ctl){
			power_continue();
		}else if(7 == ctl){
			power_halt();
		}else if(8 == ctl){
			power_commit();
		}else if(9 == ctl){
			power_drop();
		}else if(10 == ctl){
			power_rebuild();
		}else if(11 == ctl){
			power_compare();
		}
	}else if(strcmp(cmd, "snapshot") == 0){
		scanf("%d", &ctl);
		//list, create, del, apply
		if(1 == ctl){
			snapshot_list();
		}else if(2 == ctl){
			snapshot_create();
		}else if(3 == ctl){
			snapshot_del();
		}else if(4 == ctl){
			snapshot_apply();
		}
	}else if(strcmp(cmd, "cd") == 0){
		scanf("%d", &ctl);
		//list, mounted, mount, unmount
		if(1 == ctl){
			cd_list();
		}else if(2 == ctl){
			cd_mounted();
		}else if(3 == ctl){
			cd_mount();
		}else if(4 == ctl){
			cd_unmount();
		}
	}else if(strcmp(cmd, "ppower") == 0){
		scanf("%d", &ctl);
		//status, on, off, reset, stop, continue, halt
		if(1 == ctl){
			ppower_status();
		}else if(2 == ctl){
			ppower_on();
		}else if(3 == ctl){
			ppower_off();
		}else if(4 == ctl){
			ppower_reset();
		}else if(5 == ctl){
			ppower_stop();
		}else if(6 == ctl){
			ppower_continue();
		}else if(7 == ctl){
			ppower_halt();
		}
	}else if(strcmp(cmd, "os") == 0){
		scanf("%d", &ctl);
		//list, import, query, rebuild
		if(1 == ctl){
			os_list();
		}else if(2 == ctl){
			os_import();
		}else if(3 == ctl){
			os_query();
		}else if(4 == ctl){
			os_rebuild();
		}
	}else if(strcmp(cmd, "sync") == 0){
		scanf("%d", &ctl);
		//sync, query, rebuild
		if(1 == ctl){
			sync_sync();
		}else if(2 == ctl){
			sync_query();
		}else if(3 == ctl){
			sync_rebuild();
		}
	}else if(strcmp(cmd, "migrate") == 0){
		scanf("%d", &ctl);
		//hot send, host recv, cold send, cold recv, hot query, cold query, hot cancel, cold cancel
		if(1 == ctl){
			migrate_send0();
		}else if(2 == ctl){
			migrate_recv0();
		}else if(3 == ctl){
			migrate_send1();
		}else if(4 == ctl){
//			migrate_recv1();
		}else if(5 == ctl){
			migrate_query0();
		}else if(6 == ctl){
			migrate_query1();
		}else if(7 == ctl){
			migrate_cancel0();
		}else if(8 == ctl){
			migrate_cancel1();
		}
	}else if(strcmp(cmd, "distb") == 0){
		scanf("%d", &ctl);
		//sys list、rsync、query, iso list、rsync、query
		if(1 == ctl){
			sys_list();
		}else if(2 == ctl){
			sys_rsync();
		}else if(3 == ctl){
			iso_list();
		}else if(4 == ctl){
			iso_rsync();
		}else if(5 == ctl){
			sysiso_query();
		}
	}else if(strcmp(cmd, "backup") == 0){
		scanf("%d", &ctl);
		//backup, restore, query
		if(1 == ctl){
			backup_backup();
		}else if(2 == ctl){
			backup_restore();
		}else if(3 == ctl){
			backup_query();
		}
	}else{

	}
	return 0;
}
