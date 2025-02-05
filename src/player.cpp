#include <player.hpp>
#include <utility.hpp>

namespace TTTGame
{
    Player::Player(const std::string& playerName) : playerName(playerName), currentRoom{-1, true}, lastPacketTimeStamp(Utility::GetNowTime()) { }

    // ----------------------------------------------------------------------------------------------

    std::pair<int, bool> Player::GetCurrentRoom() const
    {
        return this->currentRoom;
    }

    void Player::SetCurrentRoom(std::pair<int, bool>& currentRoom)
    {
        this->currentRoom = currentRoom;
    }

    // ----------------------------------------------------------------------------------------------

    const std::string& Player::GetName() const
    {
        return this->playerName;
    }

    // ----------------------------------------------------------------------------------------------

    std::size_t Player::GetLastPacketTimeStamp() const
    {
        return this->lastPacketTimeStamp;
    }

    void Player::SetLastPacketTimeStamp()
    {
        this->lastPacketTimeStamp = Utility::GetNowTime();
    }

    // ----------------------------------------------------------------------------------------------
    
    bool Player::operator==(const Player& otherPlayer)
    {
        return this->playerName.compare(otherPlayer.playerName) == 0;
    }

    bool Player::operator!=(const Player& otherPlayer)
    {
        return !(*this == otherPlayer);
    }
}