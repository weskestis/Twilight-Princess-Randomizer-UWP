#include "../model_lab/party_save.hpp"
#include <array>
#include <cstdlib>
#include <cstring>
#include <iostream>

using namespace pokemon_battle::field;
namespace {
void require(bool condition,const char* text) { if(!condition) { std::cerr<<text<<'\n'; std::exit(1); } }
std::array<std::vector<std::uint8_t>,3> slots;
int slot=-1,writes=0;
bool read_fail=false,write_fail=false,short_read=false;
ModResult read(ModContext*,const char* name,void* buffer,std::size_t* size) {
    require(std::strcmp(name,party_blob_name)==0,"scoped party key");
    if(read_fail) return MOD_ERROR;
    if(slot<0 || slots[static_cast<std::size_t>(slot)].empty()) return MOD_UNAVAILABLE;
    const auto& bytes=slots[static_cast<std::size_t>(slot)];
    if(buffer && *size<bytes.size()) return MOD_INVALID_ARGUMENT;
    if(buffer) std::memcpy(buffer,bytes.data(),bytes.size());
    *size=bytes.size()-(buffer && short_read?1u:0u); return MOD_OK;
}
ModResult write(ModContext*,const char*,const void* data,std::size_t size) {
    ++writes;
    if(slot<0 || write_fail) return MOD_UNAVAILABLE;
    const auto* first=static_cast<const std::uint8_t*>(data);
    slots[static_cast<std::size_t>(slot)].assign(first,first+size); return MOD_OK;
}
}
int main() {
    SaveService service{}; service.get_blob=read; service.set_blob=write;
    Party out; out.balls=7; std::string error;
    require(load_saved_party(&service,nullptr,out,error)==LoadResult::Unavailable && out.balls==7,"missing active slot cannot initialize a party");
    slot=0; require(load_saved_party(&service,nullptr,out,error)==LoadResult::Created && out.balls==24,"new slot initialized");
    auto captured=out; captured.count=1; captured.next_id=2; captured.members[0].id=1; captured.members[0].hp=5;
    slots[0]=encode_party(captured);
    require(load_saved_party(&service,nullptr,out,error)==LoadResult::Loaded && out==captured,"actual SDK adapter restores member");
    slot=1; require(load_saved_party(&service,nullptr,out,error)==LoadResult::Created && out.count==0,"separate slot has separate party");
    slot=0; require(load_saved_party(&service,nullptr,out,error)==LoadResult::Loaded && out==captured,"return to first slot restores its party");
    const auto original=slots; const auto before=writes;
    read_fail=true; require(load_saved_party(&service,nullptr,out,error)==LoadResult::Invalid && slots==original && writes==before,"read error never initializes over existing save");
    read_fail=false; short_read=true;
    require(load_saved_party(&service,nullptr,out,error)==LoadResult::Invalid && out==captured && writes==before,"short read preserves state");
    short_read=false; slots[0][8]^=1;
    require(load_saved_party(&service,nullptr,out,error)==LoadResult::Invalid && out==captured && writes==before,"corrupted data preserved for recovery");
    slots[0].resize(10000);
    require(load_saved_party(&service,nullptr,out,error)==LoadResult::Invalid && writes==before,"oversized blob rejected before allocation");
    slot=2; write_fail=true;
    require(load_saved_party(&service,nullptr,out,error)==LoadResult::Unavailable && out==captured && slots[2].empty(),"failed initialization does not activate a new party");
    std::cout<<"PASS: real SaveService adapter, independent slots, reload, missing slot, failed reads/writes, corrupt and oversized blobs\n";
}
