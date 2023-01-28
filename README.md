# MobDiver

In this repository I leave some code to see the effects that the mobility of the species have in biodiversity.

## About the simulation

The simulation takes place in a square lattice whith periodic boundary conditions. There are three species (A, B, C) such that A kills B, B kills C and C kills A. The cells of the lattice can also be empty. The rate at which two different species that are neighbours interact is $\sigma$ after that interaction the loser will die leaving and empty slot. One individual of a given species can reproduce if it has an empty slot as neighbour with a rate $\mu$. After the reproduction the empty slot will be occupied by a individual of the same species. Finally different cells can swap positions with a rate $\varepsilon$. Instead of using this rate in the simulations I use the mobility: $M = 2\varepsilon N$ where $N$ is the number of cells. The simulations are done using the residence time algorithm.

## Included files
