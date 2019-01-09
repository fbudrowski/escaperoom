#include <semaphore.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <sys/stat.h>        /* For mode constants */
#include <fcntl.h>           /* For O_* constants */
#include <assert.h>
#include <memory.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include "err.h"
#include "storage.h"


int compRooms(const void *room1, const void *room2) {
    const struct Room *r1 = room1, *r2 = room2;
    if (r1->capacity == r2->capacity) {
        if (r1->type < r2->type) return -1;
        if (r1->type > r2->type) return 1;
        return 0;
    }
    if (r1->capacity < r2->capacity) return -1;
    return 1;
}


plan_index_t getEmptyPlan(struct PlanPool *planPool){
    plan_index_t i = planPool->currentInd;
    size_t ct = 0;
    for (; ct < MAX_PLANS; ++ct, ++i) {
        if (i >= MAX_PLANS)
            i -= MAX_PLANS;
        if (planPool->plans[i].room_type == PLAN_INACTIVE){
            return (int) i;
        }
    }
    return PLAN_NULL;
}

int listAppend(struct LinkedList *list, struct ListPool *pool, size_t value){
    node_index_t i = pool->currentInd;
    size_t ct = 0;
    for (; ct < MAX_LIST_ELEMS; ++ct, ++i) {
        if (i >= MAX_LIST_ELEMS)
            i -= MAX_LIST_ELEMS;
        if (pool->nodes[i].next == LST_INACTIVE){
            pool->nodes[list->ending].next = i;
            pool->nodes[i].prev = i;
            break;
        }
    }
    if (ct >= MAX_LIST_ELEMS)
        return -1;

    if (list->ending == LST_NULL || list->ending == LST_INACTIVE){
        list->starting = list->ending = i;
    }
    else{
        list->ending = i;
    }
    pool->nodes[i].value = value;
    pool->nodes[i].next = LST_NULL;
    return 0;
}

void listClear(struct LinkedList *list, struct ListPool *pool){
    node_index_t current = list->starting;
    while(current != LST_INACTIVE && current != LST_NULL){
        node_index_t prev = current;
        current = pool->nodes[prev].next;
        pool->nodes[prev].next = LST_INACTIVE;
    }
    list->starting = list->ending = LST_INACTIVE;
}

plan_index_t addNewPlan(struct LinkedList *list, struct ListPool *listPool, struct PlanPool *planPool){
    plan_index_t planId = getEmptyPlan(planPool);
    if (planId == PLAN_NULL){
        return PLAN_NULL;
    }
    int ok = listAppend(list, listPool, planId);
    if (ok == -1) {
        planPool->plans[planId].room_type = PLAN_INACTIVE;
        return PLAN_NULL;
    }
    return planId;
}


void setUpLists(struct ListPool* pool, struct LinkedList* list, struct PlanPool* planPool){
    pool->currentInd = 0;
    for(int i = 0; i < MAX_LIST_ELEMS; i++) {
        pool->nodes[i].next = LST_INACTIVE;
        pool->nodes[i].prev = LST_INACTIVE;
    }
    list->starting = LST_NULL;
    list->ending = LST_NULL;

    for(int i = 0; i < MAX_PLANS; i++){
        planPool->plans[i].room_type = 0;
    }
    planPool->currentInd = 0;
}



void deletePlan(struct LinkedList *list, struct ListPool *listPool, struct PlanPool *planPool, node_index_t currentNode) {
    listClear(&planPool->plans[currentNode].elements, listPool);

    node_index_t prev = listPool->nodes[currentNode].prev;
    node_index_t next = listPool->nodes[currentNode].next;

    listPool->nodes[currentNode].prev = listPool->nodes[currentNode].next = LST_INACTIVE;

    if (prev == LST_NULL && next == LST_NULL){
        list->starting = list->ending = LST_INACTIVE;
    } else if (prev == LST_NULL){
        list->starting = next;
        listPool->nodes[next].prev = prev;
    } else if (next == LST_NULL){
        list->ending = prev;
        listPool->nodes[prev].next = next;
    } else {
        listPool->nodes[next].prev = prev;
        listPool->nodes[prev].next = next;
    }

}

int initCheckPlan(struct Storage *storage, plan_index_t planIndex){
    struct Plan *plan = &storage->planPool.plans[planIndex];
    struct ListPool *listPool = &storage->listPool;
    node_index_t nodeIndex = plan->elements.starting;

    if (storage->maxSizedRoom[plan->room_type - SMALLEST_ROOM] == -1){
        return -1;
    }
    int remainingElems[ROOM_TYPES_COUNT];
    int count = 0;
    for (int i = 0; i < ROOM_TYPES_COUNT; ++i){
        remainingElems[i] = storage->allPlayersForTypes[i];
    }
    while (nodeIndex != LST_NULL && nodeIndex != LST_INACTIVE){
        size_t value = listPool->nodes[nodeIndex].value;
        size_t color = 0;
        if (value >= COLOR_ZERO){
            color = value - COLOR_ZERO;
        } else{
            color = (size_t) (storage->playerPrefdRoom[value] - SMALLEST_ROOM);
        }
        if (remainingElems[color] == 0){
            return -2;
        }
        --remainingElems[color];

        nodeIndex = listPool->nodes[nodeIndex].next;
    }
    plan->elem_count = count;
    if (count > storage->maxSizedRoom[plan->room_type - SMALLEST_ROOM]){
        return -3;
    }
    return 1;
}

int checkPlan(struct Storage *storage, plan_index_t planIndex){
    struct Plan *plan = &storage->planPool.plans[planIndex];
    struct ListPool *listPool = &storage->listPool;
    node_index_t nodeIndex = plan->elements.starting;

    if(storage->biggestFreeRoom[plan->room_type-SMALLEST_ROOM] < plan->elem_count){
        return -1;
    }

    int remainingElems[ROOM_TYPES_COUNT];
    for (int i = 0; i < ROOM_TYPES_COUNT; ++i){
        remainingElems[i] = storage->remainingPlayersForTypes[i];
    }
    while (nodeIndex != LST_NULL && nodeIndex != LST_INACTIVE){
        size_t value = listPool->nodes[nodeIndex].value;
        size_t color = 0;
        if (value >= COLOR_ZERO){
            color = value - COLOR_ZERO;
        } else{
            if (storage->freePlayer[value] == 0){
                return -4;
            }
            color = (size_t) (storage->playerPrefdRoom[value] - SMALLEST_ROOM);
        }
        if (remainingElems[color] == 0){
            return -2;
        }
        --remainingElems[color];

        nodeIndex = listPool->nodes[nodeIndex].next;
    }

    return 1;
}

struct Storage *getFromInput() {
    size_t n, m;
    scanf("%zu%zu", &n, &m);

    int fd = shm_open(STORAGE, O_CREAT | O_RDWR, S_IWUSR | S_IRUSR);
    SYSTEM2(fd == -1, "shm_open fail");
    if (ftruncate(fd, sizeof(struct Storage)) == -1) {
        shm_unlink(STORAGE);
        SYSTEM2(1, "ftruncate fail");
    }
    struct Storage *storage =
            mmap(NULL, sizeof(struct Storage), PROT_READ | PROT_WRITE,
                 MAP_SHARED, fd, 0);
    if (storage == MAP_FAILED) {
        shm_unlink(STORAGE);
        SYSTEM2(1, "Mapping fail");
    }

    storage->playerCount = n;
    storage->roomCount = m;
    for(int i = 0; i < ROOM_TYPES_COUNT; i++){
        storage->biggestFreeRoom[i] = -1;
        storage->maxSizedRoom[i] = -1;
        storage->allPlayersForTypes[i] = 0;
        storage->remainingPlayersForTypes[i] = 0;
    }

    for (size_t i = 0; i < m; i++) {
        char type;
        size_t capacity;
        scanf("\n%c %zu", &type, &capacity); // NEED this \n
        storage->rooms[i].type = type;
        storage->rooms[i].capacity = capacity;
        storage->rooms[i].taken = 0;
        storage->rooms[i].roomOriginalId = i + 1;
        if (capacity > storage->maxSizedRoom[type-SMALLEST_ROOM]){
            storage->maxSizedRoom[type-SMALLEST_ROOM] = capacity;
            storage->biggestFreeRoom[type-SMALLEST_ROOM] = capacity;
        }
        printf("%zu (%zu), %c %zu %i\n", i, m, storage->rooms[i].type, storage->rooms[i].capacity, storage->rooms[i].taken); //Test print
    }
    for (size_t i = 1; i <= n; i++){
        storage->freePlayer[i] = 1;
        storage->currentGameByPlayer[i] = -1;
    }
    printf("OK\n");
    qsort(storage->rooms, m, sizeof(struct Room), compRooms);

    setUpLists(&storage->listPool, &storage->listOfPlans, &storage->planPool);

    char filename[30];
    for (size_t i = 1; i <= n; i++){
        sprintf(filename, "player-%zu.in", i);

        int ithFd = open(filename, O_RDONLY);
        SYSTEM2(ithFd < 0, "Bad read");
        char a; read(ithFd, &a, 1);
        printf("Player %zu has %c type\n", i, a);
        storage->playerPrefdRoom[i] = a;
        ++storage->allPlayersForTypes[a-SMALLEST_ROOM];
        close(ithFd);
    }
    struct Semaphores sems;
    initSems(storage, &sems);

    return storage;
}

int initSems(struct Storage const *storage, struct Semaphores* semaphores){ // Unix semaphores
    int sem_protection = semget(SEM_PROTECTION, 1, IPC_CREAT | 0600);
    int sems_entry = semget(SEMS_ENTRY, (int) storage->playerCount + 1, IPC_CREAT | 0600);
    int sems_exit = semget(SEMS_EXIT, (int) storage->playerCount + 1, IPC_CREAT | 0600);
    if (sem_protection == -1 || sems_entry == -1 || sems_exit == -1) return -1;
    if (semctl(sem_protection, 0, SETVAL, 0) == -1){
        return -2;
    }
    for (int i = 0; i < storage->playerCount; ++i){
        if (semctl(sems_entry, i, SETVAL, 0) == -1){
            return -2;
        }
        if (semctl(sems_exit, i, SETVAL, 0) == -1){
            return -2;
        }
    }
    semaphores->protection = sem_protection;
    semaphores->entry = sems_entry;
    semaphores->exit = sems_exit;
    return 0;
}

int getSems(struct Storage const *storage, struct Semaphores* semaphores){ // Unix semaphores
    int sem_protection = semget(SEM_PROTECTION, 1, 0600);
    int sems_entry = semget(SEMS_ENTRY, (int) storage->playerCount, 0600);
    int sems_exit = semget(SEMS_EXIT, (int) storage->playerCount, 0600);
    if (sem_protection == -1 || sems_entry == -1 || sems_exit == -1) return -1;
    semaphores->protection = sem_protection;
    semaphores->entry = sems_entry;
    semaphores->exit = sems_exit;
    return 0;
}



int main() {
    struct Storage* storage = getFromInput();
    size_t n = storage->playerCount;


    printf("OK\n");
    int pid;
    for (size_t i = 1; i <= n; i++) {
        pid = fork();
        switch (pid) {
            case -1:
                munmap(storage, sizeof(struct Storage));
                shm_unlink(STORAGE);
                SYSTEM2(1, "fork");
            case 0: // child process
                munmap(storage, sizeof(struct Storage));
                player(i);
                return 0;
            default:
                continue;
        }
    }
    printf("OK\n");
    for (size_t i = 1; i <= n; i++) {
        SYSTEM2(wait(0) == -1, "wait"); // TODO: zwalniac zasoby
    }

    munmap(storage, sizeof(struct Storage));
    shm_unlink(STORAGE);

    return 0;
}