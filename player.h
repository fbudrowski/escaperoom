//
// Created by franek on 09.01.19.
//

#ifndef ESCAPEROOM_PLAYER_H
#define ESCAPEROOM_PLAYER_H


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


void playGame(size_t playerId, struct Storage *storage, FILE *output);

void player(size_t id);

#endif //ESCAPEROOM_PLAYER_H
