#include <cstdio>
#include <stdexcept>
#define OS_REPORT(...) ((void)0)
#define JUT_ASSERT(line,value) do {if(!(value))throw std::runtime_error("missing play scene");}while(0)
struct scene_class{};scene_class scene;bool peek=false,reset=false;int requests=0,accepted=0,failures=0;
bool fopOvlpM_IsPeek(){return peek;}namespace mDoRst {bool isReset(){return reset;}}
struct dStage_roomControl_c{static int getProcID(){return 1;}};
scene_class* fopScnM_SearchByID(int){return &scene;}
enum {fpcNm_NAME_SCENE_e};
int fopScnM_ChangeReq(scene_class*,int,int,int){++requests;if(failures){--failures;return 0;}++accepted;return 1;}
namespace JUtility{struct TColor{};}JUtility::TColor g_blackColor;
struct mDoGph_gInf_c{static void setFadeColor(JUtility::TColor){}};
struct daTitle_c{int mProcID=0;void nextScene_init();void nextScene_proc();};
#include "production-title.inc"
void require(bool v,const char*s){if(!v)throw std::runtime_error(s);}
int main(int argc,char**) {try{
    daTitle_c title;title.nextScene_init();for(int i=0;i<180;++i)title.nextScene_proc();
    if(argc>1)return accepted==180?42:1;
    require(title.mProcID==4&&requests==1&&accepted==1,"Accepted transition submitted repeatedly before overlap flag");
    title.nextScene_init();requests=accepted=0;failures=2;
    for(int i=0;i<180;++i)title.nextScene_proc();require(requests==3&&accepted==1,"Failed scene request was latched or accepted request retried");
    title.nextScene_init();requests=accepted=0;peek=true;title.nextScene_proc();peek=false;reset=true;title.nextScene_proc();require(requests==0,"Overlap/reset gate ignored");
    reset=false;title.nextScene_proc();require(accepted==1,"Transition did not resume after gate");
    daTitle_c next;next.nextScene_init();next.nextScene_proc();require(accepted==2,"New title actor inherited previous transition latch");
    puts("PASS: production title accepts one request, retries failures and respects overlap/reset gates");return 0;
}catch(const std::exception&e){fprintf(stderr,"%s\n",e.what());return 1;}}
