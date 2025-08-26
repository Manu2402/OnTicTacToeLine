#include <tictactoe_client.hpp>

Client::Client(const char* ip_address, const int port)
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
        this->socket_id = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (this->socket_id < 0)
        {
            throw NetworkException("ERROR: Unable to initialize the UDP socket!\n");
        }
    }
    catch (const NetworkException& exception)
    {
        std::cout << exception.what() << "\n";
    }

    inet_pton(AF_INET, ip_address, &this->sin.sin_addr);
    this->sin.sin_family = AF_INET;
    this->sin.sin_port = htons(port);

    // --------------------------------------------------------------------------------------------

    this->command_functions_snd[Command::JOIN] = [this](const int currentCommandID) { this->JoinCommand(currentCommandID); };
    this->command_functions_snd[Command::CREATE_ROOM] = [this](const int currentCommandID) { this->CreateRoomCommand(currentCommandID); };
    this->command_functions_snd[Command::CHALLENGE] = [this](const int currentCommandID) { this->ChallengeCommand(currentCommandID); };
    this->command_functions_snd[Command::QUIT] = [this](const int currentCommandID) { this->QuitCommand(currentCommandID); };

    this->command_functions_rcv[Command::ANNOUNCE_ROOM] = [this](char* buffer, const int len) { this->AnnounceRoomCommand(buffer, len); };
    this->command_functions_rcv[Command::START_GAME] = [this](char* buffer, const int len) { this->StartGameCommand(buffer, len); };
    this->command_functions_rcv[Command::UPDATE_FIELD] = [this](char* buffer, const int len) { this->UpdateFieldCommand(buffer, len); };
    this->command_functions_rcv[Command::RESET_CLIENT] = [this](char* buffer, const int len) { this->ResetClientCommand(buffer, len); };

    // --------------------------------------------------------------------------------------------

    if (SDL_Init(SDL_INIT_VIDEO) != 0)
    {
        throw std::runtime_error(std::string("Unable to initialize SDL: %s", SDL_GetError()));
    }

    this->window = SDL_CreateWindow("TicTacToe", 100, 100, 512, 512, 0);
    if (!window)
    {
        throw std::runtime_error(std::string("Unable to create window: %s", SDL_GetError()));
    }

    this->renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer)
    {
        throw std::runtime_error(std::string("Unable to create renderer: %s", SDL_GetError()));
    }

    this->InitSprites();

    std::cout << "Client is ready!\n\n";
}

Client::~Client()
{    
    // Detach these threads from the main thread. The OS will handle they life cycle.
    send_thread.detach();
    recv_thread.detach();

    if (this->renderer) SDL_DestroyRenderer(renderer);
    if (this->window) SDL_DestroyWindow(window);
    SDL_Quit();
}

void Client::Run()
{
    // Threads init.
    this->recv_thread = std::thread(&Client::ReceiveData, this);
    this->send_thread = std::thread(&Client::SendData, this);

    while (running)
    {
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {           
            if (event.type == SDL_QUIT)
            {
                running = false;
                this->command_functions_snd[Command::QUIT](static_cast<int>(Command::QUIT));

                return;
            }

            if (event.type == SDL_MOUSEBUTTONDOWN)
            {
                int clicked_cell = ClickedOnWhichCell(&event);
                if (clicked_cell < 0) continue;

                header_t header;
                header.rid = 0;
                header.command = Command::MOVE;

                std::string move_info(std::to_string(header.rid) + std::to_string(static_cast<int>(header.command)) + std::to_string(clicked_cell));
                const char* move_packet = move_info.c_str();  

                int sent_bytes = sendto(this->socket_id, move_packet, strlen(move_packet), 0, reinterpret_cast<sockaddr*>(&sin), sizeof(sin));
                break;
            }
        }

        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255); // White.
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
    this->sprites.reserve(sprites_amount);

    int width, height, channels;
    unsigned char* pixels = stbi_load("resources/tictactoe_blocked_grid.png", &width, &height, &channels, 4);
    SDL_Texture* blocked_grid_texture = SDL_CreateTexture(this->renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, width, height);
    SDL_UpdateTexture(blocked_grid_texture, nullptr, pixels, width * 4);
    textures[BLOCKED_GRID] = blocked_grid_texture;
    SDL_Rect rect = { 0, 0, width, height };
    sprites.push_back(NaiveSDLTexture(blocked_grid_texture, rect));

    pixels = stbi_load("resources/tictactoe_play_grid.png", &width, &height, &channels, 4);
    SDL_Texture* grid_texture = SDL_CreateTexture(this->renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, width, height);
    SDL_UpdateTexture(grid_texture, nullptr, pixels, width * 4);
    textures[PLAY_GRID] = grid_texture;
    rect = { 0, 0, width, height };
    sprites.push_back(NaiveSDLTexture(nullptr, rect));

    pixels = stbi_load("resources/x_sign.png", &width, &height, &channels, 4);
    SDL_Texture* x_game_texture = SDL_CreateTexture(this->renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, width, height);
    SDL_UpdateTexture(x_game_texture, nullptr, pixels, width * 4);
    textures[X_SIGN] = x_game_texture;

    pixels = stbi_load("resources/o_sign.png", &width, &height, &channels, 4);
    SDL_Texture* o_game_texture = SDL_CreateTexture(this->renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, width, height);
    SDL_UpdateTexture(o_game_texture, nullptr, pixels, width * 4);
    textures[O_SIGN] = o_game_texture;

    for (size_t i = 0; i < sprites_cell_amount; i++)
    {
        SDL_Rect rect = { cell_coordinates[i].first, cell_coordinates[i].second, symbols_texture_size.first, symbols_texture_size.second };
        sprites.push_back(NaiveSDLTexture(nullptr, rect));
    }
    
    stbi_image_free(pixels);
}

int Client::ClickedOnWhichCell(SDL_Event* event)
{
    for (size_t i = starting_sprite_cells_index; i < sprites_amount; i++)
    {
        if (event->button.x >= sprites[i].GetTextureBox().x && event->button.x < (sprites[i].GetTextureBox().x + sprites[i].GetTextureBox().w) && event->button.y >= sprites[i].GetTextureBox().y && event->button.y < (sprites[i].GetTextureBox().y + sprites[i].GetTextureBox().h))
        {
            return i - starting_sprite_cells_index;
        }
    }

    return -1;
}

// ------------------------------------------------------------------------------------------------

void Client::ReceiveData()
{
    header_t header; 
    char buffer[buffer_size];
    sockaddr_in sender_in;
    int sender_in_size = sizeof(sender_in); 

    while (running)
    {
        int len = recvfrom(this->socket_id, buffer, buffer_size, 0, reinterpret_cast<sockaddr*>(&sender_in), &sender_in_size); 
        if (len < 0) continue;

        if (len < header_bytes_amount)
        {
            std::cout << "Invalid packet size: " << len << " bytes!\n";
            continue;
        }   

        header.rid = buffer[0] - '0';
        header.command = static_cast<Command>(buffer[1] - '0');

        this->command_functions_rcv[header.command](buffer, len);
    }
}

void Client::SendData()
{
    PrintCommands();

    std::string current_command_id_str;
    int current_command_id, sent_bytes;
    Command current_command;

    while (running)
    {
        std::cout << "\nInsert a command: ";
        std::cin >> current_command_id_str;  

        if (current_command_id_str.size() == 1) // One figure.
        {
            current_command_id = current_command_id_str[0] - '0';
            current_command = static_cast<Command>(current_command_id);
            
            // Command '3' is bound to the move, but the client activates this command by clicking on the cells.
            // So, the command '3' is not allowed! The command '4' is implicit when the window is closed.
            if (current_command >= 0 && current_command < (command_functions_snd.size() - 1))
            {
                this->command_functions_snd[current_command](current_command_id);
                continue;
            }

            std::cout << "Invalid command!\n";
        }
        else
        {
            std::cout << "Invalid command!\n";
        }
    }
}

// ------------------------------------------------------------------------------------------------

void Client::JoinCommand(const int current_command_id)
{
    std::string player_name;
    std::cout << "Insert your name: ";
    std::cin >> player_name;
     
    std::string join_info('0' + std::to_string(current_command_id) + player_name);
    const char* join_packet = join_info.c_str();

    int sent_bytes = sendto(socket_id, join_packet, join_packet_size, 0, reinterpret_cast<sockaddr*>(&sin), sizeof(sin));
    std::cout << "You have attempted to connect to the server!\n";
}

void Client::CreateRoomCommand(const int current_command_id)
{
    std::string create_room_info('0' + std::to_string(current_command_id));
    const char* create_room_packet = create_room_info.c_str();

    int sent_bytes = sendto(socket_id, create_room_packet, create_room_packet_size, 0, reinterpret_cast<sockaddr*>(&sin), sizeof(sin));
    std::cout << "You have attempted to create a room into the server!\n";
}

void Client::ChallengeCommand(const int current_command_id)
{
    std::uint32_t room_id;
    std::cout << "Insert room id: ";
    std::cin >> room_id;

    std::string room_id_str(std::to_string(room_id));   
    std::string challenge_info('0' + std::to_string(current_command_id) + Utility::GetParsedRoomIDLength(room_id_str, room_id_str.length()) + room_id_str);
    const char* challenge_packet = challenge_info.c_str();

    int sent_bytes = sendto(socket_id, challenge_packet, strlen(challenge_packet), 0, reinterpret_cast<sockaddr*>(&sin), sizeof(sin));
    std::cout << "You have attempted to challenge someone into the server!\n";
}

void Client::QuitCommand(const int current_command_id)
{
    std::string quit_info('0' + std::to_string(current_command_id));
    const char* quit_packet = quit_info.c_str(); 

    int sent_bytes = sendto(socket_id, quit_packet, quit_packet_size, 0, reinterpret_cast<sockaddr*>(&sin), sizeof(sin));
    std::cout << "\nYou have attempted to quit to the server!\n";
}

// ------------------------------------------------------------------------------------------------

void Client::StartGameCommand(char* buffer, const int len)
{
    this->sprites[0].SetTexture(nullptr);
    this->sprites[1].SetTexture(textures[PLAY_GRID]);
}

void Client::AnnounceRoomCommand(char* buffer, const int len)
{
    std::string room_id_length_str;
    std::memcpy(&room_id_length_str, &buffer[header_bytes_amount], room_id_len);
    const std::uint32_t room_id_length = std::stoull(room_id_length_str);

    std::string room_id_bytes;
    std::memcpy(&room_id_bytes, &buffer[header_bytes_amount + room_id_len], room_id_length);

    int room_id = std::stoi(room_id_bytes);
    std::cout << "\nAnnouncing room " << room_id;
}

void Client::UpdateFieldCommand(char* buffer, const int len)
{
    if (len != sprites_amount) return;

    for (size_t i = starting_sprite_cells_index; i < sprites_amount; i++)
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
            case '?': break; // Nothing.
        }
    }
}

void Client::ResetClientCommand(char* buffer, const int len)
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
    return this->texture_box;
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