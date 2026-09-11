# civ-mp

An open-source multiplayer rewrite of Sid Meier's Civilization 1, written in C with the Raylib library.

Currently in active development.

## Screenshots

<img width="964" height="742" alt="Screenshot 2026-09-10 16:14:21" src="https://github.com/user-attachments/assets/7993a2ce-2f69-418d-a553-5a68ba0047d9" />
<img width="964" height="742" alt="Screenshot 2026-09-10 16:07:57" src="https://github.com/user-attachments/assets/40615b36-544c-4ccc-ab1a-158494ebdd57" />
<img width="964" height="742" alt="Screenshot 2026-09-10 16:06:00" src="https://github.com/user-attachments/assets/f6284a97-5ed3-4254-a6c9-6e9c3bc305f1" />

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
