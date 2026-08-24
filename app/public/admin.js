document.addEventListener("DOMContentLoaded", async () => {
    await requireAuth();
    loadQuestionCount();
    loadSubmissionCount();
});

async function loadQuestionCount() {
    const countEl = document.getElementById("questionCount");
    if (!countEl) return;
    try {
        const res = await fetch(`${API_BASE}/api/questions`, { credentials: "same-origin" });
        if (!res.ok) throw new Error("Failed to load questions");
        const data = await res.json();
        countEl.textContent = data.total !== undefined ? data.total : (data.questions || []).length;
    } catch (e) {
        showError("Failed to load question count");
        countEl.textContent = "0";
    }
}

async function loadSubmissionCount() {
    const countEl = document.getElementById("submissionCount");
    if (!countEl) return;
    try {
        const res = await fetch(`${API_BASE}/api/submissions?limit=1`, { credentials: "same-origin" });
        if (!res.ok) throw new Error("Failed to load submissions");
        const data = await res.json();
        countEl.textContent = (data.submissions || []).length;
    } catch (e) {
        showError("Failed to load submission count");
        countEl.textContent = "0";
    }
}
