#include <utility.hpp>

namespace Utility
{
    std::size_t GetNowTime()
    {
        auto now = std::chrono::system_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch());
        return duration.count();
    }

    std::string GetParsedRoomIDLength(const std::string& roomIDString, const std::size_t roomIDLength)
    {
        return roomIDLength < twoFiguresFactor ? std::to_string(roomIDString.length()) + "?" : std::to_string(roomIDString.length());
    }

    const char* NetworkException::what() const noexcept
    {
        // Conversion from std::string to const char*
        return message.c_str();
    }
}