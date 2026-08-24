const API_BASE = "";

// Default code templates per language
const TEMPLATES = {
  cpp: '#include <iostream>\nusing namespace std;\n\nint main() {\n    cout << "Hello, World!" << endl;\n    return 0;\n}\n',
  python: 'print("Hello, World!")\n',
  javascript: 'console.log("Hello, World!");\n',
};

const editorEl = document.getElementById("editor");
editorEl.value = TEMPLATES.cpp;

// Ctrl+Enter / Cmd+Enter to run
editorEl.addEventListener("keydown", function (e) {
  if ((e.ctrlKey || e.metaKey) && e.key === "Enter") {
    e.preventDefault();
    runCode();
  }
});

// Switch language template
document.getElementById("language").addEventListener("change", function () {
  const lang = this.value;
  editorEl.value = TEMPLATES[lang] || "";
});

// Run code
async function runCode() {
  const language = document.getElementById("language").value;
  const code = editorEl.value;
  const input = document.getElementById("userInput").value;
  const stdoutEl = document.getElementById("stdout");
  const stderrEl = document.getElementById("stderr");
  const runBtn = document.getElementById("runBtn");
  const statusEl = document.getElementById("status");

  stdoutEl.textContent = "";
  stderrEl.textContent = "";
  statusEl.textContent = "";

  runBtn.disabled = true;
  runBtn.textContent = "Running...";
  const startTime = performance.now();

  try {
    const response = await fetch(`${API_BASE}/api/playground/run`, {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ language, code, input }),
    });

    const text = await response.text();
    let data;
    try {
      data = JSON.parse(text);
    } catch {
      stderrEl.textContent = "Server returned invalid response: " + text.slice(0, 200);
      statusEl.textContent = "Error";
      statusEl.className = "status status-error";
      return;
    }
    const elapsed = ((performance.now() - startTime) / 1000).toFixed(2);

    if (!response.ok) {
      stderrEl.textContent = data.error || "Request failed";
      statusEl.textContent = "Failed";
      statusEl.className = "status status-error";
      return;
    }

    stdoutEl.textContent = data.stdout || "(no output)";
    stderrEl.textContent = data.stderr || "";

    if (data.timedOut) {
      stderrEl.textContent = "Execution timed out";
      statusEl.textContent = "Timed out";
      statusEl.className = "status status-error";
    } else if (data.exitCode !== 0) {
      statusEl.textContent = "Exit code: " + data.exitCode + " (" + elapsed + "s)";
      statusEl.className = "status status-error";
    } else {
      statusEl.textContent = "Done (" + elapsed + "s)";
      statusEl.className = "status status-ok";
    }
  } catch (error) {
    stderrEl.textContent = "Connection error: " + error.message;
    statusEl.textContent = "Error";
    statusEl.className = "status status-error";
  } finally {
    runBtn.disabled = false;
    runBtn.textContent = "\u25B6 Run";
  }
}

// Clear
function clearCode() {
  const lang = document.getElementById("language").value;
  editorEl.value = TEMPLATES[lang] || "";
  document.getElementById("userInput").value = "";
  document.getElementById("stdout").textContent = "";
  document.getElementById("stderr").textContent = "";
  document.getElementById("status").textContent = "";
  editorEl.focus();
}
