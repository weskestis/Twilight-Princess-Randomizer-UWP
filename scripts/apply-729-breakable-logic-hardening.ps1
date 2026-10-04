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

# .729: junk breakable checks complete as soon as their reward actor spawns.
$breakablesPath = "$src\mods\randomizer\src\breakables.cpp"
$breakables = Read-Normalized $breakablesPath

$rawItemAnchor = '        uint8_t rawItem = 0xFF;'
if (-not $breakables.Contains($rawItemAnchor)) {
  throw '.729 breakable hardening could not find raw item anchor.'
}
$breakables = $breakables.Replace(
  $rawItemAnchor,
  @'
        uint8_t rawItem = 0xFF;
        RandomizerContext::CheckContents contents =
            RandomizerContext::CheckContents::Junk;
'@)

$contentsAnchor = @'
            rawItem = override->second.itemId;
            if (rawItem == 0xFF) {
'@
if (-not $breakables.Contains($contentsAnchor)) {
  throw '.729 breakable hardening could not find override contents anchor.'
}
$breakables = $breakables.Replace(
  $contentsAnchor,
  @'
            rawItem = override->second.itemId;
            contents = override->second.contents;
            if (rawItem == 0xFF) {
'@)

$spawnCommitOld = @'
        std::scoped_lock lock(s_mutex);
        if (itemId == fpcM_ERROR_PROCESS_ID_e) {
            s_pendingKeys.erase(key);
            mods::log::error("Failed to create item {} for randomized breakable {}", item, key);
            // Let the caller use its vanilla drop rather than silently lose an
            // item. Because the location is not collected, reloading the room
            // permits another attempt.
            return false;
        }
        s_spawnedItemKeys[itemId] = key;
        if (spawnedItemId != nullptr) {
            *spawnedItemId = itemId;
        }
        return true;
'@
if (-not $breakables.Contains($spawnCommitOld)) {
  throw '.729 breakable hardening could not find spawned-item commit block.'
}
$spawnCommitNew = @'
        bool junkCheckCompleted = false;
        {
            std::scoped_lock lock(s_mutex);
            if (itemId == fpcM_ERROR_PROCESS_ID_e) {
                s_pendingKeys.erase(key);
                mods::log::error("Failed to create item {} for randomized breakable {}", item, key);
                // Let the caller use its vanilla drop rather than silently lose an
                // item. Because the location is not collected, reloading the room
                // permits another attempt.
                return false;
            }
            s_spawnedItemKeys[itemId] = key;
            if (contents == RandomizerContext::CheckContents::Junk) {
                // Junk can be rejected by vanilla capacity rules. The breakable
                // itself was successfully opened, so persist that junk check now
                // instead of letting a regrowing pot/pumpkin become gold/green again.
                s_pendingKeys.erase(key);
                junkCheckCompleted = s_collectedKeys.insert(key).second;
                if (junkCheckCompleted) {
                    persist_collection_locked();
                }
            }
            if (spawnedItemId != nullptr) {
                *spawnedItemId = itemId;
            }
        }
        if (junkCheckCompleted) {
            g_randomizerState.mUpdateTracker = true;
        }
        return true;
'@
$breakables = $breakables.Replace($spawnCommitOld, $spawnCommitNew)
Write-Utf8 $breakablesPath $breakables

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
  'contents == RandomizerContext::CheckContents::Junk',
  'junkCheckCompleted',
  'persist_collection_locked();')) {
  if (-not $breakablesVerify.Contains($marker)) {
    throw "Missing .729 junk breakable completion marker: $marker"
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

Write-Host 'Applied .729 junk breakable completion, final logic validation, and 50-attempt runtime reroll hardening.'
