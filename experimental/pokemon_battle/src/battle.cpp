#include "pokemon_battle/battle.hpp"

#include <algorithm>
#include <limits>
#include <set>
#include <utility>

namespace pokemon_battle {
namespace {
bool valid_fighter(const Fighter& fighter) {
    return !fighter.name.empty() && fighter.name.size() <= 64 &&
        fighter.hp > 0 && fighter.hp <= fighter.max_hp && fighter.max_hp <= 10000 &&
        fighter.attack > 0 && fighter.attack <= 10000 &&
        fighter.defense > 0 && fighter.defense <= 10000;
}

bool valid_encounter(const Encounter& encounter) {
    if (!valid_fighter(encounter.player) || !valid_fighter(encounter.enemy) ||
        encounter.enemy_moves.empty() || encounter.enemy_moves.size() > 64 ||
        encounter.arrows < 0 || encounter.arrows > 999 ||
        encounter.bombs < 0 || encounter.bombs > 999) return false;
    std::set<std::uint16_t> ids;
    for (const auto& move : encounter.enemy_moves) {
        if (move.id == 0 || move.id == 65535 || !ids.insert(move.id).second ||
            move.name.empty() || move.name.size() > 64 || move.power < 0 || move.power > 100 ||
            move.pp < 0 || move.pp > 99 || move.weight < 1 || move.weight > 100) return false;
        switch (move.effect) {
        case Effect::Damage: if (move.power == 0) return false; break;
        case Effect::LowerAttack:
        case Effect::LowerDefense:
        case Effect::Guard: break;
        default: return false;
        }
    }
    return true;
}

int with_stage(int stat, int stage) {
    return std::max(1, stat * 2 / (2 - stage)); // CP1 stages are in [-6, 0].
}
} // namespace

bool Battle::start(const Encounter& encounter) {
    if (active() || !valid_encounter(encounter) ||
        next_event_ == std::numeric_limits<std::uint64_t>::max()) return false;
    state_ = encounter;
    phase_ = Phase::Intro;
    outcome_ = Outcome::None;
    rng_ = encounter.seed == 0 ? 0x9e3779b9u : encounter.seed;
    attack_stage_ = defense_stage_ = rounds_ = 0;
    player_guard_ = enemy_guard_ = false;
    emit(Side::System, 0, 0, "A wild " + state_.enemy.name + " appeared!");
    return true;
}

bool Battle::can_submit(Command command) const {
    if (phase_ != Phase::PlayerInput) return false;
    switch (command) {
    case Command::Sword:
    case Command::Guard: return true;
    case Command::Bow: return state_.arrows > 0;
    case Command::Bomb: return state_.bombs > 0;
    case Command::Flee: return state_.allow_flee;
    default: return false;
    }
}

int Battle::hit(int attack, int defense, int power, bool guarded) const {
    const int raw = std::max(1, (attack * power + 4) / 8 - defense / 2);
    return guarded ? std::max(1, (raw + 1) / 2) : raw;
}

bool Battle::submit(Command command) {
    if (!can_submit(command)) return false;
    phase_ = Phase::PlayerAction;
    if (command == Command::Flee) {
        outcome_ = Outcome::Escaped;
        emit(Side::Player, 0, 0, state_.player.name + " escaped.");
        return true;
    }
    if (command == Command::Guard) {
        player_guard_ = true;
        emit(Side::Player, 0, 0, state_.player.name + " raised the shield.");
        return true;
    }
    const char* name = "Sword";
    int power = 8;
    if (command == Command::Bow) { --state_.arrows; power = 10; name = "Bow"; }
    if (command == Command::Bomb) { --state_.bombs; power = 16; name = "Bomb"; }
    const int damage = std::min(state_.enemy.hp,
        hit(with_stage(state_.player.attack, attack_stage_), state_.enemy.defense, power, enemy_guard_));
    state_.enemy.hp -= damage;
    enemy_guard_ = false;
    emit(Side::Player, 0, damage, state_.player.name + " used " + name +
        ". " + std::to_string(damage) + " damage.");
    if (state_.enemy.hp == 0) outcome_ = Outcome::Victory;
    return true;
}

void Battle::emit(Side side, std::uint16_t move, int damage, std::string text) {
    event_ = Event{next_event_++, side, move, damage, std::move(text)};
}

std::uint32_t Battle::random_below(std::uint32_t bound) {
    // Rejection avoids a modulo bias; no dependency on global game randomness.
    // Xorshift32 visits 1..UINT32_MAX. Translate to a zero-based range first.
    const auto maximum = std::numeric_limits<std::uint32_t>::max();
    const auto limit = maximum - maximum % bound;
    for (;;) {
        rng_ ^= rng_ << 13;
        rng_ ^= rng_ >> 17;
        rng_ ^= rng_ << 5;
        const auto value = rng_ - 1;
        if (value < limit) return value % bound;
    }
}

void Battle::enemy_turn() {
    phase_ = Phase::EnemyAction;
    ++rounds_;
    enemy_guard_ = false; // Guard lasts through the next player action, not indefinitely.
    std::uint32_t total = 0;
    for (const auto& move : state_.enemy_moves) if (move.pp > 0) total += move.weight;
    Move struggle{65535, "Struggle", Effect::Damage, 6, 1, 1};
    Move* selected = &struggle;
    if (total > 0) {
        auto roll = random_below(total);
        for (auto& move : state_.enemy_moves) {
            if (move.pp == 0) continue;
            if (roll < static_cast<std::uint32_t>(move.weight)) { selected = &move; break; }
            roll -= move.weight;
        }
        --selected->pp;
    }
    int damage = 0;
    std::string detail;
    switch (selected->effect) {
    case Effect::Damage:
        damage = std::min(state_.player.hp, hit(state_.enemy.attack,
            with_stage(state_.player.defense, defense_stage_), selected->power, player_guard_));
        state_.player.hp -= damage;
        detail = std::to_string(damage) + " damage.";
        if (state_.player.hp == 0) outcome_ = Outcome::Defeat;
        break;
    case Effect::LowerAttack:
        attack_stage_ = std::max(-6, attack_stage_ - 1);
        detail = state_.player.name + "'s attack is reduced.";
        break;
    case Effect::LowerDefense:
        defense_stage_ = std::max(-6, defense_stage_ - 1);
        detail = state_.player.name + "'s defense is reduced.";
        break;
    case Effect::Guard:
        enemy_guard_ = true;
        detail = "The next hit will deal less damage.";
        break;
    }
    player_guard_ = false;
    emit(Side::Enemy, selected->id, damage, state_.enemy.name + " used " + selected->name + ". " + detail);
}

void Battle::show_result() {
    phase_ = Phase::Result;
    emit(Side::System, 0, 0, outcome_name(outcome_));
}

bool Battle::acknowledge(std::uint64_t serial) {
    if (!event_ || serial != event_->serial) return false;
    switch (phase_) {
    case Phase::Intro:
        phase_ = Phase::PlayerInput;
        event_.reset();
        return true;
    case Phase::PlayerAction:
        if (outcome_ != Outcome::None) show_result(); else enemy_turn();
        return true;
    case Phase::EnemyAction:
        if (outcome_ != Outcome::None) show_result();
        else { phase_ = Phase::PlayerInput; event_.reset(); }
        return true;
    case Phase::Result:
        phase_ = Phase::Finished;
        event_.reset();
        return true;
    default: return false;
    }
}

void Battle::cancel() {
    if (phase_ == Phase::Idle || phase_ == Phase::Finished) return;
    outcome_ = Outcome::Cancelled;
    phase_ = Phase::Finished;
    event_.reset();
}

Summary Battle::summary() const {
    return {outcome_, state_.player.hp, state_.enemy.hp, state_.arrows, state_.bombs, rounds_};
}

Encounter pikachu_demo(std::uint32_t seed) {
    return {{"Link", 24, 24, 8, 4}, {"Pikachu", 26, 26, 6, 3}, {
        {84, "ThunderShock", Effect::Damage, 7, 20, 1},
        {98, "Quick Attack", Effect::Damage, 6, 20, 1},
        {45, "Growl", Effect::LowerAttack, 0, 5, 1},
        {39, "Tail Whip", Effect::LowerDefense, 0, 5, 1},
    }, 5, 2, true, seed};
}

const char* outcome_name(Outcome outcome) {
    switch (outcome) {
    case Outcome::Victory: return "Victory!";
    case Outcome::Defeat: return "Link was defeated.";
    case Outcome::Escaped: return "Returned from the encounter.";
    case Outcome::Cancelled: return "Encounter cancelled.";
    default: return "Encounter in progress.";
    }
}
} // namespace pokemon_battle
