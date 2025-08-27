# OnTicTacToeLine (TicTacToe Multiplayer)  

## Description  
This project implements a **multiplayer TicTacToe** in **C++**, developed as an educational exercise during the **third year of Video Game Programming course at AIV** (Accademia Italiana dei Videogiochi). <br><br>
The main goal was to practice with:  
- **Network communication via BSD sockets**  
- **Graphics handling with SDL2**  
- **Basic room and user management on the server**  
- **Handling client inactivity states**  

The game allows multiple users to connect to a central server, create or join a room, and challenge each other in TicTacToe matches.  

---

## Educational Goals  
This project is not intended for production but was created for educational purposes, focusing on:  
- Understanding the **basics of network programming**  
- Implementing a simple but functional **room management system**  
- Managing **multiple connected users** simultaneously  
- Handling **client inactivity** and graceful disconnection  

---

## Project Structure  
- **Server**: handles connections, rooms, and player states  
- **Client**: SDL-based graphical interface and communication with the server  
- **Game Logic**: TicTacToe rules implementation  

---

## Setup

### Disclaimer  

If you want to actually play in **multiplayer mode** (and not just on `localhost`), you need to configure the IP addresses correctly:  
- In **`tictactoe_client.hpp`** → set the **public IP address** of the machine running the server (or the public IP of the router).  
- In **`tictactoe_server.hpp`** → set the **IP address (public or private)** of the machine on which the server is running.  
If the server is hosted on a machine with a **private IP address**, you must configure **NAT forwarding** (or enable **DMZ**) in your gateway's web interface.  

---

### Compilation  

After editing the client and server files, compile them with the following commands:  
- Client: 
```bash
clang src/tictactoe_client.cpp src/utility.cpp -o tictactoe_client.exe -I"include\SDL2-2.30.9\include" -L"include\SDL2-2.30.9\lib\x64" -I"include" -Xlinker /subsystem:console -lSDL2main -lSDL2 -lshell32 -lws2_32 
```
- Server:
```bash
clang src/tictactoe_server.cpp src/room.cpp src/player.cpp src/utility.cpp -o tictactoe_server.exe -I"include" -lws2_32
```

### Play

1. Launch the server: **'tictactoe_server.exe'**  
2. Launch one or more clients: **'tictactoe_client.exe'**  
3. Follow the commands list in order to join in the server, create a room (or join in a room) and play!

---

## Screenshots

![1](https://github.com/user-attachments/assets/f309b064-af7c-422e-90eb-5a5bcc1a1b25)
