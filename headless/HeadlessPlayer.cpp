// headless/HeadlessPlayer.cpp
#include "HeadlessPlayer.hpp"

#include <algorithm>
#include <sstream>
#include <stdexcept>

#include "../Catan.hpp"
#include "../cards/KnightCard.hpp"
#include "../cards/MonopolyCard.hpp"
#include "../cards/PromotionCard.hpp"
#include "../cards/RoadBuildCard.hpp"
#include "../cards/YearOfPlentyCard.hpp"
#include "json_utils.hpp"

using namespace json_utils;

// ============================================================
// Constructor
// ============================================================

HeadlessPlayer::HeadlessPlayer(PlayerColor color, std::ostream& proto)
    : Player(color), proto_(proto), current_game_(nullptr) {}

// ============================================================
// Static helpers
// ============================================================

std::string HeadlessPlayer::my_color_name() const {
    return plain_color_of(const_cast<HeadlessPlayer*>(this));
}

std::string HeadlessPlayer::plain_color_of(Player* p) {
    if (p == nullptr) return "null";
    std::string c = p->get_color();
    if (c.find("RED")    != std::string::npos) return "RED";
    if (c.find("BLUE")   != std::string::npos) return "BLUE";
    if (c.find("YELLOW") != std::string::npos) return "YELLOW";
    return "UNKNOWN";
}

std::string HeadlessPlayer::resource_name(resource res) {
    switch (static_cast<resource::Value>(res.get_int())) {
        case resource::WOOD:   return "wood";
        case resource::CLAY:   return "clay";
        case resource::SHEEP:  return "sheep";
        case resource::WHEAT:  return "wheat";
        case resource::STONE:  return "stone";
        default:               return "none";
    }
}

std::string HeadlessPlayer::card_type_name(CardType t) {
    switch (t) {
        case CardType::KNIGHT:         return "knight";
        case CardType::VICTORY_POINT:  return "victory_point";
        case CardType::ROAD_BUILDING:  return "road_building";
        case CardType::MONOPOLY:       return "monopoly";
        case CardType::YEAR_OF_PLENTY: return "year_of_plenty";
        default:                       return "unknown";
    }
}

std::string HeadlessPlayer::int_array(const std::vector<int>& v) {
    std::string s;
    for (std::size_t i = 0; i < v.size(); i++) {
        if (i) s += ",";
        s += std::to_string(v[i]);
    }
    return arr(s);
}

void HeadlessPlayer::emit(const std::string& json_line) {
    proto_ << json_line << "\n";
    proto_.flush();
}

std::string HeadlessPlayer::read_line() {
    std::string line;
    while (std::getline(std::cin, line)) {
        // skip blank lines
        if (!line.empty()) return line;
    }
    return "";
}

// ============================================================
// JSON builders
// ============================================================

std::string HeadlessPlayer::build_game_state(Catan& game) {
    // --- players array ---
    std::string players_json;
    for (Player* p : game.get_players()) {
        if (!players_json.empty()) players_json += ",";

        std::string dev_arr;
        for (Card* c : p->get_dev_cards()) {
            if (!dev_arr.empty()) dev_arr += ",";
            dev_arr += quote(card_type_name(c->type()));
        }

        std::string pj =
            kv("color", plain_color_of(p)) + "," +
            kv("vp", p->get_victory_points()) + "," +
            kv("knights", p->get_knights()) + "," +
            "\"resources\":{" +
                kv("wood",  p->get_resource_count(resource::WOOD))  + "," +
                kv("clay",  p->get_resource_count(resource::CLAY))  + "," +
                kv("sheep", p->get_resource_count(resource::SHEEP)) + "," +
                kv("wheat", p->get_resource_count(resource::WHEAT)) + "," +
                kv("stone", p->get_resource_count(resource::STONE)) +
            "}," +
            "\"dev_cards\":[" + dev_arr + "]," +
            kv("total_resources", p->get_total_resources());
        players_json += "{" + pj + "}";
    }

    // --- vertices array ---
    std::string verts_json;
    for (LandVertex& v : game.get_vertices()) {
        if (!verts_json.empty()) verts_json += ",";

        std::string res_arr;
        for (auto& rp : v.get_resources()) {
            if (rp.first == resource::NONE || rp.first == resource::DESERT || rp.second == 0) continue;
            if (!res_arr.empty()) res_arr += ",";
            res_arr += "{" + kv("type", resource_name(rp.first)) + "," + kv("number", rp.second) + "}";
        }

        Player* owner = v.get_owner();
        std::string owner_val = owner ? quote(plain_color_of(owner)) : "null";

        verts_json +=
            "{" +
            kv("id", v.get_id()) + "," +
            "\"owner\":" + owner_val + "," +
            kv("is_city", v.is_contains_city()) + "," +
            "\"resources\":[" + res_arr + "]" +
            "}";
    }

    // --- edges array ---
    std::string edges_json;
    for (RoadEdge& e : game.get_edges()) {
        if (!edges_json.empty()) edges_json += ",";
        Player* owner = e.get_owner();
        std::string owner_val = owner ? quote(plain_color_of(owner)) : "null";
        edges_json += "{" + kv("id", e.get_id()) + ",\"owner\":" + owner_val + "}";
    }

    std::string state =
        "\"players\":[" + players_json + "]," +
        "\"board\":{\"vertices\":[" + verts_json + "],\"edges\":[" + edges_json + "]}," +
        kv("current_player", my_color_name()) + "," +
        kv("dev_cards_remaining", static_cast<int>(game.get_dev_cards().size()));

    return "{" + state + "}";
}

std::string HeadlessPlayer::build_playable_dev_cards(Catan& /*game*/) {
    // In the original game, playing a dev card uses the turn whether pre- or post-roll.
    // We expose all dev cards the player holds (except VP which is auto-used).
    std::string s;
    for (Card* c : get_dev_cards()) {
        if (c->type() == CardType::VICTORY_POINT) continue;
        if (!s.empty()) s += ",";
        s += quote(card_type_name(c->type()));
    }
    return s;
}

std::string HeadlessPlayer::build_legal_actions_pre_roll(Catan& game) {
    std::string dev_cards = build_playable_dev_cards(game);
    return "{" +
        kv("roll_dice", true) + "," +
        kv("end_turn", false) + "," +
        "\"place_settlement\":[]," +
        "\"place_road\":[]," +
        "\"place_city\":[]," +
        kv("buy_dev_card", false) + "," +
        "\"play_dev_card\":[" + dev_cards + "]," +
        kv("trade", false) +
        "}";
}

std::string HeadlessPlayer::build_legal_actions_post_roll(Catan& game) {
    auto settlements = game.get_legal_settlement_spots(*this);
    auto roads       = game.get_legal_road_spots(*this);
    auto cities      = game.get_legal_city_spots(*this);
    bool can_buy     = game.can_buy_dev_card(*this);
    // Can always offer a trade (even if no other player accepts)
    bool can_trade   = true;

    std::string dev_cards = build_playable_dev_cards(game);

    return "{" +
        kv("roll_dice", false) + "," +
        kv("end_turn", true) + "," +
        "\"place_settlement\":" + int_array(settlements) + "," +
        "\"place_road\":"       + int_array(roads)       + "," +
        "\"place_city\":"       + int_array(cities)      + "," +
        kv("buy_dev_card", can_buy) + "," +
        "\"play_dev_card\":[" + dev_cards + "]," +
        kv("trade", can_trade) +
        "}";
}

// ============================================================
// play_turn
// ============================================================

void HeadlessPlayer::play_turn(Catan& game) {
    current_game_ = &game;

    // ---- Phase 1: pre-roll ----
    while (true) {
        std::string msg =
            "{\"type\":\"action_request\","
            "\"player\":" + quote(my_color_name()) + ","
            "\"phase\":\"pre_roll\","
            "\"game_state\":" + build_game_state(game) + ","
            "\"legal_actions\":" + build_legal_actions_pre_roll(game) +
            "}";
        emit(msg);

        std::string line = read_line();
        std::string action = get_string(line, "action");

        if (action == "roll_dice") {
            game.roll_dice();
            break;
        }
        if (action == "play_dev_card") {
            handle_play_dev_card_line(game, line);
            current_game_ = nullptr;
            return;
        }
        // ignore unknown / invalid actions
    }

    // ---- Phase 2: post-roll ----
    while (true) {
        int dice = game.get_last_dice_sum();
        std::string msg =
            "{\"type\":\"action_request\","
            "\"player\":" + quote(my_color_name()) + ","
            "\"phase\":\"post_roll\","
            "\"dice\":" + std::to_string(dice) + ","
            "\"game_state\":" + build_game_state(game) + ","
            "\"legal_actions\":" + build_legal_actions_post_roll(game) +
            "}";
        emit(msg);

        std::string line = read_line();
        std::string action = get_string(line, "action");

        if (action == "end_turn") {
            break;
        }
        if (action == "place_settlement") {
            int vertex = get_int(line, "vertex");
            try { game.place_settlement(vertex, *this); } catch (std::exception& e) { (void)e; }
        } else if (action == "place_road") {
            int edge = get_int(line, "edge");
            try { game.place_road(edge, *this); } catch (std::exception& e) { (void)e; }
        } else if (action == "place_city") {
            int vertex = get_int(line, "vertex");
            try { game.place_city(vertex, *this); } catch (std::exception& e) { (void)e; }
        } else if (action == "buy_dev_card") {
            try { buy_dev_card(game); } catch (std::exception& e) { (void)e; }
        } else if (action == "play_dev_card") {
            handle_play_dev_card_line(game, line);
            current_game_ = nullptr;
            return;  // playing a dev card ends the turn
        } else if (action == "trade") {
            try { handle_trade_line(game, line); } catch (std::exception& e) { (void)e; }
        }
        // unknown actions are silently ignored
    }

    current_game_ = nullptr;
}

// ============================================================
// place_settlement  (called during first_round only in headless flow)
// ============================================================

int HeadlessPlayer::place_settlement(Catan& game, bool first_round) {
    current_game_ = &game;

    std::string phase = first_round ? "first_round_settlement" : "place_settlement";
    auto legal = game.get_legal_settlement_spots(*this, first_round);

    while (true) {
        std::string msg =
            "{\"type\":\"action_request\","
            "\"player\":" + quote(my_color_name()) + ","
            "\"phase\":\"" + phase + "\","
            "\"game_state\":" + build_game_state(game) + ","
            "\"legal_vertices\":" + int_array(legal) +
            "}";
        emit(msg);

        std::string line = read_line();
        std::string action = get_string(line, "action");
        if (action != "place_settlement") continue;

        int vertex = get_int(line, "vertex");
        try {
            game.place_settlement(vertex, *this, first_round);
            current_game_ = nullptr;
            return vertex;
        } catch (std::exception& e) {
            // bad vertex — re-emit and try again
            (void)e;
        }
    }
}

// ============================================================
// place_road  (called during first_round only in headless flow, or via RoadBuildCard)
// ============================================================

void HeadlessPlayer::place_road(Catan& game, bool first_round) {
    // When called from RoadBuildCard, use the pre-parsed pending edges instead
    // of doing a full JSON round-trip.
    if (using_pending_roads_) {
        int edge = (pending_road_call_count_++ == 0) ? pending_edge1_ : pending_edge2_;
        try { game.place_road(edge, *this, first_round); } catch (std::exception& e) { (void)e; }
        return;
    }

    current_game_ = &game;

    // The last_settlement vertex is tracked by Catan::first_round calling
    // place_settlement just before us; we need to pass it in the JSON.
    // We don't have direct access, so we rely on legal edges being correct.
    std::string phase = first_round ? "first_round_road" : "place_road";
    auto legal = game.get_legal_road_spots(*this, first_round);

    while (true) {
        std::string msg =
            "{\"type\":\"action_request\","
            "\"player\":" + quote(my_color_name()) + ","
            "\"phase\":\"" + phase + "\","
            "\"game_state\":" + build_game_state(game) + ","
            "\"legal_edges\":" + int_array(legal) +
            "}";
        emit(msg);

        std::string line = read_line();
        std::string action = get_string(line, "action");
        if (action != "place_road") continue;

        int edge = get_int(line, "edge");
        try {
            game.place_road(edge, *this, first_round);
            current_game_ = nullptr;
            return;
        } catch (std::exception& e) {
            (void)e;
        }
    }
}

// ============================================================
// place_city  (not typically called directly in headless; handled in play_turn)
// ============================================================

void HeadlessPlayer::place_city(Catan& game) {
    current_game_ = &game;
    auto legal = game.get_legal_city_spots(*this);

    while (true) {
        std::string msg =
            "{\"type\":\"action_request\","
            "\"player\":" + quote(my_color_name()) + ","
            "\"phase\":\"place_city\","
            "\"game_state\":" + build_game_state(game) + ","
            "\"legal_vertices\":" + int_array(legal) +
            "}";
        emit(msg);

        std::string line = read_line();
        std::string action = get_string(line, "action");
        if (action != "place_city") continue;

        int vertex = get_int(line, "vertex");
        try {
            game.place_city(vertex, *this);
            current_game_ = nullptr;
            return;
        } catch (std::exception& e) {
            (void)e;
        }
    }
}

// ============================================================
// make_trade  (handled inline in play_turn; this is a fallback)
// ============================================================

void HeadlessPlayer::make_trade(Catan& game) {
    current_game_ = &game;
    // Re-use the same logic as in play_turn for trade actions
    while (true) {
        std::string msg =
            "{\"type\":\"action_request\","
            "\"player\":" + quote(my_color_name()) + ","
            "\"phase\":\"trade\","
            "\"game_state\":" + build_game_state(game) +
            "}";
        emit(msg);

        std::string line = read_line();
        std::string action = get_string(line, "action");
        if (action != "trade") continue;

        try {
            handle_trade_line(game, line);
            current_game_ = nullptr;
            return;
        } catch (std::exception& e) {
            (void)e;
        }
    }
}

// ============================================================
// trade_request  (called on this player by another player's trade)
// ============================================================

bool HeadlessPlayer::trade_request(
    Player& trader,
    const std::vector<std::pair<resource, int>>& offer_res,
    const std::vector<Card*>& offer_dev,
    const std::vector<std::pair<resource, int>>& request_res,
    const std::vector<std::pair<CardType, int>>& /*request_dev*/)
{
    // Build offer_res JSON array
    std::string offer_arr;
    for (auto& rp : offer_res) {
        if (!offer_arr.empty()) offer_arr += ",";
        offer_arr += "{" + kv("resource", resource_name(rp.first)) + "," + kv("amount", rp.second) + "}";
    }

    // Build offer_dev JSON array
    std::string offer_dev_arr;
    for (Card* c : offer_dev) {
        if (!offer_dev_arr.empty()) offer_dev_arr += ",";
        offer_dev_arr += quote(card_type_name(c->type()));
    }

    // Build request_res JSON array
    std::string req_arr;
    for (auto& rp : request_res) {
        if (!req_arr.empty()) req_arr += ",";
        req_arr += "{" + kv("resource", resource_name(rp.first)) + "," + kv("amount", rp.second) + "}";
    }

    std::string msg =
        "{\"type\":\"trade_request\","
        "\"to\":" + quote(my_color_name()) + ","
        "\"from\":" + quote(plain_color_of(&trader)) + ","
        "\"offer_res\":[" + offer_arr + "],"
        "\"offer_dev\":[" + offer_dev_arr + "],"
        "\"request_res\":[" + req_arr + "]"
        "}";
    emit(msg);

    std::string line = read_line();
    return get_bool(line, "accept", false);
}

// ============================================================
// robber  (called when player has >7 resources on a 7 roll)
// ============================================================

void HeadlessPlayer::robber() {
    int total = get_total_resources();
    int must_discard = total / 2;

    std::string msg =
        "{\"type\":\"action_request\","
        "\"player\":" + quote(my_color_name()) + ","
        "\"phase\":\"discard\","
        "\"must_discard\":" + std::to_string(must_discard) + ","
        "\"resources\":{"
            + kv("wood",  get_resource_count(resource::WOOD))  + ","
            + kv("clay",  get_resource_count(resource::CLAY))  + ","
            + kv("sheep", get_resource_count(resource::SHEEP)) + ","
            + kv("wheat", get_resource_count(resource::WHEAT)) + ","
            + kv("stone", get_resource_count(resource::STONE)) +
        "}}";
    emit(msg);

    while (true) {
        std::string line = read_line();
        std::string action = get_string(line, "action");
        if (action != "discard") continue;

        int w  = get_int(line, "wood",  0);
        int cl = get_int(line, "clay",  0);
        int sh = get_int(line, "sheep", 0);
        int wh = get_int(line, "wheat", 0);
        int st = get_int(line, "stone", 0);

        int total_discarded = w + cl + sh + wh + st;
        if (total_discarded != must_discard) {
            // re-prompt
            emit(msg);
            continue;
        }
        // Validate amounts
        if (w  > get_resource_count(resource::WOOD)  ||
            cl > get_resource_count(resource::CLAY)  ||
            sh > get_resource_count(resource::SHEEP) ||
            wh > get_resource_count(resource::WHEAT) ||
            st > get_resource_count(resource::STONE)) {
            emit(msg);
            continue;
        }

        use_resource(resource::WOOD,  w);
        use_resource(resource::CLAY,  cl);
        use_resource(resource::SHEEP, sh);
        use_resource(resource::WHEAT, wh);
        use_resource(resource::STONE, st);
        return;
    }
}

// ============================================================
// play_dev_card  (virtual override; called in original flow when user chooses to play)
// In headless mode, this is only invoked from play_turn which already handles dev cards.
// Kept for completeness.
// ============================================================

void HeadlessPlayer::play_dev_card(Catan& game) {
    current_game_ = &game;

    std::string dev_cards = build_playable_dev_cards(game);
    std::string msg =
        "{\"type\":\"action_request\","
        "\"player\":" + quote(my_color_name()) + ","
        "\"phase\":\"play_dev_card\","
        "\"game_state\":" + build_game_state(game) + ","
        "\"play_dev_card\":[" + dev_cards + "]"
        "}";
    emit(msg);

    std::string line = read_line();
    handle_play_dev_card_line(game, line);
    current_game_ = nullptr;
}

// ============================================================
// buy_dev_card
// ============================================================

void HeadlessPlayer::buy_dev_card(Catan& game) {
    // Delegate to base class which calls game.buy_dev_card(*this)
    Player::buy_dev_card(game);
}

// ============================================================
// handle_play_dev_card_line
// ============================================================

void HeadlessPlayer::handle_play_dev_card_line(Catan& game, const std::string& line) {
    std::string card_type_str = get_string(line, "card_type");

    if (card_type_str == "knight") {
        Card* card = get_dev_card(CardType::KNIGHT);
        if (card) Player::play_dev_card(game, card);
        return;
    }

    if (card_type_str == "road_building") {
        // Store pending edges; place_road override will consume them.
        pending_edge1_ = get_int(line, "edge1", -1);
        pending_edge2_ = get_int(line, "edge2", -1);
        using_pending_roads_ = true;
        pending_road_call_count_ = 0;

        Card* card = get_dev_card(CardType::ROAD_BUILDING);
        if (card) Player::play_dev_card(game, card);  // → RoadBuildCard::use() → place_road() x2

        using_pending_roads_ = false;
        pending_road_call_count_ = 0;
        return;
    }

    if (card_type_str == "monopoly") {
        int res_int = get_int(line, "resource", 0);  // 0-indexed from JSON
        // MonopolyCard::use() expects 1-indexed (1-5) from cin
        std::istringstream injected(std::to_string(res_int + 1) + "\n");
        std::streambuf* prev_cin = std::cin.rdbuf(injected.rdbuf());

        Card* card = get_dev_card(CardType::MONOPOLY);
        if (card) Player::play_dev_card(game, card);  // calls MonopolyCard::use() which reads from injected stream

        std::cin.rdbuf(prev_cin);  // restore cin
        return;
    }

    if (card_type_str == "year_of_plenty") {
        int res1_int = get_int(line, "res1", 0);  // 0-indexed
        int res2_int = get_int(line, "res2", 0);  // 0-indexed
        std::istringstream injected(
            std::to_string(res1_int + 1) + "\n" +
            std::to_string(res2_int + 1) + "\n"
        );
        std::streambuf* prev_cin = std::cin.rdbuf(injected.rdbuf());

        Card* card = get_dev_card(CardType::YEAR_OF_PLENTY);
        if (card) Player::play_dev_card(game, card);

        std::cin.rdbuf(prev_cin);
        return;
    }
}

// ============================================================
// handle_trade_line
// ============================================================

void HeadlessPlayer::handle_trade_line(Catan& game, const std::string& line) {
    // Parse offer and request resource counts from flat JSON
    int offer_wood  = get_int(line, "offer_wood",  0);
    int offer_clay  = get_int(line, "offer_clay",  0);
    int offer_sheep = get_int(line, "offer_sheep", 0);
    int offer_wheat = get_int(line, "offer_wheat", 0);
    int offer_stone = get_int(line, "offer_stone", 0);

    int req_wood    = get_int(line, "request_wood",  0);
    int req_clay    = get_int(line, "request_clay",  0);
    int req_sheep   = get_int(line, "request_sheep", 0);
    int req_wheat   = get_int(line, "request_wheat", 0);
    int req_stone   = get_int(line, "request_stone", 0);

    std::vector<std::pair<resource, int>> offer_res;
    if (offer_wood  > 0) offer_res.push_back({resource::WOOD,  offer_wood});
    if (offer_clay  > 0) offer_res.push_back({resource::CLAY,  offer_clay});
    if (offer_sheep > 0) offer_res.push_back({resource::SHEEP, offer_sheep});
    if (offer_wheat > 0) offer_res.push_back({resource::WHEAT, offer_wheat});
    if (offer_stone > 0) offer_res.push_back({resource::STONE, offer_stone});

    std::vector<std::pair<resource, int>> request_res;
    if (req_wood  > 0) request_res.push_back({resource::WOOD,  req_wood});
    if (req_clay  > 0) request_res.push_back({resource::CLAY,  req_clay});
    if (req_sheep > 0) request_res.push_back({resource::SHEEP, req_sheep});
    if (req_wheat > 0) request_res.push_back({resource::WHEAT, req_wheat});
    if (req_stone > 0) request_res.push_back({resource::STONE, req_stone});

    // No dev card trading via this path for simplicity
    std::vector<Card*> offer_dev;
    std::vector<std::pair<CardType, int>> request_dev;

    game.make_trade_offer(*this, offer_res, offer_dev, request_res, request_dev);
}
