// headless/headless_main.cpp
// Entry point for the headless Catan game.
//
// Stdout carries the JSON protocol (Game → Orchestrator).
// Stdin carries the JSON commands (Orchestrator → Game).
// Stderr carries all existing game log output (dice rolls, board display, etc.)
//
// Usage:
//   ./headless_catan < orchestrator_input > protocol_output 2> game_log

#include <iostream>

#include "../Catan.hpp"
#include "../player/Player.hpp"
#include "HeadlessPlayer.hpp"

int main() {
    // Capture the real stdout buffer before redirecting.
    std::streambuf* real_stdout = std::cout.rdbuf();
    std::ostream proto(real_stdout);  // JSON protocol stream → real stdout

    // Redirect std::cout → stderr so all existing game log/print calls go to stderr.
    std::cout.rdbuf(std::cerr.rdbuf());

    // Create the three players.
    HeadlessPlayer p1(PlayerColor::RED,    proto);
    HeadlessPlayer p2(PlayerColor::BLUE,   proto);
    HeadlessPlayer p3(PlayerColor::YELLOW, proto);

    Catan catan(p1, p2, p3);

    // Run the game.
    Player* winner = catan.start_game();

    // Determine plain color name of winner.
    std::string winner_color = "UNKNOWN";
    if (winner != nullptr) {
        std::string c = winner->get_color();
        if (c.find("RED")    != std::string::npos) winner_color = "RED";
        else if (c.find("BLUE")   != std::string::npos) winner_color = "BLUE";
        else if (c.find("YELLOW") != std::string::npos) winner_color = "YELLOW";
    }

    // Emit game_over message.
    proto << "{\"type\":\"game_over\",\"winner\":\"" << winner_color << "\"}" << "\n";
    proto.flush();

    return 0;
}
