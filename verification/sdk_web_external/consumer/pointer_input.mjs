// DOM collection only: the caller forwards these facts to its Runtime owner.
// logicalSize is in Runtime view coordinates; it is not a camera transform.
export function installPointerInput({canvas, logicalSize, submit, lifecycle = globalThis.window}) {
  const active = new Map();
  const sourceFor = type => type === 'touch' ? 3 : type === 'pen' ? 2 : type === 'mouse' ? 1 : 0;
  function forward(event, phase) {
    const source = sourceFor(event.pointerType), key = `${source}:${event.pointerId}`;
    if (!source) return;
    const previous = active.get(key);
    if ((phase === 0 && previous) || (phase !== 0 && !previous)) return;
    if (previous && Math.round(event.timeStamp * 1000000) < previous.born) return;
    const rect = canvas.getBoundingClientRect(), [width, height] = logicalSize();
    if (!(rect.width > 0 && rect.height > 0 && width > 0 && height > 0)) return;
    const history = phase === 1 && event.getCoalescedEvents ? event.getCoalescedEvents() : [];
    const samples = [...history];
    const last = samples.at(-1);
    if (!last || last.timeStamp !== event.timeStamp || last.clientX !== event.clientX ||
        last.clientY !== event.clientY || last.pressure !== event.pressure ||
        last.width !== event.width || last.height !== event.height) samples.push(event);
    let timestamp = previous?.timestamp ?? 0;
    for (let i = 0; i < samples.length; ++i) {
      const s = samples[i]; let time = Math.max(1, Math.round(s.timeStamp * 1000000));
      if (phase === 1 && time <= timestamp) continue;
      if (phase === 0 && time <= timestamp) return;
      time = Math.max(time, timestamp + 1);
      const fact = {source, pointer: event.pointerId, phase,
        x: (s.clientX - rect.left) * width / rect.width,
        y: (s.clientY - rect.top) * height / rect.height, timestampNs: time,
        pressure: Number.isFinite(s.pressure) ? s.pressure : 0,
        pressureAvailable: source === 2 && Number.isFinite(s.pressure),
        geometryAvailable: source !== 1 && s.width > 1 && s.height > 1,
        width: s.width || 0, height: s.height || 0,
        historical: i < history.length, shift: !!s.shiftKey};
      if (!submit(fact)) {
        active.delete(key);
        if (canvas.hasPointerCapture?.(event.pointerId)) canvas.releasePointerCapture(event.pointerId);
        return;
      }
      timestamp = time; active.set(key, {timestamp, fact, born: previous?.born ?? Math.round(event.timeStamp * 1000000)});
    }
    if (phase === 2 || phase === 3) {
      active.delete(key);
      if (canvas.hasPointerCapture?.(event.pointerId)) canvas.releasePointerCapture(event.pointerId);
    }
    event.preventDefault();
  }
  const listeners = [
    ['pointerdown', e => { if (sourceFor(e.pointerType)) { canvas.setPointerCapture(e.pointerId); forward(e, 0); } }],
    ['pointermove', e => forward(e, 1)], ['pointerup', e => forward(e, 2)],
    ['pointercancel', e => forward(e, 3)],
    ['lostpointercapture', e => { if (!canvas.hasPointerCapture?.(e.pointerId)) forward(e, 3); }],
  ];
  function cancelAll() {
    const pending = [...active.values()];
    active.clear();
    for (const {fact} of pending) {
      submit({...fact, phase: 3, timestampNs: fact.timestampNs + 1, historical: false});
      if (canvas.hasPointerCapture?.(fact.pointer)) canvas.releasePointerCapture(fact.pointer);
    }
  }
  for (const [name, listener] of listeners) canvas.addEventListener(name, listener);
  lifecycle?.addEventListener('blur', cancelAll);
  const dispose = () => {
    cancelAll();
    for (const [name, listener] of listeners) canvas.removeEventListener(name, listener);
    lifecycle?.removeEventListener('blur', cancelAll);
  };
  dispose.cancelAll = cancelAll;
  return dispose;
}
