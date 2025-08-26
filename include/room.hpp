#include <player.hpp>

#include <memory>
#include <cstdint>
#include <iostream>
#include <array>
#include <random>

namespace TTTGame
{
    constexpr std::size_t field_amount = 9;
    constexpr std::size_t field_size = field_amount / 3;

    class Room
    {
    public:
        Room() { }
        Room(const int room_id, const Player& owner);

        void Reset(const bool remove_challenger);
        char ParseSymbol(const std::size_t cell_index) const;
        bool IsDoorOpen() const;

        std::shared_ptr<Player> CheckHorizontal(const std::size_t row) const;
        std::shared_ptr<Player> CheckVertical(const std::size_t col) const;
        std::shared_ptr<Player> CheckDiagonalLeft() const;
        std::shared_ptr<Player> CheckDiagonalRight() const;
        std::shared_ptr<Player> CheckVictory() const;
        bool IsDraw() const;

        bool Move(Player& player, const std::size_t cell_move);

        int GetRoomID() const;
        std::shared_ptr<Player> GetOwner() const;
        std::shared_ptr<Player> GetWinner() const;

        std::shared_ptr<Player> GetChallenger() const;
        void SetChallenger(const std::shared_ptr<Player>& challenger);

        void SetEndedChallengeTimestamp(const std::size_t ended_challenge_timestamp);
        std::size_t GetEndedChallengeTimestamp() const;
        
        bool operator==(const Room& otherRoom);
        bool operator!=(const Room& otherRoom);

    private:
        int room_id;
        std::shared_ptr<Player> owner;
        std::shared_ptr<Player> challenger;
        std::array<std::shared_ptr<Player>, field_amount> play_field;
        std::shared_ptr<Player> turn_of;
        std::shared_ptr<Player> winner;

        std::size_t ended_challenge_timestamp;

    };
}

using Room = TTTGame::Room;