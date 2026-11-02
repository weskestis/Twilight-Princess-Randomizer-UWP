#include "content.hpp"
#include "field_effects.hpp"
#include "../field/roster.hpp"
#include "mods/svc/config.h"
#include "mods/svc/file.h"
#include "mods/svc/resource.h"
#include <array>
#include <cstdio>

namespace pokemon_battle::content {
namespace {
constexpr std::size_t limit=40u*1024u*1024u;
struct Entry {
    ConfigVarHandle config=0;
    std::weak_ptr<runtime::GxAsset> cached;
    std::string location;
};
std::array<Entry,20> entries{};
bool effects_ready=false;
bool ensure_effects(std::string& error) {
    if(effects_ready)return true;
    ResourceBuffer buffer=RESOURCE_BUFFER_INIT;
    if(svc_resource->load(mod_ctx,"pokemon/effects.dpfx",&buffer)!=MOD_OK) {error="Pokemon move effects are missing from this build.";return false;}
    struct Free { ResourceBuffer& buffer;~Free(){svc_resource->free(mod_ctx,&buffer);} } free{buffer};
    if(!buffer.data || buffer.size>4u*1024u*1024u) {error="Invalid Pokemon effect pack size.";return false;}
    effects_ready=runtime::load_effects({static_cast<const std::uint8_t*>(buffer.data),buffer.size},error);return effects_ready;
}
Entry* entry_for(std::uint16_t id) {
    const auto roster=field::roster();
    for(std::size_t i=0;i<roster.size();++i)if(roster[i].id==id)return &entries[i];
    return nullptr;
}
bool read_external(const char* location,runtime::Model& model,std::string& error) {
    FileStreamHandle stream=0;
    if(!svc_file || svc_file->open(mod_ctx,location,FILE_OPEN_READ,&stream)!=MOD_OK) {
        error="The selected content patch is unavailable. Restore the bundled model or select the patch again.";return false;
    }
    struct Close { FileStreamHandle stream;~Close(){svc_file->close(mod_ctx,stream);} } close{stream};
    std::uint64_t size=0,read=0;
    if(svc_file->size(mod_ctx,stream,&size)!=MOD_OK || size<60 || size>limit) { error="Content patch exceeds the model budget.";return false; }
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
    if(svc_file->read(mod_ctx,stream,bytes.data(),size,&read)!=MOD_OK || read!=size) {error="Content patch could not be read completely.";return false;}
    return model.load(bytes,error);
}
}
std::uint16_t identify(const runtime::Model& model) {
    for(const auto& species:field::roster()) {
        const std::string prefix=std::string(species.asset)+"_";
        bool matches=!model.clips().empty(),idle=false,faint=false,hurt=false,walk=false,run=false;
        for(const auto& clip:model.clips()) {
            matches=matches && clip.name.starts_with(prefix);
            idle=idle || clip.name.ends_with("_Idle");faint=faint || clip.name.ends_with("_Faint");hurt=hurt || clip.name.ends_with("_Damage");
            walk=walk || clip.name.ends_with("_FieldWalk") || clip.name.ends_with("_15_Walk");
            run=run || clip.name.ends_with("_FieldRun") || clip.name.ends_with("_16_Run");
        }
        if(matches && idle && faint && hurt && walk && run)return species.id;
    }
    return 0;
}
bool initialize(std::string& error) {
    if(!svc_config || !svc_resource || !svc_file) {error="Content services are unavailable.";return false;}
    const auto roster=field::roster();
    for(std::size_t i=0;i<roster.size();++i) {
        ConfigVarDesc config=CONFIG_VAR_DESC_INIT;
        const auto key="model-"+std::to_string(roster[i].id);config.name=key.c_str();config.type=CONFIG_VAR_STRING;
        if(svc_config->register_var(mod_ctx,&config,&entries[i].config)!=MOD_OK) {error="Content settings could not be registered.";shutdown();return false;}
        std::size_t size=0;
        if(svc_config->get_string(mod_ctx,entries[i].config,nullptr,0,&size)!=MOD_OK || size>16384) {error="Content setting is invalid.";shutdown();return false;}
        if(size) {
            std::vector<char> value(size+1);
            if(svc_config->get_string(mod_ctx,entries[i].config,value.data(),value.size(),nullptr)!=MOD_OK) {error="Content setting could not be read.";shutdown();return false;}
            entries[i].location=value.data();
        }
    }
    return true;
}
void shutdown() {
    runtime::release_effects();effects_ready=false;
    for(auto& entry:entries) {
        if(entry.config && svc_config)svc_config->unregister_var(mod_ctx,entry.config);
        entry=Entry{};
    }
}
std::shared_ptr<runtime::GxAsset> get(std::uint16_t id,std::string& error) {
    auto* entry=entry_for(id);if(!entry) {error="Species is not in this roster.";return {};}
    if(auto held=entry->cached.lock())return held;
    runtime::Model model;
    try {
        if(!ensure_effects(error))return {};
        if(!entry->location.empty()) {
            if(!read_external(entry->location.c_str(),model,error))return {};
        } else {
            char path[32];std::snprintf(path,sizeof(path),"pokemon/%03u.dpkm",unsigned(id));
            ResourceBuffer buffer=RESOURCE_BUFFER_INIT;
            if(svc_resource->load(mod_ctx,path,&buffer)!=MOD_OK) {error=std::string(field::species_info(id)->name)+" content is missing from this build.";return {};}
            struct Free { ResourceBuffer& buffer;~Free(){svc_resource->free(mod_ctx,&buffer);} } free{buffer};
            if(!buffer.data || buffer.size>limit) {error="Invalid Pokemon model size.";return {};}
            if(!model.load({static_cast<const std::uint8_t*>(buffer.data),buffer.size},error))return {};
        }
        if(identify(model)!=id) {error="Model species or required animations do not match.";return {};}
        auto asset=std::make_shared<runtime::GxAsset>(std::move(model));entry->cached=asset;return asset;
    } catch(const std::exception& e) {error=std::string("Content load failed: ")+e.what();return {};}
}
bool install(runtime::Model model,const char* location,std::uint16_t& id,std::string& error) {
    const auto species=identify(model);auto* entry=entry_for(species);
    if(!entry || !location || !*location || model.bytes()>limit) {error="Select a supported species pack with its complete field animations.";return false;}
    if(svc_config->set_string(mod_ctx,entry->config,location)!=MOD_OK) {error="Content selection could not be saved.";return false;}
    entry->location=location;entry->cached.reset();id=species;
    return true;
}
bool restore(std::uint16_t id,std::string& error) {
    auto* entry=entry_for(id);if(!entry)return false;
    if(svc_config->set_string(mod_ctx,entry->config,"")!=MOD_OK) {error="Content selection could not be saved.";return false;}
    entry->location.clear();entry->cached.reset();return true;
}
} // namespace pokemon_battle::content
