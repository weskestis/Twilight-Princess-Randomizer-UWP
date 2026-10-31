// Developer QA utility: export a pose sampled by the production C++ loader.
#include "model.hpp"
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <stdexcept>
int main(int argc,char** argv) {
    try {
        if(argc!=5)throw std::runtime_error("usage: snapshot model.dpkm clip seconds new-output-directory");
        std::ifstream source(argv[1],std::ios::binary);
        if(!source)throw std::runtime_error("Cannot open model");
        std::vector<std::uint8_t> bytes(std::istreambuf_iterator<char>(source),{});
        pokemon_battle::runtime::Model model;std::string error;
        if(!model.load(bytes,error))throw std::runtime_error(error);
        pokemon_battle::runtime::Pose pose;
        const auto clip=static_cast<std::size_t>(std::stoul(argv[2]));
        if(!model.sample(clip,std::stod(argv[3]),false,pose))throw std::runtime_error("Invalid pose");
        const std::filesystem::path out(argv[4]);
        if(std::filesystem::exists(out))throw std::runtime_error("Output already exists");
        std::filesystem::create_directories(out);
        std::ofstream json(out/"pose.json");json<<std::setprecision(9);
        json<<"{\"clip\":"<<std::quoted(model.clips()[clip].name)<<",\"vertices\":[";
        for(std::size_t i=0;i<pose.vertices.size();++i) {
            if(i) { json<<','; }
            json<<'[';const auto& v=pose.vertices[i];
            for(float x:v.position)json<<x<<',';
            for(float x:v.normal)json<<x<<',';
            json<<v.uv[0]<<','<<v.uv[1]<<']';
        }
        json<<"],\"indices\":[";
        for(std::size_t i=0;i<model.indices().size();++i) {if(i)json<<',';json<<model.indices()[i];}
        json<<"],\"batches\":[";
        for(std::size_t i=0;i<model.batches().size();++i) {
            if(i) { json<<','; }
            const auto& b=model.batches()[i];const auto& m=pose.materials[i];
            json<<"{\"first\":"<<b.first<<",\"count\":"<<b.count<<",\"texture\":"<<b.texture<<",\"uv\":[";
            for(std::size_t x=0;x<m.uv.size();++x){if(x)json<<',';json<<m.uv[x];}
            json<<"],\"color\":[";
            for(std::size_t x=0;x<m.color.size();++x){if(x)json<<',';json<<m.color[x];}
            json<<"]}";
        }
        json<<"],\"textures\":[";
        for(std::size_t i=0;i<model.textures().size();++i) {
            if(i) { json<<','; }
            const auto& t=model.textures()[i];
            const auto name="texture-"+std::to_string(i)+".rgba8";
            std::ofstream file(out/name,std::ios::binary);const auto data=model.texture_data(i);
            file.write(reinterpret_cast<const char*>(data.data()),static_cast<std::streamsize>(data.size()));
            if(!file)throw std::runtime_error("Failed writing texture");
            json<<"{\"width\":"<<t.width<<",\"height\":"<<t.height<<",\"file\":"<<std::quoted(name)<<'}';
        }
        json<<"]}\n";
        if(!json)throw std::runtime_error("Failed writing pose");
        std::cout<<"Sampled "<<model.clips()[clip].name<<'\n';
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
