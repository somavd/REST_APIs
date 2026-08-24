#include "database.hpp"

#include <sqlite3.h>
#include <filesystem>
#include <iostream>
#include <stdexcept>

namespace fs = std::filesystem;

static const char* CREATE_TABLE_SQL = R"(
CREATE TABLE IF NOT EXISTS submissions (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    user_id     INTEGER DEFAULT 0,
    question_id INTEGER DEFAULT 0,
    language    TEXT    NOT NULL,
    code        TEXT    NOT NULL,
    input       TEXT    DEFAULT '',
    stdout      TEXT    DEFAULT '',
    stderr      TEXT    DEFAULT '',
    exit_code   INTEGER DEFAULT 0,
    timed_out   INTEGER DEFAULT 0,
    created_at  TEXT    DEFAULT CURRENT_TIMESTAMP
);
CREATE INDEX IF NOT EXISTS idx_lang ON submissions(language);
CREATE INDEX IF NOT EXISTS idx_time ON submissions(created_at);

CREATE TABLE IF NOT EXISTS questions (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    title       TEXT    NOT NULL,
    description TEXT    DEFAULT '',
    category    TEXT    DEFAULT '',
    difficulty  TEXT    DEFAULT '',
    published   INTEGER NOT NULL DEFAULT 1,
    created_at  TEXT    DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE IF NOT EXISTS test_cases (
    id              INTEGER PRIMARY KEY AUTOINCREMENT,
    question_id     INTEGER NOT NULL,
    input           TEXT    DEFAULT '',
    expected_output TEXT    DEFAULT '',
    is_hidden       INTEGER DEFAULT 0,
    FOREIGN KEY (question_id) REFERENCES questions(id) ON DELETE CASCADE
);
CREATE INDEX IF NOT EXISTS idx_question_id ON test_cases(question_id);

CREATE TABLE IF NOT EXISTS admins (
    id             INTEGER PRIMARY KEY AUTOINCREMENT,
    email          TEXT    UNIQUE NOT NULL,
    password_hash  TEXT    NOT NULL,
    name           TEXT    NOT NULL,
    created_at     TEXT    DEFAULT CURRENT_TIMESTAMP
);
CREATE INDEX IF NOT EXISTS idx_admins_email ON admins(email);

CREATE TABLE IF NOT EXISTS users (
    id             INTEGER PRIMARY KEY AUTOINCREMENT,
    email          TEXT    UNIQUE NOT NULL,
    password_hash  TEXT    NOT NULL,
    name           TEXT    NOT NULL,
    created_at     TEXT    DEFAULT CURRENT_TIMESTAMP,
    is_verified    INTEGER DEFAULT 0,
    verification_token TEXT    DEFAULT '',
    verification_expires_at INTEGER DEFAULT 0,
    reset_otp      TEXT    DEFAULT '',
    reset_otp_expires_at INTEGER DEFAULT 0
);
CREATE INDEX IF NOT EXISTS idx_users_email ON users(email);
)";

Database::Database(const std::string& path) {
    // Ensure directory exists
    fs::path dbPath(path);
    if (dbPath.has_parent_path()) {
        fs::create_directories(dbPath.parent_path());
    }

    int rc = sqlite3_open(path.c_str(), &db_);
    if (rc != SQLITE_OK) {
        std::string err = sqlite3_errmsg(db_);
        sqlite3_close(db_);
        db_ = nullptr;
        throw std::runtime_error("Failed to open database: " + err);
    }

    // Enable WAL mode for better concurrent read performance
    sqlite3_exec(db_, "PRAGMA journal_mode=WAL;", nullptr, nullptr, nullptr);
    sqlite3_exec(db_, "PRAGMA synchronous=NORMAL;", nullptr, nullptr, nullptr);

    init();
}

Database::~Database() {
    if (db_) {
        sqlite3_close(db_);
    }
}

static bool columnExists(sqlite3* db, const std::string& table, const std::string& column) {
    std::string sql = "PRAGMA table_info(" + table + ")";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }
    bool exists = false;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const char* name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        if (name && column == name) {
            exists = true;
            break;
        }
    }
    sqlite3_finalize(stmt);
    return exists;
}

static void addColumn(sqlite3* db, const std::string& table, const std::string& column, const std::string& type) {
    std::string sql = "ALTER TABLE " + table + " ADD COLUMN " + column + " " + type;
    char* errMsg = nullptr;
    sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &errMsg);
    if (errMsg) sqlite3_free(errMsg);
}

void Database::init() {
    char* errMsg = nullptr;
    int rc = sqlite3_exec(db_, CREATE_TABLE_SQL, nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        std::string err = errMsg ? errMsg : "unknown error";
        sqlite3_free(errMsg);
        throw std::runtime_error("Failed to create table: " + err);
    }

    if (!columnExists(db_, "submissions", "user_id")) {
        addColumn(db_, "submissions", "user_id", "INTEGER DEFAULT 0");
    }
    if (!columnExists(db_, "submissions", "question_id")) {
        addColumn(db_, "submissions", "question_id", "INTEGER DEFAULT 0");
    }
    if (!columnExists(db_, "questions", "published")) {
        addColumn(db_, "questions", "published", "INTEGER NOT NULL DEFAULT 1");
    }

    bool usersHadVerificationToken = columnExists(db_, "users", "verification_token");

    if (!columnExists(db_, "users", "is_verified")) {
        addColumn(db_, "users", "is_verified", "INTEGER DEFAULT 0");
    }
    if (!columnExists(db_, "users", "verification_token")) {
        addColumn(db_, "users", "verification_token", "TEXT DEFAULT ''");
    }
    if (!columnExists(db_, "users", "verification_expires_at")) {
        addColumn(db_, "users", "verification_expires_at", "INTEGER DEFAULT 0");
    }
    if (!columnExists(db_, "users", "reset_otp")) {
        addColumn(db_, "users", "reset_otp", "TEXT DEFAULT ''");
    }
    if (!columnExists(db_, "users", "reset_otp_expires_at")) {
        addColumn(db_, "users", "reset_otp_expires_at", "INTEGER DEFAULT 0");
    }

    if (!usersHadVerificationToken && columnExists(db_, "users", "verification_token")) {
        sqlite3_exec(db_, "UPDATE users SET is_verified = 1", nullptr, nullptr, nullptr);
    }
}

int Database::saveSubmission(const Submission& sub) {
    std::lock_guard<std::mutex> lock(mutex_);

    const char* sql = R"(
        INSERT INTO submissions (user_id, question_id, language, code, input, stdout, stderr, exit_code, timed_out)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)
    )";

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "DB prepare error: " << sqlite3_errmsg(db_) << std::endl;
        return -1;
    }

    sqlite3_bind_int(stmt, 1, sub.userId);
    sqlite3_bind_int(stmt, 2, sub.questionId);
    sqlite3_bind_text(stmt, 3, sub.language.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, sub.code.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 5, sub.input.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 6, sub.stdoutStr.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 7, sub.stderrStr.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 8, sub.exitCode);
    sqlite3_bind_int(stmt, 9, sub.timedOut ? 1 : 0);

    rc = sqlite3_step(stmt);
    int rowId = -1;
    if (rc == SQLITE_DONE) {
        rowId = static_cast<int>(sqlite3_last_insert_rowid(db_));
    } else {
        std::cerr << "DB insert error: " << sqlite3_errmsg(db_) << std::endl;
    }

    sqlite3_finalize(stmt);
    return rowId;
}

std::vector<Submission> Database::getSubmissions(int limit, const std::string& language) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<Submission> results;

    // Clamp limit
    if (limit <= 0) limit = 20;
    if (limit > 100) limit = 100;

    std::string sql;
    if (language.empty()) {
        sql = "SELECT id, language, code, input, stdout, stderr, exit_code, timed_out, created_at "
              "FROM submissions ORDER BY created_at DESC LIMIT ?";
    } else {
        sql = "SELECT id, language, code, input, stdout, stderr, exit_code, timed_out, created_at "
              "FROM submissions WHERE language = ? ORDER BY created_at DESC LIMIT ?";
    }

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "DB prepare error: " << sqlite3_errmsg(db_) << std::endl;
        return results;
    }

    if (language.empty()) {
        sqlite3_bind_int(stmt, 1, limit);
    } else {
        sqlite3_bind_text(stmt, 1, language.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(stmt, 2, limit);
    }

    auto getText = [](sqlite3_stmt* st, int col) -> std::string {
        auto* p = sqlite3_column_text(st, col);
        return p ? reinterpret_cast<const char*>(p) : "";
    };

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        Submission s;
        s.id        = sqlite3_column_int(stmt, 0);
        s.language  = getText(stmt, 1);
        s.code      = getText(stmt, 2);
        s.input     = getText(stmt, 3);
        s.stdoutStr = getText(stmt, 4);
        s.stderrStr = getText(stmt, 5);
        s.exitCode  = sqlite3_column_int(stmt, 6);
        s.timedOut  = sqlite3_column_int(stmt, 7) != 0;
        s.createdAt = getText(stmt, 8);
        results.push_back(std::move(s));
    }

    sqlite3_finalize(stmt);
    return results;
}

// Question management

int Database::addQuestion(const std::string& title, const std::string& description,
                          const std::string& category, const std::string& difficulty,
                          bool published) {
    std::lock_guard<std::mutex> lock(mutex_);
    const char* sql = "INSERT INTO questions (title, description, category, difficulty, published) VALUES (?, ?, ?, ?, ?)";

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "DB prepare error: " << sqlite3_errmsg(db_) << std::endl;
        return -1;
    }

    sqlite3_bind_text(stmt, 1, title.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, description.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, category.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, difficulty.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 5, published ? 1 : 0);

    rc = sqlite3_step(stmt);
    int rowId = -1;
    if (rc == SQLITE_DONE) {
        rowId = static_cast<int>(sqlite3_last_insert_rowid(db_));
    } else {
        std::cerr << "DB insert error: " << sqlite3_errmsg(db_) << std::endl;
    }

    sqlite3_finalize(stmt);
    return rowId;
}

bool Database::updateQuestion(int id, const std::string& title, const std::string& description,
                              const std::string& category, const std::string& difficulty,
                              bool published) {
    std::lock_guard<std::mutex> lock(mutex_);
    const char* sql = "UPDATE questions SET title = ?, description = ?, category = ?, difficulty = ?, published = ? WHERE id = ?";

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "DB prepare error: " << sqlite3_errmsg(db_) << std::endl;
        return false;
    }

    sqlite3_bind_text(stmt, 1, title.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, description.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, category.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, difficulty.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 5, published ? 1 : 0);
    sqlite3_bind_int(stmt, 6, id);

    rc = sqlite3_step(stmt);
    bool ok = (rc == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return ok;
}

bool Database::deleteQuestion(int id) {
    std::lock_guard<std::mutex> lock(mutex_);
    const char* sql = "DELETE FROM questions WHERE id = ?";

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) return false;

    sqlite3_bind_int(stmt, 1, id);
    rc = sqlite3_step(stmt);
    bool ok = (rc == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return ok;
}

bool Database::deleteQuestionsBulk(const std::vector<int>& ids) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (ids.empty()) return true;

    std::string sql = "DELETE FROM questions WHERE id IN (";
    for (size_t i = 0; i < ids.size(); ++i) {
        if (i > 0) sql += ",";
        sql += "?";
    }
    sql += ")";

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "DB prepare error: " << sqlite3_errmsg(db_) << std::endl;
        return false;
    }

    for (size_t i = 0; i < ids.size(); ++i) {
        sqlite3_bind_int(stmt, static_cast<int>(i + 1), ids[i]);
    }

    rc = sqlite3_step(stmt);
    bool ok = (rc == SQLITE_DONE);
    if (!ok) {
        std::cerr << "DB bulk delete error: " << sqlite3_errmsg(db_) << std::endl;
    }
    sqlite3_finalize(stmt);
    return ok;
}

std::vector<Question> Database::getQuestions(int& total, int limit, int offset,
                                             const std::string& search,
                                             const std::string& category,
                                             const std::string& difficulty) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<Question> results;
    total = 0;

    std::string where = "WHERE 1=1";
    if (!search.empty()) {
        where += " AND (title LIKE ? OR description LIKE ?)";
    }
    if (!category.empty()) {
        where += " AND category = ?";
    }
    if (!difficulty.empty()) {
        where += " AND difficulty = ?";
    }

    std::string countSql = "SELECT COUNT(*) FROM questions " + where;
    sqlite3_stmt* countStmt = nullptr;
    int rc = sqlite3_prepare_v2(db_, countSql.c_str(), -1, &countStmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "DB prepare error: " << sqlite3_errmsg(db_) << std::endl;
        return results;
    }

    auto getText = [](sqlite3_stmt* st, int col) -> std::string {
        auto* p = sqlite3_column_text(st, col);
        return p ? reinterpret_cast<const char*>(p) : "";
    };

    int idx = 1;
    if (!search.empty()) {
        std::string like = "%" + search + "%";
        sqlite3_bind_text(countStmt, idx++, like.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(countStmt, idx++, like.c_str(), -1, SQLITE_TRANSIENT);
    }
    if (!category.empty()) {
        sqlite3_bind_text(countStmt, idx++, category.c_str(), -1, SQLITE_TRANSIENT);
    }
    if (!difficulty.empty()) {
        sqlite3_bind_text(countStmt, idx++, difficulty.c_str(), -1, SQLITE_TRANSIENT);
    }

    if (sqlite3_step(countStmt) == SQLITE_ROW) {
        total = sqlite3_column_int(countStmt, 0);
    }
    sqlite3_finalize(countStmt);

    std::string dataSql = "SELECT id, title, description, category, difficulty, published, created_at FROM questions " + where + " ORDER BY id DESC";
    if (limit > 0) {
        dataSql += " LIMIT ? OFFSET ?";
    }

    sqlite3_stmt* stmt = nullptr;
    rc = sqlite3_prepare_v2(db_, dataSql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "DB prepare error: " << sqlite3_errmsg(db_) << std::endl;
        return results;
    }

    idx = 1;
    if (!search.empty()) {
        std::string like = "%" + search + "%";
        sqlite3_bind_text(stmt, idx++, like.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, idx++, like.c_str(), -1, SQLITE_TRANSIENT);
    }
    if (!category.empty()) {
        sqlite3_bind_text(stmt, idx++, category.c_str(), -1, SQLITE_TRANSIENT);
    }
    if (!difficulty.empty()) {
        sqlite3_bind_text(stmt, idx++, difficulty.c_str(), -1, SQLITE_TRANSIENT);
    }
    if (limit > 0) {
        sqlite3_bind_int(stmt, idx++, limit);
        sqlite3_bind_int(stmt, idx++, offset);
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        Question q;
        q.id          = sqlite3_column_int(stmt, 0);
        q.title       = getText(stmt, 1);
        q.description = getText(stmt, 2);
        q.category    = getText(stmt, 3);
        q.difficulty  = getText(stmt, 4);
        q.published   = sqlite3_column_int(stmt, 5) != 0;
        q.createdAt   = getText(stmt, 6);
        results.push_back(std::move(q));
    }

    sqlite3_finalize(stmt);
    return results;
}

// Test case management

int Database::addTestCase(int questionId, const std::string& input,
                          const std::string& expectedOutput, bool isHidden) {
    std::lock_guard<std::mutex> lock(mutex_);
    const char* sql = "INSERT INTO test_cases (question_id, input, expected_output, is_hidden) VALUES (?, ?, ?, ?)";

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "DB prepare error: " << sqlite3_errmsg(db_) << std::endl;
        return -1;
    }

    sqlite3_bind_int(stmt, 1, questionId);
    sqlite3_bind_text(stmt, 2, input.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, expectedOutput.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 4, isHidden ? 1 : 0);

    rc = sqlite3_step(stmt);
    int rowId = -1;
    if (rc == SQLITE_DONE) {
        rowId = static_cast<int>(sqlite3_last_insert_rowid(db_));
    } else {
        std::cerr << "DB insert error: " << sqlite3_errmsg(db_) << std::endl;
    }

    sqlite3_finalize(stmt);
    return rowId;
}

bool Database::updateTestCase(int id, const std::string& input,
                              const std::string& expectedOutput, bool isHidden) {
    std::lock_guard<std::mutex> lock(mutex_);
    const char* sql = "UPDATE test_cases SET input = ?, expected_output = ?, is_hidden = ? WHERE id = ?";

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "DB prepare error: " << sqlite3_errmsg(db_) << std::endl;
        return false;
    }

    sqlite3_bind_text(stmt, 1, input.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, expectedOutput.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 3, isHidden ? 1 : 0);
    sqlite3_bind_int(stmt, 4, id);

    rc = sqlite3_step(stmt);
    bool ok = (rc == SQLITE_DONE);
    if (!ok) {
        std::cerr << "DB update error: " << sqlite3_errmsg(db_) << std::endl;
    }

    sqlite3_finalize(stmt);
    return ok;
}

bool Database::deleteTestCase(int id) {
    std::lock_guard<std::mutex> lock(mutex_);
    const char* sql = "DELETE FROM test_cases WHERE id = ?";

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) return false;

    sqlite3_bind_int(stmt, 1, id);
    rc = sqlite3_step(stmt);
    bool ok = (rc == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return ok;
}

std::vector<TestCase> Database::getTestCases(int questionId) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<TestCase> results;

    const char* sql = "SELECT id, question_id, input, expected_output, is_hidden FROM test_cases WHERE question_id = ?";
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "DB prepare error: " << sqlite3_errmsg(db_) << std::endl;
        return results;
    }

    sqlite3_bind_int(stmt, 1, questionId);

    auto getText = [](sqlite3_stmt* st, int col) -> std::string {
        auto* p = sqlite3_column_text(st, col);
        return p ? reinterpret_cast<const char*>(p) : "";
    };

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        TestCase t;
        t.id             = sqlite3_column_int(stmt, 0);
        t.questionId     = sqlite3_column_int(stmt, 1);
        t.input          = getText(stmt, 2);
        t.expectedOutput = getText(stmt, 3);
        t.isHidden       = sqlite3_column_int(stmt, 4) != 0;
        results.push_back(std::move(t));
    }

    sqlite3_finalize(stmt);
    return results;
}

int Database::addAdmin(const std::string& email, const std::string& passwordHash, const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);

    const char* sql = "INSERT INTO admins (email, password_hash, name) VALUES (?, ?, ?)";

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "DB prepare error: " << sqlite3_errmsg(db_) << std::endl;
        return -1;
    }

    sqlite3_bind_text(stmt, 1, email.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, passwordHash.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, name.c_str(), -1, SQLITE_TRANSIENT);

    rc = sqlite3_step(stmt);
    int rowId = -1;
    if (rc == SQLITE_DONE) {
        rowId = static_cast<int>(sqlite3_last_insert_rowid(db_));
    } else {
        std::cerr << "DB insert error: " << sqlite3_errmsg(db_) << std::endl;
    }

    sqlite3_finalize(stmt);
    return rowId;
}

Admin Database::getAdminByEmail(const std::string& email) {
    std::lock_guard<std::mutex> lock(mutex_);
    Admin admin;

    const char* sql = "SELECT id, email, password_hash, name, created_at FROM admins WHERE email = ?";
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "DB prepare error: " << sqlite3_errmsg(db_) << std::endl;
        return admin;
    }

    sqlite3_bind_text(stmt, 1, email.c_str(), -1, SQLITE_TRANSIENT);

    auto getText = [](sqlite3_stmt* st, int col) -> std::string {
        auto* p = sqlite3_column_text(st, col);
        return p ? reinterpret_cast<const char*>(p) : "";
    };

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        admin.id = sqlite3_column_int(stmt, 0);
        admin.email = getText(stmt, 1);
        admin.passwordHash = getText(stmt, 2);
        admin.name = getText(stmt, 3);
        admin.createdAt = getText(stmt, 4);
    }

    sqlite3_finalize(stmt);
    return admin;
}

int Database::addUser(const std::string& email, const std::string& passwordHash, const std::string& name,
                      const std::string& verificationToken, int verificationExpiresAt) {
    std::lock_guard<std::mutex> lock(mutex_);

    const char* sql = "INSERT INTO users (email, password_hash, name, is_verified, verification_token, verification_expires_at) VALUES (?, ?, ?, 0, ?, ?)";

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "DB prepare error: " << sqlite3_errmsg(db_) << std::endl;
        return -1;
    }

    sqlite3_bind_text(stmt, 1, email.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, passwordHash.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, verificationToken.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 5, verificationExpiresAt);

    rc = sqlite3_step(stmt);
    int rowId = -1;
    if (rc == SQLITE_DONE) {
        rowId = static_cast<int>(sqlite3_last_insert_rowid(db_));
    } else {
        std::cerr << "DB insert error: " << sqlite3_errmsg(db_) << std::endl;
    }

    sqlite3_finalize(stmt);
    return rowId;
}

User Database::getUserByEmail(const std::string& email) {
    std::lock_guard<std::mutex> lock(mutex_);
    User user;

    const char* sql = "SELECT id, email, password_hash, name, created_at, is_verified, verification_token, verification_expires_at, reset_otp, reset_otp_expires_at FROM users WHERE email = ?";
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "DB prepare error: " << sqlite3_errmsg(db_) << std::endl;
        return user;
    }

    sqlite3_bind_text(stmt, 1, email.c_str(), -1, SQLITE_TRANSIENT);

    auto getText = [](sqlite3_stmt* st, int col) -> std::string {
        auto* p = sqlite3_column_text(st, col);
        return p ? reinterpret_cast<const char*>(p) : "";
    };

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        user.id = sqlite3_column_int(stmt, 0);
        user.email = getText(stmt, 1);
        user.passwordHash = getText(stmt, 2);
        user.name = getText(stmt, 3);
        user.createdAt = getText(stmt, 4);
        user.isVerified = sqlite3_column_int(stmt, 5) != 0;
        user.verificationToken = getText(stmt, 6);
        user.verificationExpiresAt = sqlite3_column_int(stmt, 7);
        user.resetOtp = getText(stmt, 8);
        user.resetOtpExpiresAt = sqlite3_column_int(stmt, 9);
    }

    sqlite3_finalize(stmt);
    return user;
}

Question Database::getQuestionById(int id) {
    std::lock_guard<std::mutex> lock(mutex_);
    Question q;

    const char* sql = "SELECT id, title, description, category, difficulty, published, created_at FROM questions WHERE id = ?";
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "DB prepare error: " << sqlite3_errmsg(db_) << std::endl;
        return q;
    }

    sqlite3_bind_int(stmt, 1, id);

    auto getText = [](sqlite3_stmt* st, int col) -> std::string {
        auto* p = sqlite3_column_text(st, col);
        return p ? reinterpret_cast<const char*>(p) : "";
    };

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        q.id          = sqlite3_column_int(stmt, 0);
        q.title       = getText(stmt, 1);
        q.description = getText(stmt, 2);
        q.category    = getText(stmt, 3);
        q.difficulty  = getText(stmt, 4);
        q.published   = sqlite3_column_int(stmt, 5) != 0;
        q.createdAt   = getText(stmt, 6);
    }

    sqlite3_finalize(stmt);
    return q;
}

std::vector<Submission> Database::getSubmissionsByUser(int userId, int limit) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<Submission> results;

    if (limit <= 0) limit = 20;
    if (limit > 100) limit = 100;

    const char* sql = "SELECT id, user_id, question_id, language, code, input, stdout, stderr, exit_code, timed_out, created_at "
                      "FROM submissions WHERE user_id = ? ORDER BY id DESC LIMIT ?";

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "DB prepare error: " << sqlite3_errmsg(db_) << std::endl;
        return results;
    }

    sqlite3_bind_int(stmt, 1, userId);
    sqlite3_bind_int(stmt, 2, limit);

    auto getText = [](sqlite3_stmt* st, int col) -> std::string {
        auto* p = sqlite3_column_text(st, col);
        return p ? reinterpret_cast<const char*>(p) : "";
    };

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        Submission s;
        s.id          = sqlite3_column_int(stmt, 0);
        s.userId      = sqlite3_column_int(stmt, 1);
        s.questionId  = sqlite3_column_int(stmt, 2);
        s.language    = getText(stmt, 3);
        s.code        = getText(stmt, 4);
        s.input       = getText(stmt, 5);
        s.stdoutStr   = getText(stmt, 6);
        s.stderrStr   = getText(stmt, 7);
        s.exitCode    = sqlite3_column_int(stmt, 8);
        s.timedOut    = sqlite3_column_int(stmt, 9) != 0;
        s.createdAt   = getText(stmt, 10);
        results.push_back(std::move(s));
    }

    sqlite3_finalize(stmt);
    return results;
}

StudentStats Database::getStudentStats(int userId) {
    std::lock_guard<std::mutex> lock(mutex_);
    StudentStats stats;

    const char* sql = "SELECT COUNT(*), COUNT(DISTINCT CASE WHEN question_id > 0 THEN question_id END) FROM submissions WHERE user_id = ?";
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "DB prepare error: " << sqlite3_errmsg(db_) << std::endl;
        return stats;
    }

    sqlite3_bind_int(stmt, 1, userId);

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        stats.totalSubmissions = sqlite3_column_int(stmt, 0);
        stats.questionsAttempted = sqlite3_column_int(stmt, 1);
    }

    sqlite3_finalize(stmt);
    return stats;
}

User Database::getUserByVerificationToken(const std::string& token) {
    std::lock_guard<std::mutex> lock(mutex_);
    User user;

    const char* sql = "SELECT id, email, password_hash, name, created_at, is_verified, verification_token, verification_expires_at, reset_otp, reset_otp_expires_at FROM users WHERE verification_token = ?";
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "DB prepare error: " << sqlite3_errmsg(db_) << std::endl;
        return user;
    }

    sqlite3_bind_text(stmt, 1, token.c_str(), -1, SQLITE_TRANSIENT);

    auto getText = [](sqlite3_stmt* st, int col) -> std::string {
        auto* p = sqlite3_column_text(st, col);
        return p ? reinterpret_cast<const char*>(p) : "";
    };

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        user.id = sqlite3_column_int(stmt, 0);
        user.email = getText(stmt, 1);
        user.passwordHash = getText(stmt, 2);
        user.name = getText(stmt, 3);
        user.createdAt = getText(stmt, 4);
        user.isVerified = sqlite3_column_int(stmt, 5) != 0;
        user.verificationToken = getText(stmt, 6);
        user.verificationExpiresAt = sqlite3_column_int(stmt, 7);
        user.resetOtp = getText(stmt, 8);
        user.resetOtpExpiresAt = sqlite3_column_int(stmt, 9);
    }

    sqlite3_finalize(stmt);
    return user;
}

bool Database::verifyUser(int userId) {
    std::lock_guard<std::mutex> lock(mutex_);
    const char* sql = "UPDATE users SET is_verified = 1, verification_token = '', verification_expires_at = 0 WHERE id = ?";
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "DB prepare error: " << sqlite3_errmsg(db_) << std::endl;
        return false;
    }
    sqlite3_bind_int(stmt, 1, userId);
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return rc == SQLITE_DONE;
}

bool Database::setUserVerificationToken(int userId, const std::string& token, int expiresAt) {
    std::lock_guard<std::mutex> lock(mutex_);
    const char* sql = "UPDATE users SET verification_token = ?, verification_expires_at = ?, is_verified = 0 WHERE id = ?";
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "DB prepare error: " << sqlite3_errmsg(db_) << std::endl;
        return false;
    }
    sqlite3_bind_text(stmt, 1, token.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, expiresAt);
    sqlite3_bind_int(stmt, 3, userId);
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return rc == SQLITE_DONE;
}

bool Database::setUserResetOtp(int userId, const std::string& otp, int expiresAt) {
    std::lock_guard<std::mutex> lock(mutex_);
    const char* sql = "UPDATE users SET reset_otp = ?, reset_otp_expires_at = ? WHERE id = ?";
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "DB prepare error: " << sqlite3_errmsg(db_) << std::endl;
        return false;
    }
    sqlite3_bind_text(stmt, 1, otp.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, expiresAt);
    sqlite3_bind_int(stmt, 3, userId);
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return rc == SQLITE_DONE;
}

bool Database::clearUserResetOtp(int userId) {
    std::lock_guard<std::mutex> lock(mutex_);
    const char* sql = "UPDATE users SET reset_otp = '', reset_otp_expires_at = 0 WHERE id = ?";
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "DB prepare error: " << sqlite3_errmsg(db_) << std::endl;
        return false;
    }
    sqlite3_bind_int(stmt, 1, userId);
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return rc == SQLITE_DONE;
}

bool Database::updateUserPassword(int userId, const std::string& newHash) {
    std::lock_guard<std::mutex> lock(mutex_);
    const char* sql = "UPDATE users SET password_hash = ? WHERE id = ?";
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "DB prepare error: " << sqlite3_errmsg(db_) << std::endl;
        return false;
    }
    sqlite3_bind_text(stmt, 1, newHash.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, userId);
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return rc == SQLITE_DONE;
}
