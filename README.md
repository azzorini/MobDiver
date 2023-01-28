# MobDiver

In this repository I leave some code to see the effects that the mobility of the species have in biodiversity.

## About the simulation

The simulation takes place in a square lattice whith periodic boundary conditions. There are three species (A, B, C) such that A kills B, B kills C and C kills A. The cells of the lattice can also be empty. The rate at which two different species that are neighbours interact is $\sigma$ after that interaction the loser will die leaving and empty slot. One individual of a given species can reproduce if it has an empty slot as neighbour with a rate $\mu$. After the reproduction the empty slot will be occupied by a individual of the same species. Finally different cells can swap positions with a rate $\varepsilon$. Instead of using this rate in the simulations I use the mobility: $M = 2\varepsilon N$ where $N$ is the number of cells. The simulations are done using the residence time algorithm.

## Included files

The files included are the following:

1. **Mobility_image.cpp:** This file runs the simulation with the given parameters and give back one image of the final state of the system. It is useful to get some images of how is the system at a given time and it also can give us a idea of how many time we need to run future simulations in our machine.
2. **Mobility_gif.cpp:** This file assumes that there is a subdirectory called *Frames/* and it tries to put there screenshots of the system taking at a given time interval in order to create a GIF. Warning: This program save the files in a ASCII [Netpbm format]{https://en.wikipedia.org/wiki/Netpbm} so the compression of the images is very bad. A large amount of frames can take a lot of space in disk. The maximum number of frames allowed by the program is 99999.
3. **FunctionThreadPool.hpp** and **FunctionThreadPool.tpp:** This is a library that I created and uploaded to my github ([Function Thread Pool]{https://github.com/azzorini/FunctionThreadPool}). It allows to run a given function with different parameters in multiple threads. If you are interested in how that works go to the README in the given repository link.
4. **ExtinctionProb.cpp:** Calculates the extinction probability for a given system size using a given number of simulations to compute each average. Since this can take a long time for big system sizes it uses several threads to do work in parallel.
