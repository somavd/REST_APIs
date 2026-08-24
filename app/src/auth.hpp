#pragma once

#include <string>
#include <chrono>
#include <map>
#include <mutex>

// Authenticated session: token -> user info with expiry.
struct Session {
    int userId = 0;
    std::string email;
    std::string name;
    std::chrono::steady_clock::time_point expiresAt;
};

// Lightweight auth layer: password hashing and cookie-based session store.
class Auth {
public:
    // Generate a cryptographically-acceptable random hex token.
    static std::string generateToken();

    // Generate a random numeric code (e.g., OTP) of the requested length.
    static std::string generateNumericCode(int digits = 6);

    // Hash password with random salt using SHA-256. Returns "salt:hash".
    static std::string hashPassword(const std::string& password);

    // Verify a raw password against a stored "salt:hash" string.
    static bool verifyPassword(const std::string& password, const std::string& stored);

    // Create a session and return the token.
    std::string createSession(int userId, const std::string& email, const std::string& name, int ttlSeconds = 86400);

    // Validate a token and return a pointer to the session (nullptr if invalid/expired).
    // Removes expired sessions during validation.
    const Session* validateSession(const std::string& token);

    // Delete a session.
    void destroySession(const std::string& token);

    // Constant-time string comparison (prevents timing attacks on OTP/tokens).
    static bool constantTimeEquals(const std::string& a, const std::string& b);

    // Extract a cookie value by name from a raw Cookie header.
    static std::string getCookieValue(const std::string& cookieHeader, const std::string& name);

private:
    std::map<std::string, Session> sessions_;
    std::mutex mutex_;

    void purgeExpired();
};
