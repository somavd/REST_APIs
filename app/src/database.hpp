#pragma once

#include <string>
#include <vector>
#include <mutex>

struct sqlite3;

struct Question {
    int id = 0;
    std::string title;
    std::string description;
    std::string category;
    std::string difficulty;
    bool published = true;
    std::string createdAt;
};

struct TestCase {
    int id = 0;
    int questionId = 0;
    std::string input;
    std::string expectedOutput;
    bool isHidden = false;
};

struct Submission {
    int id = 0;
    int userId = 0;
    int questionId = 0;
    std::string language;
    std::string code;
    std::string input;
    std::string stdoutStr;
    std::string stderrStr;
    int exitCode = 0;
    bool timedOut = false;
    std::string createdAt;
};

struct Admin {
    int id = 0;
    std::string email;
    std::string passwordHash;
    std::string name;
    std::string createdAt;
};

struct User {
    int id = 0;
    std::string email;
    std::string passwordHash;
    std::string name;
    std::string createdAt;
    bool isVerified = false;
    std::string verificationToken;
    int verificationExpiresAt = 0;
    std::string resetOtp;
    int resetOtpExpiresAt = 0;
};

struct StudentStats {
    int totalSubmissions = 0;
    int questionsAttempted = 0;
};

class Database {
public:
    explicit Database(const std::string& path = "data/submissions.db");
    ~Database();

    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

    // Save a submission, returns the new row ID
    int saveSubmission(const Submission& sub);

    // Retrieve recent submissions
    std::vector<Submission> getSubmissions(int limit = 20, const std::string& language = "");

    // Question management
    int addQuestion(const std::string& title, const std::string& description,
                    const std::string& category, const std::string& difficulty,
                    bool published = true);
    bool updateQuestion(int id, const std::string& title, const std::string& description,
                        const std::string& category, const std::string& difficulty,
                        bool published = true);
    bool deleteQuestion(int id);
    bool deleteQuestionsBulk(const std::vector<int>& ids);
    std::vector<Question> getQuestions(int& total, int limit = 0, int offset = 0,
                                       const std::string& search = "",
                                       const std::string& category = "",
                                       const std::string& difficulty = "");
    Question getQuestionById(int id);

    // Test case management
    int addTestCase(int questionId, const std::string& input,
                    const std::string& expectedOutput, bool isHidden);
    bool updateTestCase(int id, const std::string& input,
                        const std::string& expectedOutput, bool isHidden);
    bool deleteTestCase(int id);
    std::vector<TestCase> getTestCases(int questionId);

    // Student management
    int addUser(const std::string& email, const std::string& passwordHash, const std::string& name,
                const std::string& verificationToken = "", int verificationExpiresAt = 0);
    User getUserByEmail(const std::string& email);
    User getUserByVerificationToken(const std::string& token);
    bool verifyUser(int userId);
    bool setUserVerificationToken(int userId, const std::string& token, int expiresAt);
    bool setUserResetOtp(int userId, const std::string& otp, int expiresAt);
    bool clearUserResetOtp(int userId);
    bool updateUserPassword(int userId, const std::string& newHash);
    std::vector<Submission> getSubmissionsByUser(int userId, int limit = 20);
    StudentStats getStudentStats(int userId);

    // Admin management
    int addAdmin(const std::string& email, const std::string& passwordHash, const std::string& name);
    Admin getAdminByEmail(const std::string& email);

private:
    void init();
    sqlite3* db_ = nullptr;
    std::mutex mutex_;
};
