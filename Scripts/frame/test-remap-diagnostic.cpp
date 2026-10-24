#include <cassert>
#include <cstdio>
#include <sstream>
#include "../../Src/Graphics/New3D/VRRemapDiagnostic.h"

int main()
{
  New3D::VRRemapDiagnostic capture;
  New3D::VRRemapDiagnostic::Viewport view;
  view.id = 3;
  view.rect = {0, 52, 496, 280};
  view.frustum = {-.7f, .9f, .5f, -.4f};
  view.matrix = {2,0,0,0, 0,1,0,0, .25f,0,1,0, 0,0,0,1};
  New3D::VRRemapDiagnostic::Draw draw;
  draw.viewport = view.id;
  draw.model = 7;
  draw.mesh = 2;
  draw.address = 0x123456;
  draw.vertices = 6;
  draw.material = 7; // textured + texture alpha + alpha test
  draw.texture = {1,32,64,128,128,0,16,8};

  // An observed scene without remapping must be distinct from no observation.
  capture.observed = true;
  capture.Record(view, draw);
  assert(capture.draws == 1 && capture.remappedDraws == 0 && capture.details.empty());
  std::ostringstream plain;
  capture.Write(plain, 0);
  assert(plain.str().find("eye0_remap_observed=1\n") != std::string::npos);
  assert(plain.str().find("eye0_remap_counts=1,0,1,0,1,0,0,0\n") != std::string::npos);

  // A second viewport can reuse model-vector indices. Repeated alpha/overlay passes
  // count submissions, but must not inflate unique viewport/model counts.
  view.id = draw.viewport = 4;
  view.remapped = true;
  capture.Record(view, draw);
  draw.layer = 1;
  capture.Record(view, draw);
  draw.overlay = true;
  capture.Record(view, draw);
  assert(capture.viewports.size() == 2 && capture.models.size() == 2);
  assert(capture.remappedModels.size() == 1 && capture.remappedDraws == 3);
  assert(capture.remappedOpaqueDraws == 1 && capture.remappedVertices == 18);
  assert(capture.viewports.at(4).matrix == view.matrix);
  assert(capture.viewports.at(4).frustum == view.frustum);
  assert(capture.details.front().material == 7 && capture.details.front().texture == draw.texture);

  for (size_t i = 3; i < New3D::VRRemapDiagnostic::DetailLimit + 2; ++i)
    capture.Record(view, draw);
  assert(capture.details.size() == New3D::VRRemapDiagnostic::DetailLimit);
  assert(capture.remappedDraws == New3D::VRRemapDiagnostic::DetailLimit + 2);
  std::ostringstream bounded;
  capture.Write(bounded, 1);
  assert(bounded.str().find("eye1_remap_details_omitted=2\n") != std::string::npos);
  assert(bounded.str().find("eye1_remap_draw=4,7,2,1193046,0,0,0,6,1,7,1,32,64,128,128,0,16,8\n") != std::string::npos);
  capture.Reset();
  assert(!capture.observed && capture.models.empty() && capture.viewports.empty() && capture.details.empty());
  assert(capture.draws == 0 && capture.remappedDraws == 0 && capture.remappedVertices == 0);
  std::puts("PASS: remap capture identity, pass deduplication, exact totals beyond detail cap, metadata and reset");
}
