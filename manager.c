#include <semaphore.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <stdlib.h>
#include "err.h"
#include "storage.h"


int main() {
    struct Storage *storage = getFromInput();
    size_t n = storage->playerCount;


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
    for (size_t i = 1; i <= n; i++) {
        if (wait(0) == -1){
            break;
        }
    }

    for(int i = 0; i <= n; i++){
        sem_destroy(&storage->isToEnter[i]);
        sem_destroy(&storage->entry[i]);
    }
    sem_destroy(&storage->forLastToExit);
    sem_destroy(&storage->protection);
    munmap(storage, sizeof(struct Storage));
    shm_unlink(STORAGE);

    return 0;
}