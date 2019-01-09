#include <semaphore.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/mman.h>
#include <stdlib.h>
#include <sys/stat.h>        /* For mode constants */
#include <fcntl.h>           /* For O_* constants */
#include <sys/sem.h>
#include <stdbool.h>
#include "err.h"
#include "storage.h"


void startGame(struct Storage *storage, struct Semaphores *semaphores, struct Plan *plan, plan_index_t planId);

void playGame(size_t id, struct Storage* storage, struct Semaphores* semaphores, FILE* output){
    struct sembuf sbuf_protection;
    sbuf_protection.sem_num = 0;
    sbuf_protection.sem_flg = 0;

    struct Plan* plan = &storage->planPool.plans[storage->currentGameByPlayer[id]];
    struct Room* room = &storage->rooms[plan->assignedRoomNewId];
    size_t currentIterator = plan->elements.starting;
    struct ListNode* currentPlayer = &storage->listPool.nodes[currentIterator];
    char printout[120];
    sprintf(printout, "Entered room %d, game defined by %zu, waiting for players (", plan->assignedRoomNewId, currentPlayer->value);
    bool isFirst = true;
    while(currentIterator != LST_NULL && currentIterator != LST_INACTIVE){
        currentPlayer = &storage->listPool.nodes[currentIterator];
        int result = semctl(semaphores->entry, (int)currentPlayer->value, GETVAL);
        SYSTEM2(result < 0, "semaphore");
        if (result > 0){ // if it is not yet waiting
            if (!isFirst){
                sprintf(printout, ", ");
            }
            isFirst = false;
            sprintf(printout, "%zu", currentPlayer->value);
        }
        currentIterator = currentPlayer->next;
    }
    sprintf(printout, ")\n");
    SYSTEM2(fprintf(output, "%s", printout) == -1, "write2");


    struct sembuf sbuf_entry;

    ++room->taken;

    if (room->taken == plan->elem_count){ // If I'm the last one
        sbuf_entry.sem_op = 1;
        sbuf_entry.sem_flg = 0;

        size_t element = plan->elements.starting;                   // Allows all others to enter the game
        while(element != LST_INACTIVE && element != LST_NULL){

            sbuf_entry.sem_num = (unsigned short) element;
            SYSTEM2(semop(semaphores->entry, &sbuf_entry, storage->playerCount + 1) == -1, "semaphore");

            element = storage->listPool.nodes[element].next;
        }


        sbuf_protection.sem_op = 1;
        SYSTEM2(semop(semaphores->protection, &sbuf_protection, 1) == -1, "semaphore");// Give protection
    }
    else {
        sbuf_protection.sem_op = 1;
        SYSTEM2(semop(semaphores->protection, &sbuf_protection, 1) == -1, "semaphore");// Give protection

        sbuf_entry.sem_num = id;
        sbuf_entry.sem_op = -1;
        sbuf_entry.sem_flg = 0;
        SYSTEM2(semop(semaphores->entry, &sbuf_entry, storage->playerCount + 1) != -1, "semaphore"); // Wait until allowed to enter
    }
    SYSTEM(sleep(rand() % 10)); //Play

    sbuf_protection.sem_op = -1;
    SYSTEM2(semop(semaphores->protection, &sbuf_protection, 1) == -1, "semaphore");// Get protection


    --room->taken;
    storage->freePlayer[id] = 1;
    int planId = storage->currentGameByPlayer[id];
    storage->currentGameByPlayer[id] = -1; // Get out

    --storage->remainingPlayersForTypes[storage->playerPrefdRoom[id] - SMALLEST_ROOM];
    if (room->taken == 0){ // If we're the last to leave, clean up
        if (room->capacity > storage->biggestFreeRoom[room->type - SMALLEST_ROOM]){
            storage->biggestFreeRoom[room->type - SMALLEST_ROOM] = (int)room->capacity;
        }
        listClear(&plan->elements, &storage->listPool);
        deletePlan(&storage->listOfPlans, &storage->listPool, &storage->planPool, (node_index_t) planId);

    }
    sbuf_protection.sem_op = 1;
    SYSTEM2(semop(semaphores->protection, &sbuf_protection, 1) == -1, "semaphore");// Give protection
}



void player(size_t id){
    char allPlansString[4 * MAX_PLAYERS];
    srand(id * 114 + 413243);

    int storageDesc = shm_open(STORAGE, O_RDWR, S_IRUSR | S_IWUSR);
    struct Storage* storage = mmap(NULL, sizeof(struct Storage), PROT_READ | PROT_WRITE, MAP_SHARED, storageDesc, 0);
    struct Semaphores semaphores;
    getSems(storage, &semaphores);

    printf("Player %zu\n", id);
    char inputFilename[20];
    sprintf(inputFilename, "player-%zu.in", id);
    char outputFilename[20];
    sprintf(outputFilename, "player-%zu.out", id);

//    int inputFd = open(inputFilename, O_RDONLY);
//    SYSTEM2(inputFd < 0, "Input open failed");
//    int outputFd = open(outputFilename, O_RDWR);
//    SYSTEM2(outputFd < 0, "Output open failed");
    FILE* input = fopen(inputFilename, "r");
    SYSTEM2(input == NULL, "input file");
    FILE* output= fopen(outputFilename, "w");
    SYSTEM2(input == NULL, "output file");

//    printf("Opened files %s (%d), %s (%d)\n", inputFilename, inputFd, outputFilename, outputFd);


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
     *    1) Read the line (fopen, fgets)
     *    2) Read following strings (%s) TO THE PLAN (set them to either int or char)
     *    3) Init check the new plan. If fails, give away protection, write & continue
     *    4) See if this plan can go.
     *    5) Give protection.
     */
    struct sembuf sbuf_protection;

    while(1){

        sbuf_protection.sem_num = 0;
        sbuf_protection.sem_op = -1;
        sbuf_protection.sem_flg = 0;

        SYSTEM2(semop(semaphores.protection, &sbuf_protection, 1) == -1, "semaphore"); // Get protection

        //TODO: Check if id will ever be needed.
        if (id == -1) break;


        if (storage->currentGameByPlayer[id] > -1){ // If anyone is waiting for current player to play
            playGame(id, storage, &semaphores, output);
            continue;
        }
        else{ // Read the plan
            printf("Player %zu reads the plan", id);
            char *planString = fgets(allPlansString, 4 * MAX_PLAYERS, input);
            if (planString == NULL){
                break;
            }
            plan_index_t planId = addNewPlan(&storage->listOfPlans, &storage->listPool, &storage->planPool);
            struct Plan* plan = &storage->planPool.plans[planId];
            int pos = 0;
            sscanf(planString + pos, "%c %n", &plan->room_type, &pos);
            char local[7];
            int change = 0;
            while(sscanf(planString + pos, "%s%n", local, &change) > 0){
                ++plan->elem_count;
                pos += change;
                if ('A' <= local[0] && local[0] <= 'Z'){
                    listAppend(&plan->elements, &storage->listPool, (size_t)local[0] + COLOR_ZERO - SMALLEST_ROOM);
                } else{
                    long int val = strtol(local, NULL, 10);
                    listAppend(&plan->elements, &storage->listPool, (size_t)val);
                }
            }
            int ok = initCheckPlan(storage, planId);
            if (ok < 0){
                deletePlan(&storage->listOfPlans, &storage->listPool, &storage->planPool, planId);
            }

            ok = checkPlan(storage, planId);
            if (ok >= 0){
                startGame(storage, &semaphores, plan, planId);
            }


        }

    }
    fclose(input);
    fclose(output);
}

void startGame(struct Storage *storage, struct Semaphores* semaphores, struct Plan *plan, plan_index_t planId) {

    node_index_t elemIterator = plan->elements.starting;

    struct Room* room = NULL;
    bool roomSelected = false;
    for (size_t i = 0; i < ROOM_TYPES_COUNT; i++) {
        storage->biggestFreeRoom[i] = -1;
    }
    for (size_t i = 0; i <= storage->roomCount; i++){
        if (storage->rooms[i].taken == 0 && !roomSelected && storage->rooms[i].type == plan->room_type
            && storage->rooms[i].capacity >= plan->elem_count){
            room = &storage->rooms[i];
            roomSelected = true;
        } else if (storage->rooms[i].taken == 0 ){
            storage->biggestFreeRoom[storage->rooms[i].type - SMALLEST_ROOM] = (int)storage->rooms[i].capacity;
        }
    }



    while(elemIterator != LST_INACTIVE && elemIterator != LST_NULL){
        size_t val = storage->listPool.nodes[elemIterator].value;
        if (val < MAX_PLAYERS){
            storage->freePlayer[val] = 0;
            storage->currentGameByPlayer[val] = planId;
            // Raise semaphore
        }
        elemIterator = storage->listPool.nodes[elemIterator].next;
    }
}

