#include "crow.h"
#include "config.hpp"
#include "executor.hpp"
#include "validator.hpp"
#include "rate_limiter.hpp"
#include "database.hpp"
#include "auth.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <iostream>
#include <thread>
#include <csignal>
#include <cstdlib>
#include <ctime>
#include <vector>
#include <unistd.h>

namespace fs = std::filesystem;

static fs::path getPublicDir() {
    const char* env = std::getenv("PUBLIC_DIR");
    if (env) return fs::weakly_canonical(env);
    return fs::weakly_canonical("public");
}

// JSON response helper
static crow::response jsonResponse(int code, crow::json::wvalue& body) {
    crow::response res(code, body.dump());
    res.set_header("Content-Type", "application/json");
    return res;
}

static crow::response jsonError(int code, const std::string& message) {
    crow::json::wvalue body;
    body["error"] = message;
    return jsonResponse(code, body);
}

// Auth helpers
static const char* ADMIN_COOKIE = "oc_admin_session";

static const Session* getAdminSession(const crow::request& req, Auth& auth) {
    std::string cookieHeader = req.get_header_value("Cookie");
    std::string token = Auth::getCookieValue(cookieHeader, ADMIN_COOKIE);
    return auth.validateSession(token);
}

static void setCookie(crow::response& res, const std::string& name, const std::string& value, int maxAgeSeconds = 86400) {
    std::stringstream ss;
    ss << name << "=" << value
       << "; Path=/; HttpOnly; SameSite=Lax; Max-Age=" << maxAgeSeconds;
    res.add_header("Set-Cookie", ss.str());
}

static void setSessionCookie(crow::response& res, const std::string& token, int maxAgeSeconds = 86400) {
    setCookie(res, ADMIN_COOKIE, token, maxAgeSeconds);
}

static void clearSessionCookie(crow::response& res) {
    setCookie(res, ADMIN_COOKIE, "", 0);
}

static const char* STUDENT_COOKIE = "oc_student_session";

static const Session* getStudentSession(const crow::request& req, Auth& auth) {
    std::string cookieHeader = req.get_header_value("Cookie");
    std::string token = Auth::getCookieValue(cookieHeader, STUDENT_COOKIE);
    return auth.validateSession(token);
}

struct FileReadResult {
    bool found = false;
    std::string content;
};

static FileReadResult readFileContents(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) return {false, ""};
    std::ostringstream ss;
    ss << file.rdbuf();
    return {true, ss.str()};
}

static std::string getAppUrl() {
    const char* env = std::getenv("APP_URL");
    if (env && *env) return env;
    return "http://localhost:3000";
}

static std::string findEmailScript() {
    const char* envScript = std::getenv("SEND_EMAIL_SCRIPT");
    std::vector<std::string> candidates;
    if (envScript && *envScript) candidates.push_back(envScript);
    candidates.push_back("scripts/send_email.py");
    candidates.push_back("../scripts/send_email.py");
    for (const auto& c : candidates) {
        if (fs::exists(c)) return c;
    }
    return "";
}

static void sendEmail(const std::string& to, const std::string& subject, const std::string& body, bool isHtml = false) {
    std::string script = findEmailScript();
    if (script.empty()) {
        std::cerr << "Email: send_email.py not found" << std::endl;
        return;
    }

    crow::json::wvalue payload;
    payload["to"] = to;
    payload["subject"] = subject;
    payload["body"] = body;
    payload["html"] = isHtml;
    std::string json = payload.dump();

    std::string filename = "oc_email_" + Auth::generateToken() + ".json";
    fs::path tmpPath = fs::temp_directory_path() / filename;
    std::ofstream out(tmpPath);
    if (!out) {
        std::cerr << "Email: failed to write temp payload" << std::endl;
        return;
    }
    out << json;
    out.close();

    std::string tmpStr = tmpPath.string();
    std::thread([script, tmpStr, to]() {
        std::string cmd = "python3 '" + script + "' '" + tmpStr + "' 2>&1";
        int rc = std::system(cmd.c_str());
        if (rc != 0) {
            std::cerr << "Email: failed to send email to " << to << std::endl;
        }
        std::error_code ec;
        fs::remove(tmpStr, ec);
    }).detach();
}

static std::string buildVerificationEmail(const std::string& name, const std::string& verifyUrl) {
    return "<p>Hi " + name + ",</p>" +
        "<p>Thanks for registering. Please click the button below to verify your email:</p>" +
        "<p><a href=\"" + verifyUrl + "\" style=\"display:inline-block;padding:12px 24px;background:#3b82f6;color:#fff;text-decoration:none;border-radius:6px;font-weight:bold;\">Verify Email</a></p>" +
        "<p>This link expires in 24 hours.</p>" +
        "<p>- Online Compiler</p>";
}

static std::string getMimeType(const std::string& ext) {
    if (ext == ".html") return "text/html";
    if (ext == ".css")  return "text/css";
    if (ext == ".js")   return "application/javascript";
    if (ext == ".json") return "application/json";
    if (ext == ".png")  return "image/png";
    if (ext == ".svg")  return "image/svg+xml";
    return "application/octet-stream";
}

static std::string trim(const std::string& s) {
    size_t start = 0;
    while (start < s.size() && (s[start] == ' ' || s[start] == '\t' || s[start] == '\n' || s[start] == '\r')) ++start;
    size_t end = s.size();
    while (end > start && (s[end-1] == ' ' || s[end-1] == '\t' || s[end-1] == '\n' || s[end-1] == '\r')) --end;
    return s.substr(start, end - start);
}

// Global app pointer for graceful shutdown
static crow::SimpleApp* g_app = nullptr;

static void signalHandler(int /*sig*/) {
    const char msg[] = "\nShutdown requested...\n";
    (void)write(STDERR_FILENO, msg, sizeof(msg) - 1);
    if (g_app) g_app->stop();
}

static void loadDotEnv() {
    auto strip = [](const std::string& s) -> std::string {
        size_t start = s.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) return "";
        size_t end = s.find_last_not_of(" \t\r\n");
        return s.substr(start, end - start + 1);
    };

    const char* candidates[] = {".env", "../.env", "../../.env"};
    for (const char* c : candidates) {
        fs::path p = fs::weakly_canonical(c);
        std::ifstream f(p);
        if (!f.is_open()) continue;

        std::string line;
        while (std::getline(f, line)) {
            std::string t = strip(line);
            if (t.empty() || t[0] == '#') continue;
            if (t.compare(0, 7, "export ") == 0) t = strip(t.substr(7));

            size_t pos = t.find('=');
            if (pos == std::string::npos) continue;

            std::string key = strip(t.substr(0, pos));
            std::string value = strip(t.substr(pos + 1));
            if (!key.empty()) {
                setenv(key.c_str(), value.c_str(), 0);
            }
        }
        std::cout << "Loaded .env from " << p << std::endl;
        return;
    }
}

int main() {
    loadDotEnv();

    Config config = Config::fromEnv();

    // Check Docker availability
    if (!isDockerAvailable()) {
        std::cerr << "WARNING: Docker is not available. Code execution will fail." << std::endl;
    }

    RateLimiter limiter(config.rateLimitWindowSeconds, config.rateLimitMaxRequests);
    Database db("data/submissions.db");
    std::cout << "Database initialized (data/submissions.db)" << std::endl;

    Auth auth;
    Auth studentAuth;

    // Seed default admin if none exists.
    const char* defaultAdminEmail = "admin@example.com";
    const char* defaultAdminPass = "admin123";
    const char* defaultAdminName = "Administrator";
    Admin admin = db.getAdminByEmail(defaultAdminEmail);
    if (admin.id == 0) {
        try {
            std::string hash = Auth::hashPassword(defaultAdminPass);
            if (db.addAdmin(defaultAdminEmail, hash, defaultAdminName) > 0) {
                std::cout << "Default admin created: " << defaultAdminEmail << std::endl;
            }
        } catch (const std::exception& e) {
            std::cerr << "Failed to seed default admin: " << e.what() << std::endl;
        }
    }

    crow::SimpleApp app;
    g_app = &app;

    // Graceful shutdown on SIGINT / SIGTERM
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    const fs::path publicDir = getPublicDir();

    // Serve index.html at root
    CROW_ROUTE(app, "/")([&publicDir]() {
        auto [found, content] = readFileContents((publicDir / "index.html").string());
        if (!found) {
            return crow::response(404, "Frontend not found");
        }
        auto res = crow::response(200, content);
        res.set_header("Content-Type", "text/html");
        return res;
    });

    // Serve static files: /public/<path>
    CROW_ROUTE(app, "/public/<path>")([&publicDir](const std::string& filePath) {
        fs::path fullPath = publicDir / filePath;

        // Prevent path traversal via canonical path check
        auto canonical = fs::weakly_canonical(fullPath);
        auto publicRoot = fs::weakly_canonical(publicDir);
        if (canonical.string().rfind(publicRoot.string(), 0) != 0) {
            return crow::response(403, "Forbidden");
        }

        auto [found, content] = readFileContents(fullPath.string());
        if (!found) {
            return crow::response(404, "Not found");
        }
        std::string ext = fs::path(fullPath).extension().string();
        auto res = crow::response(200, content);
        res.set_header("Content-Type", getMimeType(ext));
        return res;
    });

    auto runCodeHandler = [&config, &limiter](const crow::request& req, Database* dbPtr) {
        std::string clientIp = req.remote_ip_address;
        if (!limiter.allow(clientIp)) {
            return jsonError(429, "Too many requests. Please try again later.");
        }
        if (req.body.size() > 1024 * 1024) {
            return jsonError(413, "Request body too large");
        }
        auto v = validateRunRequest(req);
        if (!v.ok) {
            return jsonError(400, v.error);
        }
        ExecResult result = executeCode(v.language, v.code, v.input, config);

        if (dbPtr) {
            Submission sub;
            sub.language  = v.language;
            sub.code      = v.code;
            sub.input     = v.input;
            sub.stdoutStr = result.stdoutStr;
            sub.stderrStr = result.stderrStr;
            sub.exitCode  = result.exitCode;
            sub.timedOut  = result.timedOut;
            dbPtr->saveSubmission(sub);
        }

        crow::json::wvalue body;
        body["stdout"] = result.stdoutStr;
        body["stderr"] = result.stderrStr;
        body["exitCode"] = result.exitCode;
        body["timedOut"] = result.timedOut;
        return jsonResponse(200, body);
    };

    CROW_ROUTE(app, "/api/run").methods("POST"_method)
    ([&runCodeHandler, &db](const crow::request& req) {
        return runCodeHandler(req, &db);
    });

    CROW_ROUTE(app, "/api/playground/run").methods("POST"_method)
    ([&runCodeHandler](const crow::request& req) {
        return runCodeHandler(req, nullptr);
    });

    // GET /api/submissions — retrieve submission history
    CROW_ROUTE(app, "/api/submissions").methods("GET"_method)
    ([&db](const crow::request& req) {
        int limit = 20;
        std::string language;

        if (auto* v = req.url_params.get("limit")) {
            try { limit = std::stoi(v); } catch (...) {}
        }
        if (auto* v = req.url_params.get("language")) {
            language = v;
        }

        auto submissions = db.getSubmissions(limit, language);

        crow::json::wvalue body;
        std::vector<crow::json::wvalue> items;
        for (const auto& s : submissions) {
            crow::json::wvalue item;
            item["id"] = s.id;
            item["language"] = s.language;
            item["code"] = s.code;
            item["input"] = s.input;
            item["stdout"] = s.stdoutStr;
            item["stderr"] = s.stderrStr;
            item["exitCode"] = s.exitCode;
            item["timedOut"] = s.timedOut;
            item["createdAt"] = s.createdAt;
            items.push_back(std::move(item));
        }
        body["submissions"] = std::move(items);
        return jsonResponse(200, body);
    });

    // POST /api/admin/login
    CROW_ROUTE(app, "/api/admin/login").methods("POST"_method)
    ([&db, &auth](const crow::request& req) {
        auto body = crow::json::load(req.body);
        if (!body || !body.has("email") || !body.has("password")) {
            return jsonError(400, "Missing 'email' or 'password'");
        }

        std::string email = body["email"].s();
        std::string password = body["password"].s();
        if (email.empty() || password.empty()) {
            return jsonError(400, "Email and password cannot be empty");
        }

        Admin admin = db.getAdminByEmail(email);
        if (admin.id == 0 || !Auth::verifyPassword(password, admin.passwordHash)) {
            return jsonError(401, "Invalid email or password");
        }

        std::string token = auth.createSession(admin.id, admin.email, admin.name);

        crow::json::wvalue res;
        res["success"] = true;
        res["name"] = admin.name;
        res["email"] = admin.email;

        crow::response response = jsonResponse(200, res);
        setSessionCookie(response, token);
        return response;
    });

    // POST /api/admin/logout
    CROW_ROUTE(app, "/api/admin/logout").methods("POST"_method)
    ([&auth](const crow::request& req) {
        std::string cookieHeader = req.get_header_value("Cookie");
        std::string token = Auth::getCookieValue(cookieHeader, ADMIN_COOKIE);
        auth.destroySession(token);

        crow::json::wvalue res;
        res["success"] = true;

        crow::response response = jsonResponse(200, res);
        clearSessionCookie(response);
        return response;
    });

    // GET /api/admin/session
    CROW_ROUTE(app, "/api/admin/session").methods("GET"_method)
    ([&auth](const crow::request& req) {
        auto session = getAdminSession(req, auth);
        if (!session) return jsonError(401, "Unauthorized");

        crow::json::wvalue res;
        res["id"] = session->userId;
        res["email"] = session->email;
        res["name"] = session->name;
        return jsonResponse(200, res);
    });

    // Student auth
    CROW_ROUTE(app, "/api/student/register").methods("POST"_method)
    ([&db](const crow::request& req) {
        auto body = crow::json::load(req.body);
        if (!body || !body.has("email") || !body.has("password") || !body.has("name")) {
            return jsonError(400, "Missing 'name', 'email' or 'password'");
        }

        std::string email    = body["email"].s();
        std::string password = body["password"].s();
        std::string name     = body["name"].s();

        if (name.empty() || email.empty() || password.empty()) {
            return jsonError(400, "Name, email and password are required");
        }
        if (name.size() < 3 || name.size() > 32) {
            return jsonError(400, "Name must be between 3 and 32 characters");
        }
        if (password.size() < 8 || password.size() > 32) {
            return jsonError(400, "Password must be between 8 and 32 characters");
        }

        User existing = db.getUserByEmail(email);
        if (existing.id != 0) {
            return jsonError(409, "Email already registered");
        }

        std::string hash = Auth::hashPassword(password);
        std::string verificationToken = Auth::generateToken();
        int now = static_cast<int>(std::time(nullptr));
        int expiresAt = now + 24 * 3600;

        int id = db.addUser(email, hash, name, verificationToken, expiresAt);
        if (id <= 0) {
            return jsonError(500, "Failed to register user");
        }

        std::string verifyUrl = getAppUrl() + "/public/student-verify.html?token=" + verificationToken;
        sendEmail(email, "Verify your Online Compiler account", buildVerificationEmail(name, verifyUrl), true);

        crow::json::wvalue resp;
        resp["success"] = true;
        resp["message"] = "Registration successful. Please check your email to verify your account.";
        return jsonResponse(200, resp);
    });

    CROW_ROUTE(app, "/api/student/login").methods("POST"_method)
    ([&db, &studentAuth](const crow::request& req) {
        auto body = crow::json::load(req.body);
        if (!body || !body.has("email") || !body.has("password")) {
            return jsonError(400, "Missing 'email' or 'password'");
        }

        std::string email    = body["email"].s();
        std::string password = body["password"].s();

        if (email.empty() || password.empty()) {
            return jsonError(400, "Email and password cannot be empty");
        }

        User user = db.getUserByEmail(email);
        if (user.id == 0 || !Auth::verifyPassword(password, user.passwordHash)) {
            return jsonError(401, "Invalid email or password");
        }
        if (!user.isVerified) {
            return jsonError(401, "Please verify your email before logging in");
        }

        std::string token = studentAuth.createSession(user.id, user.email, user.name);

        crow::json::wvalue resp;
        resp["success"] = true;
        resp["id"] = user.id;
        resp["email"] = user.email;
        resp["name"] = user.name;
        crow::response response = jsonResponse(200, resp);
        setCookie(response, STUDENT_COOKIE, token);
        return response;
    });
    CROW_ROUTE(app, "/api/student/verify").methods("GET"_method)
    ([&db](const crow::request& req) {
        const char* token = req.url_params.get("token");
        if (!token || *token == '\0') {
            return jsonError(400, "Missing 'token'");
        }
        User user = db.getUserByVerificationToken(token);
        if (user.id == 0) {
            return jsonError(400, "Invalid or expired verification link");
        }
        int now = static_cast<int>(std::time(nullptr));
        if (user.isVerified) {
            return jsonError(400, "Account already verified");
        }
        if (user.verificationExpiresAt < now) {
            return jsonError(400, "Verification link has expired");
        }
        if (!db.verifyUser(user.id)) {
            return jsonError(500, "Failed to verify account");
        }
        crow::json::wvalue resp;
        resp["success"] = true;
        resp["message"] = "Email verified successfully. You can now log in.";
        return jsonResponse(200, resp);
    });

    CROW_ROUTE(app, "/api/student/resend-verification").methods("POST"_method)
    ([&db](const crow::request& req) {
        auto body = crow::json::load(req.body);
        if (!body || !body.has("email")) {
            return jsonError(400, "Missing 'email'");
        }
        std::string email = body["email"].s();
        User user = db.getUserByEmail(email);
        if (user.id != 0 && !user.isVerified) {
            std::string verificationToken = Auth::generateToken();
            int now = static_cast<int>(std::time(nullptr));
            int expiresAt = now + 24 * 3600;
            db.setUserVerificationToken(user.id, verificationToken, expiresAt);
            std::string verifyUrl = getAppUrl() + "/public/student-verify.html?token=" + verificationToken;
            sendEmail(email, "Verify your Online Compiler account", buildVerificationEmail(user.name, verifyUrl), true);
        }
        crow::json::wvalue resp;
        resp["success"] = true;
        resp["message"] = "If the email is registered and unverified, a new verification link has been sent.";
        return jsonResponse(200, resp);
    });

    CROW_ROUTE(app, "/api/student/forgot-password").methods("POST"_method)
    ([&db](const crow::request& req) {
        auto body = crow::json::load(req.body);
        if (!body || !body.has("email")) {
            return jsonError(400, "Missing 'email'");
        }
        std::string email = body["email"].s();
        User user = db.getUserByEmail(email);
        if (user.id != 0) {
            std::string otp = Auth::generateNumericCode(6);
            int now = static_cast<int>(std::time(nullptr));
            int expiresAt = now + 15 * 60;
            db.setUserResetOtp(user.id, otp, expiresAt);
            std::string emailBody = "Hi " + user.name + ",\n\n" +
                "Your Online Compiler password reset code is: " + otp + "\n\n" +
                "This code expires in 15 minutes.\n\n" +
                "- Online Compiler";
            sendEmail(email, "Online Compiler password reset code", emailBody);
        }
        crow::json::wvalue resp;
        resp["success"] = true;
        resp["message"] = "If the email is registered, a reset code has been sent.";
        return jsonResponse(200, resp);
    });

    CROW_ROUTE(app, "/api/student/reset-password").methods("POST"_method)
    ([&db](const crow::request& req) {
        auto body = crow::json::load(req.body);
        if (!body || !body.has("email") || !body.has("otp") || !body.has("new_password")) {
            return jsonError(400, "Missing 'email', 'otp' or 'new_password'");
        }
        std::string email = body["email"].s();
        std::string otp = body["otp"].s();
        std::string newPassword = body["new_password"].s();

        if (newPassword.size() < 8 || newPassword.size() > 32) {
            return jsonError(400, "New password must be between 8 and 32 characters");
        }

        User user = db.getUserByEmail(email);
        if (user.id == 0) {
            return jsonError(400, "Invalid or expired reset code");
        }

        int now = static_cast<int>(std::time(nullptr));
        if (!Auth::constantTimeEquals(user.resetOtp, otp) || user.resetOtpExpiresAt < now) {
            return jsonError(400, "Invalid or expired reset code");
        }

        std::string hash = Auth::hashPassword(newPassword);
        if (!db.updateUserPassword(user.id, hash)) {
            return jsonError(500, "Failed to update password");
        }
        db.clearUserResetOtp(user.id);

        crow::json::wvalue resp;
        resp["success"] = true;
        resp["message"] = "Password updated successfully. Please log in.";
        return jsonResponse(200, resp);
    });

    CROW_ROUTE(app, "/api/student/logout").methods("POST"_method)
    ([&studentAuth](const crow::request& req) {
        std::string cookieHeader = req.get_header_value("Cookie");
        std::string token = Auth::getCookieValue(cookieHeader, STUDENT_COOKIE);
        studentAuth.destroySession(token);

        crow::json::wvalue resp;
        resp["success"] = true;
        crow::response response = jsonResponse(200, resp);
        setCookie(response, STUDENT_COOKIE, "", 0);
        return response;
    });

    CROW_ROUTE(app, "/api/student/session").methods("GET"_method)
    ([&studentAuth](const crow::request& req) {
        auto session = getStudentSession(req, studentAuth);
        if (!session) return jsonError(401, "Unauthorized");

        crow::json::wvalue res;
        res["id"] = session->userId;
        res["email"] = session->email;
        res["name"] = session->name;
        return jsonResponse(200, res);
    });

    CROW_ROUTE(app, "/api/student/dashboard").methods("GET"_method)
    ([&db, &studentAuth](const crow::request& req) {
        auto session = getStudentSession(req, studentAuth);
        if (!session) return jsonError(401, "Unauthorized");

        auto stats = db.getStudentStats(session->userId);
        crow::json::wvalue res;
        res["name"] = session->name;
        res["email"] = session->email;
        res["total_submissions"] = stats.totalSubmissions;
        res["questions_attempted"] = stats.questionsAttempted;
        return jsonResponse(200, res);
    });

    // Platform: published questions
    CROW_ROUTE(app, "/api/platform/questions").methods("GET"_method)
    ([&db, &studentAuth](const crow::request& req) {
        auto session = getStudentSession(req, studentAuth);
        if (!session) return jsonError(401, "Unauthorized");

        int page = 1;
        int limit = 20;
        std::string search, category, difficulty;

        if (auto* v = req.url_params.get("page")) { try { page = std::max(1, std::stoi(v)); } catch (...) {} }
        if (auto* v = req.url_params.get("limit")) { try { limit = std::max(0, std::stoi(v)); } catch (...) {} }
        if (auto* v = req.url_params.get("search")) { search = v; }
        if (auto* v = req.url_params.get("category")) { category = v; }
        if (auto* v = req.url_params.get("difficulty")) { difficulty = v; }

        int total = 0;
        auto all = db.getQuestions(total, 0, 0, search, category, difficulty);
        std::vector<Question> published;
        for (const auto& q : all) {
            if (q.published) published.push_back(q);
        }
        total = static_cast<int>(published.size());

        if (limit <= 0) limit = 20;
        int offset = (page - 1) * limit;
        int end = std::min(offset + limit, total);

        std::vector<crow::json::wvalue> items;
        for (int i = offset; i < end; ++i) {
            const auto& q = published[i];
            crow::json::wvalue item;
            item["id"] = q.id;
            item["title"] = q.title;
            item["description"] = q.description;
            item["category"] = q.category;
            item["difficulty"] = q.difficulty;
            item["createdAt"] = q.createdAt;
            items.push_back(std::move(item));
        }

        crow::json::wvalue body;
        body["questions"] = std::move(items);
        body["total"] = total;
        body["page"] = page;
        body["limit"] = limit;
        return jsonResponse(200, body);
    });

    CROW_ROUTE(app, "/api/platform/questions/<int>").methods("GET"_method)
    ([&db, &studentAuth](const crow::request& req, int id) {
        auto session = getStudentSession(req, studentAuth);
        if (!session) return jsonError(401, "Unauthorized");

        Question q = db.getQuestionById(id);
        if (q.id == 0 || !q.published) {
            return jsonError(404, "Question not found");
        }

        auto testcases = db.getTestCases(id);
        std::vector<crow::json::wvalue> items;
        for (const auto& t : testcases) {
            if (t.isHidden) continue;
            crow::json::wvalue item;
            item["id"] = t.id;
            item["input"] = t.input;
            item["expected_output"] = t.expectedOutput;
            items.push_back(std::move(item));
        }

        crow::json::wvalue body;
        body["id"] = q.id;
        body["title"] = q.title;
        body["description"] = q.description;
        body["category"] = q.category;
        body["difficulty"] = q.difficulty;
        body["testcases"] = std::move(items);
        return jsonResponse(200, body);
    });

    CROW_ROUTE(app, "/api/platform/submit").methods("POST"_method)
    ([&config, &db, &studentAuth](const crow::request& req) {
        auto session = getStudentSession(req, studentAuth);
        if (!session) return jsonError(401, "Unauthorized");

        auto body = crow::json::load(req.body);
        if (!body || !body.has("question_id") || !body.has("language") || !body.has("code")) {
            return jsonError(400, "Missing 'question_id', 'language' or 'code'");
        }

        int questionId = static_cast<int>(body["question_id"].i());
        std::string language = body["language"].s();
        std::string code = body["code"].s();

        Question q = db.getQuestionById(questionId);
        if (q.id == 0 || !q.published) {
            return jsonError(404, "Question not found");
        }

        auto testcases = db.getTestCases(questionId);
        int passed = 0;
        int total = 0;
        std::string allStderr;
        bool allPassed = true;

        for (const auto& t : testcases) {
            total++;
            ExecResult result = executeCode(language, code, t.input, config);
            if (result.timedOut) {
                allPassed = false;
                allStderr += "Execution timed out\n";
                continue;
            }
            if (result.exitCode != 0) {
                allPassed = false;
                allStderr += "Exit code " + std::to_string(result.exitCode) + "\n";
                continue;
            }
            if (trim(result.stdoutStr) == trim(t.expectedOutput)) {
                passed++;
            } else {
                allPassed = false;
            }
        }

        if (total == 0) {
            ExecResult result = executeCode(language, code, "", config);
            allStderr = result.stderrStr;
        }

        Submission sub;
        sub.userId = session->userId;
        sub.questionId = questionId;
        sub.language = language;
        sub.code = code;
        sub.input = "";
        sub.stdoutStr = std::to_string(passed) + " / " + std::to_string(total) + " test cases passed";
        sub.stderrStr = allStderr;
        sub.exitCode = allPassed ? 0 : 1;
        sub.timedOut = false;
        int submissionId = db.saveSubmission(sub);

        crow::json::wvalue resp;
        resp["passed"] = passed;
        resp["total"] = total;
        resp["allPassed"] = allPassed;
        resp["submissionId"] = submissionId;
        return jsonResponse(200, resp);
    });

    CROW_ROUTE(app, "/api/platform/submissions").methods("GET"_method)
    ([&db, &studentAuth](const crow::request& req) {
        auto session = getStudentSession(req, studentAuth);
        if (!session) return jsonError(401, "Unauthorized");

        int limit = 20;
        if (auto* v = req.url_params.get("limit")) {
            try { limit = std::max(1, std::stoi(v)); } catch (...) {}
        }

        auto submissions = db.getSubmissionsByUser(session->userId, limit);

        crow::json::wvalue body;
        std::vector<crow::json::wvalue> items;
        for (const auto& s : submissions) {
            crow::json::wvalue item;
            item["id"] = s.id;
            item["questionId"] = s.questionId;
            item["language"] = s.language;
            item["stdout"] = s.stdoutStr;
            item["stderr"] = s.stderrStr;
            item["exitCode"] = s.exitCode;
            item["timedOut"] = s.timedOut;
            item["createdAt"] = s.createdAt;
            items.push_back(std::move(item));
        }
        body["submissions"] = std::move(items);
        return jsonResponse(200, body);
    });

    // Admin: questions
    CROW_ROUTE(app, "/api/questions").methods("GET"_method)
    ([&db, &auth](const crow::request& req) {
        if (!getAdminSession(req, auth)) return jsonError(401, "Unauthorized");

        int page = 1;
        int limit = 0;
        int offset = 0;
        std::string search, category, difficulty;

        if (auto* v = req.url_params.get("page")) { try { page = std::max(1, std::stoi(v)); } catch (...) {} }
        if (auto* v = req.url_params.get("limit")) { try { limit = std::max(0, std::stoi(v)); } catch (...) {} }
        if (auto* v = req.url_params.get("search")) { search = v; }
        if (auto* v = req.url_params.get("category")) { category = v; }
        if (auto* v = req.url_params.get("difficulty")) { difficulty = v; }

        if (limit > 0) {
            offset = (page - 1) * limit;
        }

        int total = 0;
        auto questions = db.getQuestions(total, limit, offset, search, category, difficulty);

        crow::json::wvalue body;
        std::vector<crow::json::wvalue> items;
        for (const auto& q : questions) {
            crow::json::wvalue item;
            item["id"] = q.id;
            item["title"] = q.title;
            item["description"] = q.description;
            item["category"] = q.category;
            item["difficulty"] = q.difficulty;
            item["published"] = q.published;
            item["createdAt"] = q.createdAt;
            items.push_back(std::move(item));
        }
        body["questions"] = std::move(items);
        body["total"] = total;
        body["page"] = page;
        body["limit"] = limit;
        return jsonResponse(200, body);
    });

    CROW_ROUTE(app, "/api/questions").methods("POST"_method)
    ([&db, &auth](const crow::request& req) {
        if (!getAdminSession(req, auth)) return jsonError(401, "Unauthorized");
        auto body = crow::json::load(req.body);
        if (!body || !body.has("title")) {
            return jsonError(400, "Missing 'title'");
        }

        std::string title       = body["title"].s();
        std::string description = body.has("description") ? std::string(body["description"].s()) : "";
        std::string category    = body.has("category")    ? std::string(body["category"].s())    : "";
        std::string difficulty  = body.has("difficulty")  ? std::string(body["difficulty"].s())  : "";
        bool published          = body.has("published")   ? static_cast<bool>(body["published"].b()) : true;

        if (title.empty()) {
            return jsonError(400, "Empty title");
        }

        int id = db.addQuestion(title, description, category, difficulty, published);
        if (id < 0) {
            return jsonError(500, "Failed to save question");
        }

        crow::json::wvalue res;
        res["success"] = true;
        res["id"] = id;
        return jsonResponse(200, res);
    });

    CROW_ROUTE(app, "/api/questions/<int>").methods("PUT"_method)
    ([&db, &auth](const crow::request& req, int id) {
        if (!getAdminSession(req, auth)) return jsonError(401, "Unauthorized");
        auto body = crow::json::load(req.body);
        if (!body || !body.has("title")) {
            return jsonError(400, "Missing 'title'");
        }

        std::string title       = body["title"].s();
        std::string description = body.has("description") ? std::string(body["description"].s()) : "";
        std::string category    = body.has("category")    ? std::string(body["category"].s())    : "";
        std::string difficulty  = body.has("difficulty")  ? std::string(body["difficulty"].s())  : "";
        bool published          = body.has("published")   ? static_cast<bool>(body["published"].b()) : true;

        if (title.empty()) {
            return jsonError(400, "Empty title");
        }

        if (!db.updateQuestion(id, title, description, category, difficulty, published)) {
            return jsonError(500, "Failed to update question");
        }

        crow::json::wvalue res;
        res["success"] = true;
        return jsonResponse(200, res);
    });

    CROW_ROUTE(app, "/api/questions/<int>").methods("DELETE"_method)
    ([&db, &auth](const crow::request& req, int id) {
        if (!getAdminSession(req, auth)) return jsonError(401, "Unauthorized");
        if (!db.deleteQuestion(id)) {
            return jsonError(500, "Failed to delete question");
        }

        crow::json::wvalue res;
        res["success"] = true;
        return jsonResponse(200, res);
    });

    CROW_ROUTE(app, "/api/questions/bulk").methods("DELETE"_method)
    ([&db, &auth](const crow::request& req) {
        if (!getAdminSession(req, auth)) return jsonError(401, "Unauthorized");
        auto body = crow::json::load(req.body);
        if (!body || !body.has("ids")) {
            return jsonError(400, "Missing 'ids'");
        }

        std::vector<int> ids;
        auto& idList = body["ids"];
        for (size_t i = 0; i < idList.size(); ++i) {
            ids.push_back(static_cast<int>(idList[i].i()));
        }

        if (ids.empty()) {
            return jsonError(400, "Empty 'ids'");
        }

        if (!db.deleteQuestionsBulk(ids)) {
            return jsonError(500, "Failed to delete questions");
        }

        crow::json::wvalue res;
        res["success"] = true;
        res["deleted"] = static_cast<int>(ids.size());
        return jsonResponse(200, res);
    });

    // Admin: test cases
    CROW_ROUTE(app, "/api/questions/<int>/testcases").methods("GET"_method)
    ([&db, &auth](const crow::request& req, int questionId) {
        if (!getAdminSession(req, auth)) return jsonError(401, "Unauthorized");
        auto testcases = db.getTestCases(questionId);

        crow::json::wvalue body;
        std::vector<crow::json::wvalue> items;
        for (const auto& t : testcases) {
            crow::json::wvalue item;
            item["id"] = t.id;
            item["question_id"] = t.questionId;
            item["input"] = t.input;
            item["expected_output"] = t.expectedOutput;
            item["is_hidden"] = t.isHidden;
            items.push_back(std::move(item));
        }
        body["testcases"] = std::move(items);
        return jsonResponse(200, body);
    });

    CROW_ROUTE(app, "/api/questions/<int>/testcases").methods("POST"_method)
    ([&db, &auth](const crow::request& req, int questionId) {
        if (!getAdminSession(req, auth)) return jsonError(401, "Unauthorized");
        auto body = crow::json::load(req.body);
        if (!body) {
            return jsonError(400, "Invalid JSON");
        }

        std::string input          = body.has("input")          ? std::string(body["input"].s())          : "";
        std::string expectedOutput = body.has("expected_output") ? std::string(body["expected_output"].s()) : "";
        bool isHidden              = body.has("is_hidden")       ? (body["is_hidden"].b() || body["is_hidden"].t() == crow::json::type::True) : false;

        int id = db.addTestCase(questionId, input, expectedOutput, isHidden);
        if (id < 0) {
            return jsonError(500, "Failed to save test case");
        }

        crow::json::wvalue res;
        res["success"] = true;
        res["id"] = id;
        return jsonResponse(200, res);
    });

    CROW_ROUTE(app, "/api/testcases/<int>").methods("PUT"_method)
    ([&db, &auth](const crow::request& req, int id) {
        if (!getAdminSession(req, auth)) return jsonError(401, "Unauthorized");
        auto body = crow::json::load(req.body);
        if (!body) {
            return jsonError(400, "Invalid JSON");
        }

        std::string input          = body.has("input")          ? std::string(body["input"].s())          : "";
        std::string expectedOutput = body.has("expected_output") ? std::string(body["expected_output"].s()) : "";
        bool isHidden              = body.has("is_hidden")       ? (body["is_hidden"].b() || body["is_hidden"].t() == crow::json::type::True) : false;

        if (!db.updateTestCase(id, input, expectedOutput, isHidden)) {
            return jsonError(500, "Failed to update test case");
        }

        crow::json::wvalue res;
        res["success"] = true;
        return jsonResponse(200, res);
    });

    CROW_ROUTE(app, "/api/testcases/<int>").methods("DELETE"_method)
    ([&db, &auth](const crow::request& req, int id) {
        if (!getAdminSession(req, auth)) return jsonError(401, "Unauthorized");
        if (!db.deleteTestCase(id)) {
            return jsonError(500, "Failed to delete test case");
        }

        crow::json::wvalue res;
        res["success"] = true;
        return jsonResponse(200, res);
    });

    unsigned int threads = std::thread::hardware_concurrency();
    if (threads == 0) threads = 4;

    std::cout << "Server running on http://0.0.0.0:" << config.port
              << " (" << threads << " threads)" << std::endl;
    app.bindaddr("0.0.0.0").port(config.port).concurrency(threads).run();

    std::cout << "Server stopped." << std::endl;
    return 0;
}
