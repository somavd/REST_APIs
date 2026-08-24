-- schema.sql — Complete database schema for Online Compiler
-- Safe to run on an existing database: uses IF NOT EXISTS everywhere.
-- This file mirrors the schema created by database.cpp::init().
-- For reference only — the C++ server creates tables automatically on startup.

-- Code execution history
CREATE TABLE IF NOT EXISTS submissions (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
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

-- Coding questions managed by admin
CREATE TABLE IF NOT EXISTS questions (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    title       TEXT    NOT NULL,
    description TEXT    DEFAULT '',
    category    TEXT    DEFAULT '',
    difficulty  TEXT    DEFAULT '',
    created_at  TEXT    DEFAULT CURRENT_TIMESTAMP
);

-- Test cases per question
CREATE TABLE IF NOT EXISTS test_cases (
    id              INTEGER PRIMARY KEY AUTOINCREMENT,
    question_id     INTEGER NOT NULL,
    input           TEXT    DEFAULT '',
    expected_output TEXT    DEFAULT '',
    is_hidden       INTEGER DEFAULT 0,
    FOREIGN KEY (question_id) REFERENCES questions(id) ON DELETE CASCADE
);
CREATE INDEX IF NOT EXISTS idx_question_id ON test_cases(question_id);

-- Admin accounts
CREATE TABLE IF NOT EXISTS admins (
    id             INTEGER PRIMARY KEY AUTOINCREMENT,
    email          TEXT    UNIQUE NOT NULL,
    password_hash  TEXT    NOT NULL,
    name           TEXT    NOT NULL,
    created_at     TEXT    DEFAULT CURRENT_TIMESTAMP
);
CREATE INDEX IF NOT EXISTS idx_admins_email ON admins(email);
