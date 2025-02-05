#include <player.hpp>

#include <memory>
#include <cstdint>
#include <iostream>
#include <array>
#include <random>

namespace TTTGame
{
    constexpr std::size_t fieldAmount = 9;
    constexpr std::size_t fieldSize = fieldAmount / 3;

    class Room
    {
    public:
        Room() { }
        Room(const int roomID, const Player& owner);

        void Reset(const bool removeChallenger);
        bool IsDoorOpen() const;
        bool HasStarted() const; // Unused
        char ParseSymbol(const std::size_t cellIndex) const;

        std::shared_ptr<Player> CheckHorizontal(const std::size_t row) const;
        std::shared_ptr<Player> CheckVertical(const std::size_t col) const;
        std::shared_ptr<Player> CheckDiagonalLeft() const;
        std::shared_ptr<Player> CheckDiagonalRight() const;
        std::shared_ptr<Player> CheckVictory() const;
        bool IsDraw() const;

        bool Move(Player& player, const std::size_t cellMove);

        int GetRoomID() const;
        std::shared_ptr<Player> GetOwner() const;
        std::shared_ptr<Player> GetWinner() const;

        std::shared_ptr<Player> GetChallenger() const;
        void SetChallenger(const std::shared_ptr<Player>& challenger);

        void SetEndedChallengeTimestamp(const std::size_t endedChallengeTimestamp);
        std::size_t GetEndedChallengeTimestamp() const;
        
        bool operator==(const Room& otherRoom);
        bool operator!=(const Room& otherRoom);

    private:
        int roomID;
        std::shared_ptr<Player> owner;
        std::shared_ptr<Player> challenger;
        std::array<std::shared_ptr<Player>, fieldAmount> playField;
        std::shared_ptr<Player> turnOf;
        std::shared_ptr<Player> winner;

        std::size_t endedChallengeTimeStamp;

    };
}

using Room = TTTGame::Room;