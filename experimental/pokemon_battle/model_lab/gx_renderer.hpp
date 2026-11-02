#pragma once
#include "../runtime/model.hpp"
#include <dolphin/gx.h>
#include <dolphin/mtx.h>
#include <memory>

namespace pokemon_battle::runtime {
class GxAsset {
public:
    explicit GxAsset(Model model);
    ~GxAsset();
    GxAsset(const GxAsset&) = delete;
    GxAsset& operator=(const GxAsset&) = delete;
    const Model& model() const { return model_; }
    void draw(const Pose& pose, const Mtx view_model,float opacity=1);
private:
    Model model_;
    std::vector<GXTexObj> textures_;
};
}
