
## Multiprocess room allocation simulator

The purpose of this project is to demonstrate concurrent resource management and features of POSIX-compliant semaphores and shared memory in Unix. This project was an assignment for a Concurrent Programming course at the University of Warsaw.

#### Description

The project implements two processes: `manager` and `player`, which interact with each other according to the following story.

There are n players playing in m escape rooms. Each room has an integer size and a type (A-Z) of riddles inside. Each player has an identifier, a favorite type of a room (which remains constant) and a certain number of game proposals, consisting of a room type and a list of players (or 'any player with a given preferred room type' entries) playing in the game. The game's proponent plays in that game.

Firstly, player submits his proposal onto a list. Then he iterates over, checking if each proposal can be played. When a proposal becomes doable, he immediately calls other players to join in and reserves the room. Then players join in, when all are in they begin to play. When any player is done playing, he leaves, and after the last player leaves the game, the manager frees all the resources, including the room. After exiting, the player can initiate a game, join a game, wait or exit. 

#### Technical details

The process `manager` is to be run without parameters. In the first line of standard it expects numbers `n` and `m`, and in each of the following `m` lines there should be room descriptions: type (as a capital English letter) and room capacity. 

The process `player` is initiated by the manager, via `fork` and `execlp`. It reads data from the file `player-k.in`, where `k` is a 1-based player id number.
The first line of that file has a favourite-type letter. Each following line has a game proposal: firstly a letter indicating room type, and then some amount of numbers and/or letters describing players or player-preferred-room-types.

#### Additional restrictions
Could use only shared memory and semaphores for IPC. Limits for n: 2 <= n <= 1000, m: 1 <= m <= 1000
