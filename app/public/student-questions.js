const API_BASE = "";

function showStatus(msg, isError) {
    const status = document.getElementById("status");
    status.textContent = msg;
    status.className = "status " + (isError ? "status-error" : "status-ok");
}

async function loadQuestions() {
    try {
        const res = await fetch(`${API_BASE}/api/platform/questions`, { credentials: "same-origin" });
        const data = await res.json();

        if (!res.ok) {
            showStatus(data.error || "Failed to load questions", true);
            return;
        }

        const grid = document.getElementById("questionsGrid");
        grid.innerHTML = "";

        if (data.questions.length === 0) {
            grid.innerHTML = "<p style='color: #94a3b8;'>No questions available yet.</p>";
            return;
        }

        for (const q of data.questions) {
            const card = document.createElement("div");
            card.className = "question-card";
            const desc = q.description || "";
            const shortDesc = desc.length > 120 ? desc.slice(0, 120) + "..." : desc;
            card.innerHTML = `
                <h3>${q.title}</h3>
                <p>${shortDesc}</p>
                <div class="question-meta">
                    <span class="badge badge-category">${q.category || "General"}</span>
                    <span class="badge badge-difficulty">${q.difficulty || "Unknown"}</span>
                </div>
                <a href="student-question.html?id=${q.id}">Solve</a>
            `;
            grid.appendChild(card);
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

loadQuestions();
