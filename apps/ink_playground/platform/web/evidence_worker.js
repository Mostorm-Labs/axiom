const hex = buffer => [...new Uint8Array(buffer)]
  .map(value => value.toString(16).padStart(2, "0")).join("");

// Evidence worker is intentionally not a renderer. Displayed pixels are
// produced by Runtime → SkiaRenderer → WebGLSurfaceProvider.
self.addEventListener("message", async event => {
  const snapshot = event.data;
  const traceJson = `${JSON.stringify(snapshot.trace, null, 2)}\n`;
  const traceBlob = new Blob([traceJson], { type: "application/json" });
  const traceSha256 = hex(await crypto.subtle.digest("SHA-256", await traceBlob.arrayBuffer()));
  const captureBlob = snapshot.captureBlob
    ? await (await fetch(snapshot.captureBlob)).blob() : null;
  const captureSha256 = captureBlob
    ? hex(await crypto.subtle.digest("SHA-256", await captureBlob.arrayBuffer())) : null;
  const record = {
    platform: "web",
    user_agent: snapshot.userAgent || "",
    sample_count: snapshot.trace.length,
    viewport_scale: snapshot.viewportScale,
    viewport_translation_x: snapshot.viewportTranslationX,
    viewport_translation_y: snapshot.viewportTranslationY,
    trace_sha256: traceSha256,
    capture_sha256: captureSha256,
    render_path: snapshot.renderPath,
    pixels_source: "SkiaRenderer→WebGLSurfaceProvider→WebGL2 present"
  };
  const recordBlob = new Blob([`${JSON.stringify(record)}\n`], { type: "application/json" });
  const baselineBlob = snapshot.baselineObservation
    ? new Blob([snapshot.baselineObservation], { type: "application/json" }) : null;
  self.postMessage({ version: snapshot.version, traceBlob, captureBlob, recordBlob,
    baselineBlob, captureSha256, traceSha256, sampleCount: snapshot.trace.length,
    strokeCount: 0 });
});
