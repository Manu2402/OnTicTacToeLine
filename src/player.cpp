#include <player.hpp>
#include <utility.hpp>

namespace TTTGame
{
    Player::Player(const std::string& player_name) : player_name(player_name), current_room { -1, true }, last_packet_timestamp(Utility::GetNowTime()) { }

    // ----------------------------------------------------------------------------------------------

    std::pair<int, bool> Player::GetCurrentRoom() const
    {
        return this->current_room;
    }

    void Player::SetCurrentRoom(std::pair<int, bool>& current_room)
    {
        this->current_room = current_room;
    }

    // ----------------------------------------------------------------------------------------------

    const std::string& Player::GetName() const
    {
        return this->player_name;
    }

    // ----------------------------------------------------------------------------------------------

    std::size_t Player::GetLastPacketTimeStamp() const
    {
        return this->last_packet_timestamp;
    }

    void Player::SetLastPacketTimeStamp()
    {
        this->last_packet_timestamp = Utility::GetNowTime();
    }

    // ----------------------------------------------------------------------------------------------
    
    bool Player::operator==(const Player& other_player)
    {
        return this->player_name.compare(other_player.player_name) == 0;
    }

    bool Player::operator!=(const Player& other_player)
    {
        return !(*this == other_player);
    }
}