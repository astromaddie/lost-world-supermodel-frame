#ifndef INCLUDED_DIAGNOSTIC_VIEW_LOCK_H
#define INCLUDED_DIAGNOSTIC_VIEW_LOCK_H
#include <openxr/openxr.h>

// Held-scene observations must hold projection and eye separation as well as head pose.
// Apply to the actual XrViews consumed by both rendering and xrEndFrame submission.
struct DiagnosticViewLock
{
  bool locked = false;
  XrFovf fov[2]{};
  float ipd = 0;

  void Apply(bool hold, XrView views[2], float& currentIpd)
  {
    if (!hold) return;
    if (!locked) {
      for (int eye = 0; eye < 2; ++eye) fov[eye] = views[eye].fov;
      ipd = currentIpd;
      locked = true;
    }
    for (int eye = 0; eye < 2; ++eye) views[eye].fov = fov[eye];
    currentIpd = ipd;
  }
};
#endif
