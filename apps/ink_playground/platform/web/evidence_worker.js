const hex = buffer => [...new Uint8Array(buffer)]
  .map(value => value.toString(16).padStart(2, "0")).join("");

self.addEventListener("message", async event => {
  const snapshot = event.data;
  const traceJson = `${JSON.stringify(snapshot.trace, null, 2)}\n`;
  const traceBlob = new Blob([traceJson], { type: "application/json" });
  const traceSha256 = hex(await crypto.subtle.digest("SHA-256", await traceBlob.arrayBuffer()));
  const capture = new OffscreenCanvas(Math.max(1, snapshot.width), Math.max(1, snapshot.height));
  const context = capture.getContext("2d");
  context.fillStyle = "#ffffff";
  context.fillRect(0, 0, capture.width, capture.height);
  context.save();
  context.translate(snapshot.viewportTranslationX, snapshot.viewportTranslationY);
  context.scale(snapshot.viewportScale, snapshot.viewportScale);
  context.strokeStyle = "#1a5bff";
  context.lineWidth = 3;
  context.lineCap = "round";
  context.lineJoin = "round";
  snapshot.strokes.forEach(points => {
    if (points.length === 0) return;
    context.beginPath();
    points.forEach((point, index) => {
      if (index === 0) context.moveTo(point.x, point.y);
      else context.lineTo(point.x, point.y);
    });
    context.stroke();
  });
  context.restore();
  const captureBlob = await capture.convertToBlob({ type: "image/png" });
  const captureSha256 = hex(await crypto.subtle.digest("SHA-256", await captureBlob.arrayBuffer()));
  const record = {
    platform: "web",
    user_agent: navigator.userAgent,
    sample_count: snapshot.trace.length,
    stroke_count: snapshot.strokes.length,
    viewport_scale: snapshot.viewportScale,
    trace_sha256: traceSha256,
    capture: "web-ink-playground.png",
    capture_sha256: captureSha256
  };
  const recordBlob = new Blob([`${JSON.stringify(record)}\n`], { type: "application/json" });
  self.postMessage({ version: snapshot.version, traceBlob, captureBlob, recordBlob,
    captureSha256, traceSha256, sampleCount: snapshot.trace.length,
    strokeCount: snapshot.strokes.length });
});
