#include "GameServer.h"
#include <Logger.h>
#include <iostream>
#include <thread>
#include <csignal>

using namespace Utilities;

std::unique_ptr<GameServerSample::GameServer> g_server;
std::atomic<bool> g_running{true};

void signal_handler(int signal)
{
    if (signal == SIGINT || signal == SIGTERM)
    {
        std::cout << "\nShutting down server..." << std::endl;
        g_running = false;
    }
}

int main(int argc, char* argv[])
{
    // Setup signal handlers
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);
    
    // Initialize logger
    Logger::handle().set_log_level(LogTypes::Information);
    Logger::handle().enable_console_output(true);
    Logger::handle().set_log_path("./logs/");
    
    // Parse port from command line
    uint16_t port = 8080;
    if (argc > 1)
    {
        port = static_cast<uint16_t>(std::atoi(argv[1]));
    }
    
    // Create and start server
    g_server = std::make_unique<GameServerSample::GameServer>();
    
    auto result = g_server->start(port);
    if (!std::get<0>(result))
    {
        std::cerr << "Failed to start server: " 
                  << std::get<1>(result).value_or("Unknown error") << std::endl;
        return 1;
    }
    
    std::cout << "Game server started on port " << port << std::endl;
    std::cout << "Press Ctrl+C to stop..." << std::endl;
    
    // Main server loop
    while (g_running && g_server->is_running())
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    // Cleanup
    g_server->stop();
    g_server.reset();
    
    std::cout << "Server stopped successfully" << std::endl;
    return 0;
}
