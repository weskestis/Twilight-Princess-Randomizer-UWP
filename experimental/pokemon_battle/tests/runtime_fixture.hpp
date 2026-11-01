#pragma once
#include "../runtime/model.hpp"
#include <bit>
#include <cstring>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <vector>
namespace fixture {
using Bytes=std::vector<std::uint8_t>;
inline void integer(Bytes& b,std::uint32_t n) { for(int i=0;i<4;++i)b.push_back(static_cast<std::uint8_t>(n>>(8*i))); }
inline void number(Bytes& b,float n) { integer(b,std::bit_cast<std::uint32_t>(n)); }
inline void overwrite(Bytes& b,std::size_t at,std::uint32_t n) { for(int i=0;i<4;++i)b.at(at+std::size_t(i))=static_cast<std::uint8_t>(n>>(8*i)); }
inline void checksum(Bytes& b) { overwrite(b,8,static_cast<std::uint32_t>(b.size())); overwrite(b,12,pokemon_battle::runtime::crc32(std::span(b).subspan(16))); }
inline Bytes make() {
    Bytes b{'D','P','K','M','0','0','0','1'}; integer(b,0);integer(b,0);
    for(auto n:{3u,3u,1u,1u,1u})integer(b,n);
    for(float n:{0.f,0.f,0.f,1.f,1.f,1.f})number(b,n);
    for(float n:{0.f,0.f,1.f,0.f,0.f,1.f})number(b,n);
    for(auto n:{0u,1u,2u,0u,3u,0u,0u,4u,4u,64u})integer(b,n);
    b.insert(b.end(),64,255);
    const char name[64]="test"; b.insert(b.end(),name,name+64);integer(b,2);number(b,60.f);
    for(int frame=0;frame<2;++frame) {
        for(int vertex=0;vertex<3;++vertex) {
            for(std::uint16_t n:{static_cast<std::uint16_t>(frame?65535:0),std::uint16_t(0),std::uint16_t(0),std::uint16_t(0),std::uint16_t(32767),std::uint16_t(0)}) {
                b.push_back(static_cast<std::uint8_t>(n)); b.push_back(static_cast<std::uint8_t>(n>>8));
            }
        }
        for(float n:{1.f,0.f,float(frame),0.f,1.f,0.f,1.f,1.f,1.f,1.f})number(b,n);
    }
    checksum(b);return b;
}
inline Bytes read(const char* path) {
    std::ifstream file(path,std::ios::binary);
    if(!file)throw std::runtime_error("Cannot open supplied test pack");
    return Bytes(std::istreambuf_iterator<char>(file),{});
}
inline void require(bool value,const char* message) { if(!value)throw std::runtime_error(message); }
}
