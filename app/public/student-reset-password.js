const API_BASE = "";

function showStatus(msg, isError) {
    const status = document.getElementById("status");
    status.textContent = msg;
    status.className = "status " + (isError ? "status-error" : "status-ok");
}

document.getElementById("resetForm").addEventListener("submit", async (e) => {
    e.preventDefault();
    showStatus("", false);

    const email = document.getElementById("email").value.trim();
    const otp = document.getElementById("otp").value.trim();
    const newPassword = document.getElementById("newPassword").value;
    const confirmPassword = document.getElementById("confirmPassword").value;

    if (!email || !otp || !newPassword) {
        showStatus("All fields are required", true);
        return;
    }
    if (newPassword !== confirmPassword) {
        showStatus("Passwords do not match", true);
        return;
    }
    if (newPassword.length < 8 || newPassword.length > 32) {
        showStatus("Password must be between 8 and 32 characters", true);
        return;
    }

    try {
        const res = await fetch(`${API_BASE}/api/student/reset-password`, {
            method: "POST",
            headers: { "Content-Type": "application/json" },
            credentials: "same-origin",
            body: JSON.stringify({ email, otp, new_password: newPassword })
        });

        const data = await res.json();
        if (res.ok) {
            showStatus(data.message, false);
            setTimeout(() => {
                location.href = "student-login.html";
            }, 1500);
        } else {
            showStatus(data.error || "Reset failed", true);
        }
    } catch (err) {
        showStatus("Network error. Please try again.", true);
    }
});
