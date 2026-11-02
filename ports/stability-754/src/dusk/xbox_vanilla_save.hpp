#pragma once
#include <array>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <span>
#include <string_view>
#include <vector>

namespace dusk::xbox_vanilla_save {
constexpr size_t kGciHeaderSize=64, kPayloadSize=0x8000, kQuestSize=0xA94;
enum class Status { Missing, Valid, Invalid };

inline bool read_gci(const std::filesystem::path& path,std::string_view game,
                     std::string_view maker,std::span<uint8_t> output,
                     bool (*questValid)(void*)) {
    if(game.size()!=4||maker.size()!=2||output.size()!=kQuestSize*3)return false;
    std::error_code ec;const auto status=std::filesystem::symlink_status(path,ec);
    if(ec||!std::filesystem::is_regular_file(status))return false;
    if(std::filesystem::file_size(path,ec)!=kGciHeaderSize+kPayloadSize||ec)return false;
    std::array<uint8_t,kGciHeaderSize+kPayloadSize> packet{};
    std::ifstream in(path,std::ios::binary);
    if(!in.read(reinterpret_cast<char*>(packet.data()),packet.size()))return false;
    if(std::memcmp(packet.data(),game.data(),4)||std::memcmp(packet.data()+4,maker.data(),2)||
       std::memcmp(packet.data()+8,"gczelda2\0",8)||packet[56]!=0||packet[57]!=4)return false;
    const auto versionValid=[&](size_t n){
        // PC GCI metadata is little-endian; Dolphin/original-console GCI uses big-endian.
        return (packet[n]==6&&packet[n+1]==0&&packet[n+2]==0&&packet[n+3]==0)||
               (packet[n]==0&&packet[n+1]==0&&packet[n+2]==0&&packet[n+3]==6);
    };
    const size_t primary=kGciHeaderSize+0x4000,backup=kGciHeaderSize+0x6000;
    const bool primaryVersion=versionValid(primary+4),backupVersion=versionValid(backup+4);
    if(!primaryVersion&&!backupVersion)return false;
    bool anyValid=false;
    std::array<uint8_t,kQuestSize*3> recovered{};
    for(size_t i=0;i<3;++i){
        auto* original=packet.data()+primary+8+i*kQuestSize;
        auto* alternate=packet.data()+backup+8+i*kQuestSize;
        const bool originalValid=primaryVersion&&questValid(original);
        const bool alternateValid=backupVersion&&questValid(alternate);
        anyValid=anyValid||originalValid||alternateValid;
        std::memcpy(recovered.data()+i*kQuestSize,
                    !originalValid&&alternateValid?alternate:original,kQuestSize);
    }
    if(anyValid)std::memcpy(output.data(),recovered.data(),recovered.size());
    return anyValid;
}

inline Status inspect(const std::filesystem::path& path,std::string_view game,
                      std::string_view maker,bool (*questValid)(void*)) {
    std::array<uint8_t,kQuestSize*3> data{};
    std::error_code ec;
    if(!std::filesystem::exists(path,ec)){
        if(ec)return Status::Invalid;
        auto backup=path;backup+=".bak";
        if(!std::filesystem::exists(backup,ec))return ec?Status::Invalid:Status::Missing;
        if(!read_gci(backup,game,maker,data,questValid))return Status::Invalid;
        std::filesystem::rename(backup,path,ec);
        if(ec)return Status::Invalid;
    }
    return read_gci(path,game,maker,data,questValid)?Status::Valid:Status::Invalid;
}

inline bool write_atomic_packet(const std::filesystem::path& path,std::span<const uint8_t> packet) {
    if(packet.empty()||packet.size()>kGciHeaderSize+kPayloadSize)return false;
    std::error_code ec;std::filesystem::create_directories(path.parent_path(),ec);
    if(ec)return false;
    const auto status=std::filesystem::symlink_status(path,ec);
    if(ec&&ec!=std::errc::no_such_file_or_directory)return false;
    if(std::filesystem::exists(status)&&!std::filesystem::is_regular_file(status))return false;
    auto temporary=path;temporary+=".tmp";auto backup=path;backup+=".bak";
    for(const auto& candidate:{temporary,backup}){
        ec.clear();const auto candidateStatus=std::filesystem::symlink_status(candidate,ec);
        if((ec&&ec!=std::errc::no_such_file_or_directory)||
           (std::filesystem::exists(candidateStatus)&&!std::filesystem::is_regular_file(candidateStatus)))return false;
    }
    {
        std::ofstream out(temporary,std::ios::binary|std::ios::trunc);
        if(!out.write(reinterpret_cast<const char*>(packet.data()),packet.size()))return false;
        out.flush();if(!out.good())return false;out.close();if(!out.good())return false;
    }
    const bool existed=std::filesystem::exists(status);
    if(existed){
        ec.clear();std::filesystem::remove(backup,ec);if(ec)return false;
        std::filesystem::rename(path,backup,ec);if(ec)return false;
    }
    ec.clear();std::filesystem::rename(temporary,path,ec);
    if(ec){if(existed){std::error_code restore;std::filesystem::rename(backup,path,restore);}return false;}
    // Keep the preceding complete GCI for recovery; never delete a legacy save during inspection.
    return true;
}

inline bool write_atomic(const std::filesystem::path& path,std::span<const uint8_t> packet) {
    return packet.size()==kGciHeaderSize+kPayloadSize&&write_atomic_packet(path,packet);
}
} // namespace dusk::xbox_vanilla_save
