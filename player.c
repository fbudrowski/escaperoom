#include <semaphore.h>
#include <time.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>        /* For mode constants */
#include <fcntl.h>           /* For O_* constants */
#include "err.h"



void player(size_t id){
    printf("Player %zu\n", id);
    char inputFilename[20];
    sprintf(inputFilename, "player-%zu.in", id);
    char outputFilename[20];
    sprintf(outputFilename, "player-%zu.out", id);

    int inputFd = open(inputFilename, O_RDONLY);
    if (inputFd < 0) syserr("Input open failed");
    int outputFd = open(outputFilename, O_RDWR);
    if (outputFd < 0) syserr("Output open failed");

    printf("Opened files %s (%d), %s (%d)\n", inputFilename, inputFd, outputFilename, outputFd);

    close(inputFd);
    close(outputFd);
}