#include <stdio.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include "utils.h"

int main(int argc, char **argv) {
	int k, c, e, t, p, s;
	
	if( argc == 7 ) {
        k = atoi(argv[1]);
        c = atoi(argv[2]);
        e = atoi(argv[3]);
        t = atoi(argv[4]);
        p = atoi(argv[5]);
        s = atoi(argv[6]);
	}
	
    puts("Hello world");
    return 0;
}