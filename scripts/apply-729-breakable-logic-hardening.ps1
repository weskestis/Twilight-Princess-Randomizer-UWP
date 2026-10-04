$ErrorActionPreference = 'Stop'

function Read-Normalized([string]$Path) {
  $text = [IO.File]::ReadAllText($Path)
  return $text.Replace(([string][char]13 + [char]10), [string][char]10)
}
function Write-Utf8([string]$Path, [string]$Text) {
  [IO.File]::WriteAllText($Path, $Text, [Text.UTF8Encoding]::new($false))
}

$src = $env:TPR_SRC
if (-not $src) { throw 'TPR_SRC is not set.' }

# .729: make every shuffled pot/pumpkin reward collect through the Randomizer
# item table, then persist completion only after the pickup grant succeeds.
$breakablesHeaderPath = "$src\mods\randomizer\src\breakables.hpp"
$breakablesHeader = Read-Normalized $breakablesHeaderPath
if (-not $breakablesHeader.Contains('#include <optional>')) {
  $breakablesHeader = $breakablesHeader.Replace(
    '#include <cstdint>',
    '#include <cstdint>' + [string][char]10 + '#include <optional>')
}
$breakablesDeclAnchor =
  'bool refresh_spawned_item(fopAc_ac_c* actor) noexcept;'
if (-not $breakablesHeader.Contains($breakablesDeclAnchor)) {
  throw '.729 breakable hardening could not find refresh_spawned_item declaration.'
}
if (-not $breakablesHeader.Contains(
      'spawned_item_assignment(const fopAc_ac_c* actor) noexcept;')) {
  $breakablesHeader = $breakablesHeader.Replace(
    $breakablesDeclAnchor,
    $breakablesDeclAnchor + [string][char]10 +
      'std::optional<uint8_t> spawned_item_assignment(const fopAc_ac_c* actor) noexcept;')
}
Write-Utf8 $breakablesHeaderPath $breakablesHeader

$breakablesPath = "$src\mods\randomizer\src\breakables.cpp"
$breakables = Read-Normalized $breakablesPath
$collectAnchor = 'void collect_spawned_item(const fopAc_ac_c* actor) noexcept {'
if (-not $breakables.Contains($collectAnchor)) {
  throw '.729 breakable hardening could not find collect anchor.'
}
$assignmentImpl = @'
std::optional<uint8_t> spawned_item_assignment(const fopAc_ac_c* actor) noexcept {
    try {
        if (actor == nullptr) {
            return std::nullopt;
        }
        std::scoped_lock lock(s_mutex);
        ensure_collection_locked();
        const auto key = spawned_key_locked(actor);
        if (!key) {
            return std::nullopt;
        }
        const auto found = randomizer_GetContext().mBreakableOverrides.find(*key);
        if (found == randomizer_GetContext().mBreakableOverrides.end() ||
            s_collectedKeys.contains(*key))
        {
            return std::nullopt;
        }
        return found->second.itemId;
    } catch (const std::exception& error) {
        mods::log::error("Could not resolve spawned breakable item: {}", error.what());
    } catch (...) {
        mods::log::error("Could not resolve spawned breakable item: unknown error");
    }
    return std::nullopt;
}

'@
$breakables = $breakables.Replace(
  $collectAnchor, $assignmentImpl + $collectAnchor)
Write-Utf8 $breakablesPath $breakables

$hooksPath = "$src\mods\randomizer\src\hooks.cpp"
$hooks = Read-Normalized $hooksPath

$nextGetFunction = 'HookAction hookPreItemItemGetNextExecute('
$nextGetStart = $hooks.IndexOf($nextGetFunction)
if ($nextGetStart -lt 0) {
  throw '.729 breakable hardening could not find itemGetNextExecute function.'
}
$nextGetActor = '    auto* i_this = mods::arg<daItem_c*>(args, 0);'
$nextGetActorPos = $hooks.IndexOf($nextGetActor, $nextGetStart)
if ($nextGetActorPos -lt 0) {
  throw '.729 breakable hardening could not find itemGetNextExecute actor line.'
}
$nextGetInsertPos = $nextGetActorPos + $nextGetActor.Length
if (-not $hooks.Substring(
      $nextGetStart,
      [Math]::Min(1800, $hooks.Length - $nextGetStart)).Contains(
        'breakables::spawned_item_assignment(i_this)')) {
  $nextGetInjection = @'

    // A shuffled breakable reward was already resolved before this actor was
    // created. Force custom Randomizer/junk IDs through the get-demo path
    // instead of allowing the vanilla switch to disable pickup collision.
    if (breakables::spawned_item_assignment(i_this).has_value()) {
        i_this->mItemOverridden = true;
    }
'@
  $hooks = $hooks.Substring(0, $nextGetInsertPos) +
           $nextGetInjection +
           $hooks.Substring($nextGetInsertPos)
}

$itemGetFunction = 'HookAction hookPreItemItemGet('
$itemGetStart = $hooks.IndexOf($itemGetFunction)
if ($itemGetStart -lt 0) {
  throw '.729 breakable hardening could not find itemGet function.'
}
$itemGetActor = '    auto* i_this = mods::arg<daItem_c*>(args, 0);'
$itemGetActorPos = $hooks.IndexOf($itemGetActor, $itemGetStart)
if ($itemGetActorPos -lt 0) {
  throw '.729 breakable hardening could not find itemGet actor line.'
}
$itemGetInsertPos = $itemGetActorPos + $itemGetActor.Length
if (-not $hooks.Substring(
      $itemGetStart,
      [Math]::Min(2200, $hooks.Length - $itemGetStart)).Contains(
        'item::exec_item_get(*breakableItem)')) {
  $itemGetInjection = @'

    // Grant the exact breakable assignment through the Randomizer item table.
    // This handles vanilla filler, Foolish Items, progressive items, keys,
    // souls, and custom IDs without re-resolving it as a freestanding check.
    if (const auto breakableItem = breakables::spawned_item_assignment(i_this);
        breakableItem.has_value())
    {
        mDoAud_seStart(Z2SE_CONSUMP_ITEM_GET, nullptr, 0, 0);
        item::exec_item_get(*breakableItem);
        breakables::collect_spawned_item(i_this);
        return HOOK_SKIP_ORIGINAL;
    }
'@
  $hooks = $hooks.Substring(0, $itemGetInsertPos) +
           $itemGetInjection +
           $hooks.Substring($itemGetInsertPos)
}
Write-Utf8 $hooksPath $hooks

# .729: enforce beatability for every logic-enabled world.
$searchPath = "$src\mods\randomizer\generator\logic\search.cpp"
$search = Read-Normalized $searchPath
$verifyTail = @'
        return std::nullopt;
    }

    void GeneratePlaythrough(Randomizer* randomizer)
'@
if (-not $search.Contains($verifyTail)) {
  throw '.729 logic hardening could not find VerifyLogic tail.'
}
$verifyTailNew = @'
        const bool requiresBeatable = std::ranges::any_of(
            *worlds, [](const auto& world) {
                return world->Setting("Logic Rules") != "No Logic";
            });
        if (requiresBeatable && !GameBeatable(worlds, items)) {
            return "Finished seed is not beatable from the configured start.";
        }

        return std::nullopt;
    }

    void GeneratePlaythrough(Randomizer* randomizer)
'@
$search = $search.Replace($verifyTail, $verifyTailNew)
Write-Utf8 $searchPath $search

# .729: final validation after every post-fill/custom transform.
$randomizerPath = "$src\mods\randomizer\generator\randomizer.cpp"
$randomizer = Read-Normalized $randomizerPath
$finalValidationAnchor = @'
        ApplySeedAwareJunk(*this);

        // Generate Playthrough
        logic::search::GeneratePlaythrough(this);
'@
if (-not $randomizer.Contains($finalValidationAnchor)) {
  throw '.729 logic hardening could not find final post-fill validation anchor.'
}
$finalValidationNew = @'
        ApplySeedAwareJunk(*this);

        // .729: validate the final world after every post-fill/custom randomizer
        // transformation. This catches self-locks introduced by Pots, Pumpkins,
        // Shops, Souls, First-Defeat checks, entrances, Random Start, and any
        // other option represented in the finished world graph.
        UPDATE_STATUS_MESSAGE("Final logic validation...");
        if (auto finalLogicError = logic::search::VerifyLogic(&this->_worlds);
            finalLogicError.has_value())
        {
            throw std::runtime_error(
                "Final seed validation failed: " + finalLogicError.value());
        }

        // Generate Playthrough
        logic::search::GeneratePlaythrough(this);
'@
$randomizer = $randomizer.Replace($finalValidationAnchor, $finalValidationNew)
Write-Utf8 $randomizerPath $randomizer

# .729: runtime generator re-rolls invalid worlds up to 50 times.
$contextPath = "$src\mods\randomizer\src\randomizer_context.cpp"
$context = Read-Normalized $contextPath
if (-not $context.Contains('#include "ui/rando_seed_generation.hpp"')) {
  throw '.729 seed retry could not find seed-generation include anchor.'
}
if (-not $context.Contains('#include "ui/config_store.hpp"')) {
  $context = $context.Replace(
    '#include "ui/rando_seed_generation.hpp"',
    '#include "ui/rando_seed_generation.hpp"' + [string][char]10 + '#include "ui/config_store.hpp"')
}

$generateOld = @'
bool GenerateAndWriteSeed() {
    auto r = randomizer::Randomizer{::randomizer::paths::GetRandomizerPath()};

    auto generationResult = r.Generate();
    if (generationResult.has_value()) {
        randomizer::ui::UpdateGenerationStatusMsg(
            fmt::format("Failed to generate seed. Reason:\n{}", generationResult.value()));
        DeleteFailedGenerationFiles(r);
        return false;
    }

    const auto world = r.GetWorld();
    RandomizerContext randoData{};
    try {
        randoData = WriteSeedData(world);
    } catch (const std::runtime_error& e) {
        randomizer::ui::UpdateGenerationStatusMsg(
            fmt::format("Failed to write seed data. Reason:\n{}", e.what()));
        DeleteFailedGenerationFiles(r);
        return false;
    }

    randoData.mHash = r.GetConfig().GetHash();
    auto writeToFileResult = randoData.WriteToFile();
    if (writeToFileResult.has_value()) {
        randomizer::ui::UpdateGenerationStatusMsg(
            fmt::format("Failed to write seed data to file. Reason:\n{}", writeToFileResult.value()));
        DeleteFailedGenerationFiles(r);
        return false;
    }

    {
        std::scoped_lock lock(sLastGeneratedSeedMutex);
        sLastGeneratedSeedHash = randoData.mHash;
    }

    randomizer::ui::UpdateGenerationStatusMsg(fmt::format(
        "Seed generated and verified!\n\n{}", randoData.FormatSeedAuditReport()));
    return true;
}
'@
if (-not $context.Contains($generateOld)) {
  throw '.729 seed retry could not find GenerateAndWriteSeed body.'
}
$generateNew = @'
bool GenerateAndWriteSeed() {
    constexpr int kMaxSeedGenerationAttempts = 50;
    std::string lastGenerationError;

    for (int attempt = 1; attempt <= kMaxSeedGenerationAttempts; ++attempt) {
        auto r = randomizer::Randomizer{::randomizer::paths::GetRandomizerPath()};

        auto generationResult = r.Generate();
        if (generationResult.has_value()) {
            lastGenerationError = generationResult.value();
            const bool plandomizer = r.GetConfig().IsUsingPlandomizer();
            DeleteFailedGenerationFiles(r);

            if (plandomizer || attempt == kMaxSeedGenerationAttempts) {
                randomizer::ui::UpdateGenerationStatusMsg(fmt::format(
                    "Failed to generate a verified seed after {} attempt(s). Reason:\n{}",
                    attempt, lastGenerationError));
                return false;
            }

            randomizer::ui::UpdateGenerationStatusMsg(fmt::format(
                "Seed attempt {}/{} failed final validation. Re-rolling...\n{}",
                attempt, kMaxSeedGenerationAttempts, lastGenerationError));
            randomizer::ui::NewRandomSeed();
            continue;
        }

        const auto world = r.GetWorld();
        RandomizerContext randoData{};
        try {
            randoData = WriteSeedData(world);
        } catch (const std::runtime_error& e) {
            randomizer::ui::UpdateGenerationStatusMsg(
                fmt::format("Failed to write seed data. Reason:\n{}", e.what()));
            DeleteFailedGenerationFiles(r);
            return false;
        }

        randoData.mHash = r.GetConfig().GetHash();
        auto writeToFileResult = randoData.WriteToFile();
        if (writeToFileResult.has_value()) {
            randomizer::ui::UpdateGenerationStatusMsg(
                fmt::format("Failed to write seed data to file. Reason:\n{}", writeToFileResult.value()));
            DeleteFailedGenerationFiles(r);
            return false;
        }

        {
            std::scoped_lock lock(sLastGeneratedSeedMutex);
            sLastGeneratedSeedHash = randoData.mHash;
        }

        randomizer::ui::UpdateGenerationStatusMsg(fmt::format(
            "Seed generated and verified on attempt {}/{}!\n\n{}",
            attempt, kMaxSeedGenerationAttempts, randoData.FormatSeedAuditReport()));
        return true;
    }

    randomizer::ui::UpdateGenerationStatusMsg(fmt::format(
        "Failed to generate a verified seed. Reason:\n{}", lastGenerationError));
    return false;
}
'@
$context = $context.Replace($generateOld, $generateNew)
Write-Utf8 $contextPath $context

# Regression guards.
$breakablesVerify = Read-Normalized $breakablesPath
$searchVerify = Read-Normalized $searchPath
$randomizerVerify = Read-Normalized $randomizerPath
$contextVerify = Read-Normalized $contextPath

foreach ($marker in @(
  'spawned_item_assignment',
  'return found->second.itemId;')) {
  if (-not $breakablesVerify.Contains($marker)) {
    throw "Missing .729 breakable assignment marker: $marker"
  }
}
$hooksVerify = Read-Normalized $hooksPath
foreach ($marker in @(
  'breakables::spawned_item_assignment(i_this)',
  'i_this->mItemOverridden = true;',
  'item::exec_item_get(*breakableItem)',
  'breakables::collect_spawned_item(i_this)')) {
  if (-not $hooksVerify.Contains($marker)) {
    throw "Missing .729 breakable pickup marker: $marker"
  }
}
foreach ($marker in @(
  'requiresBeatable',
  'Finished seed is not beatable from the configured start.')) {
  if (-not $searchVerify.Contains($marker)) {
    throw "Missing .729 beatability marker: $marker"
  }
}
foreach ($marker in @(
  'Final logic validation...',
  'Final seed validation failed:',
  'logic::search::VerifyLogic(&this->_worlds)')) {
  if (-not $randomizerVerify.Contains($marker)) {
    throw "Missing .729 final-world validation marker: $marker"
  }
}
foreach ($marker in @(
  'kMaxSeedGenerationAttempts = 50',
  'Seed attempt {}/{} failed final validation. Re-rolling...',
  'randomizer::ui::NewRandomSeed();')) {
  if (-not $contextVerify.Contains($marker)) {
    throw "Missing .729 runtime reroll marker: $marker"
  }
}

git -C $src diff --check
if ($LASTEXITCODE -ne 0) {
  throw '.729 breakable/logic hardening failed git diff --check.'
}

Write-Host 'Applied .729 breakable pickup/collection fix, final logic validation, and 50-attempt runtime reroll hardening.'
