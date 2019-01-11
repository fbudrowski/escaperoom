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

void printList(struct ListPool *listPool, struct LinkedList* list){
    size_t elemId = list->starting;
    char ans[211];
    int pos = sprintf(ans, "List consists of following elems:");
    while(elemId != LST_NULL && elemId != LST_INACTIVE){
        struct ListNode* node = &listPool->nodes[elemId];

        pos += sprintf(ans + pos , "%zu, ", node->value);

        elemId = node->next;

    }
    DEBUG("%s\n", ans);
}

plan_index_t getEmptyPlan(struct PlanPool *planPool){
    plan_index_t i = planPool->currentInd;
    size_t ct = 0;
    for (; ct < MAX_PLANS; ++ct, ++i) {
        if (i >= MAX_PLANS)
            i -= MAX_PLANS;
        if (planPool->plans[i].room_type == PLAN_INACTIVE){
            planPool->plans[i].assignedRoomNewId = 0;
            planPool->plans[i].elements.starting = LST_INACTIVE;
            planPool->plans[i].elements.ending = LST_INACTIVE;
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
            if (list->ending != LST_INACTIVE && list->ending != LST_NULL){
                pool->nodes[list->ending].next = i;
                pool->nodes[i].prev = list->ending;
                pool->nodes[i].next = LST_NULL;
                list->ending = i;
            }
            else{
                list->starting = list->ending = i;
                pool->nodes[i].prev = LST_NULL;
                pool->nodes[i].next = LST_NULL;
            }
            break;
        }
    }
    if (ct >= MAX_LIST_ELEMS)
        return -1;

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

plan_index_t addNewEmptyPlan(struct LinkedList *list, struct ListPool *listPool, struct PlanPool *planPool){
    plan_index_t planId = getEmptyPlan(planPool);
    planPool->plans[planId].room_type = PLAN_PRESTART;
    planPool->plans[planId].elem_count = 0;
    planPool->plans[planId].assignedRoomNewId = -1;
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



void deletePlan(struct LinkedList *list, struct ListPool *listPool, struct PlanPool *planPool, plan_index_t planIndex) {
    node_index_t listElem = list->starting;
    while (listElem != LST_NULL && listElem != LST_INACTIVE && listPool->nodes[listElem].value != planIndex) {
        listElem = listPool->nodes[listElem].next;
    }

    listClear(&planPool->plans[planIndex].elements, listPool);
    DEBUG("Delete plan %zu, author %d\n", planIndex, planPool->plans[planIndex].author);

    node_index_t prev = listPool->nodes[listElem].prev;
    node_index_t next = listPool->nodes[listElem].next;

    listPool->nodes[listElem].prev = listPool->nodes[listElem].next = LST_INACTIVE;

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
        DEBUG("\tCHK %zu %d %c\n", value, remainingElems[color], (char)color + SMALLEST_ROOM);
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

    storage->alreadyFinishedWriting = 0;
    storage->playerCount = n;
    storage->roomCount = m;
    for(int i = 0; i < ROOM_TYPES_COUNT; i++){
        storage->biggestFreeRoom[i] = -1;
        storage->maxSizedRoom[i] = -1;
        storage->allPlayersForTypes[i] = 0;
        storage->remainingPlayersForTypes[i] = 0;
        storage->playerTypeInPlans[i] = 0;
    }

    for (size_t i = 0; i < m; i++) {
        char type;
        size_t capacity;
        scanf("\n%c %zu", &type, &capacity); // NEED this \n
        storage->rooms[i].type = type;
        storage->rooms[i].capacity = capacity;
        storage->rooms[i].inside = 0;
        storage->rooms[i].taken = 0;
        storage->rooms[i].roomOriginalId = i + 1;
        if ((int)capacity > storage->maxSizedRoom[type-SMALLEST_ROOM]){
            storage->maxSizedRoom[type-SMALLEST_ROOM] = (int) capacity;
            storage->biggestFreeRoom[type-SMALLEST_ROOM] = (int) capacity;
        }
        DEBUG("%zu (%zu), %c %zu %d; %d %d\n"
                , i, m, storage->rooms[i].type,
                storage->rooms[i].capacity, storage->rooms[i].inside,
                storage->maxSizedRoom[type - SMALLEST_ROOM],
                storage->biggestFreeRoom[type - SMALLEST_ROOM]); //Test print
    }
    for (size_t i = 1; i <= n; i++){
        storage->freePlayer[i] = 1;
        storage->currentGameByPlayer[i] = -1;
        storage->playerInPlans[i] = 0;
        storage->playerPrefdRoom[i] = 0;
    }
    DEBUG("OK\n");
    qsort(storage->rooms, m, sizeof(struct Room), compRooms);

    setUpLists(&storage->listPool, &storage->listOfPlans, &storage->planPool);

    char filename[30];
    for (size_t i = 1; i <= n; i++){
        sprintf(filename, "player-%zu.in", i);
        int ithFd = open(filename, O_RDONLY);
        SYSTEM2(ithFd < 0, "Bad read");
        char a; read(ithFd, &a, 1);
//        DEBUG("Player %zu has %c type\n", i, a);
        storage->playerPrefdRoom[i] = a;
        ++storage->allPlayersForTypes[a-SMALLEST_ROOM];
        close(ithFd);
    }
    initSems(storage);

    return storage;
}

int initSems(struct Storage *storage) { // Unix semaphores
    SYSTEM2(sem_init(&storage->protection, 1, 1) < 0, "seminit");
    SYSTEM2(sem_init(&storage->forLastToExit, 1, 0) < 0, "seminit");
    for(int i = 0; i < MAX_PLAYERS; i++){
        SYSTEM2(sem_init(&storage->isToEnter[i], 1, 0) < 0, "seminit");
        SYSTEM2(sem_init(&storage->entry[i], 1, 0) < 0, "seminit");
    }
    return 0;
}

int getType(size_t playerId, struct Storage *storage) {
    return storage->playerPrefdRoom[playerId] - SMALLEST_ROOM;
}

