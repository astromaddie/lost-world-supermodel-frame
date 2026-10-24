// Real production R3DShader setters + triangle shader: no copied culling shader or ROM.
#include <GL/glew.h>
#include <SDL2/SDL.h>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include "Graphics/New3D/R3DShader.h"

static void Require(bool ok, const char* message)
{ if (!ok) { std::fprintf(stderr,"FAIL: %s\n",message); std::exit(1); } }
static void Mul4(const float* m,const float* p,float* out)
{ for(int r=0;r<4;++r) out[r]=m[r]*p[0]+m[4+r]*p[1]+m[8+r]*p[2]+m[12+r]*p[3]; }
static float Dot(const float* a,const float* b)
{ return a[0]*b[0]+a[1]*b[1]+a[2]*b[2]; }
static const float corners[4][4]={{-.25f,-.4f,.25f,1},{1,-.4f,-1,1},{1,.4f,-1,1},{-.25f,.4f,.25f,1}};
static const int indices[6]={0,1,2,0,2,3};
struct State {
  float model[16], projection[16], correction[9];
  GLint enabled=0;
  std::array<std::array<float,2>,4> projected;
};
static State ReadState(GLuint program)
{
  State s{};
  glGetUniformfv(program,glGetUniformLocation(program,"modelMat"),s.model);
  glGetUniformfv(program,glGetUniformLocation(program,"projMat"),s.projection);
  glGetUniformfv(program,glGetUniformLocation(program,"vrFaceNormalCorrection"),s.correction);
  glGetUniformiv(program,glGetUniformLocation(program,"vrCorrectFaceNormals"),&s.enabled);
  for(int v=0;v<4;++v) {
    float view[4],clip[4]; Mul4(s.model,corners[v],view); Mul4(s.projection,view,clip);
    Require(clip[3]>0,"fixture plane stays in front of eye");
    s.projected[v]={clip[0]/clip[3],clip[1]/clip[3]};
  }
  return s;
}
static bool Interior(float x,float y,const State& state)
{
  for(int triangle=0;triangle<2;++triangle) {
    const auto& a=state.projected[indices[triangle*3]];
    const auto& b=state.projected[indices[triangle*3+1]];
    const auto& c=state.projected[indices[triangle*3+2]];
    const float d=(b[1]-c[1])*(a[0]-c[0])+(c[0]-b[0])*(a[1]-c[1]);
    const float u=((b[1]-c[1])*(x-c[0])+(c[0]-b[0])*(y-c[1]))/d;
    const float v=((c[1]-a[1])*(x-c[0])+(a[0]-c[0])*(y-c[1]))/d;
    if(u>.04f && v>.04f && 1-u-v>.04f) return true;
  }
  return false;
}
static std::vector<unsigned char> Pixels(int side)
{
  std::vector<unsigned char> p((size_t)side*side*4);
  glReadPixels(0,0,side,side,GL_RGBA,GL_UNSIGNED_BYTE,p.data());
  return p;
}
static size_t Missing(int side,const State& state,const std::vector<unsigned char>& pixels,size_t& checked)
{
  size_t missing=0; checked=0;
  for(int y=0;y<side;++y) for(int x=0;x<side;++x) {
    if(!Interior(2.f*(x+.5f)/side-1,2.f*(y+.5f)/side-1,state)) continue;
    ++checked;
    const auto* p=&pixels[((size_t)y*side+x)*4];
    if(p[0]!=255 || p[1]!=0 || p[2]!=0 || p[3]!=255) ++missing;
  }
  Require(checked>(size_t)side*side/100,"plane-interior check covers a material target area");
  return missing;
}
static float LodInput(const State& s,const float vertex[4])
{
  // Evaluate the unchanged shader expression using actual uploaded uniforms. Coverage
  // below independently exercises the real shader's use of the correction matrix.
  float p[4],clip[4],n[3],fixed[3]; Mul4(s.model,vertex,p); Mul4(s.projection,p,clip);
  for(int r=0;r<3;++r) n[r]=s.model[r]+s.model[8+r]; // inFaceNormal=(1,0,1)
  for(int r=0;r<3;++r) fixed[r]=s.enabled ? s.correction[r]*n[0]+s.correction[3+r]*n[1]+s.correction[6+r]*n[2] : n[r];
  return -Dot(p,fixed)/(clip[3]*clip[3]); // cota=textureNP=1
}
int main(int argc,char** argv)
{
  const int side=argc>1 ? std::atoi(argv[1]) : 128;
  Require(side>=64 && side<=4096,"size 64..4096");
  Require(SDL_Init(SDL_INIT_VIDEO)==0,"SDL video");
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,4); SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,1);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_CORE);
  SDL_Window* window=SDL_CreateWindow("R3D remap",0,0,128,128,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);
  Require(window,"GL window"); SDL_GLContext context=SDL_GL_CreateContext(window); Require(context,"GL context");
  glewExperimental=GL_TRUE; Require(glewInit()==GLEW_OK,"GLEW"); while(glGetError()!=GL_NO_ERROR) {}
  std::printf("renderer=%s size=%d\n",glGetString(GL_RENDERER),side);
  Util::Config::Node config("test"); config.Add("QuadRendering",false);
  New3D::R3DShader shader(config); Require(shader.LoadShader(),"load production shader"); shader.SetShader(true);
  GLint current=0,linked=0; glGetIntegerv(GL_CURRENT_PROGRAM,&current); GLuint program=(GLuint)current;
  glGetProgramiv(program,GL_LINK_STATUS,&linked); Require(linked,"production shader links");
  const GLint correctionEnabled=glGetUniformLocation(program,"vrCorrectFaceNormals");
  Require(correctionEnabled>=0,"production face-normal correction is active");
  GLuint vao,vbo; glGenVertexArrays(1,&vao); glGenBuffers(1,&vbo); glBindVertexArray(vao); glBindBuffer(GL_ARRAY_BUFFER,vbo);
  float vertices[24]; for(int i=0;i<6;++i) for(int j=0;j<4;++j) vertices[i*4+j]=corners[indices[i]][j];
  glBufferData(GL_ARRAY_BUFFER,sizeof(vertices),vertices,GL_STATIC_DRAW);
  GLint position=shader.GetVertexAttribPos("inVertex"); glEnableVertexAttribArray(position); glVertexAttribPointer(position,4,GL_FLOAT,GL_FALSE,0,nullptr);
  auto attribute=[&](const char* name,float x,float y,float z,float w) {
    GLint at=shader.GetVertexAttribPos(name); if(at>=0) glVertexAttrib4f(at,x,y,z,w);
  };
  attribute("inFaceNormal",1,0,1,0); attribute("inNormal",1,0,1,0);
  attribute("inColour",1,0,0,1); attribute("inTextureNP",1,0,0,1);
  GLuint banks[2]; glGenTextures(2,banks); const unsigned short white=0xffff;
  for(int i=0;i<2;++i) {
    glActiveTexture(GL_TEXTURE0+i); glBindTexture(GL_TEXTURE_2D,banks[i]);
    glTexImage2D(GL_TEXTURE_2D,0,GL_R16UI,1,1,0,GL_RED_INTEGER,GL_UNSIGNED_SHORT,&white);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST); glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
  }
  glActiveTexture(GL_TEXTURE0);
  GLuint textures[4],fbos[4]; glGenTextures(4,textures); glGenFramebuffers(4,fbos);
  for(int i=0;i<4;++i) {
    glBindTexture(GL_TEXTURE_2D,textures[i]); glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,side,side,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);
    glBindFramebuffer(GL_FRAMEBUFFER,fbos[i]); glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,textures[i],0);
    Require(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE,"final test target");
  }
  // Restore bank zero after allocating final targets; both samplers must remain integer textures.
  glBindTexture(GL_TEXTURE_2D,banks[0]);
  New3D::Viewport vp{}; vp.vpWidth=496; vp.vpHeight=384;
  vp.angle_left=-.5f; vp.angle_right=.5f; vp.angle_top=.5f; vp.angle_bottom=-.5f;
  vp.cota=1; vp.hardwareStep=0x20; vp.spotEllipse[2]=vp.spotEllipse[3]=1;
  New3D::Model model; for(int i=0;i<16;++i) model.modelMat[i]=i%5==0 ? 1.f : 0.f;
  model.modelMat[14]=-2;
  New3D::Mesh mesh;
  auto prepare=[&](New3D::VREye* eye,bool remap,bool legacy) {
    shader.SetShader(true); shader.SetVREye(eye);
    shader.SetVRReference(true,remap ? -.8f : -.5f,remap ? 1.2f : .5f,
      remap ? .225f : .5f,remap ? -.525f : -.5f);
    // FrustumRZ post-multiplies; production CalcViewport starts from identity too.
    vp.projectionMatrix.LoadIdentity();
    vp.projectionMatrix.FrustumRZ(eye ? eye->tanLeft : -.5f,eye ? eye->tanRight : .5f,
      eye ? eye->tanDown : -.5f,eye ? eye->tanUp : .5f,.01f);
    shader.SetViewportUniforms(&vp); shader.SetModelStates(&model); shader.SetMeshUniforms(&mesh);
    shader.SetLayer(New3D::Layer::colour); shader.DiscardAlpha(true);
    GLint enabled=0; glGetUniformiv(program,correctionEnabled,&enabled);
    Require((enabled!=0)==(eye && remap),"correction enable/reset follows actual viewport remap");
    // Test-only negative control restores precisely the previous CalcBackFace path.
    if(legacy) glUniform1i(correctionEnabled,GL_FALSE);
    return ReadState(program);
  };
  auto draw=[&](int target,New3D::VREye* eye,bool remap,bool legacy) {
    glBindFramebuffer(GL_FRAMEBUFFER,fbos[target]); glViewport(0,0,side,side);
    glDisable(GL_BLEND); glDisable(GL_DEPTH_TEST); glDisable(GL_STENCIL_TEST); glDisable(GL_CULL_FACE); glDisable(GL_SCISSOR_TEST);
    glClearColor(0,0,1,1); glClear(GL_COLOR_BUFFER_BIT);
    State state=prepare(eye,remap,legacy);
    glBindVertexArray(vao); glDrawArrays(GL_TRIANGLES,0,6); glFlush(); return state;
  };
  // Flat control before and after remapped eye draws, with a nonuniform legacy B.
  model.modelMat[0]=1.1f; model.modelMat[5]=.7f; model.modelMat[10]=.9f;
  State flat=draw(0,nullptr,false,false); auto flatBefore=Pixels(side); size_t checked=0;
  Require(Missing(side,flat,flatBefore,checked)==0,"original flat plane coverage");
  model.modelMat[0]=model.modelMat[5]=model.modelMat[10]=1;
  const float c=std::cos(.12f),s=std::sin(.12f);
  New3D::VREye eyes[2];
  for(int eye=0;eye<2;++eye) {
    eyes[eye].view[0]=c; eyes[eye].view[2]=-s; eyes[eye].view[8]=s; eyes[eye].view[10]=c;
    eyes[eye].tanLeft=eye ? -1.1f : -.95f; eyes[eye].tanRight=eye ? .9f : 1.05f;
    eyes[eye].tanUp=eye ? .9f : .85f; eyes[eye].tanDown=eye ? -.75f : -.8f;
  }
  for(int mode=0;mode<3;++mode) {
    size_t oldMissing=0,interior=0;
    for(int sample=0;sample<8;++sample) {
      State states[4];
      for(int eye=0;eye<2;++eye) {
        eyes[eye].stereo=mode==2; eyes[eye].convergence=4;
        eyes[eye].view[12]=mode==2 ? 0 : (eye ? .04f : -.04f);
        states[eye]=draw(eye,&eyes[eye],mode!=0,false);
        states[2+eye]=draw(2+eye,&eyes[eye],mode!=0,true);
      }
      glFinish(); // no pixel readback until both eyes and both controls are queued
      for(int eye=0;eye<2;++eye) {
        glBindFramebuffer(GL_FRAMEBUFFER,fbos[eye]); auto fixed=Pixels(side);
        size_t count=0; Require(Missing(side,states[eye],fixed,count)==0,"corrected full plane interiors");
        glBindFramebuffer(GL_FRAMEBUFFER,fbos[2+eye]); auto old=Pixels(side);
        size_t oldCount=0; const size_t missing=Missing(side,states[2+eye],old,oldCount);
        Require(count==oldCount,"identical geometry in old/new normal controls");
        if(mode==0) Require(fixed==old,"no-remap stereo is byte-identical to legacy");
        else Require(missing>count/10,"old affine-normal path must lose substantial plane interior");
        oldMissing+=missing; interior+=count;
      }
    }
    std::printf("mode=%d checked=%zu corrected_missing=0 legacy_missing=%zu pairs=8\n",mode,interior,oldMissing);
  }
  // Cinema k scales modelMat, including its normal, exactly as before. Verify the
  // culling/LOD matrix emitted by actual setters keeps w^2 * face-dot invariant.
  auto& cinema=eyes[0]; cinema.stereo=true; cinema.view[12]=0;
  float base[4]{};
  for(int scale=1;scale<=4;scale*=2) {
    cinema.convergence=2.f*scale; const State state=prepare(&cinema,true,false);
    for(int v=0;v<4;++v) {
      const float lod=LodInput(state,corners[v]); Require(lod>0 && std::isfinite(lod),"finite front-face LOD input");
      if(scale==1) base[v]=lod;
      else Require(std::fabs(lod-base[v])<1e-5f,"cinema scale preserves LOD input");
    }
  }
  model.modelMat[0]=1.1f; model.modelMat[5]=.7f; model.modelMat[10]=.9f;
  draw(0,nullptr,false,false); Require(Pixels(side)==flatBefore,"remap to flat resets correction without changing legacy B");
  Require(glGetError()==GL_NO_ERROR,"GL error-free production remap regression");
  shader.UnloadShader(); glDeleteFramebuffers(4,fbos); glDeleteTextures(4,textures); glDeleteTextures(2,banks);
  glDeleteBuffers(1,&vbo); glDeleteVertexArrays(1,&vao); SDL_GL_DeleteContext(context); SDL_DestroyWindow(window); SDL_Quit();
  std::puts("PASS: production R3DShader affine plane coverage, old-path failure, asymmetric eyes, flat reset, cinema LOD");
}
