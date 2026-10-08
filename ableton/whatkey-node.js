// Node for Max bridge: receives "analyze <path>" from live.drop, runs whatkey.py --json,
// and sends "status ...", "error ..." or "result <json>" to the UI.
const maxApi = require("max-api");
const { execFile } = require("child_process");
const fs = require("fs");
const path = require("path");

const cfg = JSON.parse(fs.readFileSync(path.join(__dirname, "config.json"), "utf8"));
let current = null;

// live.drop may hand us a Max-style path ("Macintosh HD:/Users/..."); convert to POSIX.
function toPosix(p) {
  const m = /^[^/:]+:(\/.*)$/.exec(p);
  return m ? m[1] : p;
}

maxApi.addHandler("analyze", (...parts) => {
  const file = toPosix(parts.join(" ").trim());
  if (!file) return;
  if (current) current.kill();
  maxApi.outlet("status", `Analyzing ${path.basename(file)}…`);

  const child = execFile(
    cfg.python,
    [cfg.script, file, "--json", "-n", "5"],
    { env: { ...process.env, PATH: cfg.path }, timeout: 180000 },
    (err, stdout, stderr) => {
      if (child !== current) return; // superseded by a newer drop
      current = null;
      const line = stdout.trim().split("\n").pop();
      if (line && line.startsWith("{")) {
        maxApi.outlet("result", line);
      } else {
        const msg = (stderr || (err && err.message) || "Analysis failed").trim().split("\n")[0];
        maxApi.outlet("error", msg);
      }
    }
  );
  current = child;
});
