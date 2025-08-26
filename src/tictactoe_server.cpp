#include <tictactoe_server.hpp>

#include <iostream>

Server::Server(const char* ipAddress, const int port, const std::uint32_t timeout)
{
#ifdef _WIN32
    try
    {
        WSADATA wsa_data;
        if (WSAStartup(0x202, &wsa_data))
        {
            throw NetworkException("ERROR: Unable to initialize \"winsock2\"!\n");
        }   
    }
    catch (const NetworkException& exception)
    {
        std::cout << exception.what() << "\n";
    }
#endif

    try
    {
        this->socketID = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (this->socketID < 0)
        {
            throw NetworkException("ERROR: Unable to initialize the UDP socket!\n");
        }

        inet_pton(AF_INET, ipAddress, &sin.sin_addr);
        this->sin.sin_family = AF_INET;
        this->sin.sin_port = htons(port);

        // "reinterpret_cast" is great to modify the interpretation about a memory address.
        if (setsockopt(this->socketID, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(std::uint32_t)))
        {
            throw NetworkException("ERROR: Unable to set socket option for receive timeout!\n");
        }

        if (setsockopt(this->socketID, SOL_SOCKET, SO_RCVBUF, reinterpret_cast<const char*>(&bufferSize), sizeof(std::uint32_t)))
        {
            throw NetworkException("ERROR: Unable to set socket option for receive buffer size!\n");
        }

        if (setsockopt(this->socketID, SOL_SOCKET, SO_SNDBUF, reinterpret_cast<const char*>(&bufferSize), sizeof(std::uint32_t)))
        {
            throw NetworkException("ERROR: Unable to set socket option for send buffer size!\n");
        }

        if (bind(this->socketID, reinterpret_cast<sockaddr*>(&this->sin), sizeof(this->sin)))
        {
            throw NetworkException("ERROR: Unable to bind the UDP socket!\n");
        }
    }
    catch (const NetworkException& exception)
    {
        std::cout << exception.what() << "\n";
    }

    // Init commands. I must use the lambda expressions.
    this->commandFunctions[Command::JOIN] = [this](Byte* buffer, Sender& sender, const int len) { this->JoinCommand(buffer, sender, len); };
    this->commandFunctions[Command::CREATE_ROOM] = [this](Byte* buffer, Sender& sender, const int len) { this->CreateRoomCommand(buffer, sender, len); };
    this->commandFunctions[Command::CHALLENGE] = [this](Byte* buffer, Sender& sender, const int len) { this->ChallengeCommand(buffer, sender, len); };
    this->commandFunctions[Command::MOVE] = [this](Byte* buffer, Sender& sender, const int len) { this->MoveCommand(buffer, sender, len); };
    this->commandFunctions[Command::QUIT] = [this](Byte* buffer, Sender& sender, const int len) { this->QuitCommand(buffer, sender, len); };

    std::cout << "Server is ready!\n";
}

// ----------------------------------------------------------------------------------------------

std::string Server::Sender::GetIpAddress() const
{
    return this->ipAddress;
}

void Server::Sender::SetIpAddress(const std::string& ipAddress)
{
    this->ipAddress = ipAddress;
}

// ----------------------------------------------------------------------------------------------

int Server::Sender::GetPort() const
{
    return this->port;
}

void Server::Sender::SetterPort(const int port) // SetPort was already defined.
{
    this->port = port;
}

// ----------------------------------------------------------------------------------------------

bool Server::Sender::operator==(const Sender& otherSender) const
{
    return ipAddress == otherSender.ipAddress && port == otherSender.port;
}

// ----------------------------------------------------------------------------------------------

void Server::Kick(const Sender& sender)
{
    Player& badPlayer = this->players[sender];
    int roomID = badPlayer.GetCurrentRoom().first;
    bool isOwner = badPlayer.GetCurrentRoom().second;

    if (roomID > 0)
    {
        Room& currentRoom = this->rooms[roomID];

        if (isOwner)
        { 
            this->ResetClient(currentRoom);
            this->DestroyRoom(currentRoom);
        }
        else 
        {
            this->ResetClient(currentRoom);
            currentRoom.Reset(true);
        }
    }

    std::cout << "[" << sender.GetIpAddress() << ":" << sender.GetPort() << "] \"" << badPlayer.GetName() << "\" has been kicked!\n";
    players.erase(sender);
}

void Server::DestroyRoom(const Room& room)
{
    std::pair<int, bool> noRoomInfo = { -1, true };

    // I can't use "const" because i assign a reference type.
    for (auto& player : this->players)
    {
        Player& challenger = player.second;
        if (room.GetChallenger() && *room.GetChallenger() == challenger)
        {
            challenger.SetCurrentRoom(noRoomInfo);
        }
    }

    std::cout << "Room with ID: " << room.GetRoomID() << " has been destroyed!\n";
    openedRooms.erase(room.GetRoomID());
    rooms.erase(room.GetRoomID());
}

void Server::RemovePlayer(const Sender& sender)
{
    Player& player = this->players[sender];
    int currentRoomID = player.GetCurrentRoom().first;

    if (currentRoomID <= 0)
    {
        std::cout << "Player \"" << player.GetName() << "\" removed!\n";
        players.erase(sender);
        return;
    }

    Room& room = this->rooms[currentRoomID];

    if (room.GetChallenger())
    {
        if (*room.GetChallenger() == player)
        {
            this->ResetClient(room);
            room.Reset(true);

            std::cout << "Player \"" << player.GetName() << "\" removed!\n";
            players.erase(sender);

            this->Announces(room.GetRoomID(), false);
            return;
        }
    }

    this->ResetClient(room);
    this->DestroyRoom(room);

    std::cout << "Player \"" << player.GetName() << "\" removed!\n";
    players.erase(sender);

    this->Announces(room.GetRoomID(), true);
}

void Server::Tick()
{
    Byte buffer[bufferSize];
    sockaddr_in senderInput;
    int senderInputSize = sizeof(senderInput);

    int len = recvfrom(this->socketID, buffer, bufferSize, 0, reinterpret_cast<sockaddr*>(&senderInput), &senderInputSize);
    if (len < 0) return;

    if (len < headerBytesAmount)
    {
        std::cout << "Invalid packet size: " << len << " bytes!\n";
        return;
    }

    // Packet Protocol: >II...
    // Big Endian | RID | Command | Payload (based on the command)

    Header header;
    header.rid = buffer[0] - '0';
    header.command = static_cast<Command>(buffer[1] - '0');

    Byte addrAsString[bufferSize];
    inet_ntop(AF_INET, &senderInput.sin_addr, addrAsString, bufferSize);

    Sender sender;
    sender.SetIpAddress(addrAsString);
    sender.SetterPort(ntohs(senderInput.sin_port));

    if (header.command >= 0 || header.command < commandFunctions.size())
    {
        this->commandFunctions[header.command](buffer, sender, len);
        return;
    }

    std::cout << "Unknown command from [" << sender.GetIpAddress() << ":" << sender.GetPort() << "]\n";
}

void Server::SendAnnounce(const Sender& sender)
{
    for (const int& room : this->openedRooms)
    {            
        Announce announce;
        announce.header.rid = 0;
        announce.header.command = Command::ANNOUNCE_ROOM;
        announce.roomID = room;

        std::string roomIDString(std::to_string(announce.roomID));
        std::string announceInfo(std::to_string(announce.header.rid) + std::to_string(static_cast<std::uint32_t>(announce.header.command)) + Utility::GetParsedRoomIDLength(roomIDString, roomIDString.length()) + roomIDString);
        const char* announcePacket = announceInfo.c_str();

        sockaddr_in sender_in;
        sender_in.sin_family = AF_INET;
        inet_pton(AF_INET, sender.GetIpAddress().c_str(), &sender_in.sin_addr);
        sender_in.sin_port = htons(sender.GetPort());

        int sent_bytes = sendto(this->socketID, announcePacket, strlen(announcePacket), 0, reinterpret_cast<sockaddr*>(&sender_in), sizeof(sender_in));
    }
}

void Server::Announces(const int roomID, const bool toRemove)
{
    if (toRemove) this->openedRooms.erase(roomID);
    else this->openedRooms.insert(roomID);

    if (this->openedRooms.size() <= 0) return;

    for (auto& sender : this->players)
    {
        const Player& currentPlayer = sender.second;
        int localRoomID = currentPlayer.GetCurrentRoom().first;
        if (localRoomID > 0) continue;

        this->SendAnnounce(sender.first);
    }
}

void Server::StartGame(const Room& room) const
{
    Player& owner = *room.GetOwner();
    Player& challenger = *room.GetChallenger();

    Header header;
    header.rid = 0;
    header.command = Command::START_GAME;

    for (auto& sender : this->players)
    {
        const Player& currentPlayer = sender.second;

        if (currentPlayer.GetCurrentRoom().first == owner.GetCurrentRoom().first || currentPlayer.GetCurrentRoom().first == challenger.GetCurrentRoom().first)
        {
            sockaddr_in sender_in;
            sender_in.sin_family = AF_INET;
            inet_pton(AF_INET, sender.first.GetIpAddress().c_str(), &sender_in.sin_addr);
            sender_in.sin_port = htons(sender.first.GetPort());

            std::string startGameInfo(std::to_string(header.rid) + std::to_string(static_cast<std::uint32_t>(header.command)));
            const char* startGamePacket = startGameInfo.c_str();

            int sent_bytes = sendto(this->socketID, startGamePacket, strlen(startGamePacket), 0, reinterpret_cast<struct sockaddr*>(&sender_in), sizeof(sender_in));
        }
    }
}

void Server::CheckDeadPeers()
{
    std::vector<Sender> deadPlayers;
    const std::size_t now = Utility::GetNowTime();

    for (auto& player : players) 
    {
        Player& currentPlayer = player.second;
        const int currentRoomID = currentPlayer.GetCurrentRoom().first;

        if (currentRoomID > 0)
        {
            Room& currentRoom = this->rooms[currentRoomID];

            if (!currentRoom.IsDoorOpen())
            {
                if ((now - currentPlayer.GetLastPacketTimeStamp()) > inGameTimeoutSeconds)
                {
                    deadPlayers.push_back(player.first);
                    continue;
                }
            }
        }

        if ((now - currentPlayer.GetLastPacketTimeStamp()) > timeoutSeconds)
        {
            deadPlayers.push_back(player.first);
        }
    }

    for (const Sender& sender : deadPlayers)
    {
        this->RemovePlayer(sender);
    }
}

void Server::CheckEndedChallenges()
{
    for (auto& room : this->rooms)
    {
        if (room.second.IsDraw() || room.second.GetWinner())
        {
            if ((Utility::GetNowTime() - room.second.GetEndedChallengeTimestamp()) > resetFieldTime)
            {
                room.second.Reset(false);
                this->UpdateField(room.second);
            }
        }
    }
}

void Server::Run()
{
    for (;;)
    {
        this->Tick();
        this->CheckEndedChallenges();
        this->CheckDeadPeers();
    }
}

void Server::UpdateField(const Room& room) const
{
    Player& owner = *room.GetOwner();
    Player& challenger = *room.GetChallenger();

    Header header;
    header.rid = 0;
    header.command = Command::UPDATE_FIELD;

    for (auto& sender : this->players)
    {
        const Player& currentPlayer = sender.second;

        if (currentPlayer.GetCurrentRoom().first == owner.GetCurrentRoom().first || currentPlayer.GetCurrentRoom().first == challenger.GetCurrentRoom().first)
        {
            sockaddr_in sender_in;
            sender_in.sin_family = AF_INET;
            inet_pton(AF_INET, sender.first.GetIpAddress().c_str(), &sender_in.sin_addr);
            sender_in.sin_port = htons(sender.first.GetPort());

            std::string updatedField = { room.ParseSymbol(0), room.ParseSymbol(1), room.ParseSymbol(2), room.ParseSymbol(3), room.ParseSymbol(4), room.ParseSymbol(5), room.ParseSymbol(6), room.ParseSymbol(7), room.ParseSymbol(8) };

            std::string updateFieldInfo(std::to_string(header.rid) + std::to_string(static_cast<std::uint32_t>(header.command)) + updatedField);
            const char* updateFieldPacket = updateFieldInfo.c_str();

            int sent_bytes = sendto(this->socketID, updateFieldPacket, strlen(updateFieldPacket), 0, reinterpret_cast<sockaddr*>(&sender_in), sizeof(sender_in));
        }
    }
}

void TTTServer::Server::ResetClient(const Room& room) const
{
    Player owner, challenger;

    if (room.GetOwner()) owner = *room.GetOwner();
    if (room.GetChallenger()) challenger = *room.GetChallenger();

    Header header;
    header.rid = 0;
    header.command = Command::RESET_CLIENT;

    for (auto& sender : this->players)
    {
        const Player& currentPlayer = sender.second;

        if (currentPlayer.GetCurrentRoom().first == owner.GetCurrentRoom().first || currentPlayer.GetCurrentRoom().first == challenger.GetCurrentRoom().first)
        {
            sockaddr_in sender_in;
            sender_in.sin_family = AF_INET;
            inet_pton(AF_INET, sender.first.GetIpAddress().c_str(), &sender_in.sin_addr);
            sender_in.sin_port = htons(sender.first.GetPort());

            std::string resetClientInfo("0" + std::to_string(static_cast<int>(header.command)));
            const char* resetClientPacket = resetClientInfo.c_str();

            int sent_bytes = sendto(this->socketID, resetClientPacket, strlen(resetClientPacket), 0, reinterpret_cast<sockaddr*>(&sender_in), sizeof(sender_in));
        }
    }

}

// ----------------------------------------------------------------------------------------------

void Server::JoinCommand(Byte* buffer, Sender& sender, const int len)
{
    if (len != (playerNameBytesAmount + headerBytesAmount)) return;

    if (this->players.count(sender) > 0)
    {
        std::cout << "[" << sender.GetIpAddress() << ":" << sender.GetPort() << "] has already joined!\n";
        this->Kick(sender);
        return;
    }

    Byte playerName[playerNameBytesAmount];
    std::memcpy(playerName, &buffer[headerBytesAmount], playerNameBytesAmount);

    Player player = Player(std::string(playerName));
    this->players[sender] = player;
    player.SetLastPacketTimeStamp();
    
    std::cout << "Player \"" << player.GetName() << "\" joined from [" << sender.GetIpAddress() << ":" << sender.GetPort() << "] | {" << this->players.size() << " players on server}\n";

    this->SendAnnounce(sender);
}

void Server::CreateRoomCommand(Byte* buffer, Sender& sender, const int len)
{
    if (this->players.find(sender) != this->players.end())
    {
        Player& currentPlayer = this->players.find(sender)->second;
        int currentRoomID = currentPlayer.GetCurrentRoom().first; 

        if (currentRoomID > 0)
        {
            std::cout << "Player [" << sender.GetIpAddress() << ":" << sender.GetPort() << "] \"" << currentPlayer.GetName() << "\" already has a room!\n"; 
            return;
        }   

        std::pair<int, bool> newRoomInfo = { this->roomCounter, true };
        currentPlayer.SetCurrentRoom(newRoomInfo);
        currentPlayer.SetLastPacketTimeStamp(); 

        Room newRoom(this->roomCounter, currentPlayer);
        this->rooms[this->roomCounter] = newRoom;
        this->Announces(this->roomCounter, false);  

        std::cout << "Room with ID: " << this->roomCounter << " for player [" << sender.GetIpAddress() << ":" << sender.GetPort() << "] \"" << currentPlayer.GetName() << "\" created!\n";
        this->roomCounter++;

        return;
    }   

    std::cout << "Unknown player from [" << sender.GetIpAddress() << ":" << sender.GetPort() << "]\n";
}

void Server::ChallengeCommand(Byte* buffer, Sender& sender, const int len)
{
    if (len < (headerBytesAmount + roomIDLength)) return;

    if (this->players.find(sender) != this->players.end())
    {
        Player& currentPlayer = this->players.find(sender)->second;

        int currentRoomID = currentPlayer.GetCurrentRoom().first;           
        if (currentRoomID > 0)
        {
            std::cout << "Player [" << sender.GetIpAddress() << ":" << sender.GetPort() << "] \"" << currentPlayer.GetName() << "\" already in a room!\n"; 
            return;
        }          

        // roomIDLength parse
        std::string roomIDLengthStr;
        std::memcpy(&roomIDLengthStr, &buffer[headerBytesAmount], roomIDLength);
        const std::uint32_t roomIDLengthValue = std::stoull(roomIDLengthStr);  

        // roomID
        std::string roomIDBytes;
        std::memcpy(&roomIDBytes, &buffer[headerBytesAmount + roomIDLength], roomIDLengthValue);
        int roomID = std::stoi(roomIDBytes);    

        if (this->rooms.find(roomID) == this->rooms.end()) // Iterator check.
        {
            std::cout << "Unknown room with ID: " << roomID << "!\n";
            return;
        }         

        Room& room = this->rooms[roomID];   
        if (!room.IsDoorOpen())
        {
            std::cout << "Room with ID: " << room.GetRoomID() << " is closed!\n";
            return;
        }       

        std::pair<int, bool> roomInfo = { room.GetRoomID(), false };
        currentPlayer.SetCurrentRoom(roomInfo);
        room.SetChallenger(std::make_shared<Player>(currentPlayer));   

        currentPlayer.SetLastPacketTimeStamp();
        
        // Bad code, i know.
        for (auto& player : this->players)
        {
            if (player.second == *room.GetOwner())
            {
                player.second.SetLastPacketTimeStamp();
            }
        }

        std::cout << "Game on room with ID: " << room.GetRoomID() << " started!\n";

        this->Announces(room.GetRoomID(), true);
            
        room.Reset(false);
        this->StartGame(room);
        return;
    }   

    std::cout << "Unknown player from [" << sender.GetIpAddress() << ":" << sender.GetPort() << "]\n";
}

void Server::MoveCommand(Byte* buffer, Sender& sender, const int len)
{
    if (len != (headerBytesAmount + cellBytesAmount)) return;

    if (this->players.find(sender) != this->players.end())
    {
        Player& currentPlayer = this->players.find(sender)->second;

        int currentRoomID = currentPlayer.GetCurrentRoom().first;  
        if (currentRoomID <= 0)
        {
            std::cout << "Player [" << sender.GetIpAddress() << ":" << sender.GetPort() << "] \"" << currentPlayer.GetName() << "\" is not in a room!\n"; 
            return;
        }   

        Byte cellBytes = buffer[headerBytesAmount];
        const std::uint32_t cell = cellBytes - '0'; 

        Room& room = this->rooms[currentRoomID];    
        if (!room.Move(currentPlayer, cell))
        {
            std::cout << "Player \"" << currentPlayer.GetName() << "\" did an invalid move!\n";
            return;
        }   

        currentPlayer.SetLastPacketTimeStamp(); 

        UpdateField(room);  

        if (room.GetWinner())
        {
            std::cout << "Player \"" << room.GetWinner()->GetName() << "\" WON!\n";
            room.SetEndedChallengeTimestamp(Utility::GetNowTime());
        }
        else if (room.IsDraw())
        {
            std::cout << "The game is ended in DRAW!\n";
            room.SetEndedChallengeTimestamp(Utility::GetNowTime());
        }

        return;
    }   

    std::cout << "Unknown player from [" << sender.GetIpAddress() << ":" << sender.GetPort() << "]\n";
}

void Server::QuitCommand(Byte* buffer, Sender& sender, const int len)
{
    if (this->players.count(sender) > 0)
    {
        this->RemovePlayer(sender);
        return;
    }   

    std::cout << "Unknown player from [" << sender.GetIpAddress() << ":" << sender.GetPort() << "]\n";
}

// ----------------------------------------------------------------------------------------------

int main(int argc, char** argv)
{
    Server server = {};
    server.Run();

    return EXIT_SUCCESS;
}