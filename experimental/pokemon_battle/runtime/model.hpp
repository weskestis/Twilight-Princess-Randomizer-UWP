#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace pokemon_battle::runtime {
constexpr std::size_t max_pack_bytes = 128u * 1024u * 1024u;
struct Vertex { std::array<float, 3> position{}, normal{}; std::array<float, 2> uv{}; std::array<float,4> color{1,1,1,1}; };
struct Material { std::array<float, 6> uv{}; std::array<float, 4> color{}; };
struct Batch { std::uint32_t first{}, count{}, texture{}, flags{}; };
struct Texture { std::uint16_t width{}, height{}; std::size_t offset{}, bytes{}; };
struct Clip { std::string name; std::uint32_t frames{}; float fps{}; std::size_t offset{}; };
struct Joint { std::string name; std::array<float,3> rest{}; };
struct Pose { std::vector<Vertex> vertices; std::vector<Material> materials; std::vector<std::array<float,3>> joints; bool finished = false; };

// An owned, validated blob. A failed replacement leaves the previous model intact.
// Samples never hold pointers into the blob, and never mutate the animation data.
class Model {
public:
    bool load(std::span<const std::uint8_t> data, std::string& error);
    bool sample(std::size_t clip, double seconds, bool loop, Pose& pose) const;
    const auto& batches() const { return batches_; }
    const auto& indices() const { return indices_; }
    const auto& textures() const { return textures_; }
    const auto& clips() const { return clips_; }
    const auto& joints() const { return joints_; }
    std::span<const std::uint8_t> texture_data(std::size_t index) const;
    std::size_t vertex_count() const { return uv_.size(); }
    std::size_t bytes() const { return bytes_.size(); }
    std::size_t bone_count() const { return bones_; }
private:
    struct SkinVertex {
        Vertex rest;
        std::array<std::uint16_t,4> joints{};
        std::array<float,4> weights{};
    };
    std::vector<std::uint8_t> bytes_;
    std::vector<std::array<float, 2>> uv_;
    std::vector<std::uint32_t> indices_;
    std::vector<Batch> batches_;
    std::vector<Texture> textures_;
    std::vector<Clip> clips_;
    std::vector<SkinVertex> skin_;
    std::vector<Joint> joints_;
    std::uint32_t bones_=0;
    std::array<float, 3> lower_{}, span_{};
    std::size_t stride_ = 0;
};
std::uint32_t crc32(std::span<const std::uint8_t> data);
} // namespace pokemon_battle::runtime
