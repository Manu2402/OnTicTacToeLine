#ifndef TICTACTOE_SERVER_HPP
#define TICTACTOE_SERVER_HPP

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
    constexpr std::size_t timeoutSeconds = 300;
    constexpr std::size_t inGameTimeoutSeconds = 30;

    constexpr std::size_t headerBytesAmount = 2; // 2 bytes interpreted as 16 bytes
    constexpr std::size_t playerNameBytesAmount = 20;
    constexpr std::size_t roomIDLength = 2;
    constexpr std::size_t cellBytesAmount = 1;
    constexpr int bufferSize = 64;

    constexpr std::size_t resetFieldTime = 2;

    // ----------------------------------------------------------------------------------------------

    typedef struct Header
    {
        std::uint32_t rid;
        Command command;
    } Header;

    typedef struct Announce
    {
        Header header;
        std::uint32_t roomID;
    } Announce;

    // ----------------------------------------------------------------------------------------------

    class Server
    {
    public:
        class Sender
        {
        public:
            std::string GetIpAddress() const;
            void SetIpAddress(const std::string& ipAddress);
            
            int GetPort() const;
            void SetterPort(const int port);

            bool operator==(const Sender& otherSender) const;
            void operator()() { }

        private:
            std::string ipAddress;
            int port;

            std::size_t packetsPerSecond;
        };

        // Struct that use the function call operator to hash all the fields into "Sender" class.
        // All of this in order to use it into "std::unordered_map" as a key.
        struct SenderHash 
        {
            size_t operator()(const Sender& sender) const 
            {
                // Very simple hashing algorithm.
                return std::hash<std::string>{}(sender.GetIpAddress()) + std::hash<int>{}(sender.GetPort());
            }
        };
        
        Server(const char* ipAddress = "192.168.1.106", const int port = 9999, const std::uint32_t timeout = 1000);

        void Kick(const Sender& sender);
        void DestroyRoom(const Room& room);
        void RemovePlayer(const Sender& sender);
        
        void Tick();
        void Announces(const int roomID, const bool toRemove);
        void CheckDeadPeers();
        void CheckEndedChallenges();

        void Run();

        void StartGame(const Room& room) const;
        void UpdateField(const Room& room) const;
        void ResetClient(const Room& room) const;

    private:
        int socketID;
        sockaddr_in sin;
        std::size_t roomCounter = 100;
        std::unordered_map<Sender, Player, SenderHash> players;
        std::unordered_map<int, Room> rooms;
        std::set<int> openedRooms;

        std::unordered_map<Command, std::function<void(Byte*, Sender&, const int)>> commandFunctions;
        void JoinCommand(Byte* buffer, Sender& sender, const int len);
        void CreateRoomCommand(Byte* buffer, Sender& sender, const int len);
        void ChallengeCommand(Byte* buffer, Sender& sender, const int len);
        void MoveCommand(Byte* buffer, Sender& sender, const int len);
        void QuitCommand(Byte* buffer, Sender& sender, const int len);

        void SendAnnounce(const Sender& sender);

    };
}

using Server = TTTServer::Server;

#endif // TICTACTOE_SERVER_HPP