#ifndef INCLUDED_VR_RENDER_PASS_H
#define INCLUDED_VR_RENDER_PASS_H

#include <cstddef>
#include "Model.h"

namespace New3D {

// Keep these eligibility predicates shared by the renderer and the lightweight
// GL regression. A false negative would omit a real scene draw, so the draw
// loop and its translucency preflight must use the same tests.
template<class ViewportFilter>
inline bool RenderSceneNodeEligible(const Node& node, int priority, ViewportFilter passesVRFilter)
{
	return node.viewport.priority == priority && !node.models.empty() && passesVRFilter(node.viewport);
}

inline bool RenderSceneModelEligible(const Model& model, bool vrHandDraw, bool vrFlatDraw)
{
	if (model.vrOnly && (!vrHandDraw || model.vrHidden)) {
		return false;
	}
	if (model.vrEyeHidden && (vrHandDraw || vrFlatDraw)) {
		return false;
	}
	return !model.meshes->empty();
}

inline bool RenderSceneMeshEligible(Mesh& mesh, Model& model, Layer layer, bool renderOverlay)
{
	return mesh.Render(layer, model.alpha) && mesh.highPriority == renderOverlay;
}

struct TranslucencyPassPlan
{
	bool drawTrans1;
	bool drawTrans2;
	bool preserveDepth;
};

inline TranslucencyPassPlan PlanTranslucencyPasses(std::size_t trans1Draws, std::size_t trans2Draws, bool forceLegacy)
{
	if (forceLegacy) {
		return { true, true, true };
	}
	return { trans1Draws != 0, trans2Draws != 0, trans1Draws != 0 };
}

} // namespace New3D

#endif // INCLUDED_VR_RENDER_PASS_H
