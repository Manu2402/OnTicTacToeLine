#include <tictactoe_server.hpp>

#include <iostream>

Server::Server(const char* ip_address, const int port, const std::uint32_t timeout)
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
        this->socket_id = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (this->socket_id < 0)
        {
            throw NetworkException("ERROR: Unable to initialize the UDP socket!\n");
        }

        inet_pton(AF_INET, ip_address, &sin.sin_addr);
        this->sin.sin_family = AF_INET;
        this->sin.sin_port = htons(port);

        // "reinterpret_cast" is great to modify the interpretation about a memory address.
        if (setsockopt(this->socket_id, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(std::uint32_t)))
        {
            throw NetworkException("ERROR: Unable to set socket option for receive timeout!\n");
        }

        if (setsockopt(this->socket_id, SOL_SOCKET, SO_RCVBUF, reinterpret_cast<const char*>(&buffer_size), sizeof(std::uint32_t)))
        {
            throw NetworkException("ERROR: Unable to set socket option for receive buffer size!\n");
        }

        if (setsockopt(this->socket_id, SOL_SOCKET, SO_SNDBUF, reinterpret_cast<const char*>(&buffer_size), sizeof(std::uint32_t)))
        {
            throw NetworkException("ERROR: Unable to set socket option for send buffer size!\n");
        }

        if (bind(this->socket_id, reinterpret_cast<sockaddr*>(&this->sin), sizeof(this->sin)))
        {
            throw NetworkException("ERROR: Unable to bind the UDP socket!\n");
        }
    }
    catch (const NetworkException& exception)
    {
        std::cout << exception.what() << "\n";
    }

    // Init commands. I must use the lambda expressions.
    this->commandFunctions[Command::JOIN] = [this](char* buffer, Sender& sender, const int len) { this->JoinCommand(buffer, sender, len); };
    this->commandFunctions[Command::CREATE_ROOM] = [this](char* buffer, Sender& sender, const int len) { this->CreateRoomCommand(buffer, sender, len); };
    this->commandFunctions[Command::CHALLENGE] = [this](char* buffer, Sender& sender, const int len) { this->ChallengeCommand(buffer, sender, len); };
    this->commandFunctions[Command::MOVE] = [this](char* buffer, Sender& sender, const int len) { this->MoveCommand(buffer, sender, len); };
    this->commandFunctions[Command::QUIT] = [this](char* buffer, Sender& sender, const int len) { this->QuitCommand(buffer, sender, len); };

    std::cout << "Server is ready!\n";
}

// ----------------------------------------------------------------------------------------------

std::string Server::Sender::GetIpAddress() const
{
    return this->ip_address;
}

void Server::Sender::SetIpAddress(const std::string& ip_address)
{
    this->ip_address = ip_address;
}

// ----------------------------------------------------------------------------------------------

int Server::Sender::GetPort() const
{
    return this->port;
}

void Server::Sender::SetterPort(const int port) // "SetPort" was already defined.
{
    this->port = port;
}

// ----------------------------------------------------------------------------------------------

bool Server::Sender::operator==(const Sender& other_sender) const
{
    return ip_address == other_sender.ip_address && port == other_sender.port;
}

// ----------------------------------------------------------------------------------------------

void Server::Kick(const Sender& sender)
{
    Player& bad_player = this->players[sender];
    int room_id = bad_player.GetCurrentRoom().first;
    bool is_owner = bad_player.GetCurrentRoom().second;

    if (room_id > 0)
    {
        Room& current_room = this->rooms[room_id];

        if (is_owner)
        { 
            this->ResetClient(current_room);
            this->DestroyRoom(current_room);
        }
        else 
        {
            this->ResetClient(current_room);
            current_room.Reset(true);
        }
    }

    std::cout << "[" << sender.GetIpAddress() << ":" << sender.GetPort() << "] \"" << bad_player.GetName() << "\" has been kicked!\n";
    players.erase(sender);
}

void Server::DestroyRoom(const Room& room)
{
    std::pair<int, bool> no_room_info = { -1, true };

    // I can't use "const" because i assign a reference type.
    for (auto& player : this->players)
    {
        Player& challenger = player.second;
        if (room.GetChallenger() && *room.GetChallenger() == challenger)
        {
            challenger.SetCurrentRoom(no_room_info);
        }
    }

    std::cout << "Room with ID: " << room.GetRoomID() << " has been destroyed!\n";
    opened_rooms.erase(room.GetRoomID());
    rooms.erase(room.GetRoomID());
}

void Server::RemovePlayer(const Sender& sender)
{
    Player& player = this->players[sender];
    int current_room_id = player.GetCurrentRoom().first;

    if (current_room_id <= 0)
    {
        std::cout << "Player \"" << player.GetName() << "\" removed!\n";
        players.erase(sender);
        return;
    }

    Room& room = this->rooms[current_room_id];

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
    char buffer[buffer_size];
    sockaddr_in sender_input;
    int sender_input_size = sizeof(sender_input);

    int len = recvfrom(this->socket_id, buffer, buffer_size, 0, reinterpret_cast<sockaddr*>(&sender_input), &sender_input_size);
    if (len < 0) return;

    if (len < header_bytes_amount)
    {
        std::cout << "Invalid packet size: " << len << " bytes!\n";
        return;
    }

    // Packet Protocol: >II...
    // Big Endian | RID | Command | Payload (based on the command)
    header_t header;
    header.rid = buffer[0] - '0';
    header.command = static_cast<Command>(buffer[1] - '0');

    char address_str[buffer_size];
    inet_ntop(AF_INET, &sender_input.sin_addr, address_str, buffer_size);

    Sender sender;
    sender.SetIpAddress(address_str);
    sender.SetterPort(ntohs(sender_input.sin_port));

    if (header.command >= 0 || header.command < commandFunctions.size())
    {
        this->commandFunctions[header.command](buffer, sender, len);
        return;
    }

    std::cout << "Unknown command from [" << sender.GetIpAddress() << ":" << sender.GetPort() << "]\n";
}

void Server::SendAnnounce(const Sender& sender)
{
    for (const int& room : this->opened_rooms)
    {            
        announce_t announce;
        announce.header.rid = 0;
        announce.header.command = Command::ANNOUNCE_ROOM;
        announce.room_id = room;

        std::string room_id_str(std::to_string(announce.room_id));
        std::string announce_info(std::to_string(announce.header.rid) + std::to_string(static_cast<std::uint32_t>(announce.header.command)) + Utility::GetParsedRoomIDLength(room_id_str, room_id_str.length()) + room_id_str);
        const char* announce_packet = announce_info.c_str();

        sockaddr_in sender_in;
        sender_in.sin_family = AF_INET;
        inet_pton(AF_INET, sender.GetIpAddress().c_str(), &sender_in.sin_addr);
        sender_in.sin_port = htons(sender.GetPort());

        int sent_bytes = sendto(this->socket_id, announce_packet, strlen(announce_packet), 0, reinterpret_cast<sockaddr*>(&sender_in), sizeof(sender_in));
    }
}

void Server::Announces(const int room_id, const bool to_remove)
{
    if (to_remove) this->opened_rooms.erase(room_id);
    else this->opened_rooms.insert(room_id);

    if (this->opened_rooms.size() <= 0) return;

    for (auto& sender : this->players)
    {
        const Player& current_player = sender.second;
        int local_room_id = current_player.GetCurrentRoom().first;
        if (local_room_id > 0) continue;

        this->SendAnnounce(sender.first);
    }
}

void Server::StartGame(const Room& room) const
{
    Player& owner = *room.GetOwner();
    Player& challenger = *room.GetChallenger();

    header_t header;
    header.rid = 0;
    header.command = Command::START_GAME;

    for (auto& sender : this->players)
    {
        const Player& current_player = sender.second;

        if (current_player.GetCurrentRoom().first == owner.GetCurrentRoom().first || current_player.GetCurrentRoom().first == challenger.GetCurrentRoom().first)
        {
            sockaddr_in sender_in;
            sender_in.sin_family = AF_INET;
            inet_pton(AF_INET, sender.first.GetIpAddress().c_str(), &sender_in.sin_addr);
            sender_in.sin_port = htons(sender.first.GetPort());

            std::string start_game_info(std::to_string(header.rid) + std::to_string(static_cast<std::uint32_t>(header.command)));
            const char* start_game_packet = start_game_info.c_str();

            int sent_bytes = sendto(this->socket_id, start_game_packet, strlen(start_game_packet), 0, reinterpret_cast<struct sockaddr*>(&sender_in), sizeof(sender_in));
        }
    }
}

void Server::CheckDeadPeers()
{
    std::vector<Sender> dead_players;
    const std::size_t now = Utility::GetNowTime();

    for (auto& player : players) 
    {
        Player& current_player = player.second;
        const int current_room_id = current_player.GetCurrentRoom().first;

        if (current_room_id > 0)
        {
            Room& current_room = this->rooms[current_room_id];

            if (!current_room.IsDoorOpen())
            {
                if ((now - current_player.GetLastPacketTimeStamp()) > in_game_timeout_seconds)
                {
                    dead_players.push_back(player.first);
                    continue;
                }
            }
        }

        if ((now - current_player.GetLastPacketTimeStamp()) > timeout_seconds)
        {
            dead_players.push_back(player.first);
        }
    }

    for (const Sender& sender : dead_players)
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
            if ((Utility::GetNowTime() - room.second.GetEndedChallengeTimestamp()) > reset_field_time)
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

    header_t header;
    header.rid = 0;
    header.command = Command::UPDATE_FIELD;

    for (auto& sender : this->players)
    {
        const Player& current_player = sender.second;

        if (current_player.GetCurrentRoom().first == owner.GetCurrentRoom().first || current_player.GetCurrentRoom().first == challenger.GetCurrentRoom().first)
        {
            sockaddr_in sender_in;
            sender_in.sin_family = AF_INET;
            inet_pton(AF_INET, sender.first.GetIpAddress().c_str(), &sender_in.sin_addr);
            sender_in.sin_port = htons(sender.first.GetPort());

            std::string updated_field = { room.ParseSymbol(0), room.ParseSymbol(1), room.ParseSymbol(2), room.ParseSymbol(3), room.ParseSymbol(4), room.ParseSymbol(5), room.ParseSymbol(6), room.ParseSymbol(7), room.ParseSymbol(8) };

            std::string update_field_info(std::to_string(header.rid) + std::to_string(static_cast<std::uint32_t>(header.command)) + updated_field);
            const char* update_field_packet = update_field_info.c_str();

            int sent_bytes = sendto(this->socket_id, update_field_packet, strlen(update_field_packet), 0, reinterpret_cast<sockaddr*>(&sender_in), sizeof(sender_in));
        }
    }
}

void TTTServer::Server::ResetClient(const Room& room) const
{
    Player owner, challenger;

    if (room.GetOwner()) owner = *room.GetOwner();
    if (room.GetChallenger()) challenger = *room.GetChallenger();

    header_t header;
    header.rid = 0;
    header.command = Command::RESET_CLIENT;

    for (auto& sender : this->players)
    {
        const Player& current_player = sender.second;

        if (current_player.GetCurrentRoom().first == owner.GetCurrentRoom().first || current_player.GetCurrentRoom().first == challenger.GetCurrentRoom().first)
        {
            sockaddr_in sender_in;
            sender_in.sin_family = AF_INET;
            inet_pton(AF_INET, sender.first.GetIpAddress().c_str(), &sender_in.sin_addr);
            sender_in.sin_port = htons(sender.first.GetPort());

            std::string reset_client_info("0" + std::to_string(static_cast<int>(header.command)));
            const char* reset_client_packet = reset_client_info.c_str();

            int sent_bytes = sendto(this->socket_id, reset_client_packet, strlen(reset_client_packet), 0, reinterpret_cast<sockaddr*>(&sender_in), sizeof(sender_in));
        }
    }

}

// ----------------------------------------------------------------------------------------------

void Server::JoinCommand(char* buffer, Sender& sender, const int len)
{
    if (len != (player_name_bytes_amount + header_bytes_amount)) return;

    if (this->players.count(sender) > 0)
    {
        std::cout << "[" << sender.GetIpAddress() << ":" << sender.GetPort() << "] has already joined!\n";
        this->Kick(sender);

        return;
    }

    char player_name[player_name_bytes_amount];
    std::memcpy(player_name, &buffer[header_bytes_amount], player_name_bytes_amount);

    Player player = Player(std::string(player_name));
    this->players[sender] = player;
    player.SetLastPacketTimeStamp();
    
    std::cout << "Player \"" << player.GetName() << "\" joined from [" << sender.GetIpAddress() << ":" << sender.GetPort() << "] | {" << this->players.size() << " players on server}\n";

    this->SendAnnounce(sender);
}

void Server::CreateRoomCommand(char* buffer, Sender& sender, const int len)
{
    if (this->players.find(sender) != this->players.end())
    {
        Player& current_player = this->players.find(sender)->second;
        int current_room_id = current_player.GetCurrentRoom().first; 

        if (current_room_id > 0)
        {
            std::cout << "Player [" << sender.GetIpAddress() << ":" << sender.GetPort() << "] \"" << current_player.GetName() << "\" already has a room!\n"; 
            return;
        }   

        std::pair<int, bool> new_room_info = { this->room_counter, true };
        current_player.SetCurrentRoom(new_room_info);
        current_player.SetLastPacketTimeStamp();

        Room new_room(this->room_counter, current_player);
        this->rooms[this->room_counter] = new_room;
        this->Announces(this->room_counter, false);  

        std::cout << "Room with ID: " << this->room_counter << " for player [" << sender.GetIpAddress() << ":" << sender.GetPort() << "] \"" << current_player.GetName() << "\" created!\n";
        this->room_counter++;

        return;
    }   

    std::cout << "Unknown player from [" << sender.GetIpAddress() << ":" << sender.GetPort() << "]\n";
}

void Server::ChallengeCommand(char* buffer, Sender& sender, const int len)
{
    if (len < (header_bytes_amount + room_id_len)) return;

    if (this->players.find(sender) != this->players.end())
    {
        Player& current_player = this->players.find(sender)->second;

        int current_room_id = current_player.GetCurrentRoom().first;
        if (current_room_id > 0)
        {
            std::cout << "Player [" << sender.GetIpAddress() << ":" << sender.GetPort() << "] \"" << current_player.GetName() << "\" already in a room!\n"; 
            return;
        }          

        std::string room_id_length_str;
        std::memcpy(&room_id_length_str, &buffer[header_bytes_amount], room_id_len);
        const std::uint32_t room_id_length = std::stoull(room_id_length_str);  

        std::string room_id_bytes;
        std::memcpy(&room_id_bytes, &buffer[header_bytes_amount + room_id_len], room_id_length);
        int room_id = std::stoi(room_id_bytes);    

        if (this->rooms.find(room_id) == this->rooms.end()) // Iterator check.
        {
            std::cout << "Unknown room with ID: " << room_id << "!\n";
            return;
        }         

        Room& room = this->rooms[room_id];   
        if (!room.IsDoorOpen())
        {
            std::cout << "Room with ID: " << room.GetRoomID() << " is closed!\n";
            return;
        }       

        std::pair<int, bool> room_info = { room.GetRoomID(), false };
        current_player.SetCurrentRoom(room_info);
        room.SetChallenger(std::make_shared<Player>(current_player));   

        current_player.SetLastPacketTimeStamp();
        
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

void Server::MoveCommand(char* buffer, Sender& sender, const int len)
{
    if (len != (header_bytes_amount + cell_bytes_amount)) return;

    if (this->players.find(sender) != this->players.end())
    {
        Player& current_player = this->players.find(sender)->second;

        int current_room_id = current_player.GetCurrentRoom().first;  
        if (current_room_id <= 0)
        {
            std::cout << "Player [" << sender.GetIpAddress() << ":" << sender.GetPort() << "] \"" << current_player.GetName() << "\" is not in a room!\n"; 
            return;
        }   

        char cell_bytes = buffer[header_bytes_amount];
        const std::uint32_t cell = cell_bytes - '0';

        Room& room = this->rooms[current_room_id];    
        if (!room.Move(current_player, cell))
        {
            std::cout << "Player \"" << current_player.GetName() << "\" did an invalid move!\n";
            return;
        }   

        current_player.SetLastPacketTimeStamp(); 

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

void Server::QuitCommand(char* buffer, Sender& sender, const int len)
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