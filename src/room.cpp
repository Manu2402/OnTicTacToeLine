#include <room.hpp>

namespace TTTGame
{
    Room::Room(const int room_id, const Player& owner) : room_id(room_id)
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
            this->turn_of = owner;
        }
        else
        {
            std::random_device random_device; // RNG Hardware based
            std::mt19937 generator(random_device()); // Mersenne Twister PRNG
            std::uniform_int_distribution<int> distribution(0, 1); // RNG generator object

            if (distribution(generator)) this->turn_of = owner;
            else this->turn_of = challenger;

            std::cout << "Turn of \"" << this->turn_of->GetName() << "\"!\n";
        }

        this->play_field.fill(nullptr);
        this->winner = nullptr;
    }

    // ----------------------------------------------------------------------------------------------

    char Room::ParseSymbol(const std::size_t cell_index) const
    {
        if (!play_field[cell_index]) return ' ';
        if (*play_field[cell_index] == *owner) return 'X';
        if (*play_field[cell_index] == *challenger) return 'O';
        return '?';
    }
    
    // ----------------------------------------------------------------------------------------------

    std::shared_ptr<Player> Room::CheckHorizontal(const std::size_t row) const
    {
        for (std::size_t col = 0; col < field_size; col++)
        {
            if (!this->play_field[row * field_size + col]) return nullptr;
        }

        std::shared_ptr<Player> current_player = this->play_field[row * field_size];
        if (*this->play_field[row * field_size + 1] != *current_player) return nullptr;
        if (*this->play_field[row * field_size + 2] != *current_player) return nullptr;
        return current_player;
    }

    std::shared_ptr<Player> Room::CheckVertical(const std::size_t col) const
    {
        for (std::size_t row = 0; row < field_size; row++)
        {
            if (!this->play_field[row * field_size + col]) return nullptr;
        }

        std::shared_ptr<Player> current_player = this->play_field[col /* + (0 * field_size) */ ];
        if (*this->play_field[col + 1 * field_size] != *current_player) return nullptr;
        if (*this->play_field[col + 2 * field_size] != *current_player) return nullptr;
        return current_player;
    }

    std::shared_ptr<Player> Room::CheckDiagonalLeft() const
    {
        if (!this->play_field[0]) return nullptr;
        if (!this->play_field[4]) return nullptr;
        if (!this->play_field[8]) return nullptr;

        std::shared_ptr<Player> current_player = this->play_field[0];
        if (*this->play_field[4] != *current_player) return nullptr;
        if (*this->play_field[8] != *current_player) return nullptr;

        return current_player;
    }

    std::shared_ptr<Player> Room::CheckDiagonalRight() const
    {
        if (!this->play_field[2]) return nullptr;
        if (!this->play_field[4]) return nullptr;
        if (!this->play_field[6]) return nullptr;

        std::shared_ptr<Player> current_player = this->play_field[2];
        if (*this->play_field[4] != *current_player) return nullptr;
        if (*this->play_field[6] != *current_player) return nullptr;

        return current_player;
    }

    std::shared_ptr<Player> Room::CheckVictory() const
    {
        std::shared_ptr<Player> winner_player;

        for (std::size_t row = 0; row < field_size; row++)
        {
            winner_player = this->CheckHorizontal(row);
            if (winner_player) return winner_player;
        }
        for (std::size_t col = 0; col < field_size; col++)
        {
            winner_player = this->CheckVertical(col);
            if (winner_player) return winner_player;
        }

        winner_player = this->CheckDiagonalLeft();
        if (winner_player) return winner_player;

        return this->CheckDiagonalRight();
    }

    bool Room::IsDraw() const
    {
        for (const std::shared_ptr<Player> cell : this->play_field)
        {
            if (!cell) return false;
        }

        return true;
    }

    // ----------------------------------------------------------------------------------------------

    int Room::GetRoomID() const
    {
        return this->room_id;
    }

    std::shared_ptr<Player> Room::GetOwner() const
    {
        return this->owner;
    }

    bool Room::Move(Player& player, const std::size_t cell_move)
    {
        if (cell_move < 0 || cell_move > 8) return false;
        if (this->play_field[cell_move]) return false;
        if (this->winner) return false;
        if (!this->challenger) return false;
        
        int currentRoomID = player.GetCurrentRoom().first;

        if (currentRoomID != this->room_id) return false;
        if (player != *(this->owner) && player != *this->challenger) return false;
        if (player != *this->turn_of) return false;

        this->play_field[cell_move] = std::make_shared<Player>(player);
        this->winner = this->CheckVictory();
        this->turn_of = (this->turn_of == this->owner) ? this->challenger : this->owner; 

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

    void Room::SetEndedChallengeTimestamp(const std::size_t ended_challenge_timestamp)
    {
        this->ended_challenge_timestamp = ended_challenge_timestamp;
    }

    std::size_t Room::GetEndedChallengeTimestamp() const
    {
        return this->ended_challenge_timestamp;
    }

    // ----------------------------------------------------------------------------------------------

    std::shared_ptr<Player> Room::GetWinner() const
    {
        return this->winner;
    }

    // ----------------------------------------------------------------------------------------------

    bool Room::operator==(const Room& other_room)
    {
        return this->room_id == other_room.room_id;
    }

    bool Room::operator!=(const Room& other_room)
    {
        return !(*this == other_room);
    }
}