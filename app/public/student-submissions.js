const API_BASE = "";

function showStatus(msg, isError) {
    const status = document.getElementById("status");
    status.textContent = msg;
    status.className = "status " + (isError ? "status-error" : "status-ok");
}

async function loadSubmissions() {
    try {
        const res = await fetch(`${API_BASE}/api/platform/submissions`, { credentials: "same-origin" });
        const data = await res.json();

        if (!res.ok) {
            showStatus(data.error || "Failed to load submissions", true);
            return;
        }

        const list = document.getElementById("submissionsList");
        list.innerHTML = "";

        if (data.submissions.length === 0) {
            list.innerHTML = "<p style='color: #94a3b8;'>No submissions yet.</p>";
            return;
        }

        for (const s of data.submissions) {
            const card = document.createElement("div");
            card.className = "submission-card";
            const question = s.questionId > 0 ? `Question #${s.questionId}` : "Playground";
            const result = s.stderr ? s.stderr : (s.stdout ? s.stdout : "No output");
            const shortResult = result.length > 200 ? result.slice(0, 200) + "..." : result;
            card.innerHTML = `
                <div class="submission-header">
                    <span class="sub-id">Submission #${s.id}</span>
                    <span class="sub-lang">${s.language}</span>
                </div>
                <div class="submission-body">
                    <strong>${question}</strong>
                    <pre class="output-snippet">${shortResult}</pre>
                </div>
                <div class="submission-footer">${s.createdAt}</div>
            `;
            list.appendChild(card);
        }
    } catch (err) {
        showStatus("Network error. Please try again.", true);
    }
}

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

loadSubmissions();
