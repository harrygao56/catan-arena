// @author: shay.gali@msmail.ariel.ac.il
#include "YearOfPlentyCard.hpp"

#include "../player/Player.hpp"

std::string YearOfPlentyCard::get_description() const {
    return "Year of Plenty: Gain any two resources from the bank.";
}

std::string YearOfPlentyCard::emoji() const {
    return "🌟";
}

CardType YearOfPlentyCard::type() const {
    return CardType::YEAR_OF_PLENTY;
}

Card* YearOfPlentyCard::clone() const {
    return new YearOfPlentyCard(*this);
}

void YearOfPlentyCard::use(Catan& game, Player& player) {
    auto [res1, res2] = player.choose_year_of_plenty_resources(game);
    player.add_resource(res1, 1);
    player.add_resource(res2, 1);
}