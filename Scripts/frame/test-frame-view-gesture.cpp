#include "../../Src/OSD/VR/FrameViewGesture.h"

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <initializer_list>

static int64_t TickAtHz(unsigned hz, uint64_t tick)
{
  return static_cast<int64_t>((tick * 1000000000ULL) / hz);
}

static void TestTapAtRefresh(unsigned hz)
{
  FrameViewGesture gesture;
  assert(gesture.Update(true, true, 0) == FrameViewGesture::none);
  for (uint64_t tick = 1; tick < hz / 4; ++tick)
    assert(gesture.Update(true, true, TickAtHz(hz, tick)) == FrameViewGesture::none);
  assert(gesture.Update(true, false, TickAtHz(hz, hz / 4)) == FrameViewGesture::tap);
  assert(gesture.Update(true, false, TickAtHz(hz, hz / 4 + 1)) == FrameViewGesture::none);
}

static void TestOneShotHoldAtRefresh(unsigned hz)
{
  FrameViewGesture gesture;
  assert(gesture.Update(true, true, 0) == FrameViewGesture::none);
  unsigned holds = 0;
  for (uint64_t tick = 1; tick <= hz * 2; ++tick)
    holds += gesture.Update(true, true, TickAtHz(hz, tick)) == FrameViewGesture::hold;
  assert(holds == 1);
  assert(gesture.Update(true, false, TickAtHz(hz, hz * 2 + 1)) == FrameViewGesture::none);
  assert(gesture.Update(true, false, TickAtHz(hz, hz * 2 + 2)) == FrameViewGesture::none);
}

static void TestReleaseAtHoldThreshold(unsigned hz)
{
  FrameViewGesture gesture;
  assert(gesture.Update(true, true, 0) == FrameViewGesture::none);
  assert(gesture.Update(true, false, TickAtHz(hz, hz)) == FrameViewGesture::hold);
  assert(gesture.Update(true, false, TickAtHz(hz, hz + 1)) == FrameViewGesture::none);
}

static void TestFocusLossRequiresNeutralSample(unsigned hz)
{
  FrameViewGesture gesture;
  assert(gesture.Update(true, true, 0) == FrameViewGesture::none);
  assert(gesture.Update(true, true, TickAtHz(hz, hz / 2)) == FrameViewGesture::none);
  assert(gesture.Update(false, false, TickAtHz(hz, hz / 2 + 1)) == FrameViewGesture::none);
  assert(gesture.Update(true, true, TickAtHz(hz, hz * 2)) == FrameViewGesture::none);
  assert(gesture.Update(true, true, TickAtHz(hz, hz * 4)) == FrameViewGesture::none);
  assert(gesture.Update(true, false, TickAtHz(hz, hz * 4 + 1)) == FrameViewGesture::none);
  assert(gesture.Update(true, false, TickAtHz(hz, hz * 4 + 2)) == FrameViewGesture::none);
  assert(gesture.Update(true, true, TickAtHz(hz, hz * 5)) == FrameViewGesture::none);
  assert(gesture.Update(true, false, TickAtHz(hz, hz * 5 + hz / 4)) == FrameViewGesture::tap);
}

int main()
{
  for (unsigned hz : {72, 90, 120}) {
    TestTapAtRefresh(hz);
    TestOneShotHoldAtRefresh(hz);
    TestReleaseAtHoldThreshold(hz);
    TestFocusLossRequiresNeutralSample(hz);
  }
  std::puts("Steam Frame View tap/hold state tests passed at 72/90/120 Hz.");
}
