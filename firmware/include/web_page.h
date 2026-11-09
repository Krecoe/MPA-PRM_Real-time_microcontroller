// Web page served by the board at http://192.168.4.1/ over its own Wi-Fi hotspot.
// It polls /data for the last 8 s of acceleration and the current prediction.
// No external libraries: clients on the hotspot have no internet.
#pragma once

const char WEB_PAGE[] = R"HTML(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>ESP32-BREATH live</title>
<style>
  body { margin: 0; padding: 16px; background: #14171c; color: #e8ecf1; font: 16px/1.4 system-ui, sans-serif; }
  main { max-width: 860px; margin: 0 auto; }
  h1 { font-size: 1.3rem; margin: 0 0 4px; }
  .sub { color: #97a1af; font-size: 0.9rem; margin-bottom: 14px; }
  #state { font-size: 2rem; font-weight: bold; text-align: center; padding: 16px; border-radius: 12px; background: #333; transition: background 0.3s; }
  #details { text-align: center; color: #c9d1db; margin: 6px 0 16px; }
  canvas { width: 100%; height: 260px; background: #1d222a; border-radius: 10px; display: block; }
  .legend { font-size: 0.85rem; color: #97a1af; margin: 6px 0 18px; }
  .legend i { display: inline-block; width: 18px; height: 3px; vertical-align: middle; margin: 0 4px 0 12px; }
  .bar { display: flex; align-items: center; margin: 8px 0; }
  .bar .name { width: 130px; }
  .bar .track { flex: 1; background: #2c333d; border-radius: 6px; height: 22px; overflow: hidden; }
  .bar .fill { height: 100%; width: 0; transition: width 0.3s; }
  .bar .value { width: 64px; text-align: right; font-variant-numeric: tabular-nums; }
  #status { color: #97a1af; font-size: 0.8rem; margin-top: 16px; }
</style>
</head>
<body>
<main>
  <h1>ESP32-BREATH: live signal and prediction</h1>
  <div class="sub">Everything below is measured and classified on the board itself.</div>
  <div id="state">waiting...</div>
  <div id="details"></div>

  <h3>Acceleration, last 8 s (each axis minus its mean, m/s&sup2;)</h3>
  <canvas id="plot" width="860" height="260"></canvas>
  <div class="legend"><i style="background:#ff6b6b"></i>x <i style="background:#51cf66"></i>y <i style="background:#4dabf7"></i>z</div>

  <h3>Class probabilities (averaged over the last 3 s)</h3>
  <div id="bars"></div>
  <div id="status"></div>
</main>
<script>
const CLASSES = {
  calm: { label: "Calm breathing", color: "#1faa4b" },
  fast: { label: "Fast breathing", color: "#e03131" },
  not_attached: { label: "Not attached", color: "#2f6fd6" },
  shaking: { label: "Shaking", color: "#b833c9" },
};
const AXIS_COLORS = ["#ff6b6b", "#51cf66", "#4dabf7"];

const bars = document.getElementById("bars");
for (const [name, info] of Object.entries(CLASSES)) {
  bars.insertAdjacentHTML("beforeend",
    `<div class="bar"><span class="name">${info.label}</span><div class="track"><div class="fill" id="fill-${name}" style="background:${info.color}"></div></div><span class="value" id="value-${name}">-</span></div>`);
}

function drawPlot(axes) {
  const canvas = document.getElementById("plot");
  const context = canvas.getContext("2d");
  context.clearRect(0, 0, canvas.width, canvas.height);
  const centered = axes.map((values) => {
    const mean = values.reduce((a, b) => a + b, 0) / values.length;
    return values.map((v) => v - mean);
  });
  // Autoscale, but never below +-0.1 m/s^2 so that sensor noise does not fill the plot
  const peak = Math.max(0.1, ...centered.flat().map(Math.abs));
  const toY = (v) => canvas.height / 2 - (v / peak) * (canvas.height / 2 - 10);
  context.strokeStyle = "#2c333d";
  context.beginPath(); context.moveTo(0, canvas.height / 2); context.lineTo(canvas.width, canvas.height / 2); context.stroke();
  centered.forEach((values, axis) => {
    context.strokeStyle = AXIS_COLORS[axis];
    context.lineWidth = 1.5;
    context.beginPath();
    values.forEach((v, i) => {
      const x = (i / (values.length - 1)) * canvas.width;
      if (i === 0) context.moveTo(x, toY(v)); else context.lineTo(x, toY(v));
    });
    context.stroke();
  });
  context.fillStyle = "#97a1af";
  context.font = "12px system-ui";
  context.fillText(`+${peak.toFixed(2)}`, 6, 14);
  context.fillText(`-${peak.toFixed(2)}`, 6, canvas.height - 6);
}

async function update() {
  try {
    const response = await fetch("/data", { cache: "no-store" });
    const data = await response.json();
    if (data.axes[0].length > 1) drawPlot(data.axes);
    const info = CLASSES[data.state];
    const stateBox = document.getElementById("state");
    stateBox.textContent = info ? info.label : "waiting for the first 8 s window...";
    stateBox.style.background = info ? info.color : "#333";
    document.getElementById("details").textContent = info
      ? `strongest breathing frequency in the window: ${data.breath_rate_bpm.toFixed(1)} breaths/min`
      : "";
    for (const name of Object.keys(CLASSES)) {
      const probability = data.probs[name] ?? 0;
      document.getElementById(`fill-${name}`).style.width = `${probability * 100}%`;
      document.getElementById(`value-${name}`).textContent = `${(probability * 100).toFixed(0)} %`;
    }
    document.getElementById("status").textContent = `updated ${new Date().toLocaleTimeString()} · board uptime ${(data.uptime_ms / 1000).toFixed(0)} s`;
  } catch (error) {
    document.getElementById("status").textContent = `no connection to the board (${error.message})`;
  }
  setTimeout(update, 300);
}
update();
</script>
</body>
</html>
)HTML";
