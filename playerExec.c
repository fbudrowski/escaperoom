#include "player.h"


int main(int argc, char **argv) {
    long int k = strtol(argv[0], NULL, 10);
    player((size_t) k);
    return 0;
}