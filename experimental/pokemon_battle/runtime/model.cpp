#include "model.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <utility>

namespace pokemon_battle::runtime {
namespace {
std::uint16_t u16(const std::uint8_t* p) { return std::uint16_t(p[0] | (std::uint16_t(p[1]) << 8)); }
std::uint32_t u32(const std::uint8_t* p) {
    return std::uint32_t(p[0]) | (std::uint32_t(p[1]) << 8) |
           (std::uint32_t(p[2]) << 16) | (std::uint32_t(p[3]) << 24);
}
float f32(const std::uint8_t* p) { return std::bit_cast<float>(u32(p)); }
class Reader {
public:
    explicit Reader(std::span<const std::uint8_t> data) : data_(data) {}
    const std::uint8_t* take(std::size_t size) {
        if (size > data_.size() - offset) throw std::runtime_error("Truncated runtime model");
        const auto* p = data_.data() + offset; offset += size; return p;
    }
    std::uint32_t integer() { return u32(take(4)); }
    float number() {
        const float value = f32(take(4));
        if (!std::isfinite(value) || std::abs(value) > 100000.f)
            throw std::runtime_error("Nonfinite or out-of-range model number");
        return value;
    }
    std::size_t offset = 0;
private:
    std::span<const std::uint8_t> data_;
};
void require(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
}
std::uint32_t crc32(std::span<const std::uint8_t> data) {
    static const auto table = [] {
        std::array<std::uint32_t, 256> result{};
        for (std::uint32_t i = 0; i < result.size(); ++i) {
            auto c = i;
            for (int b = 0; b < 8; ++b) c = (c >> 1) ^ ((c & 1) ? 0xedb88320u : 0u);
            result[i] = c;
        }
        return result;
    }();
    std::uint32_t c = 0xffffffffu;
    for (auto b : data) c = table[(c ^ b) & 255u] ^ (c >> 8);
    return c ^ 0xffffffffu;
}
bool Model::load(std::span<const std::uint8_t> data, std::string& error) {
    try {
        require(data.size() >= 60 && data.size() <= max_pack_bytes, "Runtime model size outside limits");
        Reader r(data);
        require(std::memcmp(r.take(8), "DPKM0001", 8) == 0, "Unsupported runtime model format");
        require(r.integer() == data.size(), "Runtime model length mismatch");
        require(r.integer() == crc32(data.subspan(16)), "Runtime model checksum mismatch");
        const auto nv=r.integer(), ni=r.integer(), nb=r.integer(), nt=r.integer(), nc=r.integer();
        require(nv>0 && nv<=16384 && ni>0 && ni<=196605 && ni%3==0 && nb>0 && nb<=256 &&
                nt>0 && nt<=256 && nc>0 && nc<=128, "Runtime model counts outside limits");
        Model next;
        for (auto& x : next.lower_) x=r.number();
        for (auto& x : next.span_) { x=r.number(); require(x>0, "Invalid quantization bounds"); }
        next.uv_.resize(nv);
        for (auto& uv : next.uv_) for (auto& x : uv) x=r.number();
        next.indices_.reserve(ni);
        for (std::uint32_t i=0; i<ni; ++i) {
            const auto v=r.integer(); require(v<nv, "Vertex index outside model"); next.indices_.push_back(v);
        }
        std::uint32_t covered=0;
        for (std::uint32_t i=0; i<nb; ++i) {
            Batch b{r.integer(),r.integer(),r.integer()}; const auto flags=r.integer();
            require(b.first==covered && b.count>0 && b.count%3==0 && b.count<=65535 &&
                    b.count<=ni-covered && b.texture<nt && flags==0, "Invalid model draw batch");
            covered += b.count; next.batches_.push_back(b);
        }
        require(covered==ni, "Draw batches do not cover indices");
        std::size_t texture_bytes=0;
        for (std::uint32_t i=0; i<nt; ++i) {
            const auto w=r.integer(), h=r.integer(), size=r.integer();
            require(w>=4 && h>=4 && w<=2048 && h<=2048 && w%4==0 && h%4==0 &&
                    size==std::size_t(w)*h*4, "Invalid GX RGBA8 texture");
            texture_bytes+=size;
            require(texture_bytes<=16u*1024u*1024u, "Texture budget exceeded");
            next.textures_.push_back({static_cast<std::uint16_t>(w),static_cast<std::uint16_t>(h),r.offset,size});
            r.take(size);
        }
        next.stride_=std::size_t(nv)*12+std::size_t(nb)*40;
        std::uint32_t total_frames=0;
        for (std::uint32_t i=0; i<nc; ++i) {
            const auto* name=r.take(64);
            const auto* end=static_cast<const std::uint8_t*>(std::memchr(name,0,64));
            require(end && end!=name, "Invalid clip name");
            for(auto p=name; p!=end; ++p) require(*p>=32 && *p<127, "Invalid clip name encoding");
            Clip clip{std::string(reinterpret_cast<const char*>(name),std::size_t(end-name)),r.integer(),r.number(),r.offset};
            require(clip.frames>0 && clip.frames<=4096 && clip.fps>=1 && clip.fps<=120, "Invalid clip timing");
            for (const auto& existing : next.clips_) require(existing.name!=clip.name, "Duplicate clip name");
            total_frames+=clip.frames; require(total_frames<=16384, "Animation frame budget exceeded");
            // Check size before iterating. This also bounds every sample offset.
            const auto* frame=r.take(next.stride_*clip.frames);
            for (std::uint32_t f=0; f<clip.frames; ++f, frame+=next.stride_) {
                for (std::uint32_t v=0; v<nv; ++v) {
                    const auto* n=frame+std::size_t(v)*12+6;
                    float length=0;
                    for (int a=0; a<3; ++a) {
                        const auto value=std::bit_cast<std::int16_t>(u16(n+2*a));
                        require(value!=-32768, "Invalid quantized normal");
                        const float x=float(value)/32767.f; length+=x*x;
                    }
                    require(length>0.98f && length<1.02f, "Invalid normal length");
                }
                Reader materials(std::span(frame+std::size_t(nv)*12,std::size_t(nb)*40));
                for (std::uint32_t b=0; b<nb; ++b) {
                    for(int x=0; x<6; ++x) (void)materials.number();
                    for(int x=0; x<4; ++x) {
                        const auto c=materials.number(); require(c>=0 && c<=1, "Invalid material color");
                    }
                }
            }
            next.clips_.push_back(std::move(clip));
        }
        require(r.offset==data.size(), "Unexpected trailing model data");
        next.bytes_.assign(data.begin(),data.end());
        *this=std::move(next); error.clear(); return true;
    } catch(const std::exception& e) { error=e.what(); return false; }
}
std::span<const std::uint8_t> Model::texture_data(std::size_t index) const {
    if (index>=textures_.size()) return {};
    const auto& t=textures_[index]; return std::span(bytes_).subspan(t.offset,t.bytes);
}
bool Model::sample(std::size_t index, double seconds, bool loop, Pose& pose) const {
    if(index>=clips_.size() || !std::isfinite(seconds) || seconds<0) return false;
    const auto& clip=clips_[index];
    // Reduce time BEFORE multiplying: finite but very large input must not overflow.
    double frame=loop ? std::fmod(seconds,double(clip.frames)/clip.fps)*clip.fps :
                       std::min(seconds,double(clip.frames-1)/clip.fps)*clip.fps;
    if(std::abs(frame-std::round(frame))<1e-9) frame=std::round(frame);
    frame=std::min(frame,double(clip.frames-1)); // hold final sample until wrap, no atlas crossfade
    const auto a=static_cast<std::uint32_t>(frame), b=std::min(a+1,clip.frames-1);
    const float mix=static_cast<float>(frame-a);
    const auto* pa=bytes_.data()+clip.offset+std::size_t(a)*stride_;
    const auto* pb=bytes_.data()+clip.offset+std::size_t(b)*stride_;
    pose.vertices.resize(uv_.size()); pose.materials.resize(batches_.size());
    pose.finished=!loop && seconds>=double(clip.frames-1)/clip.fps;
    for(std::size_t v=0; v<uv_.size(); ++v) {
        auto& out=pose.vertices[v]; out.uv=uv_[v]; float length=0;
        for(int axis=0; axis<3; ++axis) {
            const auto offset=v*12+std::size_t(axis)*2;
            const float x=float(u16(pa+offset)), y=float(u16(pb+offset));
            out.position[axis]=lower_[axis]+(x+(y-x)*mix)/65535.f*span_[axis];
            const float nx=float(std::bit_cast<std::int16_t>(u16(pa+offset+6)))/32767.f;
            const float ny=float(std::bit_cast<std::int16_t>(u16(pb+offset+6)))/32767.f;
            out.normal[axis]=nx+(ny-nx)*mix; length+=out.normal[axis]*out.normal[axis];
        }
        if(length>1e-10f) for(auto& n:out.normal) n/=std::sqrt(length);
        else out.normal={0,1,0};
    }
    const auto* materials=pa+uv_.size()*12;
    for(auto& mat:pose.materials) {
        for(auto& x:mat.uv) { x=f32(materials); materials+=4; }
        for(auto& x:mat.color) { x=f32(materials); materials+=4; }
    }
    return true;
}
} // namespace pokemon_battle::runtime
