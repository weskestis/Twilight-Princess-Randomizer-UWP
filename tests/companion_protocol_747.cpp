#include "server.hpp"
#include <borealis/log.hpp>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <random>
#include <thread>

// Logging is external to the protocol; all sockets, buffering, shutdown and
// queue limits below are the actual pinned Borealis implementation.
namespace borealis::log::detail {
bool enabled(LogLevel, std::string_view) noexcept { return false; }
void write(LogLevel, std::string_view, std::string_view) {}
[[noreturn]] void write_fatal(std::string_view, std::string_view) { std::abort(); }
}
using namespace dusk::companion;
using namespace std::chrono_literals;
#define CHECK(x) do { if (!(x)) { std::cerr << "Failed line " << __LINE__ << ": " << #x << '\n'; std::exit(1); } } while (false)
struct Response { int status; std::string etag, raw; Json body; };
int main() {
    const auto owner = std::this_thread::get_id();
    size_t samples = 0;
    std::string seed = "seed-A"; int slot = 0;
    Server server([](std::span<std::byte> out) {
        std::random_device rng;
        for (auto& b : out) b = std::byte(rng() & 255);
        return true;
    }, [&] {
        CHECK(std::this_thread::get_id() == owner); ++samples;
        return Json{{"state", "playing"}, {"save", {{"seedHash", seed}, {"slot", slot},
            {"checks", Json::array({{{"id", 14}, {"collected", true}, {"seen", true}}})},
            {"inventory", Json::array({{{"name", "Bulblin Soul"}, {"count", 1}}})},
            {"entrances", Json::array({{{"source", "shop"}, {"destination", "grotto"}}})}}}};
    });
    CHECK(!server.view().enabled);
    CHECK(server.enable("tcp://127.0.0.1:0"));
    auto now = Clock::now();
    borealis::net::Context client;
    auto call = [&](std::string wire, bool fragmented = false) {
        const auto id = client.connect(server.endpoint());
        std::string response; bool connected = false, finished = false;
        const auto deadline = Clock::now() + 5s;
        while (!finished && Clock::now() < deadline) {
            server.update(now);
            borealis::net::Event event;
            while (client.poll(event)) {
                if (event.id != id) continue;
                if (event.kind == borealis::net::Event::Kind::Connected) {
                    connected = true;
                    auto bytes = std::as_bytes(std::span(wire.data(), wire.size()));
                    if (fragmented) {
                        CHECK(client.send(id, bytes.first(17)) == borealis::net::SendResult::Ok);
                        for (int i = 0; i < 10; ++i) { server.update(now); std::this_thread::sleep_for(1ms); }
                        CHECK(client.send(id, bytes.subspan(17)) == borealis::net::SendResult::Ok);
                    } else CHECK(client.send(id, bytes) == borealis::net::SendResult::Ok);
                } else if (event.kind == borealis::net::Event::Kind::StreamData)
                    response.append(reinterpret_cast<const char*>(event.data.data()), event.data.size());
                else if (event.kind == borealis::net::Event::Kind::Closed) finished = true;
            }
            std::this_thread::sleep_for(1ms);
        }
        CHECK(connected && finished);
        const auto split = response.find("\r\n\r\n"); CHECK(split != std::string::npos);
        Response r{std::stoi(response.substr(9, 3)), {}, response, nullptr};
        const auto e = response.find("ETag: ");
        if (e != std::string::npos) r.etag = response.substr(e + 6, response.find("\r\n", e) - e - 6);
        if (r.status != 304) r.body = Json::parse(response.substr(split + 4));
        return r;
    };
    auto get = [&](std::string path, std::string headers = "") {
        return call("GET " + path + " HTTP/1.1\r\nHost: xbox\r\n" + headers + "\r\n");
    };
    auto pair = [&](std::string code) {
        const auto body = Json{{"code", code}, {"deviceName", "Android <phone>"}}.dump();
        return call("POST /v1/pair HTTP/1.1\r\nHost: xbox\r\nContent-Type: application/json\r\nContent-Length: " +
            std::to_string(body.size()) + "\r\n\r\n" + body, true);
    };
    CHECK(get("/v1/info").status == 200 && samples == 0);
    CHECK(get("/v1/snapshot").status == 401 && samples == 0);
    CHECK(pair("000000").status == 403);
    server.begin_pairing(now); CHECK(server.view().pin.size() == 6);
    auto pending = pair(server.view().pin); CHECK(pending.status == 202);
    const auto ticket = pending.body["ticket"].get<std::string>();
    CHECK(server.view().confirmation == pending.body["confirmation"].get<std::string>());
    CHECK(get("/v1/pair/" + ticket).status == 202 && samples == 0);
    CHECK(pair(server.view().pin).status == 409);
    server.decide(true, now);
    auto approved = get("/v1/pair/" + ticket); CHECK(approved.status == 200);
    const auto token = approved.body["token"].get<std::string>(); CHECK(token.size() == 64);
    const auto auth = "Authorization: Bearer " + token + "\r\n";
    auto snapshot = get("/v1/snapshot", auth); CHECK(snapshot.status == 200 && samples == 1);
    CHECK(snapshot.body["save"]["seedHash"].get<std::string>() == seed && snapshot.body["save"]["inventory"][0]["count"] == 1);
    CHECK(snapshot.raw.find(token) == std::string::npos && snapshot.raw.find(ticket) == std::string::npos);
    CHECK(get("/v1/snapshot", auth).status == 429);
    now += 500ms;
    CHECK(get("/v1/snapshot", auth + "If-None-Match: " + snapshot.etag + "\r\n").status == 304);
    // Fresh TCP connection without an ETag always gets a complete snapshot.
    now += 500ms; CHECK(get("/v1/snapshot", auth).status == 200);
    seed = "seed-B"; slot = 2; now += 500ms;
    auto changed = get("/v1/snapshot", auth);
    CHECK(changed.status == 200 && changed.body["save"]["seedHash"].get<std::string>() == seed && changed.body["save"]["slot"] == 2);
    CHECK(changed.etag != snapshot.etag && changed.body["sequence"] > snapshot.body["sequence"]);
    CHECK(get("/v1/info", "Origin: https://evil.example\r\n").status == 403);
    CHECK(get("/v1/info", "Host: duplicate\r\n").status == 400);
    CHECK(get("/v1/info", "Transfer-Encoding: chunked\r\n").status == 400);
    CHECK(call("POST /v1/pair HTTP/1.1\r\nHost: xbox\r\nContent-Length: 999999\r\n\r\n").status == 413);
    CHECK(call("PUT /v1/settings HTTP/1.1\r\nHost: xbox\r\n\r\n").status == 405);
    CHECK(get("/v1/info", std::string(5000, 'a') + "\r\n").status == 413);
    CHECK(call("GET /v1/info HTTP/1.1\r\nHost: xbox\r\n\r\nGET /v1/info HTTP/1.1\r\n\r\n").status == 400);
    server.disconnect_all(); now += 500ms;
    CHECK(get("/v1/snapshot", auth).status == 401);
    const auto disconnectedSamples = samples;
    server.begin_pairing(now);
    for (int i = 0; i < 5; ++i) CHECK(pair("badcode").status == 403);
    CHECK(pair(server.view().pin).status == 429);
    server.begin_pairing(now); pending = pair(server.view().pin);
    const auto rejectedTicket = pending.body["ticket"].get<std::string>();
    server.decide(false, now); CHECK(get("/v1/pair/" + rejectedTicket).status == 403);
    server.begin_pairing(now); pending = pair(server.view().pin);
    const auto expiredTicket = pending.body["ticket"].get<std::string>();
    now += 121s; CHECK(get("/v1/pair/" + expiredTicket).status == 410);
    CHECK(pair("000000").status == 403);
    CHECK(samples == disconnectedSamples);
    CHECK(Server::private_peer("tcp://192.168.1.5:123"));
    CHECK(Server::private_peer("tcp://172.31.2.5:123"));
    CHECK(!Server::private_peer("tcp://172.32.2.5:123"));
    CHECK(!Server::private_peer("tcp://8.8.8.8:123"));
    server.disable(); CHECK(!server.view().enabled && server.view().paired == 0);
    server.update(now); CHECK(samples == disconnectedSamples);
    Server noEntropy([](auto) { return false; }, [] { CHECK(false); return Json{}; });
    CHECK(!noEntropy.enable("tcp://127.0.0.1:0"));
    std::cout << "PASS companion v1: real TCP, fragmented HTTP, Xbox approval, auth, disconnect/reconnect/full state, ETag, seed/slot changes, rate/size limits, expiry, disabled/failed entropy and owner-thread sampling\n";
}
