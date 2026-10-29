// Owner-local Win32 input wiring. Included inside windows_main.cpp's namespace
// after State; it owns no Runtime gesture or generation policy.
bool isPromotedPointerMouse() noexcept {
  return (static_cast<ULONG_PTR>(GetMessageExtraInfo()) & 0xFFFFFF00U) == 0xFF515700U;
}

std::uint64_t nativeTimestamp(DWORD time) noexcept {
  const auto now = GetTickCount64();
  const auto age = static_cast<DWORD>(static_cast<DWORD>(now) - time);
  return (std::max)(std::uint64_t{1}, now >= age ? (now - age) * 1'000'000U : 1U);
}

void logInput(State& state, const char* event, bool accepted,
              const canvas::input::PointerKey& key,
              const canvas::input::PlatformPointerBatch* batch = nullptr) {
  if (!state.inputLog) return;
  auto& composition = state.host->composition();
  const auto receipt = composition.readback();
  const auto snapshot = composition.controlFacade().readRuntimeState();
  const auto frame = composition.lastSubmittedCanonicalFrame();
  const auto text = composition.controlFacade().readTextEditor();
  std::fprintf(state.inputLog.get(),
      "event=%s accepted=%d source=%llu pointer=%llu generation=%llu state=%u objects=%zu operations=%llu frame=%llu handoff=%zu preview=%d camera=(%.6f,%.3f,%.3f) tool=%u text_phase=%u text_session=%llu canvas_focus=%d\n",
      event, accepted, static_cast<unsigned long long>(key.source),
      static_cast<unsigned long long>(key.pointer), static_cast<unsigned long long>(key.generation),
      static_cast<unsigned>(receipt.state), composition.semanticObjectCount(),
      static_cast<unsigned long long>(snapshot.identity.documentRevision),
      static_cast<unsigned long long>(frame ? frame->frameId.value() : 0U),
      state.frameOpportunity.pendingCanonicalHandoff, composition.previewActive(),
      snapshot.camera.scale, snapshot.camera.translationX, snapshot.camera.translationY,
      snapshot.tool.toolId, static_cast<unsigned>(text.phase),
      static_cast<unsigned long long>(text.token.session), GetFocus() == state.window);
  if (batch) for (const auto& sample : batch->samples) {
    std::fprintf(state.inputLog.get(),
        " sample source=%llu pointer=%llu sequence=%llu ns=%llu phase=%u xy=(%.3f,%.3f) pressure=%.6f pressure_available=%d tool=%u geometry=(%.3f,%.3f,%.3f,%d) history=%d provenance=%u\n",
        static_cast<unsigned long long>(sample.source), static_cast<unsigned long long>(sample.pointer),
        static_cast<unsigned long long>(sample.sequence), static_cast<unsigned long long>(sample.timestampNs),
        static_cast<unsigned>(sample.phase), sample.x, sample.y, sample.pressure,
        sample.capabilities.pressure, static_cast<unsigned>(sample.capabilities.tool),
        sample.contact.width, sample.contact.height, sample.contact.orientation, sample.contact.available,
        sample.capabilities.coalescedHistory, static_cast<unsigned>(sample.provenance));
  }
  std::fflush(state.inputLog.get());
}

void cancelKey(State& state, const canvas::input::PointerKey& key, const char* reason) {
  state.timestamp = (std::max)(state.timestamp + 1U, nativeTimestamp(GetTickCount()));
  const bool cancelled = state.host->publicComposition().cancelContact(
      key, ++state.sequence, state.timestamp);
  logInput(state, reason, cancelled, key);
  InvalidateRect(state.window, nullptr, FALSE);
}

void cancelNativePointer(State& state, UINT32 id, const char* reason) {
  const auto it = state.nativeContacts.find(id);
  if (it == state.nativeContacts.end()) return;
  const auto key = it->second.key;
  state.nativeContacts.erase(it);
  cancelKey(state, key, reason);
}

void releaseHostMouseCapture(State& state) {
  if (state.window && GetCapture() == state.window) {
    const bool wasReleasing = state.releasingPointerUp;
    state.releasingPointerUp = true;
    (void)ReleaseCapture();
    state.releasingPointerUp = wasReleasing;
  }
}

void cancelHostPointers(State& state, const char* reason) {
  while (!state.nativeContacts.empty()) cancelNativePointer(state, state.nativeContacts.begin()->first, reason);
  if (state.mouseKey) cancelKey(state, *state.mouseKey, reason);
  state.mouseKey.reset();
  state.lastMousePoint.reset();
  releaseHostMouseCapture(state);
}

void removeNativeDevice(State& state, HANDLE device) {
  for (auto it = state.nativeContacts.begin(); it != state.nativeContacts.end();) {
    const auto id = it->first;
    const bool matches = it->second.last.sourceDevice == device;
    ++it;
    if (matches) cancelNativePointer(state, id, "device-removed");
  }
  for (auto it = state.nativeSources.begin(); it != state.nativeSources.end();) {
    if (it->first.first != device) { ++it; continue; }
    (void)state.host->unregisterSource(it->second, ++state.sequence, ++state.timestamp);
    it = state.nativeSources.erase(it);
  }
}

bool sameNativeObservation(const POINTER_INFO& a, const POINTER_INFO& b) noexcept {
  return a.PerformanceCount == b.PerformanceCount && a.dwTime == b.dwTime &&
      a.frameId == b.frameId && a.ptPixelLocation.x == b.ptPixelLocation.x &&
      a.ptPixelLocation.y == b.ptPixelLocation.y && a.pointerFlags == b.pointerFlags;
}

bool olderNativeObservation(const POINTER_INFO& a, const POINTER_INFO& b) noexcept {
  if (a.PerformanceCount && b.PerformanceCount) return a.PerformanceCount < b.PerformanceCount;
  return static_cast<LONG>(a.dwTime - b.dwTime) < 0;
}

bool sendNativePointer(State& state, UINT message, UINT32 id) {
  using namespace canvas::input;
  POINTER_INFO current{};
  if (!GetPointerInfo(id, &current)) {
    // An expired OS observation cannot be reconstructed from lParam. Release
    // only a contact already owned by this host and never invent an Up.
    cancelNativePointer(state, id, "pointer-info-unavailable");
    return true;
  }
  if (current.pointerType != PT_TOUCH && current.pointerType != PT_PEN) return false;
  auto existing = state.nativeContacts.find(id);
  const bool down = message == WM_POINTERDOWN;
  if (!down && existing == state.nativeContacts.end()) return true;
  if (existing != state.nativeContacts.end()) {
    if (olderNativeObservation(current, existing->second.last)) return true;
    if (down) return true; // Repeated Down cannot replace a live generation.
    if (existing->second.last.sourceDevice != current.sourceDevice ||
        existing->second.last.pointerType != current.pointerType) return true;
  }
  if (current.pointerFlags & POINTER_FLAG_CANCELED) {
    cancelNativePointer(state, id, "pointer-cancelled");
    return true;
  }
  (void)state.host->composition().ownerPump();
  const auto surface = state.host->composition().surfaceSnapshot();
  if (!surface.available || !surface.width || !surface.height ||
      !std::isfinite(surface.logicalWidth) || !std::isfinite(surface.logicalHeight) ||
      surface.logicalWidth <= 0.0F || surface.logicalHeight <= 0.0F) {
    cancelNativePointer(state, id, "surface-unavailable");
    return true;
  }
  const auto device = std::make_pair(current.sourceDevice, current.pointerType);
  auto sourceIt = state.nativeSources.find(device);
  if (sourceIt == state.nativeSources.end()) {
    const auto source = state.nextSource++;
    if (!state.host->registerSource(source)) return true;
    sourceIt = state.nativeSources.emplace(device, source).first;
  }
  const auto source = sourceIt->second;
  const auto identity = std::make_pair(source, id);
  const auto watermark = state.nativeWatermarks.find(identity);
  if (down && watermark != state.nativeWatermarks.end() &&
      (olderNativeObservation(current, watermark->second) ||
       sameNativeObservation(current, watermark->second))) return true;
  std::vector<POINTER_TOUCH_INFO> touches;
  std::vector<POINTER_PEN_INFO> pens;
  UINT32 count = 0U;
  bool historyAvailable = false;
  if (current.pointerType == PT_TOUCH && GetPointerTouchInfoHistory(id, &count, nullptr) && count) {
    touches.resize(count);
    const auto capacity = count;
    historyAvailable = GetPointerTouchInfoHistory(id, &count, touches.data()) != FALSE && count <= capacity;
    touches.resize(historyAvailable ? count : 0U);
  } else if (current.pointerType == PT_PEN && GetPointerPenInfoHistory(id, &count, nullptr) && count) {
    pens.resize(count);
    const auto capacity = count;
    historyAvailable = GetPointerPenInfoHistory(id, &count, pens.data()) != FALSE && count <= capacity;
    pens.resize(historyAvailable ? count : 0U);
  }
  if (!historyAvailable) {
    POINTER_TOUCH_INFO touch{};
    POINTER_PEN_INFO pen{};
    if (current.pointerType == PT_TOUCH && GetPointerTouchInfo(id, &touch)) touches.push_back(touch);
    else if (current.pointerType == PT_PEN && GetPointerPenInfo(id, &pen)) pens.push_back(pen);
  }
  const auto size = historyAvailable ? static_cast<std::size_t>(count) : 1U;
  const auto infoAt = [&](std::size_t i) -> const POINTER_INFO& {
    return !touches.empty() ? touches[i].pointerInfo : !pens.empty() ? pens[i].pointerInfo : current;
  };
  std::size_t end = down ? 1U : size;
  if (!down) for (std::size_t i = 0; i < size; ++i) {
    if (sameNativeObservation(infoAt(i), existing->second.last)) { end = i; break; }
  }
  PlatformPointerBatch batch;
  for (std::size_t n = end; n > 0U; --n) {
    const auto i = n - 1U;
    const auto& info = infoAt(i);
    if (info.pointerId != id || info.sourceDevice != current.sourceDevice ||
        info.pointerType != current.pointerType) continue;
    if (!down && olderNativeObservation(info, existing->second.last)) continue;
    POINT client = info.ptPixelLocation;
    if (!ScreenToClient(state.window, &client)) {
      cancelNativePointer(state, id, "coordinate-map-failed");
      return true;
    }
    PlatformPointerSample sample{};
    sample.source = source; sample.pointer = id;
    sample.sequence = ++state.sequence;
    sample.timestampNs = nativeTimestamp(info.dwTime);
    state.timestamp = (std::max)(state.timestamp, sample.timestampNs);
    sample.x = static_cast<float>(client.x) * surface.logicalWidth / surface.width;
    sample.y = static_cast<float>(client.y) * surface.logicalHeight / surface.height;
    sample.phase = down ? PointerPhase::kDown
        : i == 0U && message == WM_POINTERUP ? PointerPhase::kUp : PointerPhase::kMove;
    sample.provenance = i == 0U ? SampleProvenance::kConfirmedCurrent : SampleProvenance::kCoalescedHistory;
    sample.capabilities.tool = current.pointerType == PT_TOUCH ? PointerTool::kFinger : PointerTool::kPen;
    sample.capabilities.coalescedHistory = historyAvailable;
    sample.modifiers=(info.dwKeyStates&POINTER_MOD_SHIFT)?PointerModifiers::kShift:PointerModifiers::kNone;
    if (!touches.empty()) {
      const auto& touch = touches[i];
      sample.capabilities.pressure = (touch.touchMask & TOUCH_MASK_PRESSURE) != 0U;
      if (sample.capabilities.pressure) sample.pressure = static_cast<float>(touch.pressure) / 1024.0F;
      sample.capabilities.contactGeometry = (touch.touchMask & TOUCH_MASK_CONTACTAREA) != 0U;
      if (sample.capabilities.contactGeometry) {
        sample.contact.width = static_cast<float>(touch.rcContact.right - touch.rcContact.left) * surface.logicalWidth / surface.width;
        sample.contact.height = static_cast<float>(touch.rcContact.bottom - touch.rcContact.top) * surface.logicalHeight / surface.height;
        sample.contact.available = true;
      }
      if (touch.touchMask & TOUCH_MASK_ORIENTATION) sample.contact.orientation = static_cast<float>(touch.orientation);
    } else if (!pens.empty()) {
      const auto& pen = pens[i];
      sample.capabilities.pressure = (pen.penMask & PEN_MASK_PRESSURE) != 0U;
      if (sample.capabilities.pressure) sample.pressure = static_cast<float>(pen.pressure) / 1024.0F;
      if (pen.penMask & PEN_MASK_TILT_X) sample.tiltX = static_cast<float>(pen.tiltX);
      if (pen.penMask & PEN_MASK_TILT_Y) sample.tiltY = static_cast<float>(pen.tiltY);
      sample.capabilities.barrelButton = (pen.penFlags & PEN_FLAG_BARREL) != 0U;
      sample.capabilities.hardwareEraser = (pen.penFlags & PEN_FLAG_ERASER) != 0U;
      if (sample.capabilities.hardwareEraser) sample.capabilities.tool = PointerTool::kEraser;
    }
    batch.samples.push_back(sample);
  }
  if (batch.samples.empty()) return true;
  if (down && GetFocus() != state.window) SetFocus(state.window);
  const auto oldKey = down ? PointerKey{} : existing->second.key;
  auto key = down ? state.host->publicComposition().beginContact(batch)
                  : std::optional<PointerKey>(oldKey);
  const bool accepted = down ? key.has_value()
                             : state.host->publicComposition().submitContact(oldKey, batch);
  state.nativeWatermarks[identity] = current;
  logInput(state, "pointer", accepted, key.value_or(PointerKey{}), &batch);
  if (!accepted) {
    if (key) cancelKey(state, *key, "pointer-rejected");
    state.nativeContacts.erase(id);
  } else if (message == WM_POINTERUP) {
    state.nativeContacts.erase(id);
  } else if (key) {
    state.nativeContacts[id] = {*key, current};
  }
  InvalidateRect(state.window, nullptr, FALSE);
  return true;
}
