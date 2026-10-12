"""Compile actual release-mode item heap and shop create/delete/index functions."""
import argparse, os, re, shutil, subprocess, tempfile
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('source',type=Path);p.add_argument('--negative-control',action='store_true');a=p.parse_args()
root=a.source.resolve();control=Path(__file__).resolve().parents[1]
def function(source,signature):
    start=source.index(signature);depth=0;opened=False
    for m in re.finditer(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|//[^\n]*|/\*[\s\S]*?\*/|[{}]',source[start:]):
        if m.group()=='{':depth+=1;opened=True
        elif m.group()=='}':
            depth-=1
            if opened and depth==0:return source[start:start+m.end()]
    raise RuntimeError(signature)
with tempfile.TemporaryDirectory(prefix='shop-resource-744-') as name:
    work=Path(name);host=root
    if a.negative_control:
        host=work/'baseline'
        for path in ['src/d/actor/d_a_itembase.cpp','src/d/actor/d_a_shop_item.cpp']:
            f=host/path;f.parent.mkdir(parents=True,exist_ok=True);f.write_bytes((root/path).read_bytes().replace(b'\r\n',b'\n'))
        subprocess.run(['git','-C',str(host),'apply','--reverse',str(control/'patches/xbox-shop-resource-guards-744-host.patch')],check=True)
    cpp=r'''
#define _UWP 1
#define TARGET_PC 1
#define JUT_ASSERT(...) ((void)0)
#define ASSERT(...) ((void)0)
#define OS_REPORT(...) ((void)0)
#define JKR_NEW new
#define fopAcM_ct(...) ((void)0)
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstdarg>
#include <stdexcept>
#include <string>
using u8=uint8_t;using u16=uint16_t;using u32=uint32_t;using s8=int8_t;using s16=int16_t;
constexpr int TRUE=1,FALSE=0,SHOP_ITEMNO_MAX=23,dItemNo_NONE_e=255,cPhs_ERROR_e=5,cPhs_COMPLEATE_e=4;
struct Resource{};using J3DModelData=Resource;using J3DAnmTextureSRTKey=Resource;using J3DAnmColor=Resource;using J3DAnmTransform=Resource;using J3DAnmTevRegKey=Resource;using J3DAnmTexPattern=Resource;
Resource resource;
int missing=-999,modelCalls=0,resourceCalls=0,loads=0,deletes=0,creates=0,heapCalls=0,phase=4;
bool modelFails=false,animFails=false,heapFails=false;
std::string logs;
void OSReport(const char* format,...){char buffer[512];va_list args;va_start(args,format);vsnprintf(buffer,sizeof(buffer),format,args);va_end(args);logs+=buffer;}
void* dComIfG_getObjectRes(const char* arc,int id){++resourceCalls;if(!arc)throw std::runtime_error("null archive reached backend");return id==missing?nullptr:&resource;}
void* mDoExt_J3DModel__create(Resource* data,u32,u32){++modelCalls;if(!data)throw std::runtime_error("null model reached backend");return modelFails?nullptr:&resource;}
struct Animation {template<class...T>bool init(Resource* data,T...){if(!data)throw std::runtime_error("null animation reached backend");return !animFails;}
 bool init(Resource* model,Resource* data,int,int,float,int,int){if(!model||!data)throw std::runtime_error("null animation reached backend");return !animFails;}};
using mDoExt_btkAnm=Animation;using mDoExt_bpkAnm=Animation;using mDoExt_bckAnm=Animation;using mDoExt_brkAnm=Animation;using mDoExt_btpAnm=Animation;
struct J3DFrameCtrl{enum{EMode_LOOP};};
struct daItemBase_c {
 u8 m_itemNo=3;void* mpModel=nullptr;Animation *mpBtkAnm=nullptr,*mpBpkAnm=nullptr,*mpBckAnm=nullptr,*mpBrkAnm=nullptr,*mpBtpAnm=nullptr;
 ~daItemBase_c(){delete mpBtkAnm;delete mpBpkAnm;delete mpBckAnm;delete mpBrkAnm;delete mpBtpAnm;}
 bool chkFlag(int){return false;}s8 getTevFrm(){return -1;}int clothCreate(){return 1;}int __CreateHeap(){return 1;}
 int CreateItemHeap(const char*,s16,s16,s16,s16,s16,s16,s16);
};
struct Heap{int getHeapSize(){return 0x8000;}};
struct daShopItem_c:daItemBase_c {
 int mAngleX=0,mAngleY=0,mPhase=0;u8 original=3,display=3,shelf=2;bool mItemOverridden=false;
 const char* arc="Item";const char* fallback="Sold";Heap heapStorage;Heap* heap=&heapStorage;
 const char* getShopArcname(){return m_itemNo==255&&!mItemOverridden?fallback:arc;}
 u8 getShopItemID(){return shelf;}u8 getDisplayItemNo(){return display;}
 u16 getHeapSize();int _create();int _delete();void CreateInit(){++creates;}
 int DeleteBase(const char* a){if(!a)throw std::runtime_error("null delete archive");++deletes;return 1;}
};
namespace daShopItem_prm {u8 getItemNo(daShopItem_c* a){return a->original;}}
int dComIfG_resLoad(int*,const char* arc){if(!arc)throw std::runtime_error("null load archive");++loads;return phase;}
int CheckShopItemCreateHeap(daShopItem_c*){return 1;}
bool fopAcM_entrySolidHeap(daShopItem_c*,int(*)(daShopItem_c*),u16 size){assert(size>0);++heapCalls;return !heapFails;}
/*FUNCTIONS*/
int main(){
 try{
  {daItemBase_c item;assert(!item.CreateItemHeap(nullptr,1,0,0,0,0,0,0)&&resourceCalls==0);}
  for(int id=1;id<=6;++id){
   missing=id;modelCalls=0;logs.clear();daItemBase_c item;
   assert(!item.CreateItemHeap("Item",1,2,3,4,0,5,6));
   assert(logs.find("Xbox item resource failure (744)")!=std::string::npos);
   assert(id!=1||modelCalls==0);
  }
  missing=-999;{daItemBase_c item;assert(item.CreateItemHeap("Item",1,2,3,4,0,5,6));assert(item.mpModel&&item.mpBtkAnm&&item.mpBpkAnm&&item.mpBckAnm&&item.mpBrkAnm&&item.mpBtpAnm);}
  modelFails=true;{daItemBase_c item;assert(!item.CreateItemHeap("Item",1,0,0,0,0,0,0));}modelFails=false;
  animFails=true;{daItemBase_c item;assert(!item.CreateItemHeap("Item",1,2,0,0,0,0,0));}animFails=false;
  {daShopItem_c item;item.arc=nullptr;item.mItemOverridden=true;assert(item._create()==cPhs_ERROR_e&&loads==0);assert(item._delete()==1&&deletes==0);}
  {daShopItem_c item;item.arc=nullptr;item.fallback=nullptr;assert(item._create()==cPhs_ERROR_e&&loads==0);assert(item._delete()==1&&deletes==0);}
  {daShopItem_c item;item.arc=nullptr;assert(item._create()==cPhs_COMPLEATE_e&&creates==1);assert(item.m_itemNo==255);item._delete();assert(deletes==1);}
  for(unsigned shelf:{23u,24u,255u}){daShopItem_c item;item.shelf=static_cast<u8>(shelf);const int old=heapCalls;assert(!item.getHeapSize());assert(item._create()==cPhs_ERROR_e&&heapCalls==old);}
  for(unsigned shelf=0;shelf<23;++shelf){daShopItem_c item;item.shelf=static_cast<u8>(shelf);assert(item.getHeapSize()>0);assert(item._create()==cPhs_COMPLEATE_e);item.mItemOverridden=true;assert(item.getHeapSize()==0x8000);}
  phase=1;{daShopItem_c item;const int old=heapCalls;assert(item._create()==1&&old==heapCalls);}phase=4;
  heapFails=true;{daShopItem_c item;const int old=creates;assert(item._create()==cPhs_ERROR_e&&old==creates);}
 }catch(const std::runtime_error&){return 42;}
 puts("PASS .744: release-mode shop and pickup missing archive/model/all five animations, heap index/creation failures, valid resources and native fallback/deletion");
}
'''
    cpp=cpp.replace('/*FUNCTIONS*/','\n'.join(function((host/file).read_text(),sig) for file,sig in [('src/d/actor/d_a_itembase.cpp','int daItemBase_c::CreateItemHeap('),('src/d/actor/d_a_shop_item.cpp','u16 daShopItem_c::getHeapSize()'),('src/d/actor/d_a_shop_item.cpp','int daShopItem_c::_create()'),('src/d/actor/d_a_shop_item.cpp','int daShopItem_c::_delete()')]))
    file=work/'shop.cpp';file.write_text(cpp);exe=work/('shop.exe' if os.name=='nt' else 'shop');compiler=shutil.which('cl' if os.name=='nt' else 'c++')
    cmd=[compiler,'/nologo','/std:c++20','/EHsc',str(file),'/Fe:'+str(exe)] if os.name=='nt' else [compiler,'-std=c++20','-O1',str(file),'-o',str(exe)]
    subprocess.run(cmd,cwd=work,check=True);result=subprocess.run([str(exe)])
    if a.negative_control:
        if result.returncode!=42:raise RuntimeError('The .743 null-resource bug was not reproduced')
        print('PASS negative control: .743 passes missing resources to native backends in release mode')
    else:result.check_returncode()
