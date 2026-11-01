#pragma once

#include "pokemon_battle/battle.hpp"

namespace pokemon_battle {

struct Lease {
    std::uint64_t encounter_id = 0;
    std::uint64_t scene_generation = 0;
};

enum class Settlement { Pending, Applied, Failed };

// Production TP binding is deliberately not supplied in CP1. A binding must use
// generation-checked actor IDs, preserve previous pause/input/camera ownership,
// and atomically settle rewards + room flags under encounter_id. No raw actors.
class Host {
public:
    virtual ~Host() = default;
    // Failure must leave the world unchanged; successful acquisition returns a
    // unique, nonzero encounter_id and owns the world lock until release().
    virtual std::optional<Lease> acquire() = 0;
    virtual bool valid(const Lease&) = 0;
    // May be retried with the SAME id. Must be idempotent. Only Victory can award
    // a randomized reward/room clear. Cancelled encounters are never settled.
    virtual Settlement settle(const Lease&, const Summary&) = 0;
    virtual void release(const Lease&) = 0;
};

class Session {
public:
    explicit Session(Host& host) : host_(host) {}
    ~Session() { cancel(); }
    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;

    bool begin(const Encounter& encounter, std::uint64_t now_ms);
    bool submit(Command command, std::uint64_t now_ms);
    bool acknowledge(std::uint64_t serial, std::uint64_t now_ms);
    void tick(std::uint64_t now_ms);
    void cancel();

    const Battle& battle() const { return battle_; }
    bool owns_world() const { return lease_.has_value(); }
    bool settled() const { return settled_; }

private:
    void release();
    void observe(std::uint64_t now_ms);
    Host& host_;
    Battle battle_;
    std::optional<Lease> lease_;
    std::uint64_t event_since_ = 0;
    std::uint64_t observed_event_ = 0;
    std::uint64_t settlement_since_ = 0;
    bool settling_ = false;
    bool settled_ = false;
};

} // namespace pokemon_battle
