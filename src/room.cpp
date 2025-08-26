#include <room.hpp>

namespace TTTGame
{
    Room::Room(const int roomID, const Player& owner) : roomID(roomID)
    {
        this->owner = std::make_shared<Player>(owner);
        this->Reset(true);
    }

    bool Room::IsDoorOpen() const
    {
        return this->challenger == nullptr;
    }

    void Room::Reset(const bool removeChallenger)
    {
        if (removeChallenger)
        { 
            this->challenger = nullptr;
            this->turnOf = owner;
        }
        else
        {
            std::random_device random_device; // RNG Hardware based
            std::mt19937 generator(random_device()); // Mersenne Twister PRNG
            std::uniform_int_distribution<int> distribution(0, 1); // RNG generator object

            if (distribution(generator)) this->turnOf = owner;
            else this->turnOf = challenger;

            std::cout << "Turn of \"" << this->turnOf->GetName() << "\"!\n";
        }
        
        this->playField.fill(nullptr);
        this->winner = nullptr;
    }

    bool Room::HasStarted() const // Unused
    {
        for (const std::shared_ptr<Player>& cell : playField)
        {
            if (!cell) return true;
        }
        
        return false;
    }

    // ----------------------------------------------------------------------------------------------

    char Room::ParseSymbol(const std::size_t cellIndex) const
    {
        if (!playField[cellIndex]) return ' ';
        if (*playField[cellIndex] == *owner) return 'X';
        if (*playField[cellIndex] == *challenger) return 'O';
        return '?';
    }
    
    // ----------------------------------------------------------------------------------------------

    std::shared_ptr<Player> Room::CheckHorizontal(const std::size_t row) const
    {
        for (std::size_t col = 0; col < fieldSize; col++)
        {
            if (!this->playField[row * fieldSize + col]) return nullptr;
        }

        std::shared_ptr<Player> currentPlayer = this->playField[row * fieldSize];
        if (*this->playField[row * fieldSize + 1] != *currentPlayer) return nullptr;
        if (*this->playField[row * fieldSize + 2] != *currentPlayer) return nullptr;
        return currentPlayer;
    }

    std::shared_ptr<Player> Room::CheckVertical(const std::size_t col) const
    {
        for (std::size_t row = 0; row < fieldSize; row++)
        {
            if (!this->playField[row * fieldSize + col]) return nullptr;
        }

        std::shared_ptr<Player> currentPlayer = this->playField[col /* + (0 * fieldSize) */ ];
        if (*this->playField[col + 1 * fieldSize] != *currentPlayer) return nullptr;
        if (*this->playField[col + 2 * fieldSize] != *currentPlayer) return nullptr;
        return currentPlayer;
    }

    std::shared_ptr<Player> Room::CheckDiagonalLeft() const
    {
        if (!this->playField[0]) return nullptr;
        if (!this->playField[4]) return nullptr;
        if (!this->playField[8]) return nullptr;

        std::shared_ptr<Player> currentPlayer = this->playField[0];
        if (*this->playField[4] != *currentPlayer) return nullptr;
        if (*this->playField[8] != *currentPlayer) return nullptr;

        return currentPlayer;
    }

    std::shared_ptr<Player> Room::CheckDiagonalRight() const
    {
        if (!this->playField[2]) return nullptr;
        if (!this->playField[4]) return nullptr;
        if (!this->playField[6]) return nullptr;

        std::shared_ptr<Player> currentPlayer = this->playField[2];
        if (*this->playField[4] != *currentPlayer) return nullptr;
        if (*this->playField[6] != *currentPlayer) return nullptr;

        return currentPlayer;
    }

    std::shared_ptr<Player> Room::CheckVictory() const
    {
        std::shared_ptr<Player> winnerPlayer;

        for (std::size_t row = 0; row < fieldSize; row++)
        {
            winnerPlayer = this->CheckHorizontal(row);
            if (winnerPlayer) return winnerPlayer;
        }
        for (std::size_t col = 0; col < fieldSize; col++)
        {
            winnerPlayer = this->CheckVertical(col);
            if (winnerPlayer) return winnerPlayer;
        }

        winnerPlayer = this->CheckDiagonalLeft();
        if (winnerPlayer) return winnerPlayer;

        return this->CheckDiagonalRight();
    }

    bool Room::IsDraw() const
    {
        for (const std::shared_ptr<Player> cell : this->playField)
        {
            if (!cell) return false;
        }

        return true;
    }

    // ----------------------------------------------------------------------------------------------

    int Room::GetRoomID() const
    {
        return this->roomID;
    }

    std::shared_ptr<Player> Room::GetOwner() const
    {
        return this->owner;
    }

    bool Room::Move(Player& player, const std::size_t cellMove)
    {
        if (cellMove < 0 || cellMove > 8) return false;
        if (this->playField[cellMove]) return false;
        if (this->winner) return false;
        if (!this->challenger) return false;
        
        int currentRoomID = player.GetCurrentRoom().first;

        if (currentRoomID != this->roomID) return false;
        if (player != *(this->owner) && player != *this->challenger) return false;
        if (player != *this->turnOf) return false;

        this->playField[cellMove] = std::make_shared<Player>(player);
        this->winner = this->CheckVictory();
        this->turnOf = (this->turnOf == this->owner) ? this->challenger : this->owner; 

        return true;
    }

    // ----------------------------------------------------------------------------------------------

    std::shared_ptr<Player> Room::GetChallenger() const
    {
        return this->challenger;
    }

    void Room::SetChallenger(const std::shared_ptr<Player>& challenger)
    {
        this->challenger = challenger;
    }

    // ----------------------------------------------------------------------------------------------

    void Room::SetEndedChallengeTimestamp(const std::size_t endedChallengeTimestamp)
    {
        this->endedChallengeTimeStamp = endedChallengeTimestamp;
    }

    std::size_t Room::GetEndedChallengeTimestamp() const
    {
        return this->endedChallengeTimeStamp;
    }

    // ----------------------------------------------------------------------------------------------

    std::shared_ptr<Player> Room::GetWinner() const
    {
        return this->winner;
    }

    // ----------------------------------------------------------------------------------------------

    bool Room::operator==(const Room& otherRoom)
    {
        return this->roomID == otherRoom.roomID;
    }

    bool Room::operator!=(const Room& otherRoom)
    {
        return !(*this == otherRoom);
    }
}