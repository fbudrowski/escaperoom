
#ifndef ESCAPEROOM_STORAGE_H
#define ESCAPEROOM_STORAGE_H

#endif //ESCAPEROOM_STORAGE_H

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
#define PLAN_PRESTART (MAX_LIST_ELEMS + 3)


void player(size_t id);

typedef size_t plan_index_t;
typedef size_t node_index_t;

struct Room {
    char type;
    size_t capacity;
    int taken;
    int roomOriginalId;
};

int compRooms(const void *room1, const void *room2);

struct ListNode {
    size_t value;
    node_index_t next;
    node_index_t prev;
};

struct LinkedList { // : All nodes nexts to LST_INACTIVE
    node_index_t starting;
    node_index_t ending;
};

struct ListPool {
    struct ListNode nodes[MAX_LIST_ELEMS];
    node_index_t currentInd;
};


struct Plan {
    int assignedRoomNewId;
    char room_type;
    int elem_count;
    struct LinkedList elements;
};

struct PlanPool {
    struct Plan plans[MAX_PLANS];
    plan_index_t currentInd;
};

plan_index_t getEmptyPlan(struct PlanPool *planPool);
int listAppend(struct LinkedList *list, struct ListPool *pool, size_t value);
void listClear(struct LinkedList *list, struct ListPool *pool);
plan_index_t addNewPlan(struct LinkedList *list, struct ListPool *listPool, struct PlanPool *planPool);
void setUpLists(struct ListPool *pool, struct LinkedList *list, struct PlanPool *planPool);
void
deletePlan(struct LinkedList *list, struct ListPool *listPool, struct PlanPool *planPool, node_index_t currentNode);


struct Storage {
    int maxSizedRoom[ROOM_TYPES_COUNT];
    int biggestFreeRoom[ROOM_TYPES_COUNT];
    int freePlayer[MAX_PLAYERS];
    char playerPrefdRoom[MAX_PLAYERS];
    int currentGameByPlayer[MAX_PLAYERS];

    size_t playerCount;
    size_t roomCount;
    struct Room rooms[MAX_ROOMS];

    int allPlayersForTypes[ROOM_TYPES_COUNT];

    int remainingPlayersForTypes[ROOM_TYPES_COUNT];

    struct ListPool listPool;
    struct LinkedList listOfPlans;
    struct PlanPool planPool;
};

int initCheckPlan(struct Storage *storage, plan_index_t planIndex);
int checkPlan(struct Storage *storage, plan_index_t planIndex);

struct Storage *getFromInput();

#define SEM_PROTECTION 4444
#define SEMS_ENTRY 5555
#define SEMS_EXIT 6666

struct Semaphores {
    int protection;
    int entry;
    int exit;
};
int initSems(struct Storage const *storage, struct Semaphores *semaphores);
int getSems(struct Storage const *storage, struct Semaphores *semaphores);