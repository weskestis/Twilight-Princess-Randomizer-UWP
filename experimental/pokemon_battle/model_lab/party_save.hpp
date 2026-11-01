#pragma once
#include "../field/party.hpp"
#include "mods/svc/save.h"

namespace pokemon_battle::field {
constexpr const char* party_blob_name="field-party-v1";
enum class LoadResult { Loaded, Created, Unavailable, Invalid };
inline LoadResult load_saved_party(const SaveService* service,ModContext* context,Party& out,std::string& error) {
    if(!service || !service->get_blob || !service->set_blob) return LoadResult::Unavailable;
    std::size_t size=0;
    const auto result=service->get_blob(context,party_blob_name,nullptr,&size);
    if(result==MOD_UNAVAILABLE) {
        Party initial;
        const auto bytes=encode_party(initial);
        if(service->set_blob(context,party_blob_name,bytes.data(),bytes.size())!=MOD_OK) return LoadResult::Unavailable;
        out=initial; return LoadResult::Created;
    }
    if(result!=MOD_OK || size>190 || size<28) {
        error="Party data could not be read. Existing save data was preserved."; return LoadResult::Invalid;
    }
    std::vector<std::uint8_t> bytes(size);
    if(service->get_blob(context,party_blob_name,bytes.data(),&size)!=MOD_OK || size!=bytes.size()) {
        error="Party data could not be read. Existing save data was preserved."; return LoadResult::Invalid;
    }
    return decode_party(bytes,out,error)?LoadResult::Loaded:LoadResult::Invalid;
}
}
