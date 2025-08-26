#pragma once

#ifdef _WIN32
    #include <WinSock2.h>
    #include <ws2tcpip.h>
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
#endif

#include <utility.hpp>
#include <room.hpp>

#include <iostream>
#include <cstdint>
#include <string>
#include <vector>
#include <functional>
#include <unordered_map>
#include <set>

using Player = TTTGame::Player;
using Room = TTTGame::Room;

namespace TTTServer
{
    constexpr std::size_t timeout_seconds          = 300;
    constexpr std::size_t in_game_timeout_seconds  = 30;

    constexpr std::size_t header_bytes_amount      = 2; // 2 bytes interpreted as 16 bits.
    constexpr std::size_t player_name_bytes_amount = 20;
    constexpr std::size_t cell_bytes_amount        = 1;

    constexpr std::size_t room_id_len              = 2;
    constexpr int buffer_size                      = 64;

    constexpr std::size_t reset_field_time         = 2;

    // ----------------------------------------------------------------------------------------------

    typedef struct header_t
    {
        std::uint32_t rid;
        Command command;
    } header_t;

    typedef struct announce_t
    {
        header_t header;
        std::uint32_t room_id;
    } announce_t;

    // ----------------------------------------------------------------------------------------------

    class Server
    {
    public:
        class Sender
        {
        public:
            std::string GetIpAddress() const;
            void SetIpAddress(const std::string& ip_address);
            
            int GetPort() const;
            void SetterPort(const int port);

            bool operator==(const Sender& other_sender) const;
            void operator()() { }

        private:
            std::string ip_address;
            int port;

            std::size_t packets_per_second;
        };

        // Struct that use the "function call operator" to hash all the fields into "Sender" class.
        // All of this in order to use it into "std::unordered_map" as a key.
        struct SenderHash 
        {
            size_t operator()(const Sender& sender) const 
            {
                // Very simple hashing algorithm.
                return std::hash<std::string>{}(sender.GetIpAddress()) + std::hash<int>{}(sender.GetPort());
            }
        };
        
        Server(const char* ip_address = "192.168.1.102", const int port = 9999, const std::uint32_t timeout = 1000);

        void Kick(const Sender& sender);
        void DestroyRoom(const Room& room);
        void RemovePlayer(const Sender& sender);
        
        void Tick();
        void Announces(const int room_id, const bool to_remove);
        void CheckDeadPeers();
        void CheckEndedChallenges();

        void Run();

        void StartGame(const Room& room) const;
        void UpdateField(const Room& room) const;
        void ResetClient(const Room& room) const;

    private:
        int socket_id;
        sockaddr_in sin;
        std::unordered_map<Sender, Player, SenderHash> players;
        
        std::size_t room_counter = 100;
        std::unordered_map<int, Room> rooms;
        std::set<int> opened_rooms;

        std::unordered_map<Command, std::function<void(char*, Sender&, const int)>> commandFunctions;
        void JoinCommand(char* buffer, Sender& sender, const int len);
        void CreateRoomCommand(char* buffer, Sender& sender, const int len);
        void ChallengeCommand(char* buffer, Sender& sender, const int len);
        void MoveCommand(char* buffer, Sender& sender, const int len);
        void QuitCommand(char* buffer, Sender& sender, const int len);

        void SendAnnounce(const Sender& sender);

    };
}

using Server = TTTServer::Server;