#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <inttypes.h>
#include <arpa/inet.h>

#define ec_meno1(c, s) if( (c)==-1 ){ perror(s); exit(EXIT_FAILURE); }
#define ec_null(c, s) if( (c)==NULL ) { perror(s); exit(EXIT_FAILURE); }
#define PERROR(s) { perror(s); exit(EXIT_FAILURE); }