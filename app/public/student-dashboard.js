const API_BASE = "";

function showStatus(msg, isError) {
    const status = document.getElementById("status");
    status.textContent = msg;
    status.className = "status " + (isError ? "status-error" : "status-ok");
}

async function loadDashboard() {
    try {
        const res = await fetch(`${API_BASE}/api/student/dashboard`, {
            method: "GET",
            credentials: "same-origin"
        });

        if (!res.ok) {
            if (res.status === 401) {
                location.href = "/public/student-login.html";
            } else {
                showStatus("Failed to load dashboard", true);
            }
            return;
        }

        const data = await res.json();
        document.getElementById("profileText").textContent = `${data.name} (${data.email})`;
        document.getElementById("totalSubmissions").textContent = data.total_submissions;
        document.getElementById("questionsAttempted").textContent = data.questions_attempted;
    } catch (err) {
        showStatus("Network error. Please try again.", true);
    }
}

async function logout() {
    try {
        await fetch(`${API_BASE}/api/student/logout`, {
            method: "POST",
            credentials: "same-origin"
        });
    } catch (err) {
        // ignore
    }
    location.href = "/public/student-login.html";
}

loadDashboard();
