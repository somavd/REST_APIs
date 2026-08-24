#include "auth.hpp"

#include <array>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <random>
#include <sstream>
#include <string>
#include <vector>

namespace {

// SHA-256 implementation in plain C++17.
class Sha256 {
public:
    std::array<uint8_t, 32> hash(const std::string& data) {
        reset();
        update(reinterpret_cast<const uint8_t*>(data.data()), data.size());
        return finalize();
    }

private:
    static constexpr uint32_t K[64] = {
        0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u, 0x3956c25bu, 0x59f111f1u, 0x923f82a4u, 0xab1c5ed5u,
        0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u, 0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u,
        0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu, 0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
        0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u, 0xc6e00bf3u, 0xd5a79147u, 0x06ca6351u, 0x14292967u,
        0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u, 0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u,
        0xa2bfe8a1u, 0xa81a664bu, 0xc24b8b70u, 0xc76c51a3u, 0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
        0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u, 0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu, 0x682e6ff3u,
        0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u, 0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u
    };

    std::vector<uint8_t> buffer_;
    uint64_t bitLength_ = 0;
    uint32_t state_[8] = {0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u, 0xa54ff53au, 0x510e527fu, 0x9b05688cu, 0x1f83d9abu, 0x5be0cd19u};

    void reset() {
        buffer_.clear();
        bitLength_ = 0;
        state_[0] = 0x6a09e667u;
        state_[1] = 0xbb67ae85u;
        state_[2] = 0x3c6ef372u;
        state_[3] = 0xa54ff53au;
        state_[4] = 0x510e527fu;
        state_[5] = 0x9b05688cu;
        state_[6] = 0x1f83d9abu;
        state_[7] = 0x5be0cd19u;
    }

    static uint32_t rotr(uint32_t x, uint32_t n) { return (x >> n) | (x << (32u - n)); }

    void processBlock(const uint8_t* block) {
        uint32_t w[64];
        for (int i = 0; i < 16; ++i) {
            w[i] = (static_cast<uint32_t>(block[i * 4]) << 24) |
                   (static_cast<uint32_t>(block[i * 4 + 1]) << 16) |
                   (static_cast<uint32_t>(block[i * 4 + 2]) << 8) |
                   (static_cast<uint32_t>(block[i * 4 + 3]));
        }
        for (int i = 16; i < 64; ++i) {
            uint32_t s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
            uint32_t s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
            w[i] = w[i - 16] + s0 + w[i - 7] + s1;
        }

        uint32_t a = state_[0], b = state_[1], c = state_[2], d = state_[3];
        uint32_t e = state_[4], f = state_[5], g = state_[6], h = state_[7];

        for (int i = 0; i < 64; ++i) {
            uint32_t S1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
            uint32_t ch = (e & f) ^ (~e & g);
            uint32_t temp1 = h + S1 + ch + K[i] + w[i];
            uint32_t S0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
            uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
            uint32_t temp2 = S0 + maj;

            h = g; g = f; f = e; e = d + temp1;
            d = c; c = b; b = a; a = temp1 + temp2;
        }

        state_[0] += a; state_[1] += b; state_[2] += c; state_[3] += d;
        state_[4] += e; state_[5] += f; state_[6] += g; state_[7] += h;
    }

    void update(const uint8_t* data, size_t len) {
        bitLength_ += len * 8;
        buffer_.insert(buffer_.end(), data, data + len);

        while (buffer_.size() >= 64) {
            processBlock(buffer_.data());
            buffer_.erase(buffer_.begin(), buffer_.begin() + 64);
        }
    }

    std::array<uint8_t, 32> finalize() {
        uint64_t originalBitLength = bitLength_;

        buffer_.push_back(0x80);
        while ((buffer_.size() % 64) != 56) {
            buffer_.push_back(0);
        }

        for (int i = 7; i >= 0; --i) {
            buffer_.push_back(static_cast<uint8_t>((originalBitLength >> (i * 8)) & 0xffu));
        }

        for (size_t offset = 0; offset < buffer_.size(); offset += 64) {
            processBlock(buffer_.data() + offset);
        }

        std::array<uint8_t, 32> out;
        for (int i = 0; i < 8; ++i) {
            out[i * 4] = static_cast<uint8_t>((state_[i] >> 24) & 0xffu);
            out[i * 4 + 1] = static_cast<uint8_t>((state_[i] >> 16) & 0xffu);
            out[i * 4 + 2] = static_cast<uint8_t>((state_[i] >> 8) & 0xffu);
            out[i * 4 + 3] = static_cast<uint8_t>(state_[i] & 0xffu);
        }
        return out;
    }
};

std::string toHex(const std::array<uint8_t, 32>& digest) {
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (auto b : digest) {
        oss << std::setw(2) << static_cast<int>(b);
    }
    return oss.str();
}

std::vector<uint8_t> randomBytes(size_t n) {
    std::vector<uint8_t> out(n);
    std::ifstream urandom("/dev/urandom", std::ios::binary);
    if (urandom) {
        urandom.read(reinterpret_cast<char*>(out.data()), static_cast<std::streamsize>(n));
        if (urandom.gcount() == static_cast<std::streamsize>(n)) {
            return out;
        }
    }
    std::random_device rd;
    std::mt19937_64 gen(rd());
    std::uniform_int_distribution<unsigned int> dist(0, 255);
    for (auto& b : out) {
        b = static_cast<uint8_t>(dist(gen));
    }
    return out;
}

std::string bytesToHex(const std::vector<uint8_t>& bytes) {
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (auto b : bytes) {
        oss << std::setw(2) << static_cast<int>(b);
    }
    return oss.str();
}

} // namespace

std::string Auth::generateToken() {
    return bytesToHex(randomBytes(32));
}

std::string Auth::generateNumericCode(int digits) {
    std::random_device rd;
    std::mt19937_64 gen(rd());
    std::uniform_int_distribution<int> dist(0, 9);
    std::string code;
    code.reserve(digits);
    for (int i = 0; i < digits; ++i) {
        code += std::to_string(dist(gen));
    }
    return code;
}

std::string Auth::hashPassword(const std::string& password) {
    std::string salt = bytesToHex(randomBytes(16));

    Sha256 hasher;
    std::string current = password + salt;
    for (int i = 0; i < 100000; ++i) {
        current = std::string(reinterpret_cast<const char*>(hasher.hash(current).data()), 32) + salt;
    }

    return salt + ":" + toHex(hasher.hash(current));
}

bool Auth::verifyPassword(const std::string& password, const std::string& stored) {
    if (stored.size() != 32 + 1 + 64 || stored[32] != ':') {
        return false;
    }
    std::string salt = stored.substr(0, 32);

    Sha256 hasher;
    std::string current = password + salt;
    for (int i = 0; i < 100000; ++i) {
        current = std::string(reinterpret_cast<const char*>(hasher.hash(current).data()), 32) + salt;
    }

    return stored == salt + ":" + toHex(hasher.hash(current));
}

std::string Auth::createSession(int userId, const std::string& email, const std::string& name, int ttlSeconds) {
    std::string token = generateToken();
    Session session;
    session.userId = userId;
    session.email = email;
    session.name = name;
    session.expiresAt = std::chrono::steady_clock::now() + std::chrono::seconds(ttlSeconds);

    std::lock_guard<std::mutex> lock(mutex_);
    sessions_[token] = std::move(session);
    return token;
}

const Session* Auth::validateSession(const std::string& token) {
    if (token.empty()) return nullptr;

    std::lock_guard<std::mutex> lock(mutex_);
    purgeExpired();

    auto it = sessions_.find(token);
    if (it == sessions_.end()) return nullptr;
    if (it->second.expiresAt < std::chrono::steady_clock::now()) {
        sessions_.erase(it);
        return nullptr;
    }
    return &it->second;
}

void Auth::destroySession(const std::string& token) {
    std::lock_guard<std::mutex> lock(mutex_);
    sessions_.erase(token);
}

bool Auth::constantTimeEquals(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return false;
    volatile unsigned char result = 0;
    for (size_t i = 0; i < a.size(); ++i) {
        result |= static_cast<unsigned char>(a[i]) ^ static_cast<unsigned char>(b[i]);
    }
    return result == 0;
}

std::string Auth::getCookieValue(const std::string& cookieHeader, const std::string& name) {
    if (cookieHeader.empty()) return "";

    std::string search = name + "=";
    size_t start = cookieHeader.find(search);
    while (start != std::string::npos) {
        if (start == 0 || cookieHeader[start - 1] == ' ' || cookieHeader[start - 1] == ';') {
            size_t end = cookieHeader.find(";", start);
            size_t len = (end == std::string::npos) ? std::string::npos : end - start - search.length();
            std::string value = cookieHeader.substr(start + search.length(), len);
            size_t first = value.find_first_not_of(" \t");
            if (first == std::string::npos) return "";
            size_t last = value.find_last_not_of(" \t");
            return value.substr(first, last - first + 1);
        }
        start = cookieHeader.find(search, start + 1);
    }
    return "";
}

void Auth::purgeExpired() {
    auto now = std::chrono::steady_clock::now();
    for (auto it = sessions_.begin(); it != sessions_.end();) {
        if (it->second.expiresAt < now) {
            it = sessions_.erase(it);
        } else {
            ++it;
        }
    }
}
