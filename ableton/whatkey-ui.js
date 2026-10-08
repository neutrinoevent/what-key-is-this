// v8ui panel for the WhatKey device. Messages: status <text>, error <text>, result <json>.
autowatch = 1; // reload in Live when this file is saved
mgraphics.init();
mgraphics.relative_coords = 0;
mgraphics.autofill = 0;

const FONT = "Ableton Sans Medium";
const FONT_BOLD = "Ableton Sans Bold";
let state = { mode: "idle", text: "Drop an audio file on the left" };

function status() { state = { mode: "busy", text: arrayfromargs(arguments).join(" ") }; mgraphics.redraw(); }
function error() { state = { mode: "error", text: arrayfromargs(arguments).join(" ") }; mgraphics.redraw(); }
function result() {
  try {
    state = { mode: "result", data: JSON.parse(arrayfromargs(arguments).join(" ")) };
  } catch (e) {
    state = { mode: "error", text: "Could not read results" };
  }
  mgraphics.redraw();
}

// Colour each Camelot number by its position on the wheel.
function camelotColor(cam, alpha) {
  const h = ((parseInt(cam, 10) - 1) / 12) * 6;
  const s = 0.55, l = cam.endsWith("A") ? 0.5 : 0.6;
  const c = (1 - Math.abs(2 * l - 1)) * s, x = c * (1 - Math.abs((h % 2) - 1)), m = l - c / 2;
  const rgb = [[c, x, 0], [x, c, 0], [0, c, x], [0, x, c], [x, 0, c], [c, 0, x]][Math.floor(h) % 6];
  return [rgb[0] + m, rgb[1] + m, rgb[2] + m, alpha === undefined ? 1 : alpha];
}

function text(str, x, y, size, rgba, font) {
  mgraphics.select_font_face(font || FONT);
  mgraphics.set_font_size(size);
  mgraphics.set_source_rgba(rgba);
  mgraphics.move_to(x, y);
  mgraphics.show_text(String(str));
  return mgraphics.text_measure(String(str))[0];
}

function rect(x, y, w, h, r, rgba) {
  mgraphics.set_source_rgba(rgba);
  mgraphics.rectangle_rounded(x, y, w, h, r, r);
  mgraphics.fill();
}

// Faint backing text, sized to ~85% of the panel width and centred in it.
function watermark(str, W, H) {
  mgraphics.select_font_face("Helvetica Neue");
  mgraphics.set_font_size(100);
  const size = Math.min(100 * (W * 0.85) / mgraphics.text_measure(str)[0], H * 0.8);
  mgraphics.set_font_size(size);
  const tw = mgraphics.text_measure(str)[0];
  mgraphics.set_source_rgba(1, 1, 1, 0.035);
  mgraphics.move_to((W - tw) / 2, H / 2 + size * 0.36); // ~half cap height below centre
  mgraphics.show_text(str);
}

function shortKey(k) { return k.replace(" major", "").replace(" minor", "m"); }

function paint() {
  const [W, H] = mgraphics.size;
  const DIM = [1, 1, 1, 0.45], FG = [1, 1, 1, 0.92];
  rect(0, 0, W, H, 6, [0.11, 0.11, 0.12, 1]);
  watermark("ALL OUT", W, H);

  if (state.mode !== "result") {
    const col = state.mode === "error" ? [1, 0.45, 0.4, 1] : state.mode === "busy" ? FG : DIM;
    text("WhatKey", 12, 22, 12, DIM, FONT_BOLD);
    text(state.text, 12, H / 2 + 5, 12, col);
    return;
  }

  const d = state.data, top = d.keys[0];
  const agree = d.segment_agreement === null ? "n/a" : Math.round(d.segment_agreement * 100) + "%";
  const cents = (d.tuning_cents >= 0 ? "+" : "") + Math.round(d.tuning_cents);

  // Header: file name + stats
  const name = d.file.length > 34 ? d.file.slice(0, 33) + "…" : d.file;
  text(name, 12, 17, 10, DIM);
  const stats = `${Math.round(d.duration)}s · ${cents}¢ · ${agree} stable`;
  mgraphics.set_font_size(10);
  text(stats, W - 12 - mgraphics.text_measure(stats)[0], 17, 10, DIM);

  // Headline key + Camelot badge + confidence
  const kw = text(top.key, 12, 48, 24, FG, FONT_BOLD);
  rect(20 + kw, 30, 34, 20, 4, camelotColor(top.camelot));
  text(top.camelot, 27 + kw, 45, 12, [0.08, 0.08, 0.08, 1], FONT_BOLD);
  text(Math.round(top.prob * 100) + "%", 62 + kw, 46, 13, DIM);

  // Runner-up bars
  const barX = 108, barW = W - barX - 48;
  d.keys.slice(1).forEach((k, i) => {
    const y = 62 + i * 17;
    text(k.key, 12, y + 11, 11, FG);
    text(k.camelot, 78, y + 11, 10, camelotColor(k.camelot));
    rect(barX, y + 2, barW, 10, 3, [1, 1, 1, 0.06]);
    rect(barX, y + 2, Math.max(3, barW * k.prob), 10, 3, camelotColor(k.camelot, 0.85));
    text((k.prob * 100).toFixed(1) + "%", barX + barW + 6, y + 11, 10, DIM);
  });

  // Compatible keys
  let x = 12;
  const y = H - 10;
  x += text("Mixes with", x, y, 10, DIM) + 8;
  d.compatible.forEach((k) => {
    const label = `${shortKey(k.key)} ${k.camelot}`;
    mgraphics.set_font_size(10);
    const w = mgraphics.text_measure(label)[0] + 10;
    rect(x, y - 11, w, 15, 4, camelotColor(k.camelot, 0.25));
    text(label, x + 5, y, 10, FG);
    x += w + 5;
  });
}
