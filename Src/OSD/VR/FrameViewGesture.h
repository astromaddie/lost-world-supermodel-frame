#ifndef INCLUDED_FRAME_VIEW_GESTURE_H
#define INCLUDED_FRAME_VIEW_GESTURE_H

#include <cstdint>

// Distinguishes the Steam Frame left View button's short release from its one-second hold.
// Times are OpenXR predicted display times in nanoseconds; keeping this helper independent of
// OpenXR makes the exact production transition logic testable at Frame refresh rates.
class FrameViewGesture
{
public:
  enum Event : unsigned
  {
    none = 0,
    tap = 1,
    hold = 2
  };

  Event Update(bool focused, bool pressed, int64_t timeNs)
  {
    if (!focused) {
      m_pressed = false;
      m_holdSent = false;
      m_armed = false;
      return none;
    }

    // After focus returns, require a neutral sample before accepting a new press. If the
    // controller is still held, it cannot turn into a fresh hold or a false tap on regain.
    if (!m_armed) {
      if (!pressed)
        m_armed = true;
      m_pressed = pressed;
      return none;
    }

    if (pressed) {
      if (!m_pressed) {
        m_pressed = true;
        m_pressTimeNs = timeNs;
        m_holdSent = false;
      }
      if (!m_holdSent && timeNs >= m_pressTimeNs && timeNs - m_pressTimeNs >= kHoldNs) {
        m_holdSent = true;
        return hold;
      }
      return none;
    }

    Event event = none;
    if (m_pressed && !m_holdSent && timeNs >= m_pressTimeNs) {
      if (timeNs - m_pressTimeNs >= kHoldNs)
        event = hold;  // a release exactly at the threshold still completes the hold
      else
        event = tap;
    }
    m_pressed = false;
    m_holdSent = false;
    return event;
  }

private:
  static constexpr int64_t kHoldNs = 1000000000LL;
  bool m_armed = true;
  bool m_pressed = false;
  bool m_holdSent = false;
  int64_t m_pressTimeNs = 0;
};

#endif
