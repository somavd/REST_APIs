const API_BASE = "";

function textCell(value) {
    const td = document.createElement("td");
    td.textContent = value == null ? "" : String(value);
    return td;
}

function showStatus(message, isError, element) {
    const el = element || document.getElementById("status");
    if (!el) return;
    el.textContent = message;
    el.className = isError ? "status status-error" : "status status-ok";
    setTimeout(() => { el.textContent = ""; el.className = "status"; }, 5000);
}

function showError(message, element) {
    showStatus(message, true, element);
}

// Auth helpers
async function checkAuth() {
    try {
        const res = await fetch(`${API_BASE}/api/admin/session`, {
            method: "GET",
            credentials: "same-origin"
        });
        return res.ok;
    } catch {
        return false;
    }
}

async function requireAuth() {
    const ok = await checkAuth();
    if (!ok) {
        location.href = "/public/admin-login.html";
    }
}

async function logout() {
    try {
        await fetch(`${API_BASE}/api/admin/logout`, {
            method: "POST",
            credentials: "same-origin"
        });
    } finally {
        location.href = "/public/admin-login.html";
    }
}
