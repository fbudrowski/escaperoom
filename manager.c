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



#define ROOMS "/room"

void player(size_t id);

struct room{
    char type;
    size_t capacity;
    int taken;
};
int compRooms(const void* room1, const void* room2){
    const struct room* r1 = room1, *r2 = room2;
    if (r1->capacity == r2->capacity){
        if (r1->type < r2->type) return -1;
        if (r1->type > r2->type) return 1;
        return 0;
    }
    if (r1->capacity < r2->capacity) return -1;
    return 1;
}


int main() {
    size_t n, m; scanf("%zu%zu", &n, &m);

    int fd =shm_open(ROOMS, O_CREAT | O_RDWR, S_IWUSR | S_IRUSR);
    if (fd == -1){
        syserr("shm_open fail");
    }
    if (ftruncate(fd, m * sizeof(struct room)) == -1){
        shm_unlink(ROOMS);
        syserr("ftruncate fail");
    }
    struct room* rooms =
            mmap(NULL, m * sizeof(struct room), PROT_READ | PROT_WRITE,
                    MAP_SHARED, fd, 0);
    if (rooms == MAP_FAILED){
        shm_unlink(ROOMS);
        syserr("Mapping fail");
    }



    for (size_t i = 0; i < m; i++){
        scanf("%c %zu", &(rooms[i].type), &(rooms[i].capacity));
        rooms[i].taken = 0;
    }

    qsort(rooms, m, sizeof(struct room), compRooms);

    int pid;
    for (size_t i = 1; i <= n; i++){
        switch(pid = fork()){
            case -1:
                munmap(rooms, m * sizeof(struct room));
                shm_unlink(ROOMS);
                syserr("fork");
            case 0: // child process
                player(i);
                munmap(rooms, m * sizeof(struct room));
                return 0;
            default:
                continue;
        }
    }
    for (size_t i = 1; i <= n; i++){
        wait(0);
    }

    munmap(rooms, m * sizeof(struct room));
    shm_unlink(ROOMS);

    return 0;
}