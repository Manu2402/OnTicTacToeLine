#ifndef UTILITY_HPP
#define UTILITY_HPP

#include <string>
#include <chrono>
#include <stdexcept>

namespace Utility
{
    constexpr std::size_t twoFiguresFactor = 10;

    std::size_t GetNowTime();

    // Handle multiple bytes if the number of figures about the "room ID" is greater than nine (as 10).
    std::string GetParsedRoomIDLength(const std::string& roomIDString, const std::size_t roomIDLength);

    class NetworkException : public std::runtime_error
    {
    public:
        NetworkException(const std::string& message) : std::runtime_error(message) { }

        // "noexcept" --> no exception in the function member.
        const char* what() const noexcept override;

    private:
        std::string message;
        
    };

    // ------------------------------------------------------------------------------------------------------

    enum Command : std::uint32_t
    { 
        // Client --> Server
        JOIN = 0, 
        CREATE_ROOM = 1, 
        CHALLENGE = 2, 
        MOVE = 3, 
        QUIT = 4, 

        // Server --> Client
        ANNOUNCE_ROOM = 5,
        START_GAME = 6,
        UPDATE_FIELD = 7,
        RESET_CLIENT = 8
    };
}

using NetworkException = Utility::NetworkException;
using Command = Utility::Command;

using Byte = char;

#endif // UTILITY_HPP