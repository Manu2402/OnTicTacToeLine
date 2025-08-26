#ifndef TICTACTOE_CLIENT_HPP
#define TICTACTOE_CLIENT_HPP

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
    const std::vector<std::pair<int, int>> cellCoordinates = 
    { 
        { 12, 12 }, { 179, 12 }, { 345, 12 }, 
        { 12, 178 }, { 179, 178 }, { 345, 178 }, 
        { 12, 344 }, { 179, 344 }, { 345, 344 } 
    };

    const std::pair<int, int> symbolsTextureSize = { 155, 155 };

    enum TextureAsset : std::uint32_t 
    { 
        BLOCKED_GRID = 0, 
        PLAY_GRID = 1, 
        X_SIGN = 2, 
        O_SIGN = 3
    };

    // To avoid "race condition" about both main thread, "ReceiveData" thread and "SendData" thread.
    std::atomic<bool> running(true);

    // Packet Protocol: >[I][I][...]
    constexpr std::size_t joinPacketSize = 22;
    constexpr std::size_t createRoomPacketSize = 2;
    constexpr std::size_t quitPacketSize = 2;
    constexpr std::size_t headerBytesAmount = 2;
    constexpr std::size_t roomIDLength = 2;
    constexpr int bufferSize = 64;

    constexpr std::size_t spritesAmount = 11;
    // 0 -----> Blocked Grid
    // 1 -----> Play Grid
    // 2/11 --> Cells
    constexpr std::size_t startingSpriteCellsIndex = 2;
    constexpr std::size_t spritesCellAmount = spritesAmount - startingSpriteCellsIndex;

    using Byte = char;

    typedef struct Header
    {
        std::uint32_t rid;
        Command command;
    } Header;

    class NaiveSDLTexture
    {
    public:
        NaiveSDLTexture(SDL_Texture* texture, SDL_Rect textureBox) : texture(texture), textureBox(textureBox) { }
            
        SDL_Texture* GetTexture() const; 
        void SetTexture(SDL_Texture* texture);

        SDL_Rect& GetTextureBox();

    private:
        SDL_Texture* texture;
        SDL_Rect textureBox;

    };

    class Client
    {
    public:
        Client(const char* ipAddress = "77.32.107.199", const int port = 9999);
        ~Client();

        void Run();

        void InitSprites();
        int ClickedOnWhatCell(SDL_Event* event);

        void ReceiveData();
        void SendData();

    private:
        int socketID;
        sockaddr_in sin;

        std::unordered_map<Command, std::function<void(const int)>> commandFunctionsSnd;
        void JoinCommand(const int currentCommandID);
        void CreateRoomCommand(const int currentCommandID);
        void ChallengeCommand(const int currentCommandID);
        void QuitCommand(const int currentCommandID);

        std::unordered_map<Command, std::function<void(Byte*, const int)>> commandFunctionsRcv;
        void StartGameCommand(Byte* buffer, const int len);
        void AnnounceRoomCommand(Byte* buffer, const int len);
        void UpdateFieldCommand(Byte* buffer, const int len);
        void ResetClientCommand(Byte* buffer, const int len);

        std::thread recvThread;
        std::thread sendThread;

        SDL_Window* window;
        SDL_Renderer* renderer;
        std::map<TextureAsset, SDL_Texture*> textures;
        std::vector<NaiveSDLTexture> sprites;

        void PrintCommands() const;

    };
}

using Client = TTTClient::Client;
using NaiveSDLTexture = TTTClient::NaiveSDLTexture;

#endif //TICTACTOE_CLIENT_HPP