document.addEventListener('DOMContentLoaded', () => {
    const form = document.getElementById('loginForm');
    const status = document.getElementById('status');

    form.addEventListener('submit', async (e) => {
        e.preventDefault();
        status.textContent = '';
        status.className = 'status';

        const email = document.getElementById('email').value.trim();
        const password = document.getElementById('password').value;

        try {
            const res = await fetch(`${API_BASE}/api/admin/login`, {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                credentials: 'same-origin',
                body: JSON.stringify({ email, password })
            });

            const data = await res.json();

            if (res.ok) {
                showStatus('Login successful. Redirecting...', false, status);
                setTimeout(() => {
                    location.href = '/public/admin-dashboard.html';
                }, 500);
            } else {
                showStatus(data.error || 'Login failed', true, status);
            }
        } catch (err) {
            showStatus('Network error. Please try again.', true, status);
        }
    });
});
