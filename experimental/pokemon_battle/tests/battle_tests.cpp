#include "pokemon_battle/session.hpp"

#include <functional>
#include <iostream>
#include <set>
#include <stdexcept>
#include <string>

using namespace pokemon_battle;
#define CHECK(condition) do { if (!(condition)) throw std::runtime_error(\
    std::string(__FILE__) + ":" + std::to_string(__LINE__) + " " #condition); } while (false)

void ack(Battle& battle) {
    CHECK(battle.event());
    CHECK(battle.acknowledge(battle.event()->serial));
}

Encounter damage_only() {
    auto e = pikachu_demo();
    e.enemy_moves = {{84, "ThunderShock", Effect::Damage, 8, 99, 1}};
    e.enemy.hp = e.enemy.max_hp = 100;
    return e;
}

struct TestHost final : Host {
    bool available = true;
    bool scene_valid = true;
    bool pending = false;
    bool fail = false;
    bool apply_before_ack = false;
    int acquires = 0;
    int releases = 0;
    int calls = 0;
    int rewards = 0;
    std::uint64_t next_id = 1;
    std::set<std::uint64_t> transactions;
    std::set<std::uint64_t> released;
    Summary last{};
    std::optional<Lease> acquire() override {
        ++acquires;
        if (!available) return std::nullopt;
        return Lease{next_id++, 7};
    }
    bool valid(const Lease&) override { return scene_valid; }
    Settlement settle(const Lease& lease, const Summary& summary) override {
        ++calls;
        CHECK(summary.outcome != Outcome::None && summary.outcome != Outcome::Cancelled);
        last = summary;
        if (fail) return Settlement::Failed;
        if (!pending || apply_before_ack) {
            if (transactions.insert(lease.encounter_id).second && summary.outcome == Outcome::Victory) ++rewards;
        }
        return pending ? Settlement::Pending : Settlement::Applied;
    }
    void release(const Lease& lease) override {
        CHECK(released.insert(lease.encounter_id).second);
        ++releases;
    }
};

void win(Session& session, std::uint64_t time = 0) {
    int budget = 1000;
    while (session.battle().phase() != Phase::Finished && --budget) {
        if (const auto* event = session.battle().event()) CHECK(session.acknowledge(event->serial, time));
        else CHECK(session.submit(Command::Sword, time));
    }
    CHECK(budget > 0);
    CHECK(session.battle().outcome() == Outcome::Victory);
}

int main() {
    int passed = 0;
    auto test = [&](const char* name, const std::function<void()>& body) {
        body(); ++passed; std::cout << "PASS " << name << '\n';
    };
    try {
        test("turns require the exact presentation acknowledgement", [] {
            Battle b; CHECK(b.start(damage_only()));
            CHECK(!b.submit(Command::Sword));
            const auto intro = b.event()->serial;
            CHECK(!b.acknowledge(intro + 1)); ack(b);
            CHECK(b.submit(Command::Sword));
            const auto player = b.event()->serial;
            const int hp = b.state().enemy.hp;
            CHECK(!b.submit(Command::Bomb)); CHECK(b.state().enemy.hp == hp);
            CHECK(!b.acknowledge(intro)); ack(b);
            CHECK(b.phase() == Phase::EnemyAction);
            const int link_hp = b.state().player.hp;
            CHECK(!b.acknowledge(player)); CHECK(b.state().player.hp == link_hp);
            ack(b); CHECK(b.phase() == Phase::PlayerInput);
        });
        test("stale callbacks do not advance a restarted encounter", [] {
            Battle b; CHECK(b.start(pikachu_demo())); const auto old = b.event()->serial;
            b.cancel(); CHECK(b.start(pikachu_demo()));
            CHECK(b.event()->serial > old); CHECK(!b.acknowledge(old));
            CHECK(b.phase() == Phase::Intro);
        });
        test("invalid input cannot consume ammunition or a turn", [] {
            auto e = damage_only(); e.arrows = e.bombs = 0; e.allow_flee = false;
            Battle b; CHECK(b.start(e)); ack(b);
            CHECK(!b.submit(Command::Bow)); CHECK(!b.submit(Command::Bomb));
            CHECK(!b.submit(Command::Flee)); CHECK(!b.submit(static_cast<Command>(999)));
            CHECK(b.phase() == Phase::PlayerInput); CHECK(b.state().enemy.hp == e.enemy.hp);
        });
        test("ammunition consumed once per accepted action", [] {
            Battle b; CHECK(b.start(damage_only())); ack(b);
            CHECK(b.submit(Command::Bow)); CHECK(b.state().arrows == 4);
            CHECK(!b.submit(Command::Bow)); CHECK(b.state().arrows == 4);
            ack(b); ack(b); CHECK(b.submit(Command::Bomb)); CHECK(b.state().bombs == 1);
        });
        test("guard reduces one enemy attack and expires", [] {
            Battle plain, guarded;
            CHECK(plain.start(damage_only())); CHECK(guarded.start(damage_only()));
            ack(plain); ack(guarded);
            CHECK(plain.submit(Command::Sword)); CHECK(guarded.submit(Command::Guard));
            ack(plain); ack(guarded);
            const int normal = plain.event()->damage;
            CHECK(guarded.event()->damage == (normal + 1) / 2);
            ack(guarded); CHECK(guarded.submit(Command::Sword)); ack(guarded);
            CHECK(guarded.event()->damage == normal);
        });
        test("enemy protection expires through the next player action", [] {
            auto e = damage_only(); e.enemy_moves = {{182, "Protect", Effect::Guard, 0, 1, 1}};
            Battle b; CHECK(b.start(e)); ack(b); CHECK(b.submit(Command::Guard)); ack(b); ack(b);
            CHECK(b.submit(Command::Sword)); const int guarded = b.event()->damage;
            ack(b); ack(b); CHECK(b.submit(Command::Sword)); CHECK(b.event()->damage > guarded);
        });
        test("lowered attack has a mechanical effect and a lower bound", [] {
            auto e = damage_only(); e.enemy_moves = {{45, "Growl", Effect::LowerAttack, 0, 99, 1}};
            Battle b; CHECK(b.start(e)); ack(b); CHECK(b.submit(Command::Sword));
            const int first = b.event()->damage; ack(b); ack(b);
            CHECK(b.submit(Command::Sword)); CHECK(b.event()->damage < first); ack(b); ack(b);
            for (int i = 0; i < 12; ++i) { CHECK(b.submit(Command::Guard)); ack(b); ack(b); }
            CHECK(b.attack_stage() == -6);
        });
        test("lowered defense increases later damage", [] {
            auto e = damage_only(); e.player.defense = 8;
            e.enemy_moves = {{39, "Tail Whip", Effect::LowerDefense, 0, 1, 1}};
            Battle b; CHECK(b.start(e)); ack(b); CHECK(b.submit(Command::Sword)); ack(b); ack(b);
            CHECK(b.defense_stage() == -1);
            CHECK(b.submit(Command::Sword)); ack(b);
            CHECK(b.event()->move_id == 65535); CHECK(b.event()->damage > 1);
        });
        test("exhausted move pools fall back to Struggle", [] {
            auto e = damage_only(); e.enemy_moves[0].pp = 0;
            Battle b; CHECK(b.start(e)); ack(b); CHECK(b.submit(Command::Guard)); ack(b);
            CHECK(b.event()->move_id == 65535); CHECK(b.state().enemy_moves[0].pp == 0);
        });
        test("seeded moves are reproducible and cover the demo pool", [] {
            std::set<std::uint16_t> seen;
            for (std::uint32_t seed = 0; seed < 100; ++seed) {
                Battle a, b; CHECK(a.start(pikachu_demo(seed))); CHECK(b.start(pikachu_demo(seed)));
                ack(a); ack(b); CHECK(a.submit(Command::Guard)); CHECK(b.submit(Command::Guard));
                ack(a); ack(b); CHECK(a.event()->move_id == b.event()->move_id);
                CHECK(a.event()->damage == b.event()->damage); seen.insert(a.event()->move_id);
            }
            CHECK(seen == std::set<std::uint16_t>({84, 98, 45, 39}));
        });
        test("invalid definitions are rejected without touching the battle", [] {
            Battle b;
            for (int variant = 0; variant < 9; ++variant) {
                auto e = pikachu_demo();
                switch (variant) {
                case 0: e.player.hp = 0; break;
                case 1: e.enemy.max_hp = 10001; break;
                case 2: e.enemy_moves.clear(); break;
                case 3: e.enemy_moves.push_back(e.enemy_moves[0]); break;
                case 4: e.enemy_moves[0].effect = static_cast<Effect>(999); break;
                case 5: e.enemy_moves[0].weight = 0; break;
                case 6: e.enemy_moves[0].power = 1000000; break;
                case 7: e.arrows = -1; break;
                case 8: e.enemy.name.clear(); break;
                }
                CHECK(!b.start(e)); CHECK(b.phase() == Phase::Idle);
            }
        });
        test("victory skips the defeated enemy's turn", [] {
            auto e = damage_only(); e.enemy.hp = 1;
            Battle b; CHECK(b.start(e)); ack(b); CHECK(b.submit(Command::Sword));
            CHECK(b.event()->damage == 1); ack(b); CHECK(b.phase() == Phase::Result);
            CHECK(b.outcome() == Outcome::Victory); CHECK(b.state().player.hp == e.player.hp);
            ack(b); CHECK(b.phase() == Phase::Finished); CHECK(!b.submit(Command::Sword));
        });
        test("defeat ends the encounter with zero HP", [] {
            auto e = damage_only(); e.player.hp = 1;
            Battle b; CHECK(b.start(e)); ack(b); CHECK(b.submit(Command::Sword)); ack(b);
            CHECK(b.state().player.hp == 0); ack(b); CHECK(b.outcome() == Outcome::Defeat);
            CHECK(b.phase() == Phase::Result); ack(b); CHECK(b.phase() == Phase::Finished);
        });
        test("escape avoids an extra enemy turn", [] {
            Battle b; CHECK(b.start(pikachu_demo())); ack(b);
            CHECK(b.submit(Command::Flee)); ack(b); ack(b);
            CHECK(b.outcome() == Outcome::Escaped); CHECK(b.summary().rounds == 0);
        });
        test("successful world settlement and cleanup occur once", [] {
            TestHost host;
            { Session session(host); auto e = pikachu_demo(); e.enemy.hp = 1;
              CHECK(session.begin(e, 0)); CHECK(!session.begin(e, 0)); win(session);
              CHECK(session.settled()); CHECK(!session.owns_world());
              session.tick(90000); session.cancel(); }
            CHECK(host.calls == 1); CHECK(host.rewards == 1); CHECK(host.releases == 1);
        });
        test("pending settlement retries retain the same transaction identity", [] {
            TestHost host; host.pending = host.apply_before_ack = true;
            Session session(host); auto e = pikachu_demo(); e.enemy.hp = 1;
            CHECK(session.begin(e, 0)); win(session); session.tick(1); session.tick(2);
            CHECK(host.rewards == 1); CHECK(host.transactions.size() == 1);
            host.pending = false; session.tick(3);
            CHECK(session.settled()); CHECK(host.rewards == 1); CHECK(host.releases == 1);
        });
        test("scene replacement cancels before a command can touch the world", [] {
            TestHost host; Session session(host); CHECK(session.begin(pikachu_demo(), 0));
            host.scene_valid = false;
            CHECK(!session.acknowledge(session.battle().event()->serial, 1));
            CHECK(session.battle().outcome() == Outcome::Cancelled);
            CHECK(host.calls == 0); CHECK(host.releases == 1);
        });
        test("missing animation completion releases the world lock", [] {
            TestHost host; Session session(host); CHECK(session.begin(pikachu_demo(), 0));
            session.tick(14999); CHECK(session.owns_world());
            session.tick(15000); CHECK(!session.owns_world());
            CHECK(host.releases == 1); CHECK(host.calls == 0);
        });
        test("player input and result reading have no animation timeout", [] {
            TestHost host; Session session(host); auto e = pikachu_demo(); e.enemy.hp = 1;
            CHECK(session.begin(e, 0)); CHECK(session.acknowledge(session.battle().event()->serial, 0));
            session.tick(100000); CHECK(session.owns_world());
            CHECK(session.submit(Command::Sword, 100000));
            CHECK(session.acknowledge(session.battle().event()->serial, 100000));
            session.tick(200000); CHECK(session.owns_world());
            CHECK(session.acknowledge(session.battle().event()->serial, 200000)); CHECK(session.settled());
        });
        test("failed acquisition and invalid input never hold the world", [] {
            TestHost host; host.available = false; Session session(host);
            auto e = pikachu_demo(); e.player.hp = 0;
            CHECK(!session.begin(e, 0)); CHECK(host.acquires == 0);
            CHECK(!session.begin(pikachu_demo(), 0)); CHECK(host.acquires == 1);
            CHECK(host.releases == 0); CHECK(!session.owns_world());
        });
        test("settlement failure and timeout release once without success", [] {
            for (bool fail : {true, false}) {
                TestHost host; host.fail = fail; host.pending = !fail;
                Session session(host); auto e = pikachu_demo(); e.enemy.hp = 1;
                CHECK(session.begin(e, 0)); win(session); session.tick(5000);
                CHECK(!session.settled()); CHECK(!session.owns_world());
                CHECK(host.releases == 1); CHECK(host.rewards == 0);
            }
        });
        test("session destruction cleans up an interrupted encounter", [] {
            TestHost host;
            { Session session(host); CHECK(session.begin(pikachu_demo(), 0)); }
            CHECK(host.releases == 1); CHECK(host.calls == 0);
        });
        test("escape settles damage/resources without a victory reward", [] {
            TestHost host; Session session(host); CHECK(session.begin(damage_only(), 0));
            CHECK(session.acknowledge(session.battle().event()->serial, 0));
            CHECK(session.submit(Command::Flee, 0));
            while (session.battle().event()) CHECK(session.acknowledge(session.battle().event()->serial, 0));
            CHECK(host.calls == 1); CHECK(host.rewards == 0); CHECK(host.last.outcome == Outcome::Escaped);
        });
        test("3000 seeds terminate with legal HP and bounded ammunition", [] {
            for (std::uint32_t seed = 0; seed < 3000; ++seed) {
                Battle b; CHECK(b.start(pikachu_demo(seed))); int budget = 2000;
                while (b.phase() != Phase::Finished && --budget) {
                    if (b.event()) ack(b);
                    else CHECK(b.submit(b.can_submit(Command::Bomb) ? Command::Bomb : Command::Sword));
                    CHECK(b.state().player.hp >= 0); CHECK(b.state().enemy.hp >= 0);
                    CHECK(b.state().arrows >= 0); CHECK(b.state().bombs >= 0);
                }
                CHECK(budget > 0);
                CHECK(b.outcome() == Outcome::Victory || b.outcome() == Outcome::Defeat);
            }
        });
    } catch (const std::exception& error) {
        std::cerr << "FAIL " << error.what() << '\n'; return 1;
    }
    std::cout << passed << " battle/session checks passed.\n";
}
