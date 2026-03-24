// @author: shay.gali@msmail.ariel.ac.il
#include "MonopolyCard.hpp"

#include "../Catan.hpp"
#include "../player/Player.hpp"

std::string MonopolyCard::get_description() const {
    return "Monopoly: When you play this card, you choose a resource type. All other players must give you all of their resources of that type.";
}

std::string MonopolyCard::emoji() const {
    return "🏦";
}

CardType MonopolyCard::type() const {
    return CardType::MONOPOLY;
}

Card* MonopolyCard::clone() const {
    return new MonopolyCard(*this);
}

void MonopolyCard::use(Catan& game, Player& player) {
    cout << "Player " << player.get_color() << " is playing Monopoly Card\n";

    resource res = player.choose_monopoly_resource(game);

    for (int i = 0; i < NUM_PLAYERS; i++) {
        if (game.get_players()[i] == &player) {
            continue;
        }

        // Take all resources of type res from other_player and give them to player
        Player* other_player = game.get_players()[i];
        int num_resources = other_player->get_resource_count(res);
        other_player->use_resource(res, num_resources);
        player.add_resource(res, num_resources);
    }
}