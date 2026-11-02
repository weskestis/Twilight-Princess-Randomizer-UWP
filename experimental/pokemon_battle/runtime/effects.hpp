#pragma once
#include "model.hpp"
#include "../field/party.hpp"
#include <array>
#include <vector>

namespace pokemon_battle::effects {
using Point=std::array<float,3>;
using Color=std::array<float,4>;
enum class Image : std::uint8_t { Glow,Fire,Gas,Bolt,BoltBranch,Spark,Vortex,Ring,Flash,Rays,Mist,Droplet,Bubble,Star,Needle,Pulse,Count };
inline constexpr std::array<const char*,16> image_names{"Glow","Fire","Gas","Bolt","BoltBranch","Spark","Vortex","Ring","Flash","Rays","Mist","Droplet","Bubble","Star","Needle","Pulse"};
class Pack {
public:
    bool load(std::span<const std::uint8_t> bytes,std::string& error);
    std::span<const std::uint8_t> pixels(std::size_t index) const;
    const auto& textures() const {return textures_;}
private:
    std::vector<std::uint8_t> bytes_;
    std::vector<runtime::Texture> textures_;
};
struct Sprite {
    Image image=Image::Glow;
    Point position{};
    float width=1,height=1,rotation=0;
    Color color{1,1,1,1};
    bool additive=true;
};
struct Ribbon {Point from{},to{};float width=1;Color color{1,1,1,1};};
struct Frame {std::vector<Sprite> sprites;std::vector<Ribbon> ribbons;};
struct Origins {Point body{},mouth{},left{},right{},tail{};float size=40;};
void move(Frame& frame,field::Visual visual,float seconds,const Origins& origins,const Point& target,std::uint32_t seed=1);
void ambient(Frame& frame,std::uint16_t species,float seconds,const Origins& origins);
void transfer(Frame& frame,float progress,const Point& pokemon,const Point& ball,float size,bool emerging);
} // namespace pokemon_battle::effects
