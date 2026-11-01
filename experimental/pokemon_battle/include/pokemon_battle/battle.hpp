#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace pokemon_battle {

// CP1 balance is authored for this experiment, not a reproduction of Pokemon's formulas.
enum class Phase { Idle, Intro, PlayerInput, PlayerAction, EnemyAction, Result, Finished };
enum class Outcome { None, Victory, Defeat, Escaped, Cancelled };
enum class Command { Sword, Bow, Bomb, Guard, Flee };
enum class Effect { Damage, LowerAttack, LowerDefense, Guard };
enum class Side { Player, Enemy, System };

struct Fighter {
    std::string name;
    int hp = 24;
    int max_hp = 24;
    int attack = 8;
    int defense = 4;
};

struct Move {
    std::uint16_t id = 0;
    std::string name;
    Effect effect = Effect::Damage;
    int power = 6;
    int pp = 20;
    int weight = 1;
};

struct Encounter {
    Fighter player;
    Fighter enemy;
    std::vector<Move> enemy_moves;
    int arrows = 5;
    int bombs = 2;
    bool allow_flee = true;
    std::uint32_t seed = 1;
};

struct Event {
    // Never reused by this Battle, including across restarts. An old animation/UI
    // completion cannot advance a new encounter.
    std::uint64_t serial = 0;
    Side side = Side::System;
    std::uint16_t move_id = 0;
    int damage = 0;
    std::string text;
};

struct Summary {
    Outcome outcome = Outcome::None;
    int player_hp = 0;
    int enemy_hp = 0;
    int arrows = 0;
    int bombs = 0;
    int rounds = 0;
};

class Battle {
public:
    bool start(const Encounter& encounter);
    bool can_submit(Command command) const;
    bool submit(Command command);
    bool acknowledge(std::uint64_t event_serial);
    void cancel();

    Phase phase() const { return phase_; }
    Outcome outcome() const { return outcome_; }
    const Event* event() const { return event_ ? &*event_ : nullptr; }
    const Encounter& state() const { return state_; }
    int attack_stage() const { return attack_stage_; }
    int defense_stage() const { return defense_stage_; }
    bool active() const { return phase_ != Phase::Idle && phase_ != Phase::Finished; }
    Summary summary() const;

private:
    std::uint32_t random_below(std::uint32_t bound);
    void emit(Side side, std::uint16_t move, int damage, std::string text);
    void enemy_turn();
    void show_result();
    int hit(int attack, int defense, int power, bool guarded) const;

    Encounter state_;
    Phase phase_ = Phase::Idle;
    Outcome outcome_ = Outcome::None;
    std::optional<Event> event_;
    std::uint64_t next_event_ = 1;
    std::uint32_t rng_ = 1;
    int attack_stage_ = 0;
    int defense_stage_ = 0;
    int rounds_ = 0;
    bool player_guard_ = false;
    bool enemy_guard_ = false;
};

Encounter pikachu_demo(std::uint32_t seed = 1);
const char* outcome_name(Outcome outcome);

} // namespace pokemon_battle
