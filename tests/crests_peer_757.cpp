#include "version.hpp"
#include "net/messages.hpp"
#include "net/protocol.hpp"
#include "net/reliable.hpp"

#include <array>
#include <cstddef>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

// Only the game actor, logger and network service boundaries are substituted.
// Both pickup endpoints below are compiled from the staged features.cpp.
using f32 = float;
using s16 = int16_t;
using u32 = uint32_t;
using fpc_ProcID = uint32_t;
constexpr fpc_ProcID fpcM_ERROR_PROCESS_ID_e = ~0u;
enum { kProcItem = 1, kProcDemoItem, fopAcStts_NODRAW_e };
enum { dItemNo_KAKERA_HEART_e = 0x21, dItemNo_UTAWA_HEART_e = 0x22 };

struct cXyz {
    float x{}, y{}, z{};
    void set(float a, float b, float c) { x = a; y = b; z = c; }
};
struct fopAc_ac_c {
    struct { cXyz pos; } home;
    int kind = kProcItem;
    int room = 0;
    fpc_ProcID id = 1;
    uint32_t params = 0;
    bool hidden = false;
};
struct daItemBase_c : fopAc_ac_c {
    uint8_t item = 0;
    uint8_t getItemNo() const { return item; }
    struct {
        bool collidable = true;
        void OffCoSetBit() { collidable = false; }
    } mCcCyl;
};
struct daAlink_c {};

daAlink_c linkActor;
daItemBase_c collected;
std::array<daItemBase_c, 5> roomItems;
std::vector<uint8_t> delivered;
unsigned announcements = 0;

daAlink_c* daAlink_getAlinkActorClass() { return &linkActor; }
bool coop_net_connected() { return true; }
fopAc_ac_c* fopAcM_getItemEventPartner(daAlink_c*) { return &collected; }
s16 fopAcM_GetName(fopAc_ac_c* actor) { return static_cast<s16>(actor->kind); }
u32 fopAcM_GetParam(fopAc_ac_c* actor) { return actor->params; }
fpc_ProcID fopAcM_GetID(fopAc_ac_c* actor) { return actor->id; }
int fopAcM_GetRoomNo(fopAc_ac_c* actor) { return actor->room; }
void fopAcM_OnStatus(fopAc_ac_c* actor, int flag) {
    assert(flag == fopAcStts_NODRAW_e);
    actor->hidden = true;
}
void fopAcM_Search(void* (*callback)(void*, void*), void* arg) {
    for (auto& item : roomItems) callback(static_cast<fopAc_ac_c*>(&item), arg);
}
namespace coop_log {
template <typename... Args> void info(const char*, Args...) {}
}

void coop_net_send(uint8_t type, const void* data, size_t size) {
    assert(type == kMsgItemTaken && size == sizeof(MsgItemTaken));
    ++announcements;
    rudp::Channel sender, receiver;
    sender.start(17, 0);
    receiver.start(17, 0);
    assert(sender.write(data, size));
    delivered.clear();
    sender.flush(1, [&](const uint8_t* bytes, size_t length) {
        rudp::Header header;
        const uint8_t* payload;
        assert(rudp::Channel::parse(bytes, length, header, payload));
        receiver.on_packet(header, payload, 1, delivered);
    });
}

#include "crests_pickup.inc"

struct Peer {
    bool present = false;
    std::string name;
};
std::array<Peer, kCoopMaxPlayers> peers;
unsigned disconnects = 0, joins = 0;
std::string notice;
Peer& peer_slot(uint8_t index) { return peers.at(index); }
std::string wire_name(const char* name) { return std::string(name); }
void remember_name(uint8_t, const std::string&) {}
void toast(const char*, const std::string& text, int) { notice = text; }
void coop_net_disconnect() { ++disconnects; }
void announce_peer_once(uint8_t) { ++joins; }

#include "crests_hello.inc"

void check_handshake() {
    MsgHello hello{};
    std::memcpy(hello.name, "Peer", 5);
    hello.version = coop_version_number("1.10.0");
    const auto* bytes = reinterpret_cast<const uint8_t*>(&hello);
    on_hello(bytes, sizeof(hello) - 1, 1);
    assert(joins == 0 && disconnects == 0 && !peers[1].present);
    on_hello(bytes, sizeof(hello), 1);
    assert(joins == 0 && disconnects == 1 && notice.find("1.10.0") != notice.npos);
    hello.version = 11001;
    on_hello(bytes, sizeof(hello), 2);
    assert(joins == 1 && disconnects == 1 && peers[2].name == "Peer");
}

void check_pickup(uint8_t itemNo, bool matchesDistant) {
    constexpr uint8_t itemBit = 0x3d;
    ++collected.id;
    collected.item = itemNo;
    collected.params = itemBit << 8;
    collected.home.pos.set(3, 4, 5);
    roomItems.fill(collected);
    for (auto& item : roomItems) item.home.pos.x += 1200;
    roomItems[1].params ^= 1u << 8;   // same item type, different room flag
    roomItems[2].room = -1;           // item outside a loaded room
    roomItems[3].home = collected.home;
    roomItems[3].params ^= 1u << 8;   // nearby position still matches
    roomItems[4].item ^= 1;
    roomItems[4].home = collected.home;

    unsigned before = announcements;
    announce_item_taken();
    announce_item_taken();
    assert(announcements == before + 1 && delivered.size() == sizeof(MsgItemTaken));
    MsgItemTaken received;
    std::memcpy(&received, delivered.data(), sizeof(received));
    assert(received.itemBit == itemBit && received.itemNo == itemNo);
    apply_item_taken(received);
    const bool expected[] = {matchesDistant, false, false, true, false};
    for (size_t i = 0; i < roomItems.size(); ++i) {
        assert(roomItems[i].hidden == expected[i]);
        assert(roomItems[i].mCcCyl.collidable == !expected[i]);
    }
    collected.params = 0xff00;
    ++collected.id;
    announce_item_taken();
    assert(announcements == before + 1);
}

int main() {
    static_assert(kCoopWireVersion == 11001);
    static_assert(offsetof(MsgItemTaken, itemBit) == 13);
    static_assert(offsetof(PlayerSnapshot, gear) == 979 && sizeof(PlayerSnapshot) == 993);
    PlayerSnapshot snapshot{};
    snapshot.gear = {kGearLantern | kGearDoubleClawshot, 60, 48, 2,
                     {0x61, 0xff, 0x63, 0x64}, {0x70, 0x71, 0xff}, 0x42, 0x43};
    const uint8_t expectedGear[] = {0x02, 0x02, 60, 48, 2, 0x61, 0xff, 0x63, 0x64,
                                    0x70, 0x71, 0xff, 0x42, 0x43};
    assert(std::memcmp(&snapshot.gear, expectedGear, sizeof(expectedGear)) == 0);
    check_handshake();
    check_pickup(dItemNo_KAKERA_HEART_e, true);
    check_pickup(dItemNo_UTAWA_HEART_e, true);
    check_pickup(0x01, false);
    std::puts("PASS Crests 1.10.1 handshake, gear wire layout and peer pickup matching");
}
