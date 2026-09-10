# Civ

An open-source rewrite of Sid Meier's Civilization 1, written in C with the Raylib library.

Currently in active development.

## Screenshots



## Features so far

- LAN Multiplayer
- Procedural world generation
- `worldgendisplayer`, a tool for visualizing and tweaking world generation settings
- Server-side fog of war system
- Simultaneous turns
- Unit movement, transporting, and battles
- The original Civilization 1 sprites/artwork

## Todo

- Cities
- GUI
- Land improvements
- Diplomacy
- Many more features

## Specs

Currently this project only supports MacOS and Linux. This may change in the future.

## Building and running it

```bash
# 1. compile binaries
make

# 2. start the server
./bin/server <SERVER_PORT>

# 3. start any number of clients from machines on the same local network as the server
./bin/client <SERVER_IP> <SERVER_PORT>

# 4. To start the game, input `start` to the server program's stdin

# 5. Use QWEADZXC for 8-direction unit movement and Space to skip a unit
```
