const API_BASE = "";

function showStatus(msg, isError) {
    const status = document.getElementById("status");
    status.textContent = msg;
    status.className = "status " + (isError ? "status-error" : "status-ok");
}

function switchTab(tab) {
    document.querySelectorAll(".tab").forEach(t => t.classList.toggle("active", t.dataset.tab === tab));
    document.querySelectorAll(".tab-content").forEach(c => c.classList.toggle("active", c.id === tab + "Tab"));
    const switchText = document.getElementById("switchText");
    if (tab === "login") {
        switchText.innerHTML = "Don't have an account? <a id=\"switchLink\" data-switch=\"register\">Register</a>";
    } else {
        switchText.innerHTML = "Already have an account? <a id=\"switchLink\" data-switch=\"login\">Login</a>";
    }
    bindSwitch();
    showStatus("", false);
}

function bindSwitch() {
    document.getElementById("switchLink").addEventListener("click", (e) => {
        e.preventDefault();
        switchTab(e.target.dataset.switch);
    });
}

document.querySelectorAll(".tab").forEach(t => {
    t.addEventListener("click", () => switchTab(t.dataset.tab));
});

bindSwitch();

document.getElementById("loginForm").addEventListener("submit", async (e) => {
    e.preventDefault();
    showStatus("", false);

    const email = document.getElementById("loginEmail").value.trim();
    const password = document.getElementById("loginPassword").value;

    try {
        const res = await fetch(`${API_BASE}/api/student/login`, {
            method: "POST",
            headers: { "Content-Type": "application/json" },
            credentials: "same-origin",
            body: JSON.stringify({ email, password })
        });

        const data = await res.json();
        if (res.ok) {
            showStatus("Login successful. Redirecting...", false);
            setTimeout(() => {
                location.href = "/public/student-dashboard.html";
            }, 500);
        } else {
            showStatus(data.error || "Login failed", true);
        }
    } catch (err) {
        showStatus("Network error. Please try again.", true);
    }
});

document.getElementById("registerForm").addEventListener("submit", async (e) => {
    e.preventDefault();
    showStatus("", false);

    const name = document.getElementById("registerName").value.trim();
    const email = document.getElementById("registerEmail").value.trim();
    const password = document.getElementById("registerPassword").value;
    const confirmPassword = document.getElementById("registerConfirmPassword").value;

    if (name.length < 3 || name.length > 32) {
        showStatus("Name must be between 3 and 32 characters", true);
        return;
    }
    if (password !== confirmPassword) {
        showStatus("Passwords do not match", true);
        return;
    }
    if (password.length < 8 || password.length > 32) {
        showStatus("Password must be between 8 and 32 characters", true);
        return;
    }

    try {
        const res = await fetch(`${API_BASE}/api/student/register`, {
            method: "POST",
            headers: { "Content-Type": "application/json" },
            credentials: "same-origin",
            body: JSON.stringify({ name, email, password })
        });

        const data = await res.json();
        if (res.ok) {
            showStatus(data.message || "Registration successful. Check your email.", false);
            setTimeout(() => {
                switchTab("login");
            }, 1500);
        } else {
            showStatus(data.error || "Registration failed", true);
        }
    } catch (err) {
        showStatus("Network error. Please try again.", true);
    }
});
