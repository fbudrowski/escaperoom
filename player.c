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
#include <sys/ipc.h>
#include <sys/sem.h>
#include <stdbool.h>
#include "err.h"
#include "storage.h"



void player(size_t id){

    int storageDesc = shm_open(STORAGE, O_RDWR, S_IRUSR | S_IWUSR);
    struct Storage* storage = mmap(NULL, sizeof(struct Storage), PROT_READ | PROT_WRITE, MAP_SHARED, storageDesc, 0);
    struct Semaphores semaphores;
    getSems(storage, &semaphores);

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


    /* getOut procedure:
     *    1) Get protection
     *    2) Get out of game: reduce number of active players (by type), set yourself to available
     *    3) If I'm the last, set biggestFreeRoom, and remove plan
     *    4) Look up all existing plans and see if any can go.
     *    5) Give protection and continue.
     *
     * While there are plans to go (not EOF):
     * 1. Get protection
     * 2. Check if it will ever be needed. If so, give away protection & exit
     * 3. Check if it is needed now (by ENTRY SEMAPHORE!!). If so, then
     *    1) Give away protection
     *    2) Play
     *    3) Do getOut procedure and break.
     * 4. Else:
     *    1) Read the line (gets)
     *    2) Read following strings (%s) TO THE PLAN (set them to either int or char)
     *    3) Init check the new plan. If fails, give away protection, write & continue
     *    4) See if this plan can go.
     *    5) Give protection.
     */
    struct sembuf sbuf;

    while(1){

        sbuf.sem_num = 0;
        sbuf.sem_op = -1;
        sbuf.sem_flg = 0;

        if (semop(semaphores.protection, &sbuf, 1) == -1){
            syserr("semaphore");
        }

        //TODO: Check if id will ever be needed.
        if (id == -1) break;


        if (semctl(semaphores.entry, (int)id, GETVAL) > 0){
            struct Plan* plan = &storage->planPool.plans[storage->currentGameByPlayer[id]];
            size_t currentIterator = plan->elements.starting;
            struct ListNode* currentPlayer = &storage->listPool.nodes[currentIterator];
            printf("Entered room %d, game defined by %zu, waiting for players (", plan->assignedRoomNewId, currentPlayer->value);
            bool isFirst = true;
            while(currentIterator != LST_NULL && currentIterator != LST_INACTIVE){
                currentPlayer = &storage->listPool.nodes[currentIterator];
                if (semctl(semaphores.entry, (int)currentPlayer->value, GETVAL) > 0){
                    if (!isFirst){
                        printf(", ");
                    }
                    isFirst = false;
                    printf("%zu", currentPlayer->value);
                }
                currentIterator = currentPlayer->next;
            }
            printf(")\n");

            semop(semaphores.entry, ())

        }





            close(inputFd);
    }
    close(outputFd);
}