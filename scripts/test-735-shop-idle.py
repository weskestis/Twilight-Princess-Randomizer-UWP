"""Compile real shop setup, shelf access, and retained message ownership functions."""
from __future__ import annotations
import argparse
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile

def function(source: str, signature: str) -> str:
    start = source.index(signature)
    token = re.compile(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|//[^\n]*|/\*[\s\S]*?\*/|[{}]')
    depth = 0
    opened = False
    for match in token.finditer(source, start):
        if match.group() == '{':
            opened = True
            depth += 1
        elif match.group() == '}':
            depth -= 1
            if opened and depth == 0:
                return source[start:match.end()]
    raise RuntimeError(f'Unclosed production function: {signature}')

parser = argparse.ArgumentParser()
parser.add_argument('source', nargs='?', default=os.environ.get('TPR_SRC'))
parser.add_argument('--original-sort-negative-control', action='store_true')
args = parser.parse_args()
root = Path(args.source)
shop = (root/'src/d/d_shop_system.cpp').read_text()
ctrl = (root/'src/d/d_shop_item_ctrl.cpp').read_text()
header = (root/'include/d/d_shop_item_ctrl.h').read_text()
flow = (root/'src/dusk/mods/svc/flow.cpp').read_text()
hooks = (root/'mods/randomizer/src/hooks.cpp').read_text()
texts = (root/'mods/randomizer/src/shop_items.cpp').read_text()
message_control = (root/'libs/JSystem/src/JMessage/control.cpp').read_text()
assert 'tpr_xbox_retain_shop_text(control, text, shops::text_override_size())' in hooks
assert 's_pendingTextKeys.clear();' in function(texts,'void clear_scene_actors()')
for signature in ['JMessage::TControl::~TControl()', 'void JMessage::TControl::reset()']:
    assert 'release_message_control(this);' in function(message_control,signature)

preamble = r'''
#define _UWP 1
#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <memory>
#include <ranges>
#include <string>
#include <unordered_map>
#include <vector>
using u8 = uint8_t; using u16 = uint16_t; using u32 = uint32_t; using s16 = int16_t; using f32 = float;
struct cXyz {
    float x=0,y=0,z=0;
    cXyz() = default;
    cXyz(float a,float b,float c) : x(a),y(b),z(c) {}
    void set(const cXyz& other) { *this=other; }
    void set(float a,float b,float c) { x=a;y=b;z=c; }
};
struct fopAc_ac_c {
    u32 id=0,parameters=0;
    struct { struct { u16 z=0xFFFF; } angle; } home;
    struct { cXyz pos; } current;
    int profile=1;
};
struct daTag_ShopItem_c : fopAc_ac_c {
    u32 processId=0;
    int flowNode=0;
    u32 getProcessID() { return processId; }
    int getFlowNodeNum() { return flowNode; }
};
std::unordered_map<u32,fopAc_ac_c*> actors;
fopAc_ac_c* fopAcM_SearchByID(u32 id) {
    const auto found=actors.find(id);
    return found==actors.end()?nullptr:found->second;
}
bool fopAcM_IsActor(void* actor) { return actor!=nullptr; }
int fopAcM_GetName(void* actor) { return static_cast<fopAc_ac_c*>(actor)->profile; }
u32 fopAcM_GetParam(void* actor) { return static_cast<fopAc_ac_c*>(actor)->parameters; }
bool dComIfGs_isSaveSwitch(u8) { return false; }
void dComIfGs_onSaveSwitch(u8) {}
bool checkItemGet(u8,bool) { return false; }
constexpr int fpcNm_TAG_SHOPITM_e=1;
constexpr u8 dItemNo_NONE_e=0xFF,dItemNo_HYLIA_SHIELD_e=44;
#define JUT_ASSERT(...) ((void)0)
/*CONTROL_CLASS*/
/*CONTROL_CONSTRUCTOR*/
/*CONTROL_DESTRUCTOR*/
/*CONTROL_GET_POSITION*/
struct ShopCamera {
    template<class... T> void setCamDataIdx(T...) {}
    template<class... T> void setCamDataIdx2(T...) {}
    void setMasterCamCtrPos(cXyz*) {}
};
struct dShopSystem_c : fopAc_ac_c {
    enum { ITEM_MAX_e=7 };
    u8 mSoldOutFlag=0,mMasterType=0;
    u16 flags=0;u8 soldFlags=0;
    dShopItemCtrl_c mItemCtrl;
    ShopCamera mShopCamAction;
    void onFlag(int i) { flags|=static_cast<u16>(1u<<i); }
    void offFlag(int i) { flags&=static_cast<u16>(~(1u<<i)); }
    void onSoldOutItemFlag(int i) { soldFlags|=static_cast<u8>(1u<<i); }
    void offSoldOutItemFlag(int i) { soldFlags&=static_cast<u8>(~(1u<<i)); }
    bool searchItemActor();
};
daTag_ShopItem_c* dShopSystem_itemActor[7]{};
u8 dShopSystem_itemNo[7]{};
u8 dShopSystem_sellItemMax=3,data_80451060=0;
int dShopSystem_item_count=0,dShopSystem_camera_count=0;
fopAc_ac_c* dShopSystem_cameraActor[2]{};
float fopAcM_searchActorDistance(fopAc_ac_c*,fopAc_ac_c* actor) {
    assert(actor!=nullptr);return actor->current.pos.x;
}
using fpcLyIt_JudgeFunc=int(*)(void*,void*);
void fpcEx_Search(fpcLyIt_JudgeFunc,void*) {}
int dShopSystem_searchCameraActor(void*,void*) { return 0; }
/*STOCK_SCAN*/
/*SHOP_SEARCH*/
namespace JMessage { struct TControl {}; struct TProcessor {}; }
struct MessageVariantRecord {};
/*ACTIVE_BINDING*/
std::unordered_map<const JMessage::TControl*,ActiveBinding> s_bindings;
std::unordered_map<const JMessage::TProcessor*,ActiveBinding> s_processorBindings;
/*RETAIN_BINDING*/
/*RETAIN_SHOP_TEXT*/
/*RELEASE_MESSAGE*/
void reset_stock(int count) {
    std::fill_n(dShopSystem_itemActor,7,nullptr);
    std::fill_n(dShopSystem_itemNo,7,dItemNo_NONE_e);
    dShopSystem_sellItemMax=static_cast<u8>(count);
    dShopSystem_item_count=0;
    dShopSystem_camera_count=0;
    data_80451060=0;
    dShopSystem_cameraActor[0]=dShopSystem_cameraActor[1]=nullptr;
}
int main() {
    std::array<daTag_ShopItem_c,7> tags;
    fopAc_ac_c camera0,camera1;
    dShopSystem_c shop;
    for(int count=1;count<=7;++count) {
        std::vector<int> order;
        for(int i=0;i<count;++i) order.push_back(i);
        do {
            reset_stock(count);
            for(int i=0;i<count;++i) {
                tags[i].id=static_cast<u32>(100+i);
                tags[i].processId=static_cast<u32>(200+i);
                tags[i].flowNode=300+i;
                tags[i].parameters=static_cast<u32>(60+i);
                tags[i].current.pos.x=static_cast<float>(order[i]);
                dShopSystem_searchItemActor(&tags[i],&shop);
            }
            assert(dShopSystem_item_count==count);
            dShopSystem_camera_count=2;
            dShopSystem_cameraActor[0]=&camera0;dShopSystem_cameraActor[1]=&camera1;
            assert(shop.searchItemActor());
            for(int i=0;i<count;++i) {
                auto* tag=dShopSystem_itemActor[i];
                assert(tag->current.pos.x==static_cast<float>(i));
                assert(shop.mItemCtrl.getItemIndex(i)==tag->processId);
                assert(shop.mItemCtrl.getMessageIndex(i)==tag->flowNode);
                assert(dShopSystem_itemNo[i]==(tag->parameters&0xFF));
            }
        } while(std::next_permutation(order.begin(),order.end()));
    }
    // Idle calls must keep completed stock stable without searching it again.
    for(int i=0;i<10000;++i) assert(shop.searchItemActor());
    reset_stock(3);
    tags[0].parameters=(1u<<24)|10;
    tags[1].parameters=(1u<<24)|11;
    dShopSystem_searchItemActor(&tags[0],&shop);
    dShopSystem_searchItemActor(&tags[1],&shop);
    assert(dShopSystem_item_count==1&&dShopSystem_itemActor[0]==&tags[0]);
    tags[2].parameters=12;
    dShopSystem_searchItemActor(&tags[2],&shop);
    assert(dShopSystem_item_count==2&&dShopSystem_itemActor[1]==&tags[2]);
    for(int bad : {8,15}) {
        tags[3].parameters=static_cast<u32>(bad)<<24;
        dShopSystem_searchItemActor(&tags[3],&shop);
        assert(dShopSystem_item_count==2);
    }
    dShopSystem_item_count=3;
    dShopSystem_camera_count=2;
    dShopSystem_cameraActor[0]=&camera0;dShopSystem_cameraActor[1]=&camera1;
    assert(!shop.searchItemActor()); // count with a hole
    dShopSystem_itemActor[2]=&tags[4];dShopSystem_cameraActor[1]=nullptr;
    assert(!shop.searchItemActor()); // count with a missing camera
    for(int max : {0,8,255}) {
        reset_stock(max);assert(!shop.searchItemActor());
    }
    dShopItemCtrl_c ctrl;
    for(int i=0;i<7;++i) { ctrl.setItemIndex(i,static_cast<u32>(i+1));ctrl.setMessageIndex(i,static_cast<u16>(i+50)); }
    for(int bad : {-32768,-1,7,9999}) {
        ctrl.setItemIndex(bad,123);ctrl.setMessageIndex(bad,456);
        assert(ctrl.getItemIndex(bad)==UINT32_MAX&&ctrl.getMessageIndex(bad)==0);
        auto pos=ctrl.getCurrentPos(bad);assert(pos.x==0&&pos.y==0&&pos.z==0);
    }
    for(int i=0;i<7;++i) assert(ctrl.getItemIndex(i)==static_cast<u32>(i+1)&&ctrl.getMessageIndex(i)==i+50);
    JMessage::TControl first,second;
    std::string scratch="First randomized shelf";
    const std::string expected=scratch;
    const char* firstText=tpr_xbox_retain_shop_text(&first,scratch.data(),scratch.size());
    assert(firstText!=nullptr);
    scratch.assign(8000,'B'); // invalidates the original mutable string storage
    const char* secondText=tpr_xbox_retain_shop_text(&second,scratch.data(),scratch.size());
    assert(secondText!=nullptr&&std::strcmp(firstText,expected.c_str())==0);
    const std::array<char,7> binary{'X','\x1a','\x05','\x00','\x00','\x20','Y'};
    const char* binaryText=tpr_xbox_retain_shop_text(&first,binary.data(),binary.size());
    assert(binaryText!=nullptr&&std::memcmp(binaryText,binary.data(),binary.size())==0);
    assert(std::strcmp(firstText,expected.c_str())==0); // another entry for this owner stays retained
    assert(s_bindings.at(&first).texts.size()==2);
    release_message_control(&second);
    assert(s_bindings.size()==1&&std::strcmp(firstText,expected.c_str())==0);
    release_message_control(&first);assert(s_bindings.empty());
    assert(tpr_xbox_retain_shop_text(nullptr,binary.data(),binary.size())==nullptr);
    assert(tpr_xbox_retain_shop_text(&first,nullptr,1)==nullptr);
    assert(tpr_xbox_retain_shop_text(&first,binary.data(),0)==nullptr);
    assert(tpr_xbox_retain_shop_text(&first,binary.data(),8193)==nullptr);
    assert(s_bindings.empty());
    std::cout<<"PASS: all shelf-sort permutations, idle repeats, duplicate/partial stock, bounds, and independent binary message lifetimes\n";
}
'''
parts = {
    'CONTROL_CLASS':function(header,'class dShopItemCtrl_c')+';',
    'CONTROL_CONSTRUCTOR':function(ctrl,'dShopItemCtrl_c::dShopItemCtrl_c()'),
    'CONTROL_DESTRUCTOR':function(ctrl,'dShopItemCtrl_c::~dShopItemCtrl_c()'),
    'CONTROL_GET_POSITION':function(ctrl,'cXyz dShopItemCtrl_c::getCurrentPos('),
    'STOCK_SCAN':function(shop,'static int dShopSystem_searchItemActor('),
    'SHOP_SEARCH':function(shop,'bool dShopSystem_c::searchItemActor()'),
    'ACTIVE_BINDING':function(flow,'struct ActiveBinding')+';',
    'RETAIN_BINDING':function(flow,'void retain_binding('),
    'RETAIN_SHOP_TEXT':function(flow,'extern "C" const char* tpr_xbox_retain_shop_text('),
    'RELEASE_MESSAGE':function(flow,'void release_message_control('),
}
if args.original_sort_negative_control:
    if os.name=='nt': raise RuntimeError('The negative control requires UBSan on Linux')
    assert parts['SHOP_SEARCH'].count('for (int j = i; j > 0; j--)')==1
    parts['SHOP_SEARCH']=parts['SHOP_SEARCH'].replace('for (int j = i; j > 0; j--)','for (int j = i; j >= 0; j--)')
for key,value in parts.items(): preamble=preamble.replace('/*'+key+'*/',value)
with tempfile.TemporaryDirectory(prefix='tpr-735-shop-test-') as temp:
    path=Path(temp); unit=path/'shop.cpp';unit.write_text(preamble,encoding='utf-8')
    exe=path/('shop.exe' if os.name=='nt' else 'shop')
    if os.name=='nt':
        command=['cl','/nologo','/std:c++20','/EHsc','/utf-8',str(unit),'/Fe:'+str(exe)]
    else:
        command=[shutil.which('g++') or 'c++','-std=c++20','-O1','-g','-fsanitize=address,undefined',
                 '-fno-sanitize-recover=all','-fno-omit-frame-pointer',str(unit),'-o',str(exe)]
    subprocess.run(command,cwd=path,check=True)
    environment=dict(os.environ)
    if os.name!='nt':
        # This executor cannot expose /proc task metadata to LeakSanitizer.
        # ASan/UBSan remain enabled; message-release ownership is asserted above.
        environment['ASAN_OPTIONS']='detect_leaks=0'
    result=subprocess.run([str(exe)],cwd=path,capture_output=True,text=True,env=environment)
    if args.original_sort_negative_control:
        assert result.returncode!=0 and ("index -1 out of bounds" in result.stderr or 'stack-buffer' in result.stderr),result.stderr
        print('PASS: original shop sort reproduced an out-of-bounds read under sanitizers')
    else:
        print(result.stdout,end='')
        if result.returncode: print(result.stderr)
        result.check_returncode()
