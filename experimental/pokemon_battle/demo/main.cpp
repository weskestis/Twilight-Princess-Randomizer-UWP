#include "pokemon_battle/battle.hpp"

#include <iostream>
#include <string>

int main(int argc, char** argv) {
    using namespace pokemon_battle;
    const bool autoplay = argc == 2 && std::string(argv[1]) == "--autoplay";
    Battle battle;
    if (!battle.start(pikachu_demo(749))) return 1;
    std::cout << "Dusklight Pokemon Battle CP1 -- simulation, no game save changes\n";
    while (battle.phase() != Phase::Finished) {
        if (const auto* event = battle.event()) {
            const auto serial = event->serial;
            std::cout << event->text << '\n';
            battle.acknowledge(serial);
            continue;
        }
        std::cout << "Link " << battle.state().player.hp << "/" << battle.state().player.max_hp
                  << " | Pikachu " << battle.state().enemy.hp << "/" << battle.state().enemy.max_hp << '\n';
        Command command = Command::Sword;
        if (autoplay) {
            command = battle.can_submit(Command::Bomb) ? Command::Bomb : Command::Sword;
        } else {
            std::cout << "1 Sword  2 Bow  3 Bomb  4 Guard  5 Flee  q Quit\n> ";
            std::string input;
            if (!std::getline(std::cin, input) || input == "q") { battle.cancel(); break; }
            if (input == "2") command = Command::Bow;
            else if (input == "3") command = Command::Bomb;
            else if (input == "4") command = Command::Guard;
            else if (input == "5") command = Command::Flee;
            else if (input != "1") continue;
        }
        if (!battle.submit(command)) std::cout << "That action is unavailable.\n";
    }
    std::cout << outcome_name(battle.outcome()) << " No real item was awarded.\n";
    return 0;
}
