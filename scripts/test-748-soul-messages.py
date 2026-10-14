"""Run Soul registration through the actual SDK and native registry, including legacy seeds."""
import argparse, os, subprocess, tempfile
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('source',type=Path);p.add_argument('--negative-control',action='store_true');a=p.parse_args();r=a.source.resolve()

def function(text,signature):
    start=text.index(signature);brace=text.index('{',start);depth=1;pos=brace+1
    while depth:
        depth+=(text[pos]=='{')-(text[pos]=='}');pos+=1
    return text[start:pos]+'\n'

messages=(r/'mods/randomizer/src/messages.cpp').read_text();engine=(r/'src/dusk/mods/svc/flow.cpp').read_text()
cpp=r'''
#include <mods/svc/flow.hpp>
#include "enemy_soul_catalog.hpp"
#include <cassert>
#include <cstdio>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
using mods::read_bits;using mods::write_bits;
using u16=uint16_t;
/*SOURCE_MESSAGE_CONSTANTS*/
struct ModContext {};
ModContext fixtureContext;ModContext* mod_ctx=&fixtureContext;
const MessageService* svc_message=nullptr;
namespace mods { struct LoadedMod { struct { const char* id="test"; } metadata; };
namespace log { template<class... T>void info(T&&...){} template<class... T>void warn(T&&...){} }
}
mods::LoadedMod fixtureOwner;
mods::LoadedMod* mod_from_context(ModContext* context){return context==&fixtureContext?&fixtureOwner:nullptr;}
struct { template<class... T>void error(T&&...){} } DuskLog;
namespace JMessage::data { constexpr unsigned char gcTagBegin=26; }
#define MSGTAG_GROUP(g) (g << 16)
#define MSGTAG_COLOR (MSGTAG_GROUP(255) | 0)
namespace flow {
constexpr uint16_t kGroupMax=8,kCustomMessageMin=0x8000,kCustomMessageMax=0xfffe;
uint16_t s_nextMessage=kCustomMessageMin;uint64_t s_nextHandle=1;
std::vector<uint16_t> s_freeMessages;
'''
for signature in ['struct MessageVariantRecord','struct MessageRecord']:cpp+=function(engine,signature)+';\n'
cpp+=r'''
std::unordered_map<MessageId,std::shared_ptr<MessageRecord>> s_messages;
std::unordered_map<MessageHandle,std::shared_ptr<MessageRecord>> s_messagesByHandle;
'''
for signature in ['bool valid_group(','bool valid_encoded_text(','bool custom_message_group(']:cpp+=function(engine,signature)
cpp+='}\n'
for signature in ['ModResult register_message(','ModResult remove_message(']:cpp+=function(engine,signature)
cpp+=r'''
MessageService fixtureService{};
struct RandomizerContext { std::unordered_map<int,std::unordered_map<uint32_t,std::string>> mTextOverrides; };
namespace fmt { template<class... T>std::string format(T&&...){return {}; } }
namespace randomizer::messages {
constexpr uint16_t kMessageGroupCount=9;
constexpr uint32_t kPoeSoulGetMessage=325,kSkyCharacterGetMessage=335;
std::string s_lastActivationError;
std::vector<mods::flow::RegisteredMessage> s_messages;
std::vector<mods::flow::MessageOverride> s_overrides;
bool formatted_override(ModContext*,const MessageOverrideContext*,MessageTextData*,void*){return false;}
'''
cpp+=function(messages,'std::vector<uint8_t> encoded_text(')
if 'ModResult register_soul_pickup_messages(' in messages:
    assert not a.negative_control
    assert 'result = register_soul_pickup_messages(context);' in function(messages,'ModResult activate(')
    assert 's_soulPickupMessages.clear();' in function(messages,'void deactivate()')
    cpp+='std::unordered_map<std::uint32_t,MessageId> s_soulPickupMessages;\n'
    for signature in ['bool is_soul_pickup_message(','ModResult register_soul_pickup_messages(','std::uint32_t pickup_message_id(']:cpp+=function(messages,signature)
else:
    assert a.negative_control
cpp+=function(messages,'ModResult register_native_overrides(')
cpp+='}\n'
cpp+=r'''
int main(){
    fixtureService.register_message=register_message;fixtureService.remove_message=remove_message;svc_message=&fixtureService;
    RandomizerContext context;
    // The original seed's synthetic group-9 Huge Poe text must be migrated at runtime.
    for(const auto language:{MESSAGE_LANGUAGE_ENGLISH,MESSAGE_LANGUAGE_GERMAN,MESSAGE_LANGUAGE_FRENCH,
                            MESSAGE_LANGUAGE_SPANISH,MESSAGE_LANGUAGE_ITALIAN,MESSAGE_LANGUAGE_JAPANESE})
        context.mTextOverrides[language][(CUSTOM_BMG_GROUP<<16)|(ENEMY_SOUL_MESSAGE_BASE+0x2F)]="Huge Poe Soul legacy seed text";
'''
if a.negative_control:
    cpp+=r'''
    assert(randomizer::messages::register_native_overrides(context)==MOD_OK);
    uint16_t group=0;assert(!flow::custom_message_group(ENEMY_SOUL_MESSAGE_BASE+0x2F,group));
    assert(flow::s_messages.empty());
    puts("PASS: shipped .747 discards group-9 Huge Poe text and registers no native pickup entry");
}
'''
else:
    cpp+=r'''
    using namespace randomizer::messages;
    assert(register_soul_pickup_messages(context)==MOD_OK);
    assert(s_soulPickupMessages.size()==93&&flow::s_messages.size()==93);
    const auto huge=pickup_message_id(ENEMY_SOUL_MESSAGE_BASE+0x2F);
    for(const auto& [source,id]:s_soulPickupMessages){
        assert(id>=flow::kCustomMessageMin);uint16_t group=9;assert(flow::custom_message_group(id,group)&&group==0);
        const auto& record=*flow::s_messages.at(id);assert(record.variants.size()==6);
        for(const auto& [language,variant]:record.variants){
            assert(variant->entry.bytes[9]==MESSAGE_BOX_ITEM_GET&&variant->entry.bytes[11]==MESSAGE_POSITION_MIDDLE);
            assert(read_bits<uint16_t>(variant->entry.bytes+4)==id&&variant->text.back()==0);
            assert(flow::valid_encoded_text(variant->text.data(),variant->text.size()));
            const std::string text(reinterpret_cast<const char*>(variant->text.data()));
            if(source==ENEMY_SOUL_MESSAGE_BASE+0x2F)assert(text=="Huge Poe Soul legacy seed text");
            else if(source>=ENEMY_SOUL_MESSAGE_BASE)assert(text.find(randomizer::enemy_souls::kDefinitions[source-ENEMY_SOUL_MESSAGE_BASE].item)!=std::string::npos);
        }
    }
    assert(pickup_message_id(325)==325&&pickup_message_id(335)==335&&pickup_message_id(huge)==huge);
    // Teardown retires both the remapping and actual native message entries.
    s_soulPickupMessages.clear();s_messages.clear();assert(flow::s_messages.empty());
    assert(pickup_message_id(ENEMY_SOUL_MESSAGE_BASE+0x2F)==ENEMY_SOUL_MESSAGE_BASE+0x2F);
    // A legacy seed missing all reserved Soul text still gets every complete fallback.
    context.mTextOverrides.clear();assert(register_soul_pickup_messages(context)==MOD_OK);
    const auto newHuge=pickup_message_id(ENEMY_SOUL_MESSAGE_BASE+0x2F);
    assert(newHuge!=huge&&flow::s_messages.size()==93);
    assert(std::string(reinterpret_cast<const char*>(flow::s_messages.at(newHuge)->variants.at(0)->text.data())).find("Huge Poe Soul")!=std::string::npos);
    s_soulPickupMessages.clear();s_messages.clear();flow::s_nextMessage=0xffff;
    assert(register_soul_pickup_messages(context)==MOD_UNAVAILABLE&&s_soulPickupMessages.empty());
    puts("PASS: actual SDK/native registry routes 85 Enemy + 8 Boss Soul item textboxes in group 0 across six languages, migrates old Huge Poe seeds, supplies missing text and retires mappings on restart");
}
'''
constants='\n'.join(line for line in (r/'mods/randomizer/src/custom_flow_ids.hpp').read_text().splitlines() if line.startswith('inline constexpr'))
cpp=cpp.replace('/*SOURCE_MESSAGE_CONSTANTS*/',constants)
with tempfile.TemporaryDirectory(prefix='soul-messages-748-') as temp:
    w=Path(temp);source=w/'test.cpp';source.write_text(cpp,encoding='utf-8');exe=w/('test.exe' if os.name=='nt' else 'test')
    includes=[r/'sdk/include',r/'mods/randomizer/src',r/'include']
    command=['cl','/nologo','/EHsc','/std:c++20','/O2',*['/I'+str(i) for i in includes],str(source),'/Fe:'+str(exe)] if os.name=='nt' else ['c++','-std=c++20','-O2',*['-I'+str(i) for i in includes],str(source),'-o',str(exe)]
    subprocess.run(command,cwd=w,check=True);subprocess.run([str(exe)],cwd=w,check=True)
