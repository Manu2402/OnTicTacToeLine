#include <utility.hpp>

namespace Utility
{
    std::size_t GetNowTime()
    {
        auto now = std::chrono::system_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch());
        return duration.count();
    }

    std::string GetParsedRoomIDLength(const std::string& room_id_str, const std::size_t room_id_len)
    {
        return (room_id_len < two_figures_factor) ? std::to_string(room_id_str.length()) + "?" : std::to_string(room_id_str.length());
    }

    const char* NetworkException::what() const noexcept
    {
        // Conversion from "std::string" to "const char*".
        return message.c_str();
    }
}