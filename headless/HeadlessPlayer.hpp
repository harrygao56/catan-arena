// headless/HeadlessPlayer.hpp
// A Player subclass that speaks the JSON protocol over stdin/stdout instead of
// using the terminal interactively.
#pragma once
#include <iostream>
#include <string>
#include <vector>

#include "../player/Player.hpp"

// Forward declarations
class Catan;

class HeadlessPlayer : public Player {
   public:
    /**
     * @param color   Player color
     * @param proto   The output stream that carries the JSON protocol
     *                (the "real" stdout before it was redirected to stderr)
     */
    HeadlessPlayer(PlayerColor color, std::ostream& proto);

    // ---- Overrides ----
    void play_turn(Catan& game) override;
    int  place_settlement(Catan& game, bool first_round = false) override;
    void place_road(Catan& game, bool first_round = false) override;
    void place_city(Catan& game) override;
    void make_trade(Catan& game) override;
    bool trade_request(Player& trader,
                       const std::vector<std::pair<resource, int>>& offer_res,
                       const std::vector<Card*>& offer_dev,
                       const std::vector<std::pair<resource, int>>& request_res,
                       const std::vector<std::pair<CardType, int>>& request_dev) override;
    void robber() override;
    void play_dev_card(Catan& game) override;
    void buy_dev_card(Catan& game) override;

   private:
    std::ostream& proto_;   ///< JSON protocol output (real stdout)
    Catan* current_game_;   ///< Set at start of play_turn / place_settlement etc.

    // ---- Pending state for dev card I/O decoupling ----
    bool using_pending_roads_{false};
    int pending_edge1_{-1};
    int pending_edge2_{-1};
    int pending_road_call_count_{0};

    // ---- JSON helpers ----

    /// Plain color name: "RED", "BLUE", "YELLOW"
    std::string my_color_name() const;
    static std::string plain_color_of(Player* p);
    static std::string resource_name(resource res);
    static std::string card_type_name(CardType t);

    /// Build the full game_state JSON object string
    std::string build_game_state(Catan& game);

    /// Build legal_actions JSON object for the post-roll turn phase
    std::string build_legal_actions_post_roll(Catan& game);

    /// Build legal_actions JSON object for the pre-roll phase
    std::string build_legal_actions_pre_roll(Catan& game);

    /// Build a JSON array of playable dev card type strings for this player
    std::string build_playable_dev_cards(Catan& game);

    /// Build a JSON integer array from a vector<int>
    static std::string int_array(const std::vector<int>& v);

    /// Emit a newline-terminated JSON line to proto_
    void emit(const std::string& json_line);

    /// Read one line from stdin
    static std::string read_line();

    /// Handle a "play_dev_card" JSON line (either from pre_roll or post_roll loop)
    void handle_play_dev_card_line(Catan& game, const std::string& line);

    /// Handle a "trade" JSON line inside play_turn
    void handle_trade_line(Catan& game, const std::string& line);
};
