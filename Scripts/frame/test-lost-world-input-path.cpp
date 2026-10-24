#include "Inputs/Inputs.h"
#include "OSD/SDL/SDLInputSystem.h"
#include "OSD/VR/ControllerAim.h"
#include "OSD/VR/VRInput.h"
#include "Util/NewConfig.h"

#include <cassert>
#include <cstdio>
#include <memory>
#include <string>

VRVirtualInput g_vrInput;

static UINT16 PollGunX(CInputs& inputs, float normalizedX)
{
  g_vrInput.active = true;
  g_vrInput.x = normalizedX;
  assert(inputs.Poll(nullptr, 0, 0, 992, 768));
  return inputs.analogGunX[0]->value;
}

static UINT16 PollMappedGunX(CInputs& inputs, float screenX, const ControllerAim::GunMapping& mapping)
{
  return PollGunX(inputs, screenX * mapping.scaleX + mapping.offsetX);
}

int main()
{
  assert(SDL_InitSubSystem(SDL_INIT_JOYSTICK) == 0);
  const int neutralJoystickIndex = SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_UNKNOWN, 1, 0, 0);
  assert(neutralJoystickIndex >= 0);
  SDL_Joystick* neutralJoystick = SDL_JoystickOpen(neutralJoystickIndex);
  assert(neutralJoystick != nullptr);
  assert(SDL_JoystickSetVirtualAxis(neutralJoystick, 0, 0) == 0);

  Util::Config::Node config("Global");
  {
    auto system = std::make_shared<CSDLInputSystem>(config, false);
    CInputs inputs(system);
    system->SetDisplayGeom(0, 0, 992, 768);
    assert(inputs.Initialize());

    // Keep the shipped mouse-plus-stick mapping while the virtual stick stays neutral.
    // With no other joystick, this is the default JOY1_XAXIS mapping.
    const std::string axisMapping = "MOUSE_XAXIS,JOY" + std::to_string(neutralJoystickIndex + 1) + "_XAXIS";
    inputs["AnalogGunX"]->SetMapping(axisMapping.c_str());
    inputs["AnalogGunLeft"]->SetMapping("NONE");
    inputs["AnalogGunRight"]->SetMapping("NONE");

    const auto wide = ControllerAim::LostWorldMapping(true);
    const auto identity = ControllerAim::LostWorldMapping(false);

    // Baseline: the previous fallback offset made raw screen center land at ADC 126.
    const ControllerAim::GunMapping previousWide{wide.scaleX, 0.2162f, wide.scaleY, wide.offsetY};
    assert(PollMappedGunX(inputs, 0.5f, previousWide) == 126);

    // The corrected fallback sends screen center through CSDLInputSystem's virtual
    // mouse, CMseAxisInputSource, and the actual CAxisInput to Lost World's 0x80 center.
    assert(PollMappedGunX(inputs, 0.5f, wide) == 0x80);

    // Moving the target left/right remains monotonic and keeps the same handedness.
    const UINT16 left = PollMappedGunX(inputs, 0.25f, wide);
    const UINT16 right = PollMappedGunX(inputs, 0.75f, wide);
    assert(left < 0x80 && right > 0x80 && left < right);

    // Non-wide games retain identity mapping and the same center/direction contract.
    assert(PollMappedGunX(inputs, 0.5f, identity) == 0x80);
    assert(PollMappedGunX(inputs, 0.25f, identity) < 0x80);
    assert(PollMappedGunX(inputs, 0.75f, identity) > 0x80);

    // Main's ValueAsDefault call must keep a user's explicit calibration setting.
    Util::Config::Node runtimeConfig("Global");
    runtimeConfig.Set<float>("VRAimOffsetX", 0.4f);
    assert(runtimeConfig["VRAimOffsetX"].ValueAsDefault<float>(wide.offsetX) == 0.4f);
    assert(runtimeConfig["VRAimOffsetY"].ValueAsDefault<float>(wide.offsetY) == wide.offsetY);

    std::puts("Lost World input path: previous center 126, corrected center 128, neutral joystick fallback, horizontal direction, identity mapping and explicit override passed");
  }

  SDL_JoystickClose(neutralJoystick);
  assert(SDL_JoystickDetachVirtual(neutralJoystickIndex) == 0);
  SDL_QuitSubSystem(SDL_INIT_JOYSTICK);
}
