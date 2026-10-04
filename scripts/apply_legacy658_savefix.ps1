param(
    [Parameter(Mandatory = $true)]
    [string]$SourceRoot
)

$ErrorActionPreference = 'Stop'
$memCardPath = Join-Path $SourceRoot 'src\m_Do\m_Do_MemCard.cpp'
if (-not (Test-Path -LiteralPath $memCardPath -PathType Leaf)) {
    throw "Legacy save controller not found: $memCardPath"
}

$text = [IO.File]::ReadAllText($memCardPath).Replace("`r`n", "`n")
$utf8NoBom = New-Object System.Text.UTF8Encoding($false)

function Replace-Exact([string]$old, [string]$new, [string]$label) {
    if (-not $script:text.Contains($old)) {
        throw "Legacy 658 save-fix anchor changed: $label"
    }
    $script:text = $script:text.Replace($old, $new)
}

$includeOld = '#include "dusk/version.hpp"'
$includeNew = @(
    '#include "dusk/version.hpp"',
    '',
    '#if defined(_UWP)',
    '#include <array>',
    '#include <cstdint>',
    '#include <cstring>',
    '#include <filesystem>',
    '#include <fstream>',
    '#endif'
) -join "`n"
Replace-Exact $includeOld $includeNew 'UWP direct-save includes'

$helperOld = @(
    '#define CHECKSPACE_RESULT_ERROR    3',
    '',
    '#if PLATFORM_WII'
) -join "`n"

$helperNew = @(
    '#define CHECKSPACE_RESULT_ERROR    3',
    '',
    '#if defined(_UWP)',
    'namespace {',
    'constexpr std::array<char, 8> kLegacy658SaveMagic = {''T'',''P'',''R'',''6'',''5'',''8'',''S'',''1''};',
    'constexpr uint32_t kLegacy658SaveFormatVersion = 1;',
    '',
    'struct Legacy658SaveHeader {',
    '    std::array<char, 8> magic;',
    '    uint32_t formatVersion;',
    '    uint32_t dataVersion;',
    '    uint32_t dataSize;',
    '    uint32_t checksum;',
    '};',
    '',
    'enum class Legacy658SaveStatus { Missing, Valid, Invalid };',
    '',
    'std::filesystem::path legacy658SavePath() {',
    '    return dusk::ConfigPath / "legacy658-xbox.sav";',
    '}',
    '',
    'uint32_t legacy658Checksum(const u8* data, size_t size) {',
    '    uint32_t hash = 2166136261u;',
    '    for (size_t i = 0; i < size; ++i) {',
    '        hash ^= data[i];',
    '        hash *= 16777619u;',
    '    }',
    '    return hash;',
    '}',
    '',
    'Legacy658SaveStatus inspectLegacy658Save() {',
    '    const auto path = legacy658SavePath();',
    '    std::error_code ec;',
    '    if (!std::filesystem::exists(path, ec)) {',
    '        return ec ? Legacy658SaveStatus::Invalid : Legacy658SaveStatus::Missing;',
    '    }',
    '',
    '    std::ifstream in(path, std::ios::binary);',
    '    Legacy658SaveHeader header{};',
    '    if (!in.read(reinterpret_cast<char*>(&header), sizeof(header))) {',
    '        return Legacy658SaveStatus::Invalid;',
    '    }',
    '    if (header.magic != kLegacy658SaveMagic ||',
    '        header.formatVersion != kLegacy658SaveFormatVersion ||',
    '        header.dataVersion != SAVEDATA_VERSION ||',
    '        header.dataSize != SAVEFILE_SIZE) {',
    '        return Legacy658SaveStatus::Invalid;',
    '    }',
    '',
    '    std::array<u8, SAVEFILE_SIZE> data{};',
    '    if (!in.read(reinterpret_cast<char*>(data.data()), data.size())) {',
    '        return Legacy658SaveStatus::Invalid;',
    '    }',
    '',
    '    return legacy658Checksum(data.data(), data.size()) == header.checksum ?',
    '        Legacy658SaveStatus::Valid : Legacy658SaveStatus::Invalid;',
    '}',
    '',
    'bool readLegacy658Save(u8* outData) {',
    '    const auto path = legacy658SavePath();',
    '    std::ifstream in(path, std::ios::binary);',
    '    Legacy658SaveHeader header{};',
    '    if (!in.read(reinterpret_cast<char*>(&header), sizeof(header)) ||',
    '        header.magic != kLegacy658SaveMagic ||',
    '        header.formatVersion != kLegacy658SaveFormatVersion ||',
    '        header.dataVersion != SAVEDATA_VERSION ||',
    '        header.dataSize != SAVEFILE_SIZE) {',
    '        return false;',
    '    }',
    '',
    '    std::array<u8, SAVEFILE_SIZE> data{};',
    '    if (!in.read(reinterpret_cast<char*>(data.data()), data.size()) ||',
    '        legacy658Checksum(data.data(), data.size()) != header.checksum) {',
    '        return false;',
    '    }',
    '',
    '    std::memcpy(outData, data.data(), data.size());',
    '    return true;',
    '}',
    '',
    'bool writeLegacy658Save(const u8* data) {',
    '    const auto path = legacy658SavePath();',
    '    auto tempPath = path;',
    '    tempPath += ".tmp";',
    '    auto backupPath = path;',
    '    backupPath += ".bak";',
    '',
    '    std::error_code ec;',
    '    std::filesystem::create_directories(path.parent_path(), ec);',
    '    if (ec) {',
    '        return false;',
    '    }',
    '',
    '    Legacy658SaveHeader header{',
    '        kLegacy658SaveMagic,',
    '        kLegacy658SaveFormatVersion,',
    '        SAVEDATA_VERSION,',
    '        SAVEFILE_SIZE,',
    '        legacy658Checksum(data, SAVEFILE_SIZE),',
    '    };',
    '',
    '    {',
    '        std::ofstream out(tempPath, std::ios::binary | std::ios::trunc);',
    '        if (!out.write(reinterpret_cast<const char*>(&header), sizeof(header)) ||',
    '            !out.write(reinterpret_cast<const char*>(data), SAVEFILE_SIZE)) {',
    '            return false;',
    '        }',
    '        out.flush();',
    '        if (!out.good()) {',
    '            return false;',
    '        }',
    '    }',
    '',
    '    std::filesystem::remove(backupPath, ec);',
    '    ec.clear();',
    '    if (std::filesystem::exists(path, ec) && !ec) {',
    '        std::filesystem::rename(path, backupPath, ec);',
    '        if (ec) {',
    '            std::filesystem::remove(tempPath);',
    '            return false;',
    '        }',
    '    }',
    '',
    '    ec.clear();',
    '    std::filesystem::rename(tempPath, path, ec);',
    '    if (ec) {',
    '        std::error_code restoreEc;',
    '        if (std::filesystem::exists(backupPath, restoreEc) && !restoreEc) {',
    '            std::filesystem::rename(backupPath, path, restoreEc);',
    '        }',
    '        return false;',
    '    }',
    '',
    '    std::filesystem::remove(backupPath, ec);',
    '    return true;',
    '}',
    '',
    'bool eraseLegacy658Save() {',
    '    const auto path = legacy658SavePath();',
    '    auto tempPath = path;',
    '    tempPath += ".tmp";',
    '    auto backupPath = path;',
    '    backupPath += ".bak";',
    '    std::error_code ec;',
    '    std::filesystem::remove(path, ec);',
    '    if (ec) return false;',
    '    std::filesystem::remove(tempPath, ec);',
    '    if (ec) return false;',
    '    std::filesystem::remove(backupPath, ec);',
    '    return !ec;',
    '}',
    '}  // namespace',
    '#endif',
    '',
    '#if PLATFORM_WII'
) -join "`n"
Replace-Exact $helperOld $helperNew 'direct-save helpers'

$threadOld = @(
    '#if TARGET_PC',
    '    mCardCommand = COMM_ATTACH_e;',
    '#else',
    '    mCardCommand = COMM_NONE_e;',
    '#endif'
) -join "`n"
$threadNew = @(
    '#if defined(_UWP)',
    '    const auto directStatus = inspectLegacy658Save();',
    '    mCardState = directStatus == Legacy658SaveStatus::Valid ? CARD_STATE_READY_e :',
    '        directStatus == Legacy658SaveStatus::Missing ? CARD_STATE_NO_FILE_e : CARD_STATE_FATAL_ERROR_e;',
    '    field_0x1fc8 = 1;',
    '    mCardCommand = COMM_NONE_e;',
    '#elif TARGET_PC',
    '    mCardCommand = COMM_ATTACH_e;',
    '#else',
    '    mCardCommand = COMM_NONE_e;',
    '#endif'
) -join "`n"
Replace-Exact $threadOld $threadNew 'UWP startup card state'

$updateOld = 'void mDoMemCd_Ctrl_c::update() {' + "`n" + '    if (mDoRst::isReset()) {'
$updateNew = @(
    'void mDoMemCd_Ctrl_c::update() {',
    '#if defined(_UWP)',
    '    // Legacy 658 uses direct LocalState storage on Xbox; do not remount/rescan emulated CARD.',
    '    return;',
    '#endif',
    '    if (mDoRst::isReset()) {'
) -join "`n"
Replace-Exact $updateOld $updateNew 'disable UWP CARD probing'

$loadOld = @(
    'void mDoMemCd_Ctrl_c::load() {',
    '    if (OSTryLockMutex(&mMutex)) {',
    '        field_0x1fc8 = 0;',
    '        mCardCommand = COMM_RESTORE_e;',
    '        OSUnlockMutex(&mMutex);',
    '        OSSignalCond(&mCond);',
    '    }',
    '}'
) -join "`n"
$loadNew = @(
    'void mDoMemCd_Ctrl_c::load() {',
    '#if defined(_UWP)',
    '    if (OSTryLockMutex(&mMutex)) {',
    '        field_0x1fc8 = 0;',
    '        if (readLegacy658Save(mData)) {',
    '            mDataVersion = SAVEDATA_VERSION;',
    '            mCardState = CARD_STATE_READ_e;',
    '        } else {',
    '            const auto directStatus = inspectLegacy658Save();',
    '            mCardState = directStatus == Legacy658SaveStatus::Missing ?',
    '                CARD_STATE_NO_FILE_e : CARD_STATE_FATAL_ERROR_e;',
    '        }',
    '        field_0x1fc8 = 1;',
    '        OSUnlockMutex(&mMutex);',
    '    }',
    '    return;',
    '#endif',
    '    if (OSTryLockMutex(&mMutex)) {',
    '        field_0x1fc8 = 0;',
    '        mCardCommand = COMM_RESTORE_e;',
    '        OSUnlockMutex(&mMutex);',
    '        OSSignalCond(&mCond);',
    '    }',
    '}'
) -join "`n"
Replace-Exact $loadOld $loadNew 'direct UWP load'

$saveOld = @(
    'void mDoMemCd_Ctrl_c::save(void* i_buffer, u32 i_size, u32 i_position) {',
    '    if (OSTryLockMutex(&mMutex)) {',
    '        memcpy(&mData[i_position], i_buffer, i_size);',
    '        field_0x1fc8 = 0;',
    '        mCardCommand = COMM_STORE_e;',
    '        OSUnlockMutex(&mMutex);',
    '        OSSignalCond(&mCond);',
    '    }',
    '}'
) -join "`n"
$saveNew = @(
    'void mDoMemCd_Ctrl_c::save(void* i_buffer, u32 i_size, u32 i_position) {',
    '#if defined(_UWP)',
    '    if (OSTryLockMutex(&mMutex)) {',
    '        memcpy(&mData[i_position], i_buffer, i_size);',
    '        field_0x1fc8 = 0;',
    '        mCardState = writeLegacy658Save(mData) ? CARD_STATE_WRITE_e : CARD_STATE_FATAL_ERROR_e;',
    '        field_0x1fc8 = 1;',
    '        OSUnlockMutex(&mMutex);',
    '    }',
    '    return;',
    '#endif',
    '    if (OSTryLockMutex(&mMutex)) {',
    '        memcpy(&mData[i_position], i_buffer, i_size);',
    '        field_0x1fc8 = 0;',
    '        mCardCommand = COMM_STORE_e;',
    '        OSUnlockMutex(&mMutex);',
    '        OSSignalCond(&mCond);',
    '    }',
    '}'
) -join "`n"
Replace-Exact $saveOld $saveNew 'direct UWP save'

$formatOld = @(
    'void mDoMemCd_Ctrl_c::command_format() {',
    '    if (OSTryLockMutex(&mMutex)) {',
    '        mCardCommand = COMM_FORMAT_e;',
    '        OSUnlockMutex(&mMutex);',
    '        OSSignalCond(&mCond);',
    '    }',
    '}'
) -join "`n"
$formatNew = @(
    'void mDoMemCd_Ctrl_c::command_format() {',
    '#if defined(_UWP)',
    '    if (OSTryLockMutex(&mMutex)) {',
    '        field_0x1fc8 = 0;',
    '        mCardState = eraseLegacy658Save() ? CARD_STATE_FORMAT_e : CARD_STATE_FATAL_ERROR_e;',
    '        field_0x1fc8 = 1;',
    '        OSUnlockMutex(&mMutex);',
    '    }',
    '    return;',
    '#endif',
    '    if (OSTryLockMutex(&mMutex)) {',
    '        mCardCommand = COMM_FORMAT_e;',
    '        OSUnlockMutex(&mMutex);',
    '        OSSignalCond(&mCond);',
    '    }',
    '}'
) -join "`n"
Replace-Exact $formatOld $formatNew 'direct UWP format'

[IO.File]::WriteAllText($memCardPath, $text, $utf8NoBom)

$verify = [IO.File]::ReadAllText($memCardPath)
foreach ($marker in @(
    'legacy658-xbox.sav',
    'inspectLegacy658Save()',
    'readLegacy658Save(mData)',
    'writeLegacy658Save(mData)',
    'eraseLegacy658Save()',
    'do not remount/rescan emulated CARD'
)) {
    if (-not $verify.Contains($marker)) {
        throw "Legacy 658 save-fix marker missing after patch: $marker"
    }
}

Write-Host 'Legacy 658 Xbox direct-save backport applied successfully.'
