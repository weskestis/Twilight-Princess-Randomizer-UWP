from __future__ import annotations

import os
import re
from pathlib import Path


ROOT = Path(os.environ["TPR_SRC"])


def read_text(path: Path) -> str:
    return path.read_text(encoding="utf-8").replace("\r\n", "\n")


def write_text(path: Path, text: str) -> None:
    path.write_text(text, encoding="utf-8", newline="\n")


def replace_once(text: str, old: str, new: str, label: str) -> str:
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"{label}: expected one anchor, found {count}")
    return text.replace(old, new, 1)


def function_span(text: str, signature: str) -> tuple[int, int]:
    start = text.find(signature)
    if start < 0:
        raise RuntimeError(f"function signature not found: {signature}")
    open_brace = text.find("{", start)
    if open_brace < 0:
        raise RuntimeError(f"function open brace not found: {signature}")
    depth = 0
    i = open_brace
    in_string = False
    in_char = False
    escape = False
    while i < len(text):
        ch = text[i]
        if escape:
            escape = False
        elif ch == "\\" and (in_string or in_char):
            escape = True
        elif ch == '"' and not in_char:
            in_string = not in_string
        elif ch == "'" and not in_string:
            in_char = not in_char
        elif not in_string and not in_char:
            if ch == "{":
                depth += 1
            elif ch == "}":
                depth -= 1
                if depth == 0:
                    return start, i + 1
        i += 1
    raise RuntimeError(f"function close brace not found: {signature}")


# ---------------------------------------------------------------------------
# Ordon shop delayed-text ownership
# ---------------------------------------------------------------------------
shop_candidates: list[Path] = []
for path in ROOT.rglob("*.cpp"):
    try:
        source = read_text(path)
    except (OSError, UnicodeDecodeError):
        continue
    if "s_pendingTextKey" in source and "text_override" in source:
        shop_candidates.append(path)

if len(shop_candidates) != 1:
    raise RuntimeError(
        f".732 expected exactly one Ordon shop pending-text implementation, found {shop_candidates}"
    )

shop_path = shop_candidates[0]
shop = read_text(shop_path)

if "#include <deque>" not in shop:
    include_anchor = "#include <optional>"
    if include_anchor not in shop:
        raise RuntimeError(".732 Ordon shop source has no <optional> include anchor")
    shop = shop.replace(include_anchor, include_anchor + "\n#include <deque>", 1)

shop = replace_once(
    shop,
    "thread_local std::optional<uint64_t> s_pendingTextKey;",
    """thread_local std::deque<uint64_t> s_pendingTextKeys;

struct PendingTextKeyCompat {
    PendingTextKeyCompat& operator=(uint64_t key) {
        if (s_pendingTextKeys.empty() || s_pendingTextKeys.back() != key) {
            s_pendingTextKeys.push_back(key);
        }
        return *this;
    }

    PendingTextKeyCompat& operator=(const std::optional<uint64_t>& key) {
        if (key.has_value()) {
            *this = *key;
        } else {
            reset();
        }
        return *this;
    }

    PendingTextKeyCompat& operator=(std::nullopt_t) {
        reset();
        return *this;
    }

    void reset() {
        if (!s_pendingTextKeys.empty()) {
            s_pendingTextKeys.pop_front();
        }
    }

    bool has_value() const { return !s_pendingTextKeys.empty(); }
    explicit operator bool() const { return has_value(); }
    uint64_t operator*() const { return s_pendingTextKeys.front(); }
    uint64_t value() const { return s_pendingTextKeys.front(); }
    uint64_t value_or(uint64_t fallback) const {
        return s_pendingTextKeys.empty() ? fallback : s_pendingTextKeys.front();
    }
};

inline bool operator==(const PendingTextKeyCompat& pending, uint64_t key) {
    return pending.has_value() && *pending == key;
}
inline bool operator==(uint64_t key, const PendingTextKeyCompat& pending) {
    return pending == key;
}
inline bool operator!=(const PendingTextKeyCompat& pending, uint64_t key) {
    return !(pending == key);
}
inline bool operator!=(uint64_t key, const PendingTextKeyCompat& pending) {
    return !(pending == key);
}

thread_local PendingTextKeyCompat s_pendingTextKey;""",
    ".732 Ordon pending-key storage",
)

shop = replace_once(
    shop,
    "s_pendingTextKey = s_activeKey;",
    """if (s_activeKey) {
        if (s_pendingTextKeys.empty() || s_pendingTextKeys.back() != *s_activeKey) {
            s_pendingTextKeys.push_back(*s_activeKey);
            // A shop should never have this many outstanding delayed text actors. Bound stale
            // entries defensively so a cancelled conversation cannot poison later shelf text.
            while (s_pendingTextKeys.size() > 16) {
                s_pendingTextKeys.pop_front();
            }
        }
    }""",
    ".732 Ordon pending-key enqueue",
)

clear_start, clear_end = function_span(shop, "void clear_text_selection()")
clear_fn = shop[clear_start:clear_end]
if "s_pendingTextKey.reset();" not in clear_fn:
    raise RuntimeError(".732 Ordon clear_text_selection anchor changed")
clear_fn = clear_fn.replace("s_pendingTextKey.reset();", "s_pendingTextKeys.clear();")
shop = shop[:clear_start] + clear_fn + shop[clear_end:]

# The exact return type has changed across the shop stabilization revisions; locate by function name.
text_start = shop.find("text_override(")
if text_start < 0:
    raise RuntimeError(".732 Ordon text_override function missing")
line_start = shop.rfind("\n", 0, text_start) + 1
brace = shop.find("{", text_start)
if brace < 0:
    raise RuntimeError(".732 Ordon text_override brace missing")
depth = 0
i = brace
in_string = False
in_char = False
escape = False
while i < len(shop):
    ch = shop[i]
    if escape:
        escape = False
    elif ch == "\\" and (in_string or in_char):
        escape = True
    elif ch == '"' and not in_char:
        in_string = not in_string
    elif ch == "'" and not in_string:
        in_char = not in_char
    elif not in_string and not in_char:
        if ch == "{":
            depth += 1
        elif ch == "}":
            depth -= 1
            if depth == 0:
                text_end = i + 1
                break
    i += 1
else:
    raise RuntimeError(".732 Ordon text_override close brace missing")

text_fn = shop[line_start:text_end]
key_patterns = [
    "const auto key = s_activeKey ? s_activeKey : s_pendingTextKey;",
    "auto key = s_activeKey ? s_activeKey : s_pendingTextKey;",
]
matched = next((p for p in key_patterns if p in text_fn), None)
if matched is None:
    raise RuntimeError(".732 Ordon text_override pending-key selection anchor changed")
text_fn = text_fn.replace(
    matched,
    """std::optional<uint64_t> key = s_activeKey;
    const bool consumePendingKey = !key.has_value() && !s_pendingTextKeys.empty();
    if (consumePendingKey) {
        key = s_pendingTextKeys.front();
    }""",
    1,
)
if "s_pendingTextKey.reset();" not in text_fn:
    raise RuntimeError(".732 Ordon text_override consume anchor changed")
text_fn = text_fn.replace(
    "s_pendingTextKey.reset();",
    """if (consumePendingKey && !s_pendingTextKeys.empty() &&
        s_pendingTextKeys.front() == *key)
    {
        s_pendingTextKeys.pop_front();
    }""",
    1,
)
shop = shop[:line_start] + text_fn + shop[text_end:]

for marker in (
    "std::deque<uint64_t> s_pendingTextKeys",
    "PendingTextKeyCompat",
    "thread_local PendingTextKeyCompat s_pendingTextKey",
    "s_pendingTextKeys.push_back",
    "s_pendingTextKeys.pop_front",
    "consumePendingKey",
):
    if marker not in shop:
        raise RuntimeError(f".732 Ordon shop FIFO marker missing: {marker}")

write_text(shop_path, shop)


# ---------------------------------------------------------------------------
# Xbox Reset Game re-entry hardening
# ---------------------------------------------------------------------------
reset_path = ROOT / "src/m_Do/m_Do_Reset.cpp"
reset = read_text(reset_path)
reset_start, reset_end = function_span(reset, "void mDoRst_resetCallBack(int port, void*)")
reset_fn = reset[reset_start:reset_end]

old_guard = """    if (mDoRst::isReset()) {
        return;
    }
"""
if reset_fn.count(old_guard) != 1:
    raise RuntimeError(
        f".732 reset callback expected one late isReset guard, found {reset_fn.count(old_guard)}"
    )
reset_fn = reset_fn.replace(old_guard, "", 1)

open_brace = reset_fn.find("{")
early_guard = """
#if defined(_UWP)
    // Xbox can deliver the reset request again while the first reset is tearing down Randomizer.
    // Reject re-entry before invoking any game-mode callback or unregistering hooks.
    if (mDoRst::isReset()) {
        return;
    }
#endif
"""
reset_fn = reset_fn[: open_brace + 1] + early_guard + reset_fn[open_brace + 1 :]

activity_anchor = '    dusk::startup_guard::begin_activity("reset.deactivate-game-mode");'
if activity_anchor not in reset_fn:
    raise RuntimeError(".732 Xbox reset activity anchor changed")
reset_fn = reset_fn.replace(
    activity_anchor,
    activity_anchor
    + "\n"
    + "    dusk::ui::prelaunch_state().returnToPrelaunchOnReset = true;",
    1,
)

reset = reset[:reset_start] + reset_fn + reset[reset_end:]
write_text(reset_path, reset)


# ---------------------------------------------------------------------------
# UWP writable-file bridge for online mod downloads
# ---------------------------------------------------------------------------
io_path = ROOT / "extern/borealis/src/io.cpp"
io = read_text(io_path)
if "#include <cstdio>" not in io:
    io = replace_once(
        io,
        "#include <cerrno>",
        "#include <cerrno>\n#include <cstdio>",
        ".732 Borealis stdio include",
    )

open_anchor = """    const char* openMode = mode == File::Mode::Read   ? "rb" :
                           mode == File::Mode::Append ? "ab" :
                                                        "wb";
    SDL_ClearError();
    SDL_IOStream* handle = SDL_IOFromFile(resolved.c_str(), openMode);
"""
if open_anchor not in io:
    raise RuntimeError(".732 Borealis file-open anchor changed")

open_new = """    const char* openMode = mode == File::Mode::Read   ? "rb" :
                           mode == File::Mode::Append ? "ab" :
                                                        "wb";

#if defined(_UWP) && defined(_WIN32)
    if (mode != File::Mode::Read) {
        // SDL's UWP datastream backend can report ERROR_DISK_FULL for valid LocalState paths.
        // These paths are ordinary app-local filesystem paths, so bind an allowed CRT FILE handle
        // to SDL_IOStream instead and keep the existing Borealis downloader API unchanged.
        FILE* stdioFile = nullptr;
        const auto nativePath = fs_path_from_utf8(resolved);
        const wchar_t* nativeMode = mode == File::Mode::Append ? L"ab" : L"wb";
        const errno_t openError = _wfopen_s(&stdioFile, nativePath.c_str(), nativeMode);
        if (openError != 0 || stdioFile == nullptr) {
            detail::release_access(access);
            return failed_open(Status::Failed,
                "Failed to open writable UWP file: " +
                    std::generic_category().message(static_cast<int>(openError)));
        }
        SDL_ClearError();
        SDL_IOStream* stdioHandle = SDL_IOFromFP(stdioFile, true);
        if (stdioHandle == nullptr) {
            const std::string message =
                SDL_GetError() != nullptr && SDL_GetError()[0] != '\\0' ?
                    SDL_GetError() : "Failed to bind writable UWP file";
            std::fclose(stdioFile);
            detail::release_access(access);
            return failed_open(Status::Failed, message);
        }
        return {.status = Status::Ok, .file = File{stdioHandle, access, true}};
    }
#endif

    SDL_ClearError();
    SDL_IOStream* handle = SDL_IOFromFile(resolved.c_str(), openMode);
"""
io = io.replace(open_anchor, open_new, 1)

for marker in (
    "SDL_IOFromFP(stdioFile, true)",
    "Failed to open writable UWP file",
    "SDL's UWP datastream backend",
):
    if marker not in io:
        raise RuntimeError(f".732 UWP download bridge marker missing: {marker}")

write_text(io_path, io)


# ---------------------------------------------------------------------------
# Soul identity / shop-width invariants
# ---------------------------------------------------------------------------
catalog_matches: list[Path] = []
for path in ROOT.rglob("*enemy_soul_catalog*.hpp"):
    try:
        source = read_text(path)
    except (OSError, UnicodeDecodeError):
        continue
    if "Poe Enemy Soul" in source:
        catalog_matches.append(path)

if len(catalog_matches) != 1:
    raise RuntimeError(
        f".732 expected one Enemy Soul catalog with distinct Poe Enemy Soul identity, found {catalog_matches}"
    )

catalog = read_text(catalog_matches[0])
if '"Poe Enemy Soul"' not in catalog:
    raise RuntimeError(".732 Enemy Soul catalog lost Poe Enemy Soul identity")

context_header = read_text(ROOT / "mods/randomizer/src/randomizer_context.hpp")
shop_type_match = re.search(
    r"std::unordered_map\s*<\s*u32\s*,\s*([^>]+)>\s*mShopOverrides", context_header
)
if not shop_type_match:
    raise RuntimeError(".732 could not verify Randomizer shop item-id width")
shop_value_type = shop_type_match.group(1).strip()
if shop_value_type not in {"u8", "uint8_t", "unsigned char"}:
    raise RuntimeError(
        f".732 shop override item IDs unexpectedly changed from the runtime byte format: {shop_value_type}"
    )

world_path = ROOT / "mods/randomizer/generator/logic/world.cpp"
world = read_text(world_path)
for marker in (
    "itemId < 0 || itemId > 0xFE",
    "ForbidUnencodableRuntimeItems",
):
    if marker not in world:
        raise RuntimeError(
            f".732 logical-only Enemy Soul exclusion marker missing from runtime-byte locations: {marker}"
        )

print(
    "Applied .732 runtime polish: Ordon FIFO text ownership, reset re-entry guard, "
    "UWP writable download bridge, Poe/Enemy Soul identity guard, and logical-item shop exclusion."
)
