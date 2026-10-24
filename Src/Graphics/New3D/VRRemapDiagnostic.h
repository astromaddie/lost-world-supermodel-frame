#ifndef NEW3D_VR_REMAP_DIAGNOSTIC_H
#define NEW3D_VR_REMAP_DIAGNOSTIC_H

#include <array>
#include <cstdint>
#include <map>
#include <ostream>
#include <set>
#include <string>
#include <vector>

namespace New3D {

// CPU-only observer of submitted draws, populated only for requested eye captures.
// IDs are scene-vector indices, stable across both eyes and retained redraws.
struct VRRemapDiagnostic
{
  struct Viewport {
    unsigned id = 0;
    int priority = 0;
    std::array<float, 4> rect{}, frustum{}; // original hardware xywh, lrtb
    bool remapped = false;
    std::array<float, 16> matrix{}; // actual shader remap, column-major; identity if inactive
  };
  struct Draw {
    unsigned viewport = 0, model = 0, mesh = 0, address = 0;
    int layer = 0, vertices = 0;
    bool overlay = false, dynamic = false;
    float alpha = 1;
    unsigned material = 0; // textured, textureAlpha, alphaTest, polyAlpha, layered, noLosReturn
    std::array<int, 8> texture{}; // page,x,y,width,height,format,modelOffsetX,modelOffsetY
  };
  static constexpr size_t DetailLimit = 4096;
  bool observed = false;
  std::map<unsigned, Viewport> viewports;
  std::set<std::pair<unsigned, unsigned>> models, remappedModels;
  std::vector<Draw> details;
  uint64_t draws = 0, remappedDraws = 0, remappedVertices = 0, remappedOpaqueDraws = 0;

  void Reset() { *this = VRRemapDiagnostic{}; }
  void Record(const Viewport& vp, const Draw& draw)
  {
    viewports.emplace(vp.id, vp);
    models.emplace(vp.id, draw.model);
    ++draws;
    if (!vp.remapped) return;
    remappedModels.emplace(vp.id, draw.model);
    ++remappedDraws;
    remappedVertices += draw.vertices;
    if (draw.layer == 0) ++remappedOpaqueDraws;
    if (details.size() < DetailLimit) details.push_back(draw);
  }

  void Write(std::ostream& out, int eye) const
  {
    const std::string prefix = "eye" + std::to_string(eye) + "_remap_";
    size_t remappedViewports = 0;
    for (const auto& item : viewports) if (item.second.remapped) ++remappedViewports;
    out << prefix << "observed=" << observed << '\n'
        << prefix << "counts=" << viewports.size() << ',' << remappedViewports << ','
        << models.size() << ',' << remappedModels.size() << ',' << draws << ',' << remappedDraws
        << ',' << remappedOpaqueDraws << ',' << remappedVertices << '\n'
        << prefix << "details_omitted=" << remappedDraws - details.size() << '\n';
    for (const auto& item : viewports) {
      const auto& vp = item.second;
      out << prefix << "viewport=" << vp.id << ',' << vp.priority << ',' << vp.remapped;
      for (float value : vp.rect) out << ',' << value;
      for (float value : vp.frustum) out << ',' << value;
      for (float value : vp.matrix) out << ',' << value;
      out << '\n';
    }
    for (const auto& d : details) {
      out << prefix << "draw=" << d.viewport << ',' << d.model << ',' << d.mesh << ','
          << d.address << ',' << d.layer << ',' << d.overlay << ',' << d.dynamic << ','
          << d.vertices << ',' << d.alpha << ',' << d.material;
      for (int value : d.texture) out << ',' << value;
      out << '\n';
    }
  }
};

} // namespace New3D
#endif
