// Production eligibility helpers, R3DFrameBuffers depth blits, triangle shader,
// and MRT composite: compares the empty-pass plan against the legacy traversal.
#include <GL/glew.h>
#include <SDL2/SDL.h>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <vector>
#include "Graphics/New3D/R3DFrameBuffers.h"
#include "Graphics/New3D/R3DShaderTriangles.h"
#include "Graphics/New3D/R3DShaderCommon.h"
#include "Graphics/New3D/VRHooks.h"
#include "Graphics/New3D/VRRenderPass.h"

static void Require(bool ok, const char* what)
{
  if (!ok) { std::fprintf(stderr, "FAIL: %s\n", what); std::exit(1); }
}

static GLuint Compile(GLenum kind, const char* a, const char* b = nullptr)
{
  GLuint shader = glCreateShader(kind);
  const char* parts[] = {a, b};
  glShaderSource(shader, b ? 2 : 1, parts, nullptr);
  glCompileShader(shader);
  GLint ok = 0;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
  if (!ok) {
    char log[8192];
    glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
    std::fprintf(stderr, "%s\n", log);
  }
  Require(ok, "production triangle shader compiles");
  return shader;
}

static void Uniform1i(GLuint program, const char* name, int value)
{
  const GLint location = glGetUniformLocation(program, name);
  if (location >= 0) glUniform1i(location, value);
}

static void Attribute4(GLuint program, const char* name, float x, float y, float z, float w)
{
  const GLint location = glGetAttribLocation(program, name);
  if (location >= 0) glVertexAttrib4f(location, x, y, z, w);
}

static void AddMesh(New3D::Model& model, int id, bool polyAlpha = false,
                    bool textureAlpha = false, bool transLSelect = false,
                    bool highPriority = false)
{
  New3D::Mesh mesh;
  mesh.vboOffset = id;
  mesh.vertexCount = 6;
  mesh.polyAlpha = polyAlpha;
  mesh.textureAlpha = textureAlpha;
  mesh.textured = textureAlpha;
  mesh.transLSelect = transLSelect;
  mesh.highPriority = highPriority;
  if (textureAlpha) {
    mesh.format = 7; // RGBA4 with partial alpha, like textured foliage.
    mesh.width = 2;
    mesh.height = 2;
  }
  if (!model.meshes) model.meshes = std::make_shared<std::vector<New3D::Mesh>>();
  model.meshes->push_back(mesh);
}

static New3D::Model& AddModel(New3D::Node& node, float alpha = 1.f)
{
  New3D::Model model;
  model.alpha = alpha;
  node.models.push_back(model);
  return node.models.back();
}

static New3D::Node& AddNode(std::vector<New3D::Node>& nodes, int priority, int selection = 0)
{
  New3D::Node node{};
  node.viewport.priority = priority;
  node.viewport.select = selection;
  nodes.push_back(node);
  return nodes.back();
}

static std::vector<New3D::Node> MakeScene(const char* kind)
{
  std::vector<New3D::Node> nodes;
  New3D::Node& main = AddNode(nodes, 0);
  New3D::Model& opaque = AddModel(main);
  AddMesh(opaque, 0);

  if (std::strcmp(kind, "trans1") == 0 || std::strcmp(kind, "both") == 0)
    AddMesh(opaque, 1, true, false, false);
  if (std::strcmp(kind, "trans2") == 0 || std::strcmp(kind, "both") == 0)
    AddMesh(opaque, 2, true, false, true);

  if (std::strcmp(kind, "priority-overlay") == 0) {
    New3D::Node& priority = AddNode(nodes, 1);
    AddMesh(AddModel(priority), 3);
    New3D::Model& overlay = AddModel(priority);
    AddMesh(overlay, 4, true, false, true, true);
  }

  if (std::strcmp(kind, "filters") == 0) {
    New3D::Model& alphaNode = AddModel(main, .5f);
    AddMesh(alphaNode, 1); // node alpha alone makes trans1 eligible

    New3D::Model& vrOnly = AddModel(main);
    vrOnly.vrOnly = true;
    AddMesh(vrOnly, 2, true, false, true);

    New3D::Model& vrHidden = AddModel(main);
    vrHidden.vrOnly = true;
    vrHidden.vrHidden = true;
    AddMesh(vrHidden, 2, true, false, true);

    New3D::Model& eyeHidden = AddModel(main);
    eyeHidden.vrEyeHidden = true;
    AddMesh(eyeHidden, 2, true, false, true);

    New3D::Node& filtered = AddNode(nodes, 0, 7);
    AddMesh(AddModel(filtered), 1, true);
  }

  if (std::strcmp(kind, "texture-alpha") == 0) {
    New3D::Model& foliage = AddModel(main);
    AddMesh(foliage, 5, false, true, false);
  }

  // A real different-priority node must still draw in its own group.
  if (std::strcmp(kind, "priority-overlay") == 0) {
    New3D::Node& otherPriority = AddNode(nodes, 3);
    AddMesh(AddModel(otherPriority), 2, true, false, true);
  }
  return nodes;
}

struct DrawRef
{
  New3D::Model* model;
  New3D::Mesh* mesh;
};

struct GroupSelection
{
  bool hasOverlay = false;
  std::vector<DrawRef> draws;
};

static GroupSelection SelectDraws(std::vector<New3D::Node>& nodes, int priority,
                                  bool overlay, New3D::Layer layer,
                                  bool vrHandDraw, bool vrFlatDraw)
{
  GroupSelection result;
  for (New3D::Node& node : nodes) {
    // The test's select=7 viewport is rejected by the production VR filter.
    if (!New3D::RenderSceneNodeEligible(node, priority, [](const New3D::Viewport& viewport) {
          return viewport.select != 7;
        })) continue;
    for (New3D::Model& model : node.models) {
      if (!New3D::RenderSceneModelEligible(model, vrHandDraw, vrFlatDraw)) continue;
      for (New3D::Mesh& mesh : *model.meshes) {
        if (mesh.highPriority) result.hasOverlay = true;
        if (New3D::RenderSceneMeshEligible(mesh, model, layer, overlay))
          result.draws.push_back({&model, &mesh});
      }
    }
  }
  return result;
}

static bool HasPriorityNodes(const std::vector<New3D::Node>& nodes, int priority)
{
  return std::any_of(nodes.begin(), nodes.end(), [priority](const New3D::Node& node) {
    return node.viewport.priority == priority && !node.models.empty();
  });
}

static void DrawMesh(GLuint program, GLuint vao, GLuint vbo,
                     const DrawRef& ref, New3D::Layer layer)
{
  const int id = ref.mesh->vboOffset;
  float x0 = -.72f, x1 = .72f, y0 = -.68f, y1 = .68f, z = -4.f;
  float r = .8f, g = .12f, b = .08f;
  float alpha = 1.f;
  switch (id) {
    case 1: x0 = -.55f; x1 = .24f; z = -2.f; r = .1f; g = .95f; b = .1f; alpha = .5f; break;
    case 2: x0 = -.55f; x1 = .24f; z = -3.f; r = .95f; g = .8f; b = .05f; alpha = .5f; break;
    case 3: x0 = -.8f; x1 = .8f; z = -3.5f; r = .08f; g = .2f; b = .95f; break;
    case 4: x0 = -.24f; x1 = .55f; z = -3.f; r = .95f; g = .1f; b = .8f; alpha = .5f; break;
    case 5: x0 = -.55f; x1 = .24f; z = -2.5f; r = .9f; g = .9f; b = .9f; break;
    default: break;
  }
  const float scale = -z;
  const float vertices[] = {
    x0*scale, y0*scale, z, 1, x1*scale, y0*scale, z, 1, x1*scale, y1*scale, z, 1,
    x0*scale, y0*scale, z, 1, x1*scale, y1*scale, z, 1, x0*scale, y1*scale, z, 1
  };
  glUseProgram(program);
  glBindVertexArray(vao);
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STREAM_DRAW);
  glUniform1f(glGetUniformLocation(program, "nodeAlpha"), ref.model->alpha);
  glUniform1f(glGetUniformLocation(program, "modelScale"), 1.f);
  glUniform1f(glGetUniformLocation(program, "cota"), 1.f);
  Uniform1i(program, "colourLayer", static_cast<int>(layer));
  Uniform1i(program, "lightEnabled", GL_FALSE);
  Uniform1i(program, "textureEnabled", ref.mesh->textured);
  Uniform1i(program, "microTexture", GL_FALSE);
  Uniform1i(program, "textureAlpha", ref.mesh->textureAlpha);
  Uniform1i(program, "polyAlpha", ref.mesh->polyAlpha);
  Uniform1i(program, "alphaTest", GL_FALSE);
  Uniform1i(program, "discardAlpha", layer == New3D::Layer::colour);
  Uniform1i(program, "baseTexType", ref.mesh->format);
  Uniform1i(program, "texturePage", 0);
  const GLint baseTexInfo = glGetUniformLocation(program, "baseTexInfo");
  if (baseTexInfo >= 0)
    glUniform4i(baseTexInfo, ref.mesh->x, ref.mesh->y, ref.mesh->width, ref.mesh->height);
  const GLint textureWrapMode = glGetUniformLocation(program, "textureWrapMode");
  if (textureWrapMode >= 0) glUniform2i(textureWrapMode, 0, 0);
  Attribute4(program, "inColour", r, g, b, alpha);
  Attribute4(program, "inFaceNormal", 0.f, 0.f, 1.f, 0.f);
  Attribute4(program, "inNormal", 0.f, 0.f, 1.f, 0.f);
  Attribute4(program, "inTextureNP", 1.f, 0.f, 0.f, 1.f);
  Attribute4(program, "inFixedShade", 1.f, 0.f, 0.f, 1.f);

  // Distinct pass markers exercise the same packed depth/stencil copy that the
  // renderer uses around trans1. The high initial sentinel also proves that
  // untouched pixels survive the copy, rather than comparing two all-zero buffers.
  const GLint stencilMarker = layer == New3D::Layer::colour ? 0x11 :
                             layer == New3D::Layer::trans1 ? 0x22 : 0x33;
  glStencilMask(0xff);
  glStencilFunc(GL_ALWAYS, stencilMarker, 0xff);
  glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
  glDrawArrays(GL_TRIANGLES, 0, 6);
}

struct SceneImage
{
  std::vector<unsigned char> composite;
  std::vector<unsigned char> layerColour[3];
  std::vector<float> depth;
  std::vector<unsigned char> stencil;
  New3D::VRTranslucencyStats stats;
};

static SceneImage DrawScene(New3D::R3DFrameBuffers& layers, GLuint program, GLuint vao, GLuint vbo,
                            GLuint finalFbo, const GLuint banks[2], int side, std::vector<New3D::Node> nodes,
                            bool vrHandDraw, bool vrFlatDraw, bool forceLegacy)
{
  SceneImage image;
  auto sceneSelect = [&](int priority, bool overlay, New3D::Layer layer) {
    return SelectDraws(nodes, priority, overlay, layer, vrHandDraw, vrFlatDraw);
  };
  glDisable(GL_BLEND);
  glEnable(GL_STENCIL_TEST);
  glStencilMask(0xff);
  glClearStencil(0x05);
  glDisable(GL_CULL_FACE);
  glDisable(GL_SCISSOR_TEST);
  glEnable(GL_DEPTH_TEST);
  glDepthMask(GL_TRUE);
  glViewport(0, 0, side, side);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, banks[0]);
  glActiveTexture(GL_TEXTURE1);
  glBindTexture(GL_TEXTURE_2D, banks[1]);
  glActiveTexture(GL_TEXTURE0);
  layers.SetFBO(New3D::Layer::colour);
  glClearColor(0.f, 0.f, 0.f, 0.f);
  glClear(GL_COLOR_BUFFER_BIT);

  for (int priority = 0; priority <= 3; ++priority) {
    if (!HasPriorityNodes(nodes, priority)) continue;
    for (int pass = 0; pass < 2; ++pass) {
      const bool overlay = pass == 1;
      layers.SetFBO(New3D::Layer::colour);
      glClearDepth(0.0);
      glStencilMask(0xff);
      glClear(GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
      glDepthFunc(GL_GEQUAL);
      const GroupSelection opaque = sceneSelect(priority, overlay, New3D::Layer::colour);
      for (const DrawRef& draw : opaque.draws)
        DrawMesh(program, vao, vbo, draw, New3D::Layer::colour);

      glDepthFunc(GL_GREATER);
      const GroupSelection trans1 = sceneSelect(priority, overlay, New3D::Layer::trans1);
      const GroupSelection trans2 = sceneSelect(priority, overlay, New3D::Layer::trans2);
      ++image.stats.candidateGroups;
      image.stats.trans1Groups += !trans1.draws.empty();
      image.stats.trans2Groups += !trans2.draws.empty();
      const New3D::TranslucencyPassPlan plan = New3D::PlanTranslucencyPasses(
        trans1.draws.size(), trans2.draws.size(), forceLegacy);

      if (plan.drawTrans1) {
        layers.StoreDepth();
        ++image.stats.depthBlitsIssued;
        if (!forceLegacy) {
          layers.UseStoredDepthAttachment();
          Require(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE,
                  "scene FBO stays complete while trans1 uses the saved depth/stencil attachment");
        }
        layers.SetFBO(New3D::Layer::trans1);
        for (const DrawRef& draw : trans1.draws) {
          DrawMesh(program, vao, vbo, draw, New3D::Layer::trans1);
          ++image.stats.trans1Draws;
          image.stats.trans1Vertices += draw.mesh->vertexCount;
        }
      }
      else {
        ++image.stats.skippedTrans1Passes;
      }
      if (plan.preserveDepth) {
        if (forceLegacy) {
          layers.RestoreDepth();
          ++image.stats.depthBlitsIssued;
        }
        else {
          layers.UseOriginalDepthAttachment();
          ++image.stats.depthBlitsAvoided;
          Require(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE,
                  "scene FBO stays complete after restoring its original depth/stencil attachment");
        }
      }
      else {
        image.stats.depthBlitsAvoided += 2;
      }
      if (plan.drawTrans2) {
        layers.SetFBO(New3D::Layer::trans2);
        for (const DrawRef& draw : trans2.draws) {
          DrawMesh(program, vao, vbo, draw, New3D::Layer::trans2);
          ++image.stats.trans2Draws;
          image.stats.trans2Vertices += draw.mesh->vertexCount;
        }
      }
      else {
        ++image.stats.skippedTrans2Passes;
      }
      if (!opaque.hasOverlay) break;
    }
  }

  layers.SetFBO(New3D::Layer::none);
  glBindFramebuffer(GL_FRAMEBUFFER, finalFbo);
  glViewport(0, 0, side, side);
  glClearColor(0.f, 0.f, 1.f, 1.f);
  glClear(GL_COLOR_BUFFER_BIT);
  layers.Draw();

  const size_t pixels = static_cast<size_t>(side) * side;
  glBindFramebuffer(GL_READ_FRAMEBUFFER, layers.Framebuffer());
  for (int layer = 0; layer < 3; ++layer) {
    glReadBuffer(GL_COLOR_ATTACHMENT0 + layer);
    image.layerColour[layer].resize(pixels * 4);
    glReadPixels(0, 0, side, side, GL_RGBA, GL_UNSIGNED_BYTE, image.layerColour[layer].data());
  }
  image.depth.resize(pixels);
  glReadPixels(0, 0, side, side, GL_DEPTH_COMPONENT, GL_FLOAT, image.depth.data());
  image.stencil.resize(pixels);
  glReadPixels(0, 0, side, side, GL_STENCIL_INDEX, GL_UNSIGNED_BYTE, image.stencil.data());
  glBindFramebuffer(GL_READ_FRAMEBUFFER, finalFbo);
  glReadBuffer(GL_COLOR_ATTACHMENT0);
  image.composite.resize(pixels * 4);
  glReadPixels(0, 0, side, side, GL_RGBA, GL_UNSIGNED_BYTE, image.composite.data());
  Require(glGetError() == GL_NO_ERROR, "GL calls in production framebuffer regression");
  return image;
}

static void Compare(const SceneImage& optimized, const SceneImage& legacy, const char* scenario)
{
  Require(optimized.composite == legacy.composite, "composite framebuffer is byte-identical");
  for (int layer = 0; layer < 3; ++layer)
    Require(optimized.layerColour[layer] == legacy.layerColour[layer], "MRT colour attachments are byte-identical");
  Require(optimized.depth == legacy.depth, "final scene depth is identical");
  Require(optimized.stencil == legacy.stencil, "final scene stencil is identical");
  std::printf("scenario=%s groups=%llu trans1=%llu/%llu trans2=%llu/%llu optimized-blits=%llu avoided=%llu legacy-blits=%llu\n",
    scenario,
    static_cast<unsigned long long>(optimized.stats.candidateGroups),
    static_cast<unsigned long long>(optimized.stats.trans1Groups),
    static_cast<unsigned long long>(optimized.stats.trans1Draws),
    static_cast<unsigned long long>(optimized.stats.trans2Groups),
    static_cast<unsigned long long>(optimized.stats.trans2Draws),
    static_cast<unsigned long long>(optimized.stats.depthBlitsIssued),
    static_cast<unsigned long long>(optimized.stats.depthBlitsAvoided),
    static_cast<unsigned long long>(legacy.stats.depthBlitsIssued));
}

static size_t AlphaCoverage(const std::vector<unsigned char>& rgba)
{
  size_t count = 0;
  for (size_t i = 3; i < rgba.size(); i += 4) {
    count += rgba[i] != 0;
  }
  return count;
}

static bool HasAlphaCoverage(const std::vector<unsigned char>& rgba)
{
  return AlphaCoverage(rgba) != 0;
}

static bool HasStencilValue(const std::vector<unsigned char>& stencil, unsigned char value)
{
  return std::find(stencil.begin(), stencil.end(), value) != stencil.end();
}

static void CheckPlanCounts(const SceneImage& optimized, const SceneImage& legacy,
                            std::uint64_t trans1Groups, std::uint64_t trans2Groups, const char* scenario)
{
  Require(optimized.stats.trans1Groups == trans1Groups, scenario);
  Require(optimized.stats.trans2Groups == trans2Groups, scenario);
  const std::uint64_t expectedCopies = trans1Groups;
  Require(optimized.stats.depthBlitsIssued == expectedCopies, "optimized copies retained for each occupied trans1 group");
  Require(optimized.stats.depthBlitsAvoided ==
          optimized.stats.candidateGroups * 2 - trans1Groups,
          "optimized branch avoids two blits for empty trans1 and the restore blit for occupied trans1");
  Require(legacy.stats.depthBlitsIssued == legacy.stats.candidateGroups * 2,
          "legacy branch stores and restores depth for every candidate group");
}

int main(int argc, char** argv)
{
  const int side = argc > 1 ? std::atoi(argv[1]) : 128;
  Require(side >= 64 && side <= 4096, "test size must be 64..4096");
  Require(SDL_Init(SDL_INIT_VIDEO) == 0, "SDL video");
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
  SDL_Window* window = SDL_CreateWindow("R3D empty transparency", 0, 0, 128, 128,
                                        SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
  Require(window, "GL window");
  SDL_GLContext context = SDL_GL_CreateContext(window);
  Require(context, "GL context");
  glewExperimental = GL_TRUE;
  Require(glewInit() == GLEW_OK, "GLEW");
  while (glGetError() != GL_NO_ERROR) {}
  std::printf("renderer=%s size=%d\n", glGetString(GL_RENDERER), side);

  const GLuint vs = Compile(GL_VERTEX_SHADER, vertexShaderR3D);
  const GLuint fs = Compile(GL_FRAGMENT_SHADER, fragmentShaderR3D, fragmentShaderR3DCommon);
  const GLuint program = glCreateProgram();
  glAttachShader(program, vs);
  glAttachShader(program, fs);
  glLinkProgram(program);
  GLint linked = 0;
  glGetProgramiv(program, GL_LINK_STATUS, &linked);
  Require(linked, "production R3D triangle shader links");

  GLuint vao = 0, vbo = 0;
  glGenVertexArrays(1, &vao);
  glGenBuffers(1, &vbo);
  glBindVertexArray(vao);
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  const GLint vertex = glGetAttribLocation(program, "inVertex");
  glEnableVertexAttribArray(vertex);
  glVertexAttribPointer(vertex, 4, GL_FLOAT, GL_FALSE, 0, nullptr);
  glUseProgram(program);
  Uniform1i(program, "textureBank[0]", 0);
  Uniform1i(program, "textureBank[1]", 1);
  glUniform1f(glGetUniformLocation(program, "modelScale"), 1.f);
  Uniform1i(program, "hardwareStep", 0x20);
  glUniform4f(glGetUniformLocation(program, "spotEllipse"), 0.f, 0.f, 1.f, 1.f);
  const float identity[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
  const float projection[16] = {1,0,0,0, 0,1,0,0, 0,0,0,-1, 0,0,1,0};
  glUniformMatrix4fv(glGetUniformLocation(program, "modelMat"), 1, GL_FALSE, identity);
  glUniformMatrix4fv(glGetUniformLocation(program, "projMat"), 1, GL_FALSE, projection);
  GLuint banks[2];
  glGenTextures(2, banks);
  // The small valid 2x2 RGBA4 patch used by the texture-alpha scenario lives at
  // the origin. Keep full production texture-bank dimensions because the R3D
  // shader wraps addresses against the 2048x1024 bank, not the image rectangle.
  const std::vector<unsigned short> bankPixels(2048u * 1024u, 0xfff8u);
  for (int i = 0; i < 2; ++i) {
    glActiveTexture(GL_TEXTURE0 + i);
    glBindTexture(GL_TEXTURE_2D, banks[i]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R16UI, 2048, 1024, 0, GL_RED_INTEGER, GL_UNSIGNED_SHORT, bankPixels.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  }
  glActiveTexture(GL_TEXTURE0);

  New3D::R3DFrameBuffers layers;
  Require(layers.CreateFBO(side, side) == Result::OKAY, "production MRT/depth-copy FBO creation");
  GLuint finalTex = 0, finalFbo = 0;
  glGenTextures(1, &finalTex);
  glBindTexture(GL_TEXTURE_2D, finalTex);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, side, side, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
  glGenFramebuffers(1, &finalFbo);
  glBindFramebuffer(GL_FRAMEBUFFER, finalFbo);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, finalTex, 0);
  Require(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE, "composite target FBO");

  struct Mode { const char* name; bool hand; bool flat; };
  const Mode modes[] = {{"desktop", false, false}, {"eye", true, false}, {"cinema", false, true}};
  const char* scenarios[] = {"empty", "trans1", "trans2", "both", "priority-overlay", "filters", "texture-alpha"};
  for (const Mode& mode : modes) {
    for (const char* scenario : scenarios) {
      SceneImage optimized = DrawScene(layers, program, vao, vbo, finalFbo, banks, side,
                                       MakeScene(scenario), mode.hand, mode.flat, false);
      SceneImage legacy = DrawScene(layers, program, vao, vbo, finalFbo, banks, side,
                                    MakeScene(scenario), mode.hand, mode.flat, true);
      Compare(optimized, legacy, scenario);
      Require(HasAlphaCoverage(optimized.layerColour[0]), "opaque pass has real framebuffer coverage");
      if (std::strcmp(scenario, "trans1") == 0 || std::strcmp(scenario, "both") == 0 ||
          std::strcmp(scenario, "texture-alpha") == 0 || std::strcmp(scenario, "filters") == 0)
        Require(HasAlphaCoverage(optimized.layerColour[1]), "trans1 pass has real framebuffer coverage");
      if (std::strcmp(scenario, "trans2") == 0 || std::strcmp(scenario, "both") == 0 ||
          std::strcmp(scenario, "priority-overlay") == 0 ||
          (std::strcmp(scenario, "filters") == 0 && (mode.hand || !mode.flat)))
        Require(HasAlphaCoverage(optimized.layerColour[2]), "trans2 pass has real framebuffer coverage");
      if (std::strcmp(scenario, "empty") == 0)
        CheckPlanCounts(optimized, legacy, 0, 0, "empty scene has no transparent draws");
      if (std::strcmp(scenario, "trans1") == 0)
        CheckPlanCounts(optimized, legacy, 1, 0, "trans1-only scene");
      if (std::strcmp(scenario, "trans2") == 0)
        CheckPlanCounts(optimized, legacy, 0, 1, "trans2-only scene avoids copies");
      if (std::strcmp(scenario, "both") == 0)
        CheckPlanCounts(optimized, legacy, 1, 1, "both transparent layers keep depth restore");
      if (std::strcmp(scenario, "texture-alpha") == 0) {
        CheckPlanCounts(optimized, legacy, 1, 0, "texture-alpha-only foliage scene");
        Require(optimized.stats.trans1Draws == 1 && optimized.stats.trans1Vertices > 0,
                "textured partial-alpha mesh is submitted to trans1");
      }
      if (std::strcmp(scenario, "trans1") == 0 || std::strcmp(scenario, "texture-alpha") == 0) {
        Require(HasStencilValue(optimized.stencil, 0x05), "untouched stencil pixels retain the nonzero clear sentinel");
        Require(HasStencilValue(optimized.stencil, 0x11), "trans1-only result retains opaque stencil writes");
        Require(!HasStencilValue(optimized.stencil, 0x22), "trans1-only result restores stencil instead of retaining trans1 writes");
      }
      if (std::strcmp(scenario, "trans2") == 0 || std::strcmp(scenario, "both") == 0) {
        Require(HasStencilValue(optimized.stencil, 0x33), "trans2 writes survive as meaningful final stencil data");
        Require(!HasStencilValue(optimized.stencil, 0x22), "trans1 stencil writes are restored before trans2");
      }
      if (std::strcmp(scenario, "priority-overlay") == 0)
        Require(optimized.stats.candidateGroups > 1 && optimized.stats.trans2Groups > 0,
                "priority and overlay groups are included");
      if (std::strcmp(scenario, "filters") == 0 && std::strcmp(mode.name, "eye") == 0)
        Require(optimized.stats.trans2Groups > 0, "VR-only draw is present for an eye pass");
      if (std::strcmp(scenario, "filters") == 0 && std::strcmp(mode.name, "desktop") == 0)
        Require(optimized.stats.trans2Groups > 0, "eye-hidden model remains eligible in ordinary flat rendering");
      std::printf("  mode=%s\n", mode.name);
    }
  }

  glDeleteFramebuffers(1, &finalFbo);
  glDeleteTextures(1, &finalTex);
  glDeleteTextures(2, banks);
  glDeleteBuffers(1, &vbo);
  glDeleteVertexArrays(1, &vao);
  glDeleteProgram(program);
  glDeleteShader(vs);
  glDeleteShader(fs);
  SDL_GL_DeleteContext(context);
  SDL_DestroyWindow(window);
  SDL_Quit();
  std::puts("PASS: optimized empty-pass plan matches legacy MRT, depth, stencil and composite across alpha/VR filters");
}
