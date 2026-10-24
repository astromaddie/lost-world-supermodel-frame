// Real GL coverage test: production triangle shaders and R3DFrameBuffers, no ROM.
// Run with a desktop GL 4.1 context (builder: xvfb-run + Mesa llvmpipe).
#include <GL/glew.h>
#include <SDL2/SDL.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include "Graphics/New3D/R3DFrameBuffers.h"
#include "Graphics/New3D/R3DShaderTriangles.h"
#include "Graphics/New3D/R3DShaderCommon.h"

static void Require(bool ok, const char* what)
{
  if (!ok) { std::fprintf(stderr, "FAIL: %s\n", what); std::exit(1); }
}
static GLuint Shader(GLenum kind, const char* a, const char* b = nullptr)
{
  GLuint shader = glCreateShader(kind);
  const char* parts[] = {a, b};
  glShaderSource(shader, b ? 2 : 1, parts, nullptr);
  glCompileShader(shader);
  GLint ok = 0; glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
  if (!ok) { char log[8192]; glGetShaderInfoLog(shader, sizeof(log), nullptr, log); std::fprintf(stderr, "%s\n", log); }
  Require(ok, "production shader compiles");
  return shader;
}
static void Uniform(GLuint p, const char* name, int value)
{ glUniform1i(glGetUniformLocation(p, name), value); }
static void Attribute(GLuint p, const char* name, float x, float y, float z, float w)
{
  GLint at = glGetAttribLocation(p, name);
  if (at >= 0) glVertexAttrib4f(at, x, y, z, w);
}
static void CheckCoverage(int side, float eyeShift, int skyBlue)
{
  std::vector<unsigned char> pixels((size_t)side*side*4);
  glReadPixels(0,0,side,side,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());
  size_t checked=0;
  for(int y=0;y<side;++y) for(int x=0;x<side;++x) {
    const float sx=2.f*(x+.5f)/side-1.f+eyeShift;
    const float sy=2.f*(y+.5f)/side-1.f;
    int r=0,g=0,b=0;
    if(std::fabs(sx)>.75f || std::fabs(sy)>.75f) b=skyBlue;
    else if(std::fabs(sx)<.68f && std::fabs(sy)<.68f) {
      r=255;
      if(sx>-.58f && sx<-.22f) { r=127; g=128; }
      else if(sx>.22f && sx<.58f) g=128;
      else if((sx>-.62f && sx<-.18f) || (sx>.18f && sx<.62f)) continue;
    } else continue; // exclude rasterization edges, never covered interiors
    const auto* p=&pixels[((size_t)y*side+x)*4];
    if(std::abs(int(p[0])-r)>2 || std::abs(int(p[1])-g)>2 || std::abs(int(p[2])-b)>2) {
      std::fprintf(stderr,"pixel %d,%d actual=%u,%u,%u expected=%d,%d,%d\n",x,y,p[0],p[1],p[2],r,g,b);
      Require(false,"opaque coverage / transparency composite");
    }
    ++checked;
  }
  Require(checked>(size_t)side*side/2,"coverage check spans most of the target");
}
int main(int argc, char** argv)
{
  const int side = argc > 1 ? std::atoi(argv[1]) : 128;
  Require(side >= 64 && side <= 4096, "test size must be 64..4096");
  Require(SDL_Init(SDL_INIT_VIDEO)==0, "SDL video");
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
  SDL_Window* window = SDL_CreateWindow("R3D coverage",0,0,128,128,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);
  Require(window, "GL window");
  SDL_GLContext context = SDL_GL_CreateContext(window); Require(context, "GL context");
  glewExperimental=GL_TRUE; Require(glewInit()==GLEW_OK, "GLEW");
  while (glGetError()!=GL_NO_ERROR) {}
  std::printf("renderer=%s\n", glGetString(GL_RENDERER));
  GLuint vs=Shader(GL_VERTEX_SHADER,vertexShaderR3D);
  GLuint fs=Shader(GL_FRAGMENT_SHADER,fragmentShaderR3D,fragmentShaderR3DCommon);
  GLuint program=glCreateProgram(); glAttachShader(program,vs); glAttachShader(program,fs); glLinkProgram(program);
  GLint linked=0; glGetProgramiv(program,GL_LINK_STATUS,&linked); Require(linked,"production shader links");
  GLuint vao,vbo; glGenVertexArrays(1,&vao); glGenBuffers(1,&vbo);
  glBindVertexArray(vao); glBindBuffer(GL_ARRAY_BUFFER,vbo);
  GLint vertex=glGetAttribLocation(program,"inVertex");
  glEnableVertexAttribArray(vertex); glVertexAttribPointer(vertex,4,GL_FLOAT,GL_FALSE,0,nullptr);
  glUseProgram(program);
  Uniform(program,"textureBank[0]",0); Uniform(program,"textureBank[1]",1);
  glUniform1f(glGetUniformLocation(program,"nodeAlpha"),1);
  glUniform1f(glGetUniformLocation(program,"modelScale"),1);
  Uniform(program,"hardwareStep",0x20);
  glUniform4f(glGetUniformLocation(program,"spotEllipse"),0,0,1,1);
  // Valid integer samplers even when texturing is disabled.
  GLuint banks[2]; glGenTextures(2,banks);
  const unsigned short white=0xffff;
  for(int i=0;i<2;++i) {
    glActiveTexture(GL_TEXTURE0+i); glBindTexture(GL_TEXTURE_2D,banks[i]);
    glTexImage2D(GL_TEXTURE_2D,0,GL_R16UI,1,1,0,GL_RED_INTEGER,GL_UNSIGNED_SHORT,&white);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
  }
  glActiveTexture(GL_TEXTURE0);
  const float identity[16]={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
  // Production FrustumRZ: near=1, asymmetric eye shift supplied below.
  float projection[16]={1,0,0,0,0,1,0,0,0,0,0,-1,0,0,1,0};
  glUniformMatrix4fv(glGetUniformLocation(program,"modelMat"),1,GL_FALSE,identity);
  auto draw=[&](float x0,float x1,float z,float r,float g,float b,float alpha,int layer) {
    // Positions expressed as projected coordinates so all test depths share coverage.
    const float v[]={x0*-z,-.7f*-z,z,1,x1*-z,-.7f*-z,z,1,x1*-z,.7f*-z,z,1,
                     x0*-z,-.7f*-z,z,1,x1*-z,.7f*-z,z,1,x0*-z,.7f*-z,z,1};
    glUseProgram(program); glBindVertexArray(vao); glBindBuffer(GL_ARRAY_BUFFER,vbo);
    glBufferData(GL_ARRAY_BUFFER,sizeof(v),v,GL_STREAM_DRAW);
    Attribute(program,"inColour",r,g,b,alpha);
    Attribute(program,"inNormal",0,0,1,0); Attribute(program,"inFaceNormal",0,0,1,0);
    Uniform(program,"colourLayer",layer); glDrawArrays(GL_TRIANGLES,0,6);
  };
  {
    New3D::R3DFrameBuffers layers;
    Require(layers.CreateFBO(side,side)==Result::OKAY,"layer FBO creation");
    // XR has a distinct final image for each eye, but New3D reuses these same MRTs.
    GLuint finalTex[2],finalFbo[2];
    glGenTextures(2,finalTex); glGenFramebuffers(2,finalFbo);
    for(int eye=0;eye<2;++eye) {
      glBindTexture(GL_TEXTURE_2D,finalTex[eye]);
      glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,side,side,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);
      glBindFramebuffer(GL_FRAMEBUFFER,finalFbo[eye]);
      glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,finalTex[eye],0);
      Require(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE,"final eye FBO");
    }
    const char* skipFinish=std::getenv("SM_R3D_TEST_SKIP_FINISH");
    const bool finishPair=!(skipFinish && skipFinish[0]=='1' && skipFinish[1]=='\0');
    std::printf("size=%d pairs=8 readback=after_both_eyes pair_finish=%s\n",side,finishPair?"true":"false");
    for(int sample=0;sample<8;++sample) {
      for(int eye=0;eye<2;++eye) {
        // Distinct sky colours make accidental cross-eye target reuse observable too.
        glBindFramebuffer(GL_FRAMEBUFFER,finalFbo[eye]);
        glClearColor(0,0,eye ? 192.f/255.f : 1.f,1); glClear(GL_COLOR_BUFFER_BIT);
        layers.SetFBO(New3D::Layer::colour); glClearColor(0,0,0,0); glClear(GL_COLOR_BUFFER_BIT);
        glViewport(0,0,side,side); glDisable(GL_BLEND); glEnable(GL_DEPTH_TEST); glDepthMask(GL_TRUE);
        glDisable(GL_STENCIL_TEST); glClearDepth(0); glClear(GL_DEPTH_BUFFER_BIT|GL_STENCIL_BUFFER_BIT);
        glDepthFunc(GL_GEQUAL); glUseProgram(program);
        projection[8]=eye ? .08f : -.08f;
        glUniformMatrix4fv(glGetUniformLocation(program,"projMat"),1,GL_FALSE,projection);
        draw(-.7f,.7f,-4,1,0,0,1,0); // solid foreground
        glDepthFunc(GL_GREATER); layers.StoreDepth(); layers.SetFBO(New3D::Layer::trans1);
        draw(-.7f,.7f,-8,0,1,0,.5f,1); // behind: must not punch through
        draw(-.6f,-.2f,-2,0,1,0,.5f,1); // front, left band
        layers.RestoreDepth(); layers.SetFBO(New3D::Layer::trans2);
        draw(.2f,.6f,-2,1,1,0,.5f,2); // front, right band; restored opaque depth
        layers.SetFBO(New3D::Layer::none); glBindFramebuffer(GL_FRAMEBUFFER,finalFbo[eye]); layers.Draw();
        // DrawHandGuns leaves the conventional clear depth; the next scene must reset it.
        glClearDepth(1);
        glBindFramebuffer(GL_FRAMEBUFFER,0);
        glFlush(); // production EndEye submits work without waiting for this eye
      }
      // Never observe the left image before reusing the shared layers for the right eye.
      // Default matches production EndFrame; optional bypass isolates this explicit wait.
      if(finishPair) glFinish();
      for(int eye=0;eye<2;++eye) {
        glBindFramebuffer(GL_FRAMEBUFFER,finalFbo[eye]);
        CheckCoverage(side,eye ? .08f : -.08f,eye ? 192 : 255);
      }
      Require(glGetError()==GL_NO_ERROR,"GL errors across repeated stereo passes");
    }
    glBindFramebuffer(GL_FRAMEBUFFER,0);
    glDeleteFramebuffers(2,finalFbo); glDeleteTextures(2,finalTex);
  }
  glDeleteTextures(2,banks); glDeleteBuffers(1,&vbo); glDeleteVertexArrays(1,&vao);
  glDeleteProgram(program); glDeleteShader(vs); glDeleteShader(fs);
  SDL_GL_DeleteContext(context); SDL_DestroyWindow(window); SDL_Quit();
  std::puts("PASS: production triangle/depth/MRT composite, 8 queued asymmetric stereo pairs, no inter-eye readback");
}
