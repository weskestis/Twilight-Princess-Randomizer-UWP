#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace pokemon_battle::field {
constexpr std::size_t party_capacity = 6;
constexpr std::size_t move_capacity = 4;
constexpr std::uint16_t ball_limit = 99;
enum class Move : std::uint16_t {
#include "move_enum.inc"
};
enum class Visual { Impact,Slash,Wind,Vine,Needles,Bite,Tail,Sound,Flame,Water,Ice,Leaf,Solar,
                    Poison,Dragon,Electric,Psychic,Hypnosis,Dash,Smoke,Confuse,Lick,Stars,Dark,Spin,Shadow,Bubble };
enum class Status { None,AttackDown,DefenseDown,Poison,Paralysis,Sleep,AccuracyDown,Confusion };
struct MoveInfo {
    Move move; const char* name; std::uint8_t pp; std::uint8_t clip; bool damaging;
    std::uint8_t type,power,accuracy;
    Visual visual;
    Status status;
};
const MoveInfo* move_info(Move move);
struct Creature {
    std::uint64_t id=0;
    std::uint16_t species=25;
    std::uint8_t level=12;
    std::uint16_t hp=32;
    std::uint16_t max_hp=32;
    std::array<Move,move_capacity> moves{Move::ThunderShock,Move::QuickAttack,Move::Growl,Move::TailWhip};
    std::array<std::uint8_t,move_capacity> pp{30,30,40,30};
    bool operator==(const Creature&) const = default;
};
struct Party {
    std::uint64_t next_id=1;
    std::uint32_t random=0x6d2b79f5u;
    std::uint16_t balls=24;
    std::uint8_t count=0;
    std::uint8_t selected=0;
    std::array<Creature,party_capacity> members{};
    bool operator==(const Party&) const = default;
};
bool valid_creature(const Creature& creature, bool captured);
bool valid_party(const Party& party);
std::uint32_t random_next(std::uint32_t& state);
Move choose_move(Creature& creature, std::uint32_t& random);
int capture_chance(const Creature& creature);
void heal(Party& party);
std::vector<std::uint8_t> encode_party(const Party& party);
bool decode_party(std::span<const std::uint8_t> bytes, Party& out, std::string& error);

// The host stores a copy in the active save slot. A failed write must leave its
// previous blob intact. The engine flushes that blob with the next Zelda save.
class PartyStorage {
public:
    virtual ~PartyStorage()=default;
    virtual bool write(std::span<const std::uint8_t> bytes)=0;
};
enum class CaptureResult { Caught, Escaped, Full, NoBalls, Invalid, SaveFailed };
class PartySession {
public:
    explicit PartySession(PartyStorage& storage):storage_(storage) {}
    void activate(const Party& party);
    void deactivate();
    const Party& party() const { return party_; }
    bool active() const { return active_; }
    std::uint64_t epoch() const { return epoch_; }
    bool pending() const { return pending_target_!=0; }
    bool begin_throw(std::uint64_t target);
    CaptureResult finish_throw(std::uint64_t target, std::uint64_t epoch, const Creature& wild);
    void cancel_throw(std::uint64_t target=0);
    bool update_member(const Creature& creature, std::uint64_t epoch);
    bool select_next(int direction);
    bool choose_starter(std::uint16_t species);
    bool rest();
    bool restock();
private:
    bool commit(const Party& candidate);
    PartyStorage& storage_;
    Party party_{};
    bool active_=false;
    std::uint64_t epoch_=0;
    std::uint64_t pending_target_=0;
};
} // namespace pokemon_battle::field
