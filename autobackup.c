#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char **argv) {
	int t = 60;
	if(argc == 2) {
		t = atoi(argv[1]);
	}
//	printf("%d\n", t);
	while(true) {
		system("/var/cos/cvm/autobackup.php &");
		sleep(t);
	}
	return 0;
}
