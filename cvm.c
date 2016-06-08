//ln -s /usr/libexec/qemu-kvm /usr/bin/qemu-kvm
//ln -s /usr/bin/qemu-kvm /usr/bin/qemu-kvm
//ln -s /usr/libexec/qemu-kvm /usr/bin/qemu-kvm
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

const char *log = "/var/cos/cvm/log/cvm.log";
const char *logdir = "/var/cos/cvm/log";
const char *dir = "/var/cos/cvm";
const char *vmdir = "/var/cos/cvm/vm";
const char *diskdir = "/var/cos/cvm/disk";
const char *cldir = "/var/cos/cvm/vm/cl";
const char *qmdir = "/var/cos/cvm/vm/qm";
const char *isodir = "/var/cos/cvm/iso";
const char *sysdir = "/var/cos/cvm/sys";
const char *dataisodir = "/var/cos/cvm/data/iso";
const char *datasysdir = "/var/cos/cvm/data/sys";
const char *backupdir = "/var/cos/cvm/data/backup";
const char *ifup = "/var/cos/cvm/qemu-ifup";
const char *ifdown = "/var/cos/cvm/qemu-ifdown";

const int filelen = 1024;
const int namelen = 1024;
const int cmdlen = 4*1024;
const int resultlen = 64*1024;
char file[filelen], name[namelen], cmd[cmdlen], result[resultlen], result2[resultlen];

//**********************************************************************************************************************************
//**********************************************************************************************************************************

int setnoblock(int fd) {
	int flag = fcntl(fd, F_GETFL, 0);
	flag |= O_NONBLOCK;
	return fcntl(fd, F_SETFL, flag);
}

int setblock(int fd) {
	int flag = fcntl(fd, F_GETFL, 0);
	flag &= ~O_NONBLOCK;
	return fcntl(fd, F_SETFL, flag);
}

char *getTime() {
  time_t t;
  time(&t);
  return ctime(&t);
}

void xmlog(char *str) {
  FILE *file = fopen(log, "a");
  fprintf(file, "%s%s\n", getTime(), str);
  fclose(file);
}

void getresult(char *cmd, char *result, int olog = 1) {
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
	if(olog) {
		xmlog(cmd);
		xmlog(result);
	}
}

bool fileExist(char *file) {
	return 0 == access(file, F_OK);
}

long long fileSize(char *file) {
	struct stat buf;
	stat(file, &buf);
	return buf.st_size;
}

bool monitor(int id, char *cmd, char *result, int olog = 1) {
	char path[1024];
	sprintf(path, "%s/CL%d", cldir, id);
	struct sockaddr_un un;
	un.sun_family = AF_UNIX;
	strcpy(un.sun_path, path);
	int sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
	bind(sockfd, (struct sockaddr*)&un, sizeof(struct sockaddr_un));
	if(connect(sockfd, (struct sockaddr*)&un, sizeof(struct sockaddr_un)) == -1) {
		return false;
	}
	usleep(100000);
	read(sockfd, result, resultlen);
//	printf("read: %s\n", result);
	memset(result, 0, resultlen);
//	printf("write: %s\n", cmd);
	sprintf(result, "%s\n", cmd);
	write(sockfd, result, strlen(result));
	usleep(100000);
	memset(result, 0, resultlen);
	read(sockfd, result, resultlen);
//	printf("read: %s\n", result);
	close(sockfd);
	if(olog) {
		xmlog(cmd);
		xmlog(result);
	//	printf(": %s\n: %s\n", cmd, result);
	}
	return true;
}

//rsync, migrate
int rate(unsigned long t, int type) {
	if(0 == type) {
		sprintf(file, "%s/%lld", logdir, t);
		FILE *f = fopen(file, "r");
		int c = 0;
		while(!feof(f)) {
			result[c++] = fgetc(f);
		}
		result[c] = '\0';
		if(strstr(result, "total size is"))return 100;
		char *p = strrchr(result, '%');
		if(p) {
			for(; *p != ' '; --p);
			int r;
			sscanf(p, "%d", &r);
			return r;
		} else{
			return 0;
		}
	} else if(1 == type) {
		sprintf(file, "%s/%lld", logdir, t);
    FILE *f = fopen(file, "r");
    int c = 0;
    while(!feof(f)) {
      result[c++] = fgetc(f);
    }
    result[c] = '\0';
    if(strstr(result, "total size is"))return 100;
    char *p = strrchr(result, '%');
    if(p) {
			p -= 2;
      for(; *p != ' '; --p);
      int r;
      sscanf(p, "%d", &r);
			if(r != 100) {
        return r;
			} else{
				*p = 0;
				p = strrchr(result, '%');
				if(!p) {
					return 99;
				} else{
					p -= 2;
					for(; *p != ' '; --p);
					sscanf(p, "%d", &r);
					if(r == 100) {
						return 100;
					} else{
						return 99;
					}
				}
			}
      } else{
        return 0;
      }
	}
}

int getpid(char *arg) {
	sprintf(cmd, "ps -ef | grep \"%s\" | grep -v grep", arg);
  getresult(cmd, result);
  if(result[0] == 0) {
    return 0;
  }
  int rs;
  sscanf(result, "%*s%d", &rs);
	return rs;
}

int killpid(int pid) {
	sprintf(cmd, "kill -9 %d > /dev/null 2>&1", pid);
	system(cmd);
	xmlog(cmd);
}

bool exec(char *cmd) {
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

bool vmadd(int type, int arg, int id) {
	if(type == 1) {
		sprintf(cmd, "qemu-img create -f qcow2 -o backing_file=%s/BACK%d %s/VM%d > /dev/null 2>&1", vmdir, arg, vmdir, id);
		xmlog(cmd);
		system(cmd);
	} else if(type == 2) {
		sprintf(cmd, "qemu-img create -f qcow2  %s/BACK%d %dG > /dev/null 2>&1", vmdir,id, arg);
		xmlog(cmd);
		system(cmd);
		vmadd(1, id, id);
	}
	sprintf(file, "%s/VM%d", vmdir, id);
	if(fileExist(file)) {
		return true;
	} else{
		return false;
	}
	return true;
}

bool vmdel(int id) {
	sprintf(file, "%s/VM%d", vmdir, id);
	sprintf(cmd, "rm -f %s > /dev/null 2>&1", file);
	xmlog(cmd);
	system(cmd);
	sprintf(file, "%s/BACK%d", vmdir, id);
  sprintf(cmd, "rm -f %s > /dev/null 2>&1", file);
  xmlog(cmd);
  system(cmd);
	if(fileExist(file)) {
		return false;
	} else{
		return true;
	}
}

bool vmison(int id) {
	sprintf(cmd, "ps -ef | grep \"qemu-kvm -name VM%d \" | grep -v grep | wc -l", id);
	getresult(cmd, result, 0);
	int rs;
	sscanf(result, "%d", &rs);
	return 1 == rs;
}

int vmstatus(int id) {
/*
	if(!vmison(id)) {
		return 1;
	}
	monitor(id, "info status", result, 0);
	if(strstr(result, "running")) {
		return 0;
	} else if(strstr(result, "pause")) {
		return 2;
	} else{
		return 3;
	}
*/
	if(vmison(id)) {
		return 0;
	} else{
		return 1;
	}
}

bool vmon(int id, int cpu, int mem, char *mac, int type=0, char *br=NULL) {
	if(vmison(id)) {
		return true;
	}
	sprintf(file, "%s/VM%d", vmdir, id);
	sprintf(cmd, "qemu-kvm -name VM%d -smp %d,maxcpus=240 -m %d -rtc base=localtime -drive file=%s,if=virtio,media=disk,index=0 -vnc :%d -net nic,macaddr=%s,model=virtio -net tap,script=%s,downscript=%s,ifname=VM%d -usb -usbdevice tablet -enable-kvm -chardev socket,id=CL%d,path=%s/CL%d,server,nowait -monitor chardev:CL%d -chardev socket,id=QM%d,path=%s/QM%d,server,nowait -qmp chardev:QM%d > /dev/null 2>&1 &", id, cpu, mem, file, id+100, mac, ifup, ifdown, id, id, cldir, id, id, id, qmdir, id, id);
	xmlog(cmd);
//	printf("%s\n", cmd);
//	system(cmd);
	exec(cmd);
//	return true;

	usleep(100000);
//		printf("type=%d br=%s\n", type, br);
	if(vmison(id)) {
/*
		if(0 == type) {	//bridge
			sprintf(cmd, "/sbin/ifconfig VM%d up; brctl addif br0 VM%d", id, id);
			xmlog(cmd);
			system(cmd);
		} else if(1 == type) {	//vm-alone
			//NULL
		} else if(2 == type) {	//vswitch
			sprintf(cmd, "/sbin/ifconfig VM%d up; brctl addif %s VM%d", id, br, id);
			xmlog(cmd);
			system(cmd);
		} else if(3 == type) {	//host-only
			sprintf(cmd, "/sbin/ifconfig VM%d up; brctl addif br1 VM%d", id, id);
			xmlog(cmd);
			system(cmd);
		} else if(4 == type) {	//nat

		}
*/
		monitor(id, "loadvm init", result);
		monitor(id, "c", result);
		return true;
	} else{
		return false;
	}
}

bool vmoff(int id) {
	if(!vmison(id)) {
		return true;
	}
	if(monitor(id, "system_powerdown", result)) {
		return true;
	} else{
		return false;
	}
}

bool vmhalt(int id) {
	if(!vmison(id)) {
		return true;
	}
  sprintf(cmd, "ps -ef | grep \"qemu-kvm -name VM%d \" | grep -v grep", id);
  getresult(cmd, result);
  if(result[0] == 0) {
    return true;
  }
  int rs;
  sscanf(result, "%*s%d", &rs);
  sprintf(cmd, "kill -9 %d > /dev/null 2>&1", rs);
  xmlog(cmd);
  system(cmd);
	usleep(100000);
	if(!vmison(id)) {
		return true;
	} else {
 		return false;
	}
}

bool vmreset(int id) {
	if(monitor(id, "system_reset", result)) {
		return true;
	} else{
		return false;
	}
}

bool vmstop(int id) {
	if(monitor(id, "stop", result)) {
		return true;
	} else{
		return false;
	}
}

bool vmcontinue(int id) {
	if(monitor(id, "c", result)) {
		return true;
	} else{
		return false;
	}
}


void diskmount(int id, char *disk){
/*
	sprintf(cmd, );
	sprintf(file, "%s/%s", diskdir, name);
	sprintf(cmd, "drive_add 0 if=none,file=%s,format=qcow2,id=%s", file, name);
//	printf("%s\n", cmd);
	if(!monitor(id, cmd, result)) {
		printf("error\n");
	}
	sprintf(cmd, "device_add virtio-blk-pci,drive=%s,id=%s", name, name);
//	printf("%s\n", cmd);
	if(monitor(id, cmd, result)) {
		printf("ok\n");
	} else {
		printf("error\n");
	}
*/
}

//**********************************************************************************************************************************
//**********************************************************************************************************************************

//1--vm,	base,id...[-1]
//2--tp,	disk,id...[-1]
void addvm() {
	int type, arg, id;
	scanf("%d%d", &type, &arg);
	while(true) {
		scanf("%d", &id);
		if(id == -1)break;
		if(vmadd(type, arg, id)) {
			printf("ok\n");
		} else{
			printf("error\n");
		}
		fflush(stdout);
	}
}

//id...[-1]
void delvm() {
	int id;
	while(true) {
		scanf("%d", &id);
		if(-1 == id) {
			break;
		}
		if(vmdel(id)) {
			printf("ok\n");
		} else{
			printf("error\n");
		}
		fflush(stdout);
	}
}

//0 on 1 off 2 stop 4 unknown
void ppower_status() {
	int id;
	while(true) {
		scanf("%d", &id);
		if(-1 == id)break;
		int ret = vmstatus(id);
		if(0 == ret) {
			printf("on\n");
		} else if(1 == ret) {
			printf("off\n");
		} else if(2 == ret) {
			printf("stop\n");
		} else{
			printf("unknown\n");
		}
		fflush(stdout);
	}
}

//id,cpu,mem,mac
void ppower_on() {
	int id, cpu, mem;
	char mac[1024];
	int type; char br[32];
	while(true) {
		scanf("%d", &id);
		if(-1 == id)break;
//		scanf("%d %d %s %d %s", &cpu, &mem, mac, &type, br);
		scanf("%d %d %s", &cpu, &mem, mac);
		if(vmon(id, cpu, mem, mac, type, br)) {
			printf("ok\n");
		} else{
			printf("error\n");
		}
		fflush(stdout);
	}
}

void ppower_off() {
	int id;
	while(true) {
		scanf("%d", &id);
		if(-1 == id)break;
		if(vmoff(id)) {
			printf("ok\n");
		} else{
			printf("error\n");
		}
		fflush(stdout);
	}
}

void ppower_halt() {
	int id;
	while(true) {
		scanf("%d", &id);
		if(-1 == id)break;
		if(vmhalt(id)) {
			printf("ok\n");
		} else{
			printf("error\n");
		}
		fflush(stdout);
	}
}

void ppower_reset() {
	int id;
	while(true) {
		scanf("%d", &id);
		if(-1 == id)break;
		if(vmreset(id)) {
			printf("ok\n");
		} else{
			printf("error\n");
		}
		fflush(stdout);
	}
}

void ppower_stop() {
	int id;
	while(true) {
		scanf("%d", &id);
		if(-1 == id)break;
		if(vmstop(id)) {
			printf("ok\n");
		} else{
			printf("error\n");
		}
		fflush(stdout);
	}
}

void ppower_continue() {
	int id;
	while(true) {
		scanf("%d", &id);
		if(-1 == id)break;
		if(vmcontinue(id)) {
			printf("ok\n");
		} else{
			printf("error\n");
		}
		fflush(stdout);
	}
}

//0 on 1 off 2 pause 4 unknown
void power_status() {
	int id;
	scanf("%d", &id);
	int ret = vmstatus(id);
	if(0 == ret) {
		printf("on\n");
	} else if(1 == ret) {
		printf("off\n");
	} else if(2 == ret) {
		printf("stop\n");
	} else{
		printf("unknown\n");
	}
}

//id,cpu,mem,mac
void power_on() {
  int id, cpu, mem;
  char mac[1024];
	int type; char br[32];
//  scanf("%d %d %d %s %d %s", &id, &cpu, &mem, mac, &type, br);
  scanf("%d %d %d %s", &id, &cpu, &mem, mac);
  if(vmon(id, cpu, mem, mac, type, br)) {
    printf("ok\n");
  } else{
    printf("error\n");
  }
}

void power_off() {
  int id;
  scanf("%d", &id);
  if(vmoff(id)) {
    printf("ok\n");
  } else{
    printf("error\n");
  }
}

void power_halt() {
  int id;
  scanf("%d", &id);
  if(vmhalt(id)) {
    printf("ok\n");
  } else{
    printf("error\n");
  }
}

void power_reset() {
  int id;
  scanf("%d", &id);
  if(vmreset(id)) {
    printf("ok\n");
  } else{
    printf("error\n");
  }
}

void power_stop() {
  int id;
  scanf("%d", &id);
  if(vmstop(id)) {
    printf("ok\n");
  } else{
    printf("error\n");
  }
}

void power_continue() {
  int id;
  scanf("%d", &id);
  if(vmcontinue(id)) {
    printf("ok\n");
  } else{
    printf("error\n");
  }
}

void power_commit() {
	int id;
	scanf("%d", &id);
	sprintf(cmd, "qemu-img commit -f qcow2 %s/VM%d > /dev/null 2>&1", vmdir, id);
	xmlog(cmd);
	system(cmd);
	printf("ok\n");
}

void power_drop() {
	int id;
	scanf("%d", &id);
	vmadd(1, id, id);
	printf("ok\n");
}

void power_rebuild() {
  int base, id;
  scanf("%d", &base);
  while(true) {
    scanf("%d", &id);
    if(id == -1)break;
    vmadd(1, base, id);
  }
  printf("ok\n");
}

void power_compare() {
	int id;
	scanf("%d", &id);
	sprintf(file, "%s/VM%d", vmdir, id);
	long long ret = fileSize(file);
	if(ret < 1000000) {
		printf("ok\n");
	} else{
		printf("error\n");
	}
}

void snapshot_list() {
	int id;
	scanf("%d", &id);
	sprintf(cmd, "qemu-img snapshot -l %s/VM%d", vmdir, id);
	xmlog(cmd);
	getresult(cmd, result);
	if(result[0] == 0) {
		printf("-1\n");
		return;
	}
	char *p = strchr(result, 10);
  ++p;
  p = strchr(p, 10);
  ++p;
  while(*p) {
    sscanf(p, "%*d%s", name);
    printf("%s\n", name);
    p = strchr(p, 10);
    ++p;
  }
  printf("-1\n");
}

void snapshot_create() {
	int id;
	scanf("%d %s", &id, name);
	sprintf(cmd, "savevm %s", name);
	if(monitor(id, cmd, result)) {
		printf("ok\n");
	} else{
		printf("error\n");
	}
//	sprintf(cmd, "%s/snap.php %d &", dir, id);
//	xmlog(cmd);
//	system(cmd);
}

void snapshot_del() {
	int id;
	scanf("%d %s", &id, name);
	sprintf(cmd, "delvm %s", name);
	if(monitor(id, cmd, result)) {
		printf("ok\n");
	} else{
		printf("error\n");
	}
//	sprintf(cmd, "%s/snap.php %d &", dir, id);
//	xmlog(cmd);
//	system(cmd);
}

void snapshot_apply() {
	int id;
	scanf("%d %s", &id, name);
	sprintf(cmd, "loadvm %s", name);
	if(monitor(id, cmd, result)) {
		printf("ok\n");
	} else{
		printf("error\n");
	}
//	sprintf(cmd, "%s/snap.php %d &", dir, id);
//	xmlog(cmd);
//	system(cmd);
}

void cd_list() {
  sprintf(cmd, "ls %s", isodir);
  getresult(cmd, result);
	if(0 == result[0]) {
		printf("-1\n");
		return;
	}
  char *p = result;
  while(*p) {
    sscanf(p, "%s", name);
    printf("%s\n", name);
    p = strchr(p, 10);
    ++p;
  }
  printf("-1\n");
}

void cd_mounted() {
	int id;
	scanf("%d", &id);
	monitor(id, "info block ide1-cd0", result);
	if(strstr(result, "ide1-cd0: [not inserted]")) {
		printf("null\n");
	} else {
		char *p  = strstr(result, ".iso");
		*p = 0;
		while(*(p-1) != '/') {
			--p;
		}
		printf("%s\n", p);
	}
}

void cd_mount() {
	int id;
	scanf("%d%s", &id, name);
	sprintf(cmd, "change ide1-cd0 %s/%s", isodir, name);
	if(monitor(id, cmd, result)) {
		printf("ok\n");
	} else{
		printf("error\n");
	}
}


void cd_unmount() {
	int id;
	scanf("%d", &id);
	if(monitor(id, "eject -f ide1-cd0", result)) {
		printf("ok\n");
	} else{
		printf("error\n");
	}
}

void os_list() {
	sprintf(cmd, "ls %s", sysdir);
    getresult(cmd, result);
	if(0 == result[0]) {
		printf("-1\n");
		return;
	}
  char *p = result;
  while(*p) {
    sscanf(p, "%s", name);
    printf("%s\n", name);
    p = strchr(p, 10);
    ++p;
  }
  printf("-1\n");
}

void os_import() {
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

void os_query() {
	long long t;
	scanf("%lld", &t);
	printf("%d\n", rate(t, 0));
}

void os_rebuild() {
	int base, id;
	scanf("%d", &base);
	while(true) {
		scanf("%d", &id);
		if(id == -1)break;
		vmadd(1, base, id);
    usleep(100000);
    vmadd(1, base, id);
	}
	printf("ok\n");
}

void sync_sync() {
	int id;
	char ip[32];
	scanf("%d%s", &id, ip);
	time_t t;
  time(&t);
	sprintf(cmd, "rsync -avP %s/BACK%d root@%s:%s/BACK%d > %s/%lld &", vmdir,id, ip, vmdir, id, logdir, t);
  xmlog(cmd);
  system(cmd);
  printf("%lld\n", t);
}

void sync_query() {
	long long t;
  scanf("%lld", &t);
  printf("%d\n", rate(t, 0));
}

void sync_rebuild() {
  int base, id;
  scanf("%d", &base);
  while(true) {
    scanf("%d", &id);
    if(id == -1)break;
    vmadd(1, base, id);
    usleep(100000);
    vmadd(1, base, id);
  }
  printf("ok\n");
}

void migrate_send0() {
	int id;
	char ip[32];
	scanf("%d%s", &id, ip);
	sprintf(cmd, "migrate -d -i tcp:%s:%d", ip, id+8000);
	if(monitor(id, cmd, result)) {
		printf("ok\n");
	} else{
		printf("error\n");
	}
}

void migrate_recv0() {
	int id, cpu, mem, base;
  char mac[1024];
  scanf("%d %d %d %s %d", &id, &cpu, &mem, mac, &base);
	if(vmison(id)) {
		vmhalt(id);
	}
	time_t t;
  time(&t);
	vmadd(1, base, id);
	sprintf(file, "%s/VM%d", vmdir, id);
	sprintf(cmd, "qemu-kvm -name VM%d -smp %d,maxcpus=240 -m %d -rtc base=localtime -drive file=%s,if=virtio,media=disk,index=0 -vnc :%d -net nic,macaddr=%s,model=virtio -net tap,script=%s,downscript=%s,ifname=VM%d -usb -usbdevice tablet -enable-kvm -chardev socket,id=CL%d,path=%s/CL%d,server,nowait -monitor chardev:CL%d -chardev socket,id=QM%d,path=%s/QM%d,server,nowait -qmp chardev:QM%d -incoming tcp:0:%d > %s/%lld &", id, cpu, mem, file, id+100, mac, ifup, ifdown, id, id, cldir, id, id, id, qmdir, id, id, id+8000, logdir, t);
	xmlog(cmd);
//	system(cmd);
	exec(cmd);
	printf("%lld\n", t);
}

void migrate_query0() {
	long long t;
  scanf("%lld", &t);
  printf("%d\n", rate(t, 1));
}

void migrate_cancel0() {
	int id;
	scanf("%d", &id);
	if(monitor(id, "migrate_cancel", result)) {
    printf("ok\n");
  } else{
    printf("error\n");
  }
}

void migrate_send1() {
	int id;
	char ip[32];
  scanf("%d%s", &id, ip);
  time_t t;
  time(&t);
  sprintf(cmd, "rsync -avP %s/VM%d root@%s:%s/VM%d > %s/%lld &", vmdir,id, ip, vmdir, id, logdir, t);
  xmlog(cmd);
  system(cmd);
  printf("%lld\n", t);
}

void migrate_query1() {
	long long t;
  scanf("%lld", &t);
  printf("%d\n", rate(t, 0));
}

void migrate_cancel1() {
	int id;
  char ip[32];
  scanf("%d%s", &id, ip);
  sprintf(name, "rsync -avP %s/VM%d root@%s:%s/VM%d", vmdir,id, ip, vmdir, id);
	int pid;
	while(pid = getpid(name)) {
		killpid(pid);
		usleep(100000);
	}
	printf("ok\n");
}

void sys_list() {
  char ip[32];
  scanf("%s", ip);
	sprintf(cmd, "ssh root@%s ls %s", ip, datasysdir);
  getresult(cmd, result);
  if(0 == result[0]) {
    printf("-1\n");
    return;
  }
  char *p = result;
  while(*p) {
    sscanf(p, "%s", name);
    printf("%s\n", name);
    p = strchr(p, 10);
    ++p;
  }
  printf("-1\n");
}

void sys_rsync() {
  char ip[32];
  scanf("%s%s", name, ip);
  time_t t;
  time(&t);
  sprintf(cmd, "rsync -avP root@%s:%s/%s %s/%s > %s/%lld &", ip, datasysdir, name, sysdir, name, logdir, t);
  xmlog(cmd);
  system(cmd);
  printf("%lld\n", t);
}

void iso_list() {
  char ip[32];
  scanf("%s", ip);
  sprintf(cmd, "ssh root@%s ls %s", ip, dataisodir);
  getresult(cmd, result);
  if(0 == result[0]) {
    printf("-1\n");
    return;
  }
  char *p = result;
  while(*p) {
    sscanf(p, "%s", name);
    printf("%s\n", name);
    p = strchr(p, 10);
    ++p;
  }
  printf("-1\n");
}

void iso_rsync() {
  char ip[32];
  scanf("%s%s", name, ip);
  time_t t;
  time(&t);
  sprintf(cmd, "rsync -avP root@%s:%s/%s %s/%s > %s/%lld &", ip, dataisodir, name, isodir, name, logdir, t);
  xmlog(cmd);
  system(cmd);
  printf("%lld\n", t);
}

void sysiso_query() {
	long long t;
  scanf("%lld", &t);
  printf("%d\n", rate(t, 0));
}

void backup_backup() {
	int vmid, backid;
	char ip[32];
	scanf("%d%d%s", &vmid, &backid, ip);
  time_t t;
  time(&t);
  sprintf(cmd, "rsync -avP %s/BACK%d root@%s:%s/BACK%d > %s/%lld &", vmdir, vmid, ip, backupdir, backid, logdir, t);
  xmlog(cmd);
  system(cmd);
  sprintf(cmd, "rsync -avP %s/VM%d root@%s:%s/VM%d > %s/%lld &", vmdir, vmid, ip, backupdir, backid, logdir, t+1);
  xmlog(cmd);
  system(cmd);
  printf("%lld\n", t);
}

void backup_restore() {
	int vmid, backid;
  char ip[32];
  scanf("%d%d%s", &vmid, &backid, ip);
  time_t t;
  time(&t);
  sprintf(cmd, "rsync -avP root@%s:%s/BACK%d %s/BACK%d > %s/%lld &", ip, backupdir, backid, vmdir, vmid, logdir, t);
  xmlog(cmd);
  system(cmd);
  sprintf(cmd, "rsync -avP root@%s:%s/VM%d %s/VM%d > %s/%lld &", ip, backupdir, backid, vmdir, vmid, logdir, t+1);
  xmlog(cmd);
  system(cmd);
  printf("%lld\n", t);
}

void backup_query() {
	long long t;
  scanf("%lld", &t);
  printf("%d\n", (int)(rate(t, 0)*0.9+rate(t+1, 0)*0.1));
}

void backup_rebase() {
  int id;
  scanf("%d", &id);
  sprintf(cmd, "qemu-img rebase -f qcow2 -u -b %s/BACK%d %s/VM%d > /dev/null 2>&1", vmdir, id, vmdir, id);
  xmlog(cmd);
	system(cmd);
  printf("ok\n");
}

void alter_alter() {
  int id, baseid;
	scanf("%d%d", &id, &baseid);
  sprintf(cmd, "qemu-img rebase -f qcow2 -u -b %s/BACK%d %s/VM%d > /dev/null 2>&1", vmdir, id, vmdir, id);
  xmlog(cmd);
	system(cmd);
	time_t t;
  time(&t);
	sprintf(cmd, "rsync -avP %s/BACK%d %s/BACK%d > %s/%lld &", vmdir, baseid, vmdir, id, logdir, t);
	xmlog(cmd);
	system(cmd);
	printf("%lld\n", t);
}

void alter_query() {
	long long t;
  scanf("%lld", &t);
  printf("%d\n", rate(t, 0));
}

void alter_rebase() {
  int id;
  scanf("%d", &id);
  sprintf(cmd, "qemu-img rebase -f qcow2 -u -b %s/BACK%d %s/VM%d > /dev/null 2>&1", vmdir, id, vmdir, id);
  xmlog(cmd);
	system(cmd);
  printf("ok\n");
}

void online() {
	sprintf(cmd, "ls %s | grep VM", vmdir);
  getresult(cmd, result2, 0);
  if(0 == result2[0]) {
    printf("-1\n");
    return;
  }
	int id;
  char *p = result2;
  while(*p) {
		if(*p == 'V' && *(p+1) == 'M') {
    	sscanf(p+2, "%d", &id);
			int state = 0;
			if(vmison(id)) state = 1;
			printf("%d\n", id);
			printf("%d\n", state);
		}
    p = strchr(p, 10);
    ++p;
  }
  printf("-1\n");	
}

void disk_list() {
	sprintf(cmd, "ls %s", diskdir);
  getresult(cmd, result);
	if(0 == result[0]) {
		printf("-1\n");
		return;
	}
  char *p = result;
  while(*p) {
    sscanf(p, "%s", name);
    printf("%s\n", name);
    p = strchr(p, 10);
    ++p;
  }
  printf("-1\n");
}

void disk_create() {
	int size;
	scanf("%s%d", name, &size);
	sprintf(file, "%s/%s%", diskdir, name);
	sprintf(cmd, "qemu-img create -f qcow2  %s %dG > /dev/null 2>&1", file, size);
	xmlog(cmd);
	system(cmd);
	if(fileExist(file)) {
		printf("ok\n");
	} else {
		printf("error\n");
	}
}

void disk_mount() {
	int id;
	scanf("%s%d", name, &id);
	sprintf(file, "%s/%s", diskdir, name);
	sprintf(cmd, "drive_add 0 if=none,file=%s,format=qcow2,id=%s", file, name);
//	printf("%s\n", cmd);
	if(!monitor(id, cmd, result)) {
		printf("error\n");
	}
	sprintf(cmd, "device_add virtio-blk-pci,drive=%s,id=%s", name, name);
//	printf("%s\n", cmd);
	if(monitor(id, cmd, result)) {
		printf("ok\n");
	} else {
		printf("error\n");
	}
}

void disk_umount() {
	int id;
	scanf("%s%d", name, &id);
	sprintf(cmd, "device_del %s", name);
	if(monitor(id, cmd, result)) {
		printf("ok\n");
	} else {
		printf("error\n");
	}
}

void brctl_addbr() {
	int type, tag;
	char br[32], eth[32], ip[32], mask[32];
	scanf("%d%d%s%s%s%s", &type, &tag, br, eth, ip, mask);
	sprintf(cmd, "%s/addbr.sh %d %d %s %s %s %s > /dev/null 2>&1", dir, type, tag, br, eth, ip, mask);
//	printf("%s\n", cmd);
	xmlog(cmd);
	system(cmd);
	printf("ok\n");
}

void brctl_addif() {
	int id, type;
	char br[32];
	while(true) {
		scanf("%d", &id);
		if(-1 == id) break;
		scanf("%d%s", &type, &br);
		sprintf(cmd, "%s/addif.sh %d %d %s > /dev/null 2>&1", dir, id, type, br);
//	printf("%s\n", cmd);
		xmlog(cmd);
		system(cmd);
		printf("ok\n");
		fflush(stdout);
	}
}

void diskk_mount() {
	int id;
	char disk[64];
	scanf("%d%s", &id, disk);
  sprintf(cmd, "%s/file.sh %d %s", dir, id, disk);
	system(cmd);
	sprintf(file, "%s/%d-%s", vmdir, id, disk);
  sprintf(cmd, "drive_add 0 if=none,file=%s,format=qcow2,id=xmdisk", file);
//  printf("%s\n", cmd);
  if(!monitor(id, cmd, result)) {
    printf("error\n");
  }
  sprintf(cmd, "device_add virtio-blk-pci,drive=xmdisk,id=xmdisk");
//  printf("%s\n", cmd);
  if(monitor(id, cmd, result)) {
    printf("ok\n");
  } else {
    printf("error\n");
  }			
}

void diskk_umount() {
	int id;
	scanf("%d", &id);
	sprintf(cmd, "device_del xmdisk");
  if(monitor(id, cmd, result)) {
    printf("ok\n");
  } else {                                                                                                               
    printf("error\n");
	}
}

//**********************************************************************************************************************************
//**********************************************************************************************************************************

int main(int argc, char **argv) {
	setuid(geteuid());
  setgid(getegid());
	char cmd[1024];
	int ctl;
	scanf("%s", cmd);
	if(strcmp(cmd, "addvm") == 0) {
		addvm();
	} else if(strcmp(cmd, "delvm") == 0) {
		delvm();
	} else if(strcmp(cmd, "power") == 0) {
		scanf("%d", &ctl);
		//status, on, off, reset, stop, continue, halt, commit, drop, rebuild, compare
		if(1 == ctl) {
			power_status();
		} else if(2 == ctl) {
			power_on();
		} else if(3 == ctl) {
			power_off();
		} else if(4 == ctl) {
			power_reset();
		} else if(5 == ctl) {
			power_stop();
		} else if(6 == ctl) {
			power_continue();
		} else if(7 == ctl) {
			power_halt();
		} else if(8 == ctl) {
			power_commit();
		} else if(9 == ctl) {
			power_drop();
		} else if(10 == ctl) {
			power_rebuild();
		} else if(11 == ctl) {
			power_compare();
		}
	} else if(strcmp(cmd, "snapshot") == 0) {
		scanf("%d", &ctl);
		//list, create, del, apply
		if(1 == ctl) {
			snapshot_list();
		} else if(2 == ctl) {
			snapshot_create();
		} else if(3 == ctl) {
			snapshot_del();
		} else if(4 == ctl) {
			snapshot_apply();
		}
	} else if(strcmp(cmd, "cd") == 0) {
		scanf("%d", &ctl);
		//list, mounted, mount, unmount
		if(1 == ctl) {
			cd_list();
		} else if(2 == ctl) {
			cd_mounted();
		} else if(3 == ctl) {
			cd_mount();
		} else if(4 == ctl) {
			cd_unmount();
		}
	} else if(strcmp(cmd, "ppower") == 0) {
		scanf("%d", &ctl);
		//status, on, off, reset, stop, continue, halt
		if(1 == ctl) {
			ppower_status();
		} else if(2 == ctl) {
			ppower_on();
		} else if(3 == ctl) {
			ppower_off();
		} else if(4 == ctl) {
			ppower_reset();
		} else if(5 == ctl) {
			ppower_stop();
		} else if(6 == ctl) {
			ppower_continue();
		} else if(7 == ctl) {
			ppower_halt();
		}
	} else if(strcmp(cmd, "os") == 0) {
		scanf("%d", &ctl);
		//list, import, query, rebuild
		if(1 == ctl) {
			os_list();
		} else if(2 == ctl) {
			os_import();
		} else if(3 == ctl) {
			os_query();
		} else if(4 == ctl) {
			os_rebuild();
		}
	} else if(strcmp(cmd, "sync") == 0) {
		scanf("%d", &ctl);
		//sync, query, rebuild
		if(1 == ctl) {
			sync_sync();
		} else if(2 == ctl) {
			sync_query();
		} else if(3 == ctl) {
			sync_rebuild();
		}
	} else if(strcmp(cmd, "migrate") == 0) {
		scanf("%d", &ctl);
		//hot send, host recv, cold send, cold recv, hot query, cold query, hot cancel, cold cancel
		if(1 == ctl) {
			migrate_send0();
		} else if(2 == ctl) {
			migrate_recv0();
		} else if(3 == ctl) {
			migrate_send1();
		} else if(4 == ctl) {
//			migrate_recv1();
		} else if(5 == ctl) {
			migrate_query0();
		} else if(6 == ctl) {
			migrate_query1();
		} else if(7 == ctl) {
			migrate_cancel0();
		} else if(8 == ctl) {
			migrate_cancel1();
		}
	} else if(strcmp(cmd, "distb") == 0) {
		scanf("%d", &ctl);
		//sys list、rsync、query, iso list、rsync、query
		if(1 == ctl) {
			sys_list();
		} else if(2 == ctl) {
			sys_rsync();
		} else if(3 == ctl) {
			iso_list();
		} else if(4 == ctl) {
			iso_rsync();
		} else if(5 == ctl) {
			sysiso_query();
		}
	} else if(strcmp(cmd, "backup") == 0) {
		scanf("%d", &ctl);
		//backup, restore, query, rebase
		if(1 == ctl) {
			backup_backup();
		} else if(2 == ctl) {
			backup_restore();
		} else if(3 == ctl) {
			backup_query();
		} else if(4 == ctl) {
      backup_rebase();
    }
  } else if(strcmp(cmd, "alter") == 0) {
    scanf("%d", &ctl);
		//alter, query, rebase
		if(1 == ctl) {
			alter_alter();
		} else if(2 == ctl) {
			alter_query();
		} else if(3 == ctl) {
      alter_rebase();
    }
	} else if(strcmp(cmd, "online") == 0) {
		online();
	} else if(strcmp(cmd, "disk") == 0) {
		scanf("%d", &ctl);
		if(1 == ctl) {
			disk_list();
		} else if(2 == ctl) {
			disk_create();
		} else if(3 == ctl) {
			disk_mount();
		} else if(4 == ctl) {
			disk_umount();
		}
	} else if(strcmp(cmd, "brctl") == 0) {
		scanf("%d", &ctl);
		//addbr, delif , addif, delif
		if(1 == ctl) {
			brctl_addbr();
		} else if(2 == ctl) {

		} else if(3 == ctl) {
			brctl_addif();
		}
	} else if(strcmp(cmd, "diskk") == 0) {
		scanf("%d", &ctl);
		//mount, umount
		if(1 == ctl) {
			diskk_mount();
		} else if(2 == ctl) {
			diskk_umount();
		}
	} else{

	}
	return 0;
}
