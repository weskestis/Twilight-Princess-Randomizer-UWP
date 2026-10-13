# Dusklight companion API v1 — Xbox build 1.4.1.747

This is the launcher/game side of the Android companion connection. It does not include an Android APK. Use the same local IPv4 network as the Xbox. The service is off until the player selects **Companion Tracker → Pair Android**. The launcher and in-game menu both expose this screen. Closing the screen keeps the service running. **Turn connection off** stops listening; **Disconnect devices** revokes every token.

Transport: HTTP/1.1 JSON at `http://<Xbox-IP>:18473`. The Xbox screen displays available local addresses and can cycle through them. If address enumeration is unavailable, enter the IP shown by Xbox Dev Home. Only local/private IPv4 peers are accepted. Native Android clients may use local cleartext HTTP for this endpoint; the app must configure Android network security accordingly. There is no internet relay or TLS in v1. Never forward this port or log pairing codes/tickets/tokens.

## Pairing

1. On Xbox select **Pair Android**. The six-digit code (including leading zeroes) lasts two minutes. QR content is `dusklight://pair?api=1&host=<IPv4>&port=18473&code=<six digits>`; the app should register the `dusklight` scheme and offer manual IP/code entry.
2. `POST /v1/pair` with `Content-Type: application/json` and `{"code":"012345","deviceName":"Wes Android"}`. Names must be 1–64 UTF-8 bytes without control characters. Response **202**: `{"ticket":"<32 hex>","confirmation":"<6 hex>","expiresIn":120,"pollAfterMs":1000}`.
3. Display the matching confirmation on the phone and compare it with Xbox. The player uses **Approve matching code** or **Reject pairing** on Xbox. Until approval there is no tracker access.
4. Poll `GET /v1/pair/<ticket>` once a second from the same phone/IP. **202** means pending, **403** rejected, **410** expired, **200** approved: `{"state":"approved","token":"<64 hex>","readOnly":true,"snapshotPath":"/v1/snapshot","pollAfterMs":500}`. Approved tickets remain retrievable for the original two-minute request window so a lost response can be retried.
5. Store the token privately for the running session. Send `Authorization: Bearer <token>` on every snapshot request. Tokens are kept only in Xbox memory and expire when the UI/runtime shuts down, Dusklight closes, connection turns off or devices are disconnected. A restart requires pairing again. Four devices maximum; one outstanding pair request. Five incorrect PIN attempts lock that code; select **Pair Android** again to start a new window.

`GET /v1/info` needs no token and returns only API/build/name, read-only status and whether pairing is open. It never returns the PIN, save data or tokens.

## Live snapshots and reconnects

`GET /v1/snapshot` returns a full JSON snapshot. Poll every **500 ms** while connected. Polling faster than 250 ms per token returns **429** with `Retry-After: 1`; back off on errors. The Xbox samples on the game/UI thread at most twice a second, only while a device is paired. Socket I/O stays in the existing Borealis networking worker.

The response carries an `ETag`; send `If-None-Match` on subsequent polls to receive **304** when unchanged. After a network reconnect, omit the ETag and replace live state with the complete snapshot. Retain offline/manual notes separately. A token rejected with **401** requires pairing again. Use an exponential reconnect delay (1, 2, 4, up to 10 seconds); never spin retry loops.

Every snapshot includes `apiVersion:1`, `build`, random `sessionId`, monotonically increasing `sequence` and `state`. Compare `(sessionId, save.seedHash, save.slot, save.revision)` before merging any data. A changed scope must replace live state, including collected-check flags that may revert when an earlier save is loaded. Ignore older sequences. `state` can be `launcher`, `vanilla`, `loading`, `playing`, `tracker_unavailable` or `snapshot_too_large`. Every state except `playing` has `save:null`; never display old live state as the current run during transitions or seed creation. Manual/offline data belongs to its own seed/slot scope.

Typical playing response (arrays shortened):

```json
{
  "apiVersion": 1,
  "build": "1.4.1.747",
  "sessionId": "unpredictable-session-id",
  "sequence": 12,
  "state": "playing",
  "save": {
    "seedHash": "active-seed-hash",
    "seedId": 12345,
    "slot": 0,
    "revision": 4,
    "stage": 17,
    "gameTimeCentiseconds": 45678,
    "deaths": 2,
    "settings": {"Enemy Souls": "On"},
    "checks": [
      {"id": 14, "name": "Example check", "categories": ["Chest"], "randomized": true, "collected": true, "seen": true}
    ],
    "inventory": [{"id": 8200, "name": "Bulblin Soul", "count": 1}],
    "entrances": [{"source": "Example door", "destination": "Example room", "category": "Interior", "sourceStage": 1, "destinationStage": 2}],
    "entranceTotal": 100,
    "dungeons": [{"name": "Forest Temple", "smallKeysFound": 2, "bigKey": false, "map": true, "compass": false, "completed": false}]
  }
}
```

Settings are the active seed's resolved anti-spoiler settings, not unsaved launcher selections. The check catalog includes generated pots, pumpkins, expanded shops and enemy first-defeat checks for that seed. IDs are scoped to that seed/catalog; do not carry an ID across seeds. `randomized` means the check is present in the active seed's item-location map. `collected` uses the same save flags as the native tracker. `seen` uses the native seen-check record and is always true for collected checks. Inventory contains owned logical items/counts, including enemy/boss Souls; missing items have count zero. Dungeon `smallKeysFound` is the native cumulative keys-found tracker, not the number currently unspent; map/compass/big-key fields are owned flags. `completed` follows the dungeon goal's native tracker check.

Only **discovered** entrances have destinations in the response. Hidden item placements, unseen contents classifications, required/barren hints and raw seed/spoiler files are never exported. The first safe gameplay snapshot waits for three seconds of stable runtime and an idle event system before constructing a cached catalog. It skips the tracker's reachability-flattening search. There is no per-request generation or reachability result in v1.

## Bounds and errors

Only the routes above exist. The sole POST route requests pairing; there are no settings, launch, mod download, file access or game-control commands. HTTP headers are bounded to 4 KiB, request bodies to 2 KiB and responses to 2 MiB. Eight concurrent streams maximum. Requests must include `Host` and a valid `Content-Length` when they have a body. Chunked encoding, duplicate headers, pipelining and browser `Origin` requests are rejected. Each response closes the TCP connection; reuse the token when opening the next connection.

Errors are JSON `{ "error": "code" }`: **400** malformed request, **401** missing/invalid token, **403** closed/invalid/rejected pairing or browser request, **404** unknown route/ticket, **405** unsupported method, **409** pairing busy/device limit, **410** expired ticket, **413** request too large, **429** PIN/poll limit, **503** initial snapshot pending. A phone may keep manual notes offline, but must label live data unavailable on these errors.

## Validation

Build gates exercise the production server with real pinned Borealis TCP (fragmented requests, Xbox approval, authentication, full reconnect snapshots, ETags, seed/slot replacement, limits, expiry and disabled sampling). The real pinned RmlUi fixture exercises controller pairing/approval/off actions and one QR texture per revision at 720p/1080p/4K. The complete Windows Store build, existing runtime/native-mod gates and 67 strict generation scenarios remain required. Physical Xbox/Android LAN pairing must be tested after installation; these fixtures are not a console FPS measurement.

QR generator: Nayuki v1.8.0, commit `720f62bddb7226106071d4728c292cb1df519ceb`, MIT; vendored source and license notices retained in the patch.
