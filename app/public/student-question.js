const API_BASE = "";
const params = new URLSearchParams(location.search);
const questionId = parseInt(params.get("id"), 10);

const codeTemplates = {
    cpp: '#include <iostream>\nusing namespace std;\n\nint main() {\n    // your code\n    return 0;\n}',
    python: '# your code',
    javascript: '// your code'
};

function showStatus(msg, isError) {
    const status = document.getElementById("status");
    status.textContent = msg;
    status.className = "status " + (isError ? "status-error" : "status-ok");
}

async function loadQuestion() {
    try {
        const res = await fetch(`${API_BASE}/api/platform/questions/${questionId}`, { credentials: "same-origin" });
        const data = await res.json();

        if (!res.ok) {
            showStatus(data.error || "Question not found", true);
            return;
        }

        document.getElementById("title").textContent = data.title;
        document.getElementById("description").textContent = data.description;

        const list = document.getElementById("testcases");
        list.innerHTML = "";
        for (const t of data.testcases) {
            const li = document.createElement("li");
            li.innerHTML = `<strong>Input:</strong> ${t.input}<br><strong>Expected:</strong> ${t.expected_output}`;
            list.appendChild(li);
        }
    } catch (err) {
        showStatus("Network error. Please try again.", true);
    }
}

document.getElementById("language").addEventListener("change", (e) => {
    const ta = document.getElementById("code");
    if (ta.value.trim() === "") {
        ta.value = codeTemplates[e.target.value];
    }
});

document.getElementById("submitButton").addEventListener("click", async () => {
    const language = document.getElementById("language").value;
    const code = document.getElementById("code").value;
    const output = document.getElementById("output");

    showStatus("Running...", false);
    output.textContent = "";

    try {
        const res = await fetch(`${API_BASE}/api/platform/submit`, {
            method: "POST",
            headers: { "Content-Type": "application/json" },
            credentials: "same-origin",
            body: JSON.stringify({ question_id: questionId, language, code })
        });
        const data = await res.json();

        if (!res.ok) {
            showStatus(data.error || "Submit failed", true);
            return;
        }

        output.textContent = `Passed: ${data.passed} / ${data.total}\nAll passed: ${data.allPassed}\nSubmission ID: ${data.submissionId}`;
        showStatus("Submitted successfully", false);
    } catch (err) {
        showStatus("Network error. Please try again.", true);
    }
});

document.getElementById("logoutLink").addEventListener("click", async (e) => {
    e.preventDefault();
    try {
        await fetch(`${API_BASE}/api/student/logout`, {
            method: "POST",
            credentials: "same-origin"
        });
    } finally {
        location.href = "student-login.html";
    }
});

document.getElementById("code").value = codeTemplates.cpp;
loadQuestion();
