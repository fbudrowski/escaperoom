#include <semaphore.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/mman.h>
#include <stdlib.h>
#include <sys/stat.h>        /* For mode constants */
#include <fcntl.h>           /* For O_* constants */
#include <sys/sem.h>
#include <stdbool.h>
#include <assert.h>
#include "err.h"
#include "storage.h"

void playGameLogEntryStatus(struct Storage *storage, struct Plan *plan, struct Room *room, size_t playerId,
                            FILE *output);
void startGame(struct Storage *storage, struct Plan *plan, plan_index_t planId);
void tryStartingAnyGame(size_t playerId, struct Storage *storage);


void playGameLogEntryStatus(struct Storage *storage, struct Plan *plan, struct Room *room, size_t playerId,
                            FILE *output){
    size_t currentIterator = plan->elements.starting;
    char printout[120];

    { // Writing stuff
        struct ListNode* currentPlayer = &storage->listPool.nodes[currentIterator];
        int move = sprintf(printout, "Entered room %d, game defined by %d, waiting for players (", room->roomOriginalId, plan->author);
        bool isFirst = true;
        while(currentIterator != LST_NULL && currentIterator != LST_INACTIVE) { // For each player write stuff
            currentPlayer = &storage->listPool.nodes[currentIterator];

            if (storage->playerEnteredRoom[currentPlayer->value] == 0) { // if it is not yet waiting
                if (!isFirst) {
                    move += sprintf(printout + move, ", ");
                }
                isFirst = false;
                move += sprintf(printout + move, "%zu", currentPlayer->value);
            }
            currentIterator = currentPlayer->next;
        }
        move += sprintf(printout + move, ")\n");
        DEBUG("(%zu): %s\n", playerId, printout);
        SYSTEM2(fprintf(output, "%s", printout) == -1, "write2");
    }

};

void playGameEntrySection(struct Storage* storage, struct Room* room, struct Plan* plan, size_t playerId){
    SYSTEM2(sem_wait(&storage->protection), "protection-");


    ++room->inside;
    DEBUG("Game room entered by %zu with %d out of %d ppl inside\n", playerId, room->inside, plan->elem_count);

    if (room->inside == plan->elem_count){ // If I'm the last one
        size_t element = plan->elements.starting;                   // Allows all others to enter the game
        while(element != LST_INACTIVE && element != LST_NULL){
            size_t localPlayer = storage->listPool.nodes[element].value;
            DEBUG("Player %zu invites player %zu to start the game in %d\n", playerId, localPlayer, plan->assignedRoomNewId);
            if (localPlayer != playerId){
                SYSTEM2(sem_post(&storage->entry[localPlayer]) < 0, "entry semaphore+"); // Allow this one to enter
            }
            element = storage->listPool.nodes[element].next;
        }

        SYSTEM2(sem_post(&storage->protection) < 0, "protection+");// Give protection

    }
    else {

        SYSTEM2(sem_post(&storage->protection) < 0, "protection+");// Give protection
        SYSTEM2(sem_wait(&storage->entry[playerId]) < 0, "entry semaphore-"); // Wait until allowed to enter

    }
}

void playGame(size_t playerId, struct Storage* storage, FILE* output){

    struct Plan* plan = &storage->planPool.plans[storage->currentGameByPlayer[playerId]];
    struct Room* room = &storage->rooms[plan->assignedRoomNewId];


    storage->playerEnteredRoom[playerId] = 1;
    playGameLogEntryStatus(storage,plan,room,playerId,output);

    playGameEntrySection(storage, room, plan, playerId);

    DEBUG("Player %zu starts to play the game in room %d\n", playerId, plan->assignedRoomNewId);


    SYSTEM(sleep(rand() % 3)); //Play



    DEBUG("Player %zu ends to play the game in room %d\n", playerId, plan->assignedRoomNewId);
    SYSTEM2(sem_wait(&storage->protection) < 0, "protection-"); // Get protection
    --room->inside;
    storage->freePlayer[playerId] = 1;
    int planId = storage->currentGameByPlayer[playerId];
    storage->currentGameByPlayer[playerId] = -1; // Get out
    storage->playerEnteredRoom[playerId] = 0;
    SYSTEM2(fprintf(output, "Left room %d\n", room->roomOriginalId) <= 0, "fprintf3");

    ++storage->remainingPlayersForTypes[getType(playerId, storage)];
    storage->playerInPlans[playerId]--;


    if (storage->alreadyFinishedWriting == storage->playerCount
        && storage->playerTypeInPlans[getType(playerId, storage)] == 0){
        for (int i = 1; i <= storage->playerCount; ++i){
            if (getType(playerId, storage) == getType((size_t) i, storage) && storage->playerInPlans[i] == 0){
                SYSTEM2(sem_post(&storage->isToEnter[i]), "isToEnter+");
            }
        }
    }

    DEBUG("Leaving %zu, plan %d\n", playerId, planId);
    if (room->inside == 0){ // If we're the last to leave, clean up

        DEBUG("Clean up %zu, plan %d, room %zu, %c\n", playerId, planId, room->capacity, room->type);

        room->taken = 0;
        if ((int)room->capacity > storage->biggestFreeRoom[room->type - SMALLEST_ROOM]){ // Update biggestFreeRoom
            storage->biggestFreeRoom[room->type - SMALLEST_ROOM] = (int)room->capacity;
        }

        listClear(&plan->elements, &storage->listPool);
        deletePlan(&storage->listOfPlans, &storage->listPool, &storage->planPool, planId);
        printList(&storage->listPool, &storage->listOfPlans);

        DEBUG("Player %zu tries to start any game\n", playerId);

        tryStartingAnyGame(playerId, storage);
        DEBUG("Player %zu has finished looking for any new game\n", playerId);
    }
    SYSTEM2(sem_post(&storage->protection) < 0, "protection+");// Give protection
}


void readPlan(size_t playerId, struct Storage *storage, const char *planString, FILE* output) { // Gets
    DEBUG("Player %zu reads the plan: %s\n", playerId, planString);
    plan_index_t planId = addNewEmptyPlan(&storage->listOfPlans, &storage->listPool, &storage->planPool);
    printList(&storage->listPool, &storage->listOfPlans);

    struct Plan *plan = &storage->planPool.plans[planId];
    int pos = 0;
    sscanf(planString + pos, "%c %n", &plan->room_type, &pos);
    char local[7];
    int change = 0;

    plan->author = (int) playerId;


    while (sscanf(planString + pos, "%s%n", local, &change) > 0) {
        ++plan->elem_count;
        pos += change;
        if ('A' <= local[0] && local[0] <= 'Z') {
            listAppend(&plan->elements, &storage->listPool, (size_t) local[0] + COLOR_ZERO - SMALLEST_ROOM);
        } else {
            long int val = strtol(local, NULL, 10);
            listAppend(&plan->elements, &storage->listPool, (size_t) val);
        }
    }
    int ok = initCheckPlan(storage, planId);
    DEBUG("Player %zu has init-checked the plan %zu (%s) with status %d\n", playerId, planId, planString, ok);
    if (ok < 0) {
        fprintf(output, "Invalid plan: \"%s\"", planString);
        deletePlan(&storage->listOfPlans, &storage->listPool, &storage->planPool, planId);

    } else{
        ok = checkPlan(storage, planId);
        DEBUG("Player %zu has checked the plan %zu with status %d\n",playerId, planId, ok); fflush(stdout);
        if (ok >= 0) {
            startGame(storage, plan, planId);
        }
    }

}

void startGame(struct Storage *storage, struct Plan *plan, plan_index_t planId) { // MUST be called with protection, exits with protection


    struct Room* room = NULL;
    bool roomSelected = false;
    for (size_t i = 0; i < ROOM_TYPES_COUNT; i++) {
        storage->biggestFreeRoom[i] = -1;
    }
    DEBUG("One out of %zu rooms will be selected for game %zu\n", storage->roomCount, planId);
    for (size_t i = 0; i < storage->roomCount; i++){ // Getting a room
        if (storage->rooms[i].taken == 0 && !roomSelected && storage->rooms[i].type == plan->room_type
            && storage->rooms[i].capacity >= plan->elem_count){
            room = &storage->rooms[i];
            plan->assignedRoomNewId = (int) i;
            roomSelected = true;
            room->taken = 1;
        } else if (storage->rooms[i].inside == 0 ){
            storage->biggestFreeRoom[storage->rooms[i].type - SMALLEST_ROOM] = (int)storage->rooms[i].capacity;
        }
    }
    DEBUG("Game %zu assigned room %d\n", planId, plan->assignedRoomNewId);
    int playersOfType[ROOM_TYPES_COUNT];
    for (int i = 0; i < ROOM_TYPES_COUNT; i++){
        playersOfType[i] = 0;
    }

    node_index_t elemIterator = plan->elements.starting;
    int count = 0;
    while(elemIterator != LST_INACTIVE && elemIterator != LST_NULL){ // Getting all named players to go
        size_t val = storage->listPool.nodes[elemIterator].value;
        ++count;
        DEBUG("Plan %zu will have element %zu\n", planId, val);
        if (val < MAX_PLAYERS){
            assert(storage->freePlayer[val] == 1);
            storage->freePlayer[val] = 0;
            storage->currentGameByPlayer[val] = (int) planId;
            // Raise semaphore
            DEBUG("(1)Adding to plan %zu element %zu\n", planId, val);
            storage->playerEnteredRoom[val] = 0;
            SYSTEM2(sem_post(&storage->isToEnter[val]), "isToEnter+");
        } else{
            ++playersOfType[val - COLOR_ZERO];
        }
        elemIterator = storage->listPool.nodes[elemIterator].next;
    }

    DEBUG("Plan %zu has %d elements, should have %d\n", planId, count, plan->elem_count);

    elemIterator = plan->elements.starting;

    for(size_t i = 1; i <= storage->playerCount; i++){
        if (storage->freePlayer[i] && playersOfType[storage->playerPrefdRoom[i] - SMALLEST_ROOM] > 0){

            playersOfType[storage->playerPrefdRoom[i] - SMALLEST_ROOM]--;
            storage->freePlayer[i] = 0;

            storage->currentGameByPlayer[i] = (int)planId;
            while (storage->listPool.nodes[elemIterator].value < COLOR_ZERO){
                elemIterator = storage->listPool.nodes[elemIterator].next;
            }

            storage->playerInPlans[i]++;
            storage->playerTypeInPlans[getType(i, storage)]--;

            storage->listPool.nodes[elemIterator].value = i;
            DEBUG("(2)Adding to plan %zu element %zu\n", planId, i);
            storage->playerEnteredRoom[i] = 0;
            SYSTEM2(sem_post(&storage->isToEnter[i]), "isToEnter+");// Raise semaphore
        }
    }

}

void tryStartingAnyGame(size_t playerId, struct Storage *storage) { // MUST be called WITH protection, ends WITH protection
    node_index_t currentPlanNode = storage->listOfPlans.starting;
    while (currentPlanNode != LST_NULL && currentPlanNode != LST_INACTIVE) {
        plan_index_t planId = storage->listPool.nodes[currentPlanNode].value;
        struct Plan* plan = &storage->planPool.plans[planId];
        int ok = checkPlan(storage, planId);
        printf("Player %zu checks plan %zu with %d\n", playerId, planId, ok);
        if (ok >= 0){
            startGame(storage, plan, planId);
            return;
        }
        currentPlanNode = storage->listPool.nodes[currentPlanNode].next;
    }
}

void player(size_t playerId){
    char allPlansString[4 * MAX_PLAYERS];
    srand(playerId * 114 + 413243);

    int storageDesc = shm_open(STORAGE, O_RDWR, S_IRUSR | S_IWUSR);
    struct Storage* storage = mmap(NULL, sizeof(struct Storage), PROT_READ | PROT_WRITE, MAP_SHARED, storageDesc, 0);

    DEBUG("Player %zu\n", playerId);
    char inputFilename[20];
    sprintf(inputFilename, "player-%zu.in", playerId);
    char outputFilename[20];
    sprintf(outputFilename, "player-%zu.out", playerId);

    FILE* input = fopen(inputFilename, "r");
    SYSTEM2(input == NULL, "input file");
    FILE* output= fopen(outputFilename, "w");
    SYSTEM2(input == NULL, "output file");

    char playerType[10];
    fgets(playerType, 10, input);


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
    printf("Player %zu waits to enter\n", playerId);

    SYSTEM2(sem_wait(&storage->protection), "protection-");// Get protection

    printf("Player %zu enters\n", playerId);
    ++storage->remainingPlayersForTypes[storage->playerPrefdRoom[playerId] - SMALLEST_ROOM];

    tryStartingAnyGame(playerId, storage);


    SYSTEM2(sem_post(&storage->protection), "protection+");// Give protection



    while(1){

        SYSTEM2(sem_wait(&storage->protection), "protection-");// Get protection

        if (storage->currentGameByPlayer[playerId] > -1){ // If anyone is waiting for current player to play
            SYSTEM2(sem_post(&storage->protection), "protection+");
            DEBUG("(in)Player %zu waits to enter the game %d\n", playerId, storage->currentGameByPlayer[playerId]);
            SYSTEM2(sem_wait(&storage->isToEnter[playerId]) < 0, "isToEnter-"); // Notice that he enters
            DEBUG("Player %zu enters the game %d\n", playerId, storage->currentGameByPlayer[playerId]);

            playGame(playerId, storage, output);
            continue;
        }
        else{ // Read the plan
            char *planString = fgets(allPlansString, 4 * MAX_PLAYERS, input);

            if (planString == NULL){
                SYSTEM2(sem_post(&storage->protection), "protection+");// Give protection
//                break;
                continue;
            }
            readPlan(playerId, storage, planString, output);
            SYSTEM2(sem_post(&storage->protection), "protection+"); // Give protection
            puts("Plan read");
        }
    }
    DEBUG("Player %zu exited the loop\n", playerId);
    ++storage->alreadyFinishedWriting;
    if (storage->alreadyFinishedWriting == storage->playerCount){
        DEBUG("Everything read\n");
        for (int i = 1; i <= storage->playerCount; i++){
            if (storage->playerInPlans[i] == 0
               && storage->playerTypeInPlans[storage->playerPrefdRoom[i] - SMALLEST_ROOM] == 0){
                SYSTEM2(sem_post(&storage->isToEnter[i]), "isToEnter+");
            }
        }

//        for(int i = 0; i < storage->playerCount; i++){
//            SYSTEM2(sem_post(&storage->forLastToExit) < 0, "forLastToExit");
//        }
    }

    int v;
    sem_getvalue(&storage->protection, &v);
    DEBUG("%zu:: %d\n",playerId, v);

    while(1){
        DEBUG("(out)Player %zu waits to enter the game\n", playerId);
        SYSTEM2(sem_wait(&storage->isToEnter[playerId]) < 0, "isToEnter-"); // Notice that he enters
        DEBUG("Player %zu waits for protection\n", playerId);
        SYSTEM2(sem_wait(&storage->protection) < 0, "protection-");


        if (// If this player is no longer needed
            storage->playerInPlans[playerId] == 0 &&
            storage->playerTypeInPlans[storage->playerPrefdRoom[playerId] - SMALLEST_ROOM] == 0
            && storage->alreadyFinishedWriting == storage->playerCount){
                DEBUG("Player %zu wants to exit\n", playerId);
                SYSTEM2(sem_post(&storage->protection) < 0, "protection+");
                break;
        } else if (storage->currentGameByPlayer[playerId] == -1){
            SYSTEM2(sem_post(&storage->protection) < 0, "protection+");
            continue;
        }

        DEBUG("Player %zu enters the game %d\n", playerId, storage->currentGameByPlayer[playerId]);
        playGame(playerId, storage, output); // returns protection
    }

    DEBUG("\t\tPlayer %zu EXITS\n", playerId);
    fclose(input);
    fclose(output);
}

