#include <tictactoe_client.hpp>

Client::Client(const char* ipAddress, const int port)
{
#ifdef _WIN32
    try
    {
        WSADATA wsa_data;
        if (WSAStartup(0x202, &wsa_data))
        {
            throw NetworkException("ERROR: Unable to initialize \"winsock2\"!\n");
        }   
    }
    catch (const NetworkException& exception)
    {
        std::cout << exception.what() << "\n";
    }
#endif
    try
    {
        this->socketID = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (this->socketID < 0)
        {
            throw NetworkException("ERROR: Unable to initialize the UDP socket!\n");
        }
    }
    catch (const NetworkException& exception)
    {
        std::cout << exception.what() << "\n";
    }

    inet_pton(AF_INET, ipAddress, &this->sin.sin_addr);
    this->sin.sin_family = AF_INET;
    this->sin.sin_port = htons(port);

    // --------------------------------------------------------------------------------------------

    this->commandFunctionsSnd[Command::JOIN] = [this](const int currentCommandID) { this->JoinCommand(currentCommandID); };
    this->commandFunctionsSnd[Command::CREATE_ROOM] = [this](const int currentCommandID) { this->CreateRoomCommand(currentCommandID); };
    this->commandFunctionsSnd[Command::CHALLENGE] = [this](const int currentCommandID) { this->ChallengeCommand(currentCommandID); };
    this->commandFunctionsSnd[Command::QUIT] = [this](const int currentCommandID) { this->QuitCommand(currentCommandID); };

    this->commandFunctionsRcv[Command::ANNOUNCE_ROOM] = [this](Byte* buffer, const int len) { this->AnnounceRoomCommand(buffer, len); };
    this->commandFunctionsRcv[Command::START_GAME] = [this](Byte* buffer, const int len) { this->StartGameCommand(buffer, len); };
    this->commandFunctionsRcv[Command::UPDATE_FIELD] = [this](Byte* buffer, const int len) { this->UpdateFieldCommand(buffer, len); };
    this->commandFunctionsRcv[Command::RESET_CLIENT] = [this](Byte* buffer, const int len) { this->ResetClientCommand(buffer, len); };

    // --------------------------------------------------------------------------------------------

    if (SDL_Init(SDL_INIT_VIDEO) != 0)
    {
        SDL_Log("Unable to initialize SDL: %s", SDL_GetError());
        this->~Client();
    }

    this->window = SDL_CreateWindow("TicTacToe", 100, 100, 512, 512, 0);
    if (!window)
    {
        SDL_Log("Unable to create window: %s", SDL_GetError());
        this->~Client();
    }

    this->renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer)
    {
        SDL_Log("Unable to create renderer: %s", SDL_GetError());
        this->~Client();
    }

    this->InitSprites();

    std::cout << "Client is ready!\n\n";
}

Client::~Client()
{    
    // Detach these threads from the main thread. The OS will handle they life cycle.
    sendThread.detach();
    recvThread.detach();

    if (this->renderer) SDL_DestroyRenderer(renderer);
    if (this->window) SDL_DestroyWindow(window);
    SDL_Quit();
}

void Client::Run()
{
    // Thread init.
    this->recvThread = std::thread(&Client::ReceiveData, this);
    this->sendThread = std::thread(&Client::SendData, this);

    while (running)
    {
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {           
            if (event.type == SDL_QUIT)
            {
                running = false;
                this->commandFunctionsSnd[Command::QUIT](static_cast<int>(Command::QUIT));

                return;
            }

            if (event.type == SDL_MOUSEBUTTONDOWN)
            {
                int clickedCell = ClickedOnWhatCell(&event);
                if (clickedCell < 0) continue;

                Header header;
                header.rid = 0;
                header.command = Command::MOVE;

                std::string moveInfo(std::to_string(header.rid) + std::to_string(static_cast<int>(header.command)) + std::to_string(clickedCell));
                const char* movePacket = moveInfo.c_str();  

                int sent_bytes = sendto(this->socketID, movePacket, strlen(movePacket), 0, reinterpret_cast<sockaddr*>(&sin), sizeof(sin));
                break;
            }
        }

        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255); // White
        SDL_RenderClear(renderer);
            
        for (NaiveSDLTexture& sprite : this->sprites)
        {
            if (!sprite.GetTexture()) continue;
            SDL_RenderCopy(renderer, sprite.GetTexture(), nullptr, &sprite.GetTextureBox());
        }

        SDL_RenderPresent(renderer);
    }   
}

void Client::InitSprites()
{
    this->sprites.reserve(spritesAmount);

    int width, height, channels;
    unsigned char* pixels = stbi_load("resources/tictactoe_blocked_grid.png", &width, &height, &channels, 4);
    SDL_Texture* blockedGridTexture = SDL_CreateTexture(this->renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, width, height);
    SDL_UpdateTexture(blockedGridTexture, nullptr, pixels, width * 4);  
    textures[BLOCKED_GRID] = blockedGridTexture;
    SDL_Rect rect = { 0, 0, width, height };
    sprites.push_back(NaiveSDLTexture(blockedGridTexture, rect));

    pixels = stbi_load("resources/tictactoe_play_grid.png", &width, &height, &channels, 4);
    SDL_Texture* gridTexture = SDL_CreateTexture(this->renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, width, height);
    SDL_UpdateTexture(gridTexture, nullptr, pixels, width * 4);
    textures[PLAY_GRID] = gridTexture;
    rect = { 0, 0, width, height };
    sprites.push_back(NaiveSDLTexture(nullptr, rect));

    pixels = stbi_load("resources/x_sign.png", &width, &height, &channels, 4);
    SDL_Texture* xGameTexture = SDL_CreateTexture(this->renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, width, height);
    SDL_UpdateTexture(xGameTexture, nullptr, pixels, width * 4);
    textures[X_SIGN] = xGameTexture;

    pixels = stbi_load("resources/o_sign.png", &width, &height, &channels, 4);
    SDL_Texture* oGameTexture = SDL_CreateTexture(this->renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, width, height);
    SDL_UpdateTexture(oGameTexture, nullptr, pixels, width * 4);
    textures[O_SIGN] = oGameTexture;

    for (size_t i = 0; i < spritesCellAmount; i++)
    {
        SDL_Rect rect = { cellCoordinates[i].first, cellCoordinates[i].second, symbolsTextureSize.first, symbolsTextureSize.second };
        sprites.push_back(NaiveSDLTexture(nullptr, rect));
    }
    
    stbi_image_free(pixels);
}

int Client::ClickedOnWhatCell(SDL_Event* event)
{
    for (size_t i = startingSpriteCellsIndex; i < spritesAmount; i++)
    {
        if (event->button.x >= sprites[i].GetTextureBox().x && event->button.x < (sprites[i].GetTextureBox().x + sprites[i].GetTextureBox().w) && event->button.y >= sprites[i].GetTextureBox().y && event->button.y < (sprites[i].GetTextureBox().y + sprites[i].GetTextureBox().h))
        {
            return i - startingSpriteCellsIndex;
        }
    }

    return -1;
}

// ------------------------------------------------------------------------------------------------

void Client::ReceiveData()
{
    Header header; 
    Byte buffer[bufferSize];
    sockaddr_in sender_in;
    int sender_in_size = sizeof(sender_in); 

    while (running)
    {
        int len = recvfrom(this->socketID, buffer, bufferSize, 0, reinterpret_cast<sockaddr*>(&sender_in), &sender_in_size); 
        if (len < 0) continue;

        if (len < headerBytesAmount)
        {
            std::cout << "Invalid packet size: " << len << " bytes!\n";
            continue;
        }   

        header.rid = buffer[0] - '0';
        header.command = static_cast<Command>(buffer[1] - '0');

        this->commandFunctionsRcv[header.command](buffer, len);  
    }
}

void Client::SendData()
{
    PrintCommands();

    int currentCommandID, sentBytes;
    Command currentCommand;

    while (running)
    {
        std::cout << "\nInsert a command: ";
        std::cin >> currentCommandID;  
        currentCommand = static_cast<Command>(currentCommandID);

        // Command '3' is bound to the move, but the client activates this command by clicking on the cells.
        // So, the command '3' is not allowed! The command '4' is implicit when the window is closed.
        if ((currentCommand >= 0 && currentCommand < (commandFunctionsSnd.size() - 1)))
        {
            this->commandFunctionsSnd[currentCommand](currentCommandID);
            continue;
        }

        std::cout << "Invalid command!\n";
    }
}

// ------------------------------------------------------------------------------------------------

void Client::JoinCommand(const int currentCommandID)
{
    std::string playerName;
    std::cout << "Insert your name: ";
    std::cin >> playerName;
     
    std::string joinInfo('0' + std::to_string(currentCommandID) + playerName);
    const char* joinPacket = joinInfo.c_str();  

    int sentBytes = sendto(socketID, joinPacket, joinPacketSize, 0, reinterpret_cast<sockaddr*>(&sin), sizeof(sin));
    
    std::cout << "You have attempted to connect to the server!\n";
}

void Client::CreateRoomCommand(const int currentCommandID)
{
    std::string createRoomInfo('0' + std::to_string(currentCommandID));
    const char* createRoomPacket = createRoomInfo.c_str();

    int sentBytes = sendto(socketID, createRoomPacket, createRoomPacketSize, 0, reinterpret_cast<sockaddr*>(&sin), sizeof(sin));
    std::cout << "You have attempted to create a room into the server!\n";
}

void Client::ChallengeCommand(const int currentCommandID)
{
    std::uint32_t roomID;
    std::cout << "Insert roomID: ";
    std::cin >> roomID;

    std::string roomIDString(std::to_string(roomID));   
    std::string challengeInfo('0' + std::to_string(currentCommandID) + Utility::GetParsedRoomIDLength(roomIDString, roomIDString.length()) + roomIDString);
    const char* challengePacket = challengeInfo.c_str(); 

    int sentBytes = sendto(socketID, challengePacket, strlen(challengePacket), 0, reinterpret_cast<sockaddr*>(&sin), sizeof(sin));
    std::cout << "You have attempted to challenge someone into the server!\n";
}

void Client::QuitCommand(const int currentCommandID)
{
    std::string quitInfo('0' + std::to_string(currentCommandID));
    const char* quitPacket = quitInfo.c_str(); 

    int sentBytes = sendto(socketID, quitPacket, quitPacketSize, 0, reinterpret_cast<sockaddr*>(&sin), sizeof(sin));
    std::cout << "\nYou have attempted to quit to the server!\n";
}

// ------------------------------------------------------------------------------------------------

void Client::StartGameCommand(Byte* buffer, const int len)
{
    this->sprites[0].SetTexture(nullptr);
    this->sprites[1].SetTexture(textures[PLAY_GRID]);
}

void Client::AnnounceRoomCommand(Byte* buffer, const int len)
{
    std::string roomIDLengthStr;
    std::memcpy(&roomIDLengthStr, &buffer[headerBytesAmount], roomIDLength);
    const std::uint32_t roomIDLengthValue = std::stoull(roomIDLengthStr);

    std::string roomIDBytes;
    std::memcpy(&roomIDBytes, &buffer[headerBytesAmount + roomIDLength], roomIDLengthValue);

    int roomID = std::stoi(roomIDBytes);
    std::cout << "\nAnnouncing room " << roomID << "\n";
}

void Client::UpdateFieldCommand(Byte* buffer, const int len)
{
    if (len != spritesAmount) return;

    for (size_t i = startingSpriteCellsIndex; i < spritesAmount; i++)
    {
        switch (buffer[i])
        {
            case ' ':
            {
                sprites[i].SetTexture(nullptr);
                break;
            }
            case 'X':
            {
                sprites[i].SetTexture(textures[X_SIGN]);
                break;
            }
            case 'O':
            {
                sprites[i].SetTexture(textures[O_SIGN]);
                break;
            }
            case '?': break; // Nothing
        }
    }
}

void Client::ResetClientCommand(Byte* buffer, const int len)
{
    for (auto it = sprites.begin(); it != sprites.end(); ++it)
    {
        it->SetTexture(nullptr);
    }

    sprites[0].SetTexture(textures[BLOCKED_GRID]);  
}

// ------------------------------------------------------------------------------------------------

SDL_Texture* NaiveSDLTexture::GetTexture() const
{
    return this->texture;
}

void NaiveSDLTexture::SetTexture(SDL_Texture* texture)
{
    this->texture = texture;
}

SDL_Rect& NaiveSDLTexture::GetTextureBox()
{
    return this->textureBox;
}

// ------------------------------------------------------------------------------------------------

void Client::PrintCommands() const
{
    std::cout << "COMMAND LIST:\n-0: Join\n-1: Create Room\n-2: Challenge\n-Click On a Cell: Move\n-Close Window: Quit\n";
}

// ------------------------------------------------------------------------------------------------

int main(int argc, char** argv)
{
    Client client = {};
    client.Run();

    return EXIT_SUCCESS;
}