#ifndef INCLUDED_CONTROLLER_AIM_H
#define INCLUDED_CONTROLLER_AIM_H

#include <openxr/openxr.h>
#include <cmath>

// OpenXR aim space and normalized gun assets share -Z forward, +Y up.
// Keep these operations independent of GL so the production ray/model contract is testable.
namespace ControllerAim {
inline XrQuaternionf Multiply(const XrQuaternionf& a, const XrQuaternionf& b)
{
  return {a.w*b.x+a.x*b.w+a.y*b.z-a.z*b.y,
          a.w*b.y-a.x*b.z+a.y*b.w+a.z*b.x,
          a.w*b.z+a.x*b.y-a.y*b.x+a.z*b.w,
          a.w*b.w-a.x*b.x-a.y*b.y-a.z*b.z};
}

inline XrQuaternionf Correct(const XrQuaternionf& aim, float rightDegrees, float upDegrees)
{
  const float halfDegrees = 0.008726646259971648f;
  const float x = upDegrees * halfDegrees, y = -rightDegrees * halfDegrees;
  return Multiply(aim, Multiply({0, std::sin(y), 0, std::cos(y)},
                                {std::sin(x), 0, 0, std::cos(x)}));
}

inline XrVector3f Forward(const XrQuaternionf& q)
{
  return {-2*(q.x*q.z+q.w*q.y), -2*(q.y*q.z-q.w*q.x),
          -(1-2*(q.x*q.x+q.y*q.y))};
}

inline XrVector3f SceneOrigin(XrVector3f hand, const XrVector3f& head, bool positional)
{
  return positional ? hand : XrVector3f{hand.x-head.x, hand.y-head.y, hand.z-head.z};
}

inline bool Project(const XrVector3f& origin, const XrVector3f& direction,
                    float distance, float scale, float left, float right, float top, float bottom,
                    float& u, float& v)
{
  if (direction.z >= -1e-4f || distance <= 0 || scale <= 0 || right <= left || top <= bottom)
    return false;
  const float t = (-distance-origin.z)/direction.z;
  if (t <= 0) return false;
  const float x = (origin.x+t*direction.x)/(distance*scale);
  const float y = (origin.y+t*direction.y)/(distance*scale);
  u = (x-left)/(right-left);
  v = (top-y)/(top-bottom);
  return std::isfinite(u) && std::isfinite(v);
}

// Render-time projection, including clipping, uses the current panel geometry. It never
// consumes or mutates the normalized input already sent to the asynchronous emulator.
inline bool ProjectPointer(const XrVector3f& origin, const XrVector3f& direction,
                           float distance, float scale, float left, float right, float top, float bottom,
                           bool extended, float& u, float& v)
{
  if (!Project(origin, direction, distance, scale, left, right, top, bottom, u, v))
    return false;
  const float low = extended ? -1.f : 0.f, high = extended ? 2.f : 1.f;
  return u >= low && u <= high && v >= low && v <= high;
}

// Measured Lost World screen-to-gun conversion paired with the 0x123C wide
// calibration below. Previously these lived only in the optional PC settings file.
struct GunMapping { float scaleX, offsetX, scaleY, offsetY; };
inline GunMapping LostWorldMapping(bool wide)
{
  return wide ? GunMapping{0.5497f, 0.2162f, 0.5507f, 0.2155f}
              : GunMapping{1.f, 0.f, 1.f, 0.f};
}
}
#endif
