#pragma once

#include <string>
#include <cstdint>

namespace TTTGame
{
    class Player
    {
    public:
        Player() { }
        Player(const std::string& player_name);

        std::pair<int, bool> GetCurrentRoom() const;
        void SetCurrentRoom(std::pair<int, bool>& current_room);

        const std::string& GetName() const;
        
        std::size_t GetLastPacketTimeStamp() const;
        void SetLastPacketTimeStamp();

        bool operator==(const Player& other_player);
        bool operator!=(const Player& other_player);

    private:
        std::string player_name;
        std::pair<int, bool> current_room; // <room_id, is_owner>
        std::size_t last_packet_timestamp;

    };
}

using Player = TTTGame::Player;