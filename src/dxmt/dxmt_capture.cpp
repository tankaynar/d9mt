#include "dxmt_capture.hpp"
#include "log/log.hpp"
#include "util_env.hpp"

#ifdef __WIN32
#include "windows.h"
#endif

namespace dxmt {

CaptureState::CaptureState() {
  if (env::getEnvVar("MTL_CAPTURE_ENABLED") != "1")
    return;
  if (env::getExeBaseName() != env::getEnvVar("DXMT_CAPTURE_EXECUTABLE"))
    return;
  WARN("DXMT capture enabled");
  state = State::Idle;
#ifdef _WIN32
  // d9mt diag: DXMT_CAPTURE_KEY=<VK code, e.g. 0x77 for F8> (default F10; SkyGfx uses F10)
  std::string key = env::getEnvVar("DXMT_CAPTURE_KEY");
  if (!key.empty()) {
    try {
      capture_key_ = (int)std::stoul(key, nullptr, 0);
    } catch (...) {
    }
  }
#endif
}

CaptureState::NextAction
CaptureState::getNextAction(uint64_t frame) {
  switch (state) {
  case State::Freezed:
    return NextAction::Nothing;
  case State::Idle:
    break;
  case State::WillCapture:
    if (frame == next_capture_frame) {
      state = State::Capturing;
      return NextAction::StartCapture;
    }
    break;
  case State::Capturing:
    if (frame > next_capture_frame) {
      next_capture_frame = 0;
      state = State::Idle;
      return NextAction::StopCapture;
    }
    break;
  }
  return NextAction::Nothing;
};

void
CaptureState::scheduleNextFrameCapture(uint64_t next) {
  if (state != State::Idle)
    return;
  state = State::WillCapture;
  next_capture_frame = next;
}

bool
CaptureState::shouldCaptureNextFrame() {
#ifdef _WIN32
  if (state != State::Idle)
    return false;
  SHORT state_ = GetAsyncKeyState(capture_key_);
  if (state_ & 0x8000 && !capture_key_pressed_) {
    capture_key_pressed_ = true;
    return true;
  }
  capture_key_pressed_ = state_ & 0x8000;
#endif
  return false;
};

} // namespace dxmt
