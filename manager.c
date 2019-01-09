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