#pragma once

#ifdef _WIN32
    #include <WinSock2.h>
    #include <ws2tcpip.h>
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
#endif

#include <utility.hpp>

#include <SDL.h>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include <iostream>
#include <atomic>
#include <thread>
#include <vector>
#include <functional>
#include <unordered_map>
#include <map>

namespace TTTClient
{
    const std::vector<std::pair<int, int>> cell_coordinates = 
    { 
        { 12, 12 },  { 179, 12 },  { 345, 12 }, 
        { 12, 178 }, { 179, 178 }, { 345, 178 }, 
        { 12, 344 }, { 179, 344 }, { 345, 344 }
    };

    const std::pair<int, int> symbols_texture_size = { 155, 155 };

    enum TextureAsset : std::uint32_t 
    { 
        BLOCKED_GRID = 0, 
        PLAY_GRID    = 1, 
        X_SIGN       = 2, 
        O_SIGN       = 3
    };

    // To avoid "race condition" about both main thread, "ReceiveData" thread and "SendData" thread.
    std::atomic<bool> running(true);

    // Packet Protocol: >[I][I][...]
    constexpr int buffer_size                      = 64;
    constexpr std::size_t join_packet_size        = 22;
    constexpr std::size_t create_room_packet_size = 2;
    constexpr std::size_t quit_packet_size        = 2;
    constexpr std::size_t header_bytes_amount     = 2;
    constexpr std::size_t room_id_len             = 2;

    // 0 -----> Blocked Grid
    // 1 -----> Play Grid
    // 2/11 --> Cells
    constexpr std::size_t sprites_amount = 11;
    constexpr std::size_t starting_sprite_cells_index = 2;
    constexpr std::size_t sprites_cell_amount = sprites_amount - starting_sprite_cells_index;

    typedef struct header_t
    {
        std::uint32_t rid;
        Command command;
    } header_t;

    class NaiveSDLTexture
    {
    public:
        NaiveSDLTexture(SDL_Texture* texture, SDL_Rect texture_box) : texture(texture), texture_box(texture_box) { }
            
        SDL_Texture* GetTexture() const; 
        void SetTexture(SDL_Texture* texture);

        SDL_Rect& GetTextureBox();

    private:
        SDL_Texture* texture;
        SDL_Rect texture_box;

    };

    class Client
    {
    public:
        Client(const char* ipAddress = "77.32.107.199", const int port = 9999);
        ~Client();

        void Run();

        void InitSprites();

        int ClickedOnWhichCell(SDL_Event* event);

        void ReceiveData();
        void SendData();

    private:
        int socket_id;
        sockaddr_in sin;

        std::unordered_map<Command, std::function<void(const int)>> command_functions_snd;
        void JoinCommand(const int current_command_id);
        void CreateRoomCommand(const int current_command_id);
        void ChallengeCommand(const int current_command_id);
        void QuitCommand(const int current_command_id);

        std::unordered_map<Command, std::function<void(char*, const int)>> command_functions_rcv;
        void StartGameCommand(char* buffer, const int len);
        void AnnounceRoomCommand(char* buffer, const int len);
        void UpdateFieldCommand(char* buffer, const int len);
        void ResetClientCommand(char* buffer, const int len);

        std::thread recv_thread;
        std::thread send_thread;

        SDL_Window* window;
        SDL_Renderer* renderer;
        std::map<TextureAsset, SDL_Texture*> textures;
        std::vector<NaiveSDLTexture> sprites;

        void PrintCommands() const;

    };
}

using Client = TTTClient::Client;
using NaiveSDLTexture = TTTClient::NaiveSDLTexture;