const API_BASE = "";

function showStatus(msg, isError) {
    const status = document.getElementById("status");
    status.textContent = msg;
    status.className = "status " + (isError ? "status-error" : "status-ok");
}

document.getElementById("forgotForm").addEventListener("submit", async (e) => {
    e.preventDefault();
    showStatus("", false);

    const email = document.getElementById("email").value.trim();
    if (!email) {
        showStatus("Please enter your email", true);
        return;
    }

    try {
        const res = await fetch(`${API_BASE}/api/student/forgot-password`, {
            method: "POST",
            headers: { "Content-Type": "application/json" },
            credentials: "same-origin",
            body: JSON.stringify({ email })
        });

        const data = await res.json();
        if (res.ok) {
            showStatus(data.message, false);
            setTimeout(() => {
                location.href = "student-reset-password.html";
            }, 1500);
        } else {
            showStatus(data.error || "Request failed", true);
        }
    } catch (err) {
        showStatus("Network error. Please try again.", true);
    }
});
