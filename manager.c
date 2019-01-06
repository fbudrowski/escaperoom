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
#include "err.h"

#define MAX_PLAYERS 1026
#define COLOR_ZERO (MAX_PLAYERS + 100)
#define MAX_ROOMS 1024
#define SMALLEST_ROOM 'A'
#define BIGGEST_ROOM 'Z'
#define ROOM_TYPES_COUNT (BIGGEST_ROOM - SMALLEST_ROOM +1)

#define STORAGE "/storage"

#define MAX_LIST_ELEMS 32768
#define MAX_PLANS 8096

#define LST_NULL (MAX_LIST_ELEMS + 10)
#define LST_INACTIVE (LST_NULL + 1)

#define PLAN_INACTIVE 0
#define PLAN_NULL (MAX_PLANS + 2)

void player(size_t id);

typedef size_t plan_index_t;
typedef size_t node_index_t;

struct Room {
    char type;
    size_t capacity;
    int taken;
};

struct ListNode {
    size_t value;
    node_index_t next;
};

struct ForwardList{ // : All nodes nexts to LST_INACTIVE
    node_index_t starting;
    node_index_t ending;
};

struct ListPool{
    struct ListNode nodes[MAX_LIST_ELEMS];
    node_index_t currentInd;
};


struct Plan{
    char room_type;
    int elem_count;
    struct ForwardList elements;
};

struct PlanPool{
    struct Plan plans[MAX_PLANS];
    plan_index_t currentInd;
};

plan_index_t getEmptyPlan(struct PlanPool *planPool);
int listAppend(struct ForwardList *list, struct ListPool *pool, size_t value);
void listClear(struct ForwardList *list, struct ListPool *pool);
plan_index_t addNewPlan(struct ForwardList *list, struct ListPool *listPool, struct PlanPool *planPool);
void setUpLists(struct ListPool* pool, struct ForwardList* list, struct PlanPool* planPool);
void deletePlan(struct ForwardList *list, struct ListPool *listPool, struct PlanPool *planPool, node_index_t prevNode);






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


int listAppend(struct ForwardList *list, struct ListPool *pool, size_t value){
    if (list->ending == LST_NULL || list->ending == LST_INACTIVE){
        list->starting = list->ending = 0;
    }
    else{
        node_index_t i = pool->currentInd;
        size_t ct = 0;
        for (; ct < MAX_LIST_ELEMS; ++ct, ++i) {
            if (i >= MAX_LIST_ELEMS)
                i -= MAX_LIST_ELEMS;
            if (pool->nodes[i].next == LST_INACTIVE){
                pool->nodes[list->ending].next = i;
                list->ending = i;
                break;
            }
        }
        if (ct >= MAX_LIST_ELEMS)
            return -1;
    }
    pool->nodes[list->ending].value = value;
    pool->nodes[list->ending].next = LST_NULL;
    return 0;
}

void listClear(struct ForwardList *list, struct ListPool *pool){
    node_index_t current = list->starting;
    while(current != LST_INACTIVE && current != LST_NULL){
        node_index_t prev = current;
        current = pool->nodes[prev].next;
        pool->nodes[prev].next = LST_INACTIVE;
    }
    list->starting = list->ending = LST_INACTIVE;
}

plan_index_t addNewPlan(struct ForwardList *list, struct ListPool *listPool, struct PlanPool *planPool){
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

void setUpLists(struct ListPool* pool, struct ForwardList* list, struct PlanPool* planPool){
    pool->currentInd = 0;
    for(int i = 0; i < MAX_LIST_ELEMS; i++) {
        pool->nodes[i].next = LST_INACTIVE;
    }
    list->starting = LST_NULL;
    list->ending = LST_NULL;

    for(int i = 0; i < MAX_PLANS; i++){
        planPool->plans[i].room_type = 0;
    }
    planPool->currentInd = 0;
}


void deletePlan(struct ForwardList *list, struct ListPool *listPool, struct PlanPool *planPool, node_index_t prevNode) {
    node_index_t currentNode = listPool->nodes[prevNode].next;
    assert(currentNode != LST_NULL && currentNode != LST_INACTIVE);
    listPool->nodes[prevNode].next = listPool->nodes[currentNode].next;
    listPool->nodes[currentNode].next = LST_INACTIVE;
}

struct Storage {
    int maxSizedRoom[ROOM_TYPES_COUNT];
    int biggestFree[ROOM_TYPES_COUNT];
    int freePlayer[MAX_PLAYERS];
    char player_prefd_room[MAX_PLAYERS];
    size_t player_count;
    size_t room_count;
    struct Room rooms[MAX_ROOMS];

    int allForTypes[ROOM_TYPES_COUNT];

    int remainingForElems[MAX_PLAYERS];
    int remainingForTypes[ROOM_TYPES_COUNT];

    struct ListPool listPool;
    struct ForwardList listOfPlans;
    struct PlanPool planPool;
};

int initCheckPlan(struct Storage *storage, plan_index_t planIndex);
int checkPlan(struct Storage *storage, plan_index_t planIndex);

struct Storage *getFromInput();


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
        remainingElems[i] = storage->allForTypes[i];
    }
    while (nodeIndex != LST_NULL && nodeIndex != LST_INACTIVE){
        size_t value = listPool->nodes[nodeIndex].value;
        size_t color = 0;
        if (value >= COLOR_ZERO){
            color = value - COLOR_ZERO;
        } else{
            color = (size_t) (storage->player_prefd_room[value] - SMALLEST_ROOM);
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

    if(storage->biggestFree[plan->room_type-SMALLEST_ROOM] < plan->elem_count){
        return -1;
    }

    int remainingElems[ROOM_TYPES_COUNT];
    for (int i = 0; i < ROOM_TYPES_COUNT; ++i){
        remainingElems[i] = storage->remainingForTypes[i];
    }
    while (nodeIndex != LST_NULL && nodeIndex != LST_INACTIVE){
        size_t value = listPool->nodes[nodeIndex].value;
        size_t color = 0;
        if (value >= COLOR_ZERO){
            color = value - COLOR_ZERO;
        } else{
            color = (size_t) (storage->player_prefd_room[value] - SMALLEST_ROOM);
        }
        if (remainingElems[color] == 0){
            return -2;
        }
        --remainingElems[color];

        nodeIndex = listPool->nodes[nodeIndex].next;
    }

    return 1;
}

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

struct Storage *getFromInput() {
    size_t n, m;
    scanf("%zu%zu", &n, &m);

    int fd = shm_open(STORAGE, O_CREAT | O_RDWR, S_IWUSR | S_IRUSR);
    if (fd == -1) {
        syserr("shm_open fail");
    }
    if (ftruncate(fd, sizeof(struct Storage)) == -1) {
        shm_unlink(STORAGE);
        syserr("ftruncate fail");
    }
    struct Storage *storage =
            mmap(NULL, sizeof(struct Storage), PROT_READ | PROT_WRITE,
                 MAP_SHARED, fd, 0);
    if (storage == MAP_FAILED) {
        shm_unlink(STORAGE);
        syserr("Mapping fail");
    }

    storage->player_count = n;
    storage->room_count = m;
    for(int i = 0; i < ROOM_TYPES_COUNT; i++){
        storage->biggestFree[i] = -1;
        storage->maxSizedRoom[i] = -1;
    }

    for (size_t i = 0; i < m; i++) {
        scanf("\n%c %zu", &(storage->rooms[i].type), &(storage->rooms[i].capacity)); // NEED this \n
        storage->rooms[i].taken = 0;
//        printf("%c %zu %i\n", storage->rooms[i].type, storage->rooms[i].capacity, storage->rooms[i].taken); //Test print
    }
    for (size_t i = 1; i <= n; i++){
        storage->freePlayer[i] = 1;
    }

    qsort(storage->rooms, m, sizeof(struct Room), compRooms);

    setUpLists(&storage->listPool, &storage->listOfPlans, &storage->planPool);

    char filename[30];
    for (size_t i = 1; i <= n; i++){
        sprintf(filename, "player-%zu.in", i);
        int fd = open(filename, O_RDONLY);
        char a; scanf("%c", &a);
        storage->player_prefd_room[i] = a;
        ++storage->allForTypes[a-SMALLEST_ROOM];
        ++storage->remainingForTypes[a-SMALLEST_ROOM];
        close(fd);
    }

    return storage;
}


int main() {
    struct Storage* storage = getFromInput();
    size_t n = storage->player_count;


    int pid;
    for (size_t i = 1; i <= n; i++) {
        pid = fork();
        switch (pid) {
            case -1:
                munmap(storage, sizeof(struct Storage));
                shm_unlink(STORAGE);
                syserr("fork");
            case 0: // child process
                player(i);
                munmap(storage, sizeof(struct Storage));
                return 0;
            default:
                continue;
        }
    }
    for (size_t i = 1; i <= n; i++) {
        wait(0);
    }

    munmap(storage, sizeof(struct Storage));
    shm_unlink(STORAGE);

    return 0;
}