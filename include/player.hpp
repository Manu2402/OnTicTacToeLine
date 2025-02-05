#ifndef PLAYER_HPP
#define PLAYER_HPP

#include <string>
#include <cstdint>

namespace TTTGame
{
    class Player
    {
    public:
        Player() { }
        Player(const std::string& playerName);

        std::pair<int, bool> GetCurrentRoom() const;
        void SetCurrentRoom(std::pair<int, bool>& currentRoom);

        const std::string& GetName() const;
        
        std::size_t GetLastPacketTimeStamp() const;
        void SetLastPacketTimeStamp();

        bool operator==(const Player& otherPlayer);
        bool operator!=(const Player& otherPlayer);

    private:
        std::string playerName;
        std::pair<int, bool> currentRoom; // <roomID, isOwner>
        std::size_t lastPacketTimeStamp;

    };
}

using Player = TTTGame::Player;

#endif // PLAYER_HPP