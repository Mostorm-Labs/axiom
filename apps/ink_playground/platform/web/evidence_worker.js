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
  const styles = {
    1:{color:"#1a5bff",width:3,alpha:1,dash:[]}, 2:{color:"#343434",width:2,alpha:.78,dash:[3,2]},
    3:{color:"#806048",width:7,alpha:.52,dash:[1,5]}, 4:{color:"#2070f0",width:14,alpha:.72,dash:[]},
    5:{color:"#26aac8",width:19,alpha:.38,dash:[]}, 6:{color:"#ffd820",width:24,alpha:.34,dash:[]},
    7:{color:"#ff1840",width:4,alpha:1,dash:[10,6]}
  };
  snapshot.strokes.forEach(stroke => {
    const points = stroke.points;
    const style = styles[stroke.family] || styles[1];
    if (points.length === 0) return;
    context.strokeStyle = style.color;
    context.globalAlpha = style.alpha;
    context.lineWidth = style.width;
    context.setLineDash(style.dash);
    context.lineCap = stroke.family === 3 ? "butt" : "round";
    context.lineJoin = "round";
    context.beginPath();
    points.forEach((point, index) => {
      if (index === 0) context.moveTo(point.x, point.y);
      else context.lineTo(point.x, point.y);
    });
    context.stroke();
    context.globalAlpha = 1;
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
