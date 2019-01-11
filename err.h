#ifndef _ERR_
#define _ERR_

/* wypisuje informacje o blednym zakonczeniu funkcji systemowej
i konczy dzialanie */

#define WRITE_ERR(monit) {fprintf(stderr, "ERROR: %s\n", monit);}
#define SYSTEM(x) if(x) {fprintf(stderr, "ERROR: "); exit(1);}
//#define SYSTEM2(x, monit) printf("Process %d, action %s\n", getpid(), monit);if(x) {WRITE_ERR(monit); exit(1);}
#define SYSTEM2(x, monit) if(x) {WRITE_ERR(monit); exit(1);}

#define ASSERT(x) if(x) {fprintf(stderr, "ASSERT\n");exit(-1);}

#define DEBUG(args...) printf (args)


#endif
