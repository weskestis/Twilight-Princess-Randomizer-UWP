#include "pokemon_battle/session.hpp"

namespace pokemon_battle {
bool Session::begin(const Encounter& encounter, std::uint64_t now_ms) {
    if (lease_ || !battle_.start(encounter)) return false;
    lease_ = host_.acquire();
    settled_ = settling_ = false;
    if (!lease_) { battle_.cancel(); return false; }
    if (lease_->encounter_id == 0 || !host_.valid(*lease_)) {
        cancel();
        return false;
    }
    observed_event_ = 0;
    observe(now_ms);
    return true;
}

void Session::observe(std::uint64_t now_ms) {
    const auto* event = battle_.event();
    const auto serial = event ? event->serial : 0;
    if (serial != observed_event_) {
        observed_event_ = serial;
        event_since_ = now_ms;
    }
}

bool Session::submit(Command command, std::uint64_t now_ms) {
    tick(now_ms);
    if (!lease_ || settling_) return false;
    const bool result = battle_.submit(command);
    observe(now_ms);
    return result;
}

bool Session::acknowledge(std::uint64_t serial, std::uint64_t now_ms) {
    tick(now_ms);
    if (!lease_ || settling_) return false;
    const bool result = battle_.acknowledge(serial);
    observe(now_ms);
    tick(now_ms);
    return result;
}

void Session::tick(std::uint64_t now_ms) {
    if (!lease_) return;
    if (!host_.valid(*lease_)) { cancel(); return; }
    observe(now_ms);
    // Future world presentation callbacks must acknowledge promptly. This is not
    // used by the manual Battle Lab, where people can read at their own pace.
    const auto phase = battle_.phase();
    const bool presenting = phase == Phase::Intro || phase == Phase::PlayerAction || phase == Phase::EnemyAction;
    if (presenting && now_ms >= event_since_ && now_ms - event_since_ >= 15000) {
        cancel();
        return;
    }
    if (battle_.phase() != Phase::Finished) return;
    if (battle_.outcome() == Outcome::Cancelled) { release(); return; }
    if (!settling_) { settling_ = true; settlement_since_ = now_ms; }
    if (now_ms >= settlement_since_ && now_ms - settlement_since_ >= 5000) {
        release(); // No new reward identifier or automatic restart after a timeout.
        return;
    }
    const auto result = host_.settle(*lease_, battle_.summary());
    if (result == Settlement::Applied) { settled_ = true; release(); }
    else if (result == Settlement::Failed) release();
}

void Session::release() {
    if (!lease_) return;
    const Lease old = *lease_;
    lease_.reset(); // Repeated cancellation/shutdown cannot release twice.
    settling_ = false;
    host_.release(old);
}

void Session::cancel() {
    battle_.cancel();
    release();
}
} // namespace pokemon_battle
