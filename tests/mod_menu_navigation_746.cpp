#include <cassert>
#include <chrono>
#include <iostream>
#include <thread>
#include "mods_window.hpp"
#include "dusk/mods/svc/ui.hpp"
#include "dusk/startup_guard.hpp"
#include "ImageEffects.hpp"

namespace dusk::ui {
Rml::Context* testContext=nullptr;
Document* current=nullptr;
Document* top_document() noexcept {return current;}
Document& push_document(std::unique_ptr<Document>,bool,bool) noexcept {return *current;}
void uncover_top_document() noexcept {}
void apply_scoped_styles(Document& d) noexcept {d.restyle({});}
bool game_obscured_below(const Document&) noexcept {return false;}
std::string mod_image_source(const mods::LoadedMod& m,std::string_view p) {
    return "mod://"+m.metadata.id+"/"+std::string(p)+"?rev="+std::to_string(m.cacheGeneration);
}
}
class System final:public Rml::SystemInterface {
public:
 double elapsed=1;
 double GetElapsedTime() override{return elapsed;}
 bool LogMessage(Rml::Log::Type t,const Rml::String& s) override {
  if(t<=Rml::Log::LT_ERROR)std::cerr<<s<<'\n';return true;
 }
};
class Renderer final:public Rml::RenderInterface {
public:
 int icons=0,banners=0;
 Rml::CompiledGeometryHandle CompileGeometry(Rml::Span<const Rml::Vertex>,Rml::Span<const int>)override{return 1;}
 void RenderGeometry(Rml::CompiledGeometryHandle,Rml::Vector2f,Rml::TextureHandle)override{}
 void ReleaseGeometry(Rml::CompiledGeometryHandle)override{}
 Rml::TextureHandle LoadTexture(Rml::Vector2i& size,const Rml::String& source)override {
  size={128,128};if(source.find("banner")!=std::string::npos){++banners;size={768,256};}else ++icons;return 1;
 }
 Rml::TextureHandle GenerateTexture(Rml::Span<const Rml::byte>,Rml::Vector2i)override{return 1;}
 void ReleaseTexture(Rml::TextureHandle)override{}
 void EnableScissorRegion(bool)override{}
 void SetScissorRegion(Rml::Rectanglei)override{}
 Rml::CompiledShaderHandle CompileShader(const Rml::String&,const Rml::Dictionary&)override{return 1;}
 void RenderShader(Rml::CompiledShaderHandle,Rml::CompiledGeometryHandle,Rml::Vector2f,Rml::TextureHandle)override{}
 void ReleaseShader(Rml::CompiledShaderHandle)override{}
};
namespace ui=dusk::ui;
namespace mods=dusk::mods;
System sys;Renderer renderer;
void tick(ui::ModsWindow& w) {
 sys.elapsed+=1./120.;w.update();ui::testContext->Update();ui::testContext->Render();
}
void key(ui::ModsWindow& w,Rml::Input::KeyIdentifier k) {
 ui::testContext->ProcessKeyDown(k,0);ui::testContext->ProcessKeyUp(k,0);tick(w);
}
int detail_builds() {
 int n=0;for(auto& s:dusk::startup_guard::stages)if(s.starts_with("menu.mods.detail:"))++n;return n;
}
std::string focused_mod() {
 auto* f=ui::testContext->GetFocusElement();auto* row=f?(f->GetTagName()=="mod-entry"?f:f->Closest("mod-entry")):nullptr;
 return row?row->GetAttribute<Rml::String>("mod-id",""):"";
}
int visible_rows(ui::ModsWindow& w) {
 Rml::ElementList rows;w.mContentRoot->QuerySelectorAll(rows,"mod-entry[mod-id]");int n=0;
 for(auto* row:rows)if(row->IsVisible())++n;return n;
}
void select(ui::ModsWindow& w,const std::string& id) {
 assert(w.select_mod(id));assert(w.focus());for(int i=0;i<5;++i)tick(w);
 if(focused_mod()!=id)std::cerr<<"Expected "<<id<<", focused "<<focused_mod()<<'\n';
 assert(focused_mod()==id);
}
void scenario(Rml::Vector2i dimensions,float dpi,bool negative) {
 ui::testContext=Rml::CreateContext("mods",dimensions);assert(ui::testContext);
 ui::testContext->SetDensityIndependentPixelRatio(dpi);
 auto& loader=mods::ModLoader::instance();loader.records.clear();loader.actions.clear();
 for(int i=0;i<91;++i) {
  mods::LoadedMod m;const std::string index=(i<10?"00":i<100?"0":"")+std::to_string(i);
  m.metadata.id="mod"+index;m.metadata.name="Mod "+index;m.metadata.version="1.0.0";
  m.metadata.author="Author";m.metadata.description=std::string(200,'x');
  m.metadata.iconPath="icon.png";if(i%3==0)m.metadata.bannerPath="banner.png";
  m.active=m.enabled=i<8;loader.records.push_back(std::move(m));
 }
 {
  ui::ModsWindow w;ui::current=&w;w.show();select(w,"mod000");
  const int before=detail_builds(),oldBanners=renderer.banners;
  for(int i=0;i<40;++i)key(w,Rml::Input::KI_DOWN);
  const auto selected=focused_mod();assert(!selected.empty());
  const int builds=detail_builds()-before,banners=renderer.banners-oldBanners;
  std::cout<<dimensions.x<<'x'<<dimensions.y<<": 40 focus moves, "<<builds
           <<" detail builds, "<<banners<<" banner loads, "<<visible_rows(w)<<" visible rows\n";
  if(negative) {
   assert(builds>=40);assert(visible_rows(w)==91);w.force_hide(true);
  } else {
   assert(builds==0);assert(banners==0);assert(visible_rows(w)<=22);
   std::this_thread::sleep_for(std::chrono::milliseconds(195));tick(w);
   assert(detail_builds()==before+1);
   auto* header=w.mContentRoot->QuerySelector("mod-header");assert(header);
   assert(header->GetAttribute<Rml::String>("mod-id","")==selected);
   auto* oldAction=header->QuerySelector("button");assert(oldAction);
   key(w,Rml::Input::KI_DOWN);const auto next=focused_mod();assert(next!=selected);
   oldAction->DispatchEvent(Rml::EventId::Click,{});assert(loader.actions.empty());
   // A enters the current row immediately, without waiting 180 ms.
   key(w,Rml::Input::KI_RETURN);
   auto* focus=ui::testContext->GetFocusElement();assert(focus&&focus->Closest("mod-header"));
   auto* currentHeader=w.mContentRoot->QuerySelector("mod-header");
   assert(currentHeader->GetAttribute<Rml::String>("mod-id","")==next);
   key(w,Rml::Input::KI_RETURN);assert(loader.actions.size()==1&&loader.actions.back()==next);
   // Active native controls build immediately on right, and only update their owner.
   select(w,"mod003");mods::svc::panels.clear();mods::svc::updates.clear();
   key(w,Rml::Input::KI_RIGHT);assert(mods::svc::panels==std::vector<std::string>{"mod003"});
   for(int i=0;i<4;++i)tick(w);
   for(auto& id:mods::svc::updates)assert(id=="mod003");
   // Traverse every hidden row in both directions and retain layout/scrollability.
   select(w,"mod000");
   for(int i=0;i<90;++i)key(w,Rml::Input::KI_DOWN);
   assert(focused_mod()=="mod090");assert(visible_rows(w)<=22);
   for(int i=0;i<90;++i)key(w,Rml::Input::KI_UP);
   assert(focused_mod()=="mod000");
   // Pointer hover uses the same deferred preview path.
   auto* hovered=w.mContentRoot->QuerySelector("mod-entry[mod-id=mod006]");assert(hovered);
   const int hoverBefore=detail_builds();hovered->DispatchEvent(Rml::EventId::Mouseover,{});
   tick(w);assert(detail_builds()==hoverBefore);
   // Reload/replacement must discard old pointers, even with a pending preview.
   ++loader.revision;auto replacement=loader.records;loader.records.swap(replacement);
   tick(w);key(w,Rml::Input::KI_RIGHT);
   // Closing cancels deferred work; reopening restores usable current controls.
   key(w,Rml::Input::KI_LEFT);key(w,Rml::Input::KI_DOWN);
   w.hide(false);const int hiddenBefore=detail_builds();
   std::this_thread::sleep_for(std::chrono::milliseconds(195));tick(w);
   assert(detail_builds()==hiddenBefore);w.show();tick(w);
   assert(w.mContentRoot->QuerySelector("mod-header")!=nullptr);
   // Deactivating the selected native mod invalidates its pending panel.
   select(w,"mod003");mods::svc::panels.clear();
   loader.find_mod("mod003")->active=false;tick(w);
   std::this_thread::sleep_for(std::chrono::milliseconds(120));tick(w);
   assert(mods::svc::panels.empty());
   w.force_hide(true);
  }
  ui::current=nullptr;
 }
 ui::testContext->Update();Rml::RemoveContext("mods");ui::testContext=nullptr;
}
int main(int argc,char**) {
 Rml::SetSystemInterface(&sys);Rml::SetRenderInterface(&renderer);assert(Rml::Initialise());
 assert(Rml::LoadFontFace("font.ttf"));aurora::rmlui::register_image_effects();
 const bool negative=argc>1;
 for(auto [size,dpi]:std::vector<std::pair<Rml::Vector2i,float>>{{{1280,720},1.f},{{1920,1080},1.5f},{{3840,2160},3.f}})scenario(size,dpi,negative);
 Rml::Shutdown();
 std::cout<<(negative?"PASS: shipped .745 rebuilds every traversed detail and keeps all 91 rows visible\n":"PASS: actual shipped mod UI skips scroll-burst previews; A/right, actions, hidden rows, reload, close/reopen and deactivation remain safe\n");
 return negative?42:0;
}
