const API_BASE = "";

const message = document.getElementById("message");
const loginLink = document.getElementById("loginLink");
const params = new URLSearchParams(location.search);
const token = params.get("token");

if (!token) {
    message.textContent = "Missing verification token.";
    message.className = "status status-error";
    loginLink.style.display = "block";
} else {
    fetch(`${API_BASE}/api/student/verify?token=${encodeURIComponent(token)}`, {
        credentials: "same-origin"
    })
    .then(res => res.json())
    .then(data => {
        message.textContent = data.message || data.error || "Verification failed.";
        message.className = data.success ? "status status-ok" : "status status-error";
        loginLink.style.display = "block";
    })
    .catch(() => {
        message.textContent = "Verification failed. Please try again or request a new link.";
        message.className = "status status-error";
        loginLink.style.display = "block";
    });
}
