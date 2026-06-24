/* Metis Code Analyzer Plus - authentication
 * Self-contained SHA-256 and an in-memory session store with expiry.
 * No external dependencies. 7-bit ASCII, C++20.
 */
#ifndef METIS_CODE_ANALYZER_AUTH_HPP
#define METIS_CODE_ANALYZER_AUTH_HPP

#include <array>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <map>
#include <mutex>
#include <random>
#include <string>

namespace metis::codeanalyzer {

/* ---- SHA-256 (FIPS 180-4) ---- */
class Sha256 {
public:
    static std::string hex(const std::string& input) {
        Sha256 h;
        h.update(reinterpret_cast<const std::uint8_t*>(input.data()), input.size());
        std::array<std::uint8_t, 32> digest = h.finalize();
        static const char* d = "0123456789abcdef";
        std::string out;
        out.reserve(64);
        for (std::uint8_t b : digest) {
            out.push_back(d[(b >> 4) & 0xF]);
            out.push_back(d[b & 0xF]);
        }
        return out;
    }

private:
    void update(const std::uint8_t* data, std::size_t len) {
        for (std::size_t i = 0; i < len; ++i) {
            buffer_[buffer_len_++] = data[i];
            if (buffer_len_ == 64) { transform(buffer_.data()); bit_len_ += 512; buffer_len_ = 0; }
        }
    }

    std::array<std::uint8_t, 32> finalize() {
        std::uint64_t total_bits = bit_len_ + static_cast<std::uint64_t>(buffer_len_) * 8;
        buffer_[buffer_len_++] = 0x80;
        if (buffer_len_ > 56) {
            while (buffer_len_ < 64) buffer_[buffer_len_++] = 0;
            transform(buffer_.data());
            buffer_len_ = 0;
        }
        while (buffer_len_ < 56) buffer_[buffer_len_++] = 0;
        for (int i = 7; i >= 0; --i)
            buffer_[buffer_len_++] = static_cast<std::uint8_t>((total_bits >> (i * 8)) & 0xFF);
        transform(buffer_.data());

        std::array<std::uint8_t, 32> out{};
        for (int i = 0; i < 8; ++i) {
            out[static_cast<std::size_t>(i) * 4 + 0] = static_cast<std::uint8_t>((state_[i] >> 24) & 0xFF);
            out[static_cast<std::size_t>(i) * 4 + 1] = static_cast<std::uint8_t>((state_[i] >> 16) & 0xFF);
            out[static_cast<std::size_t>(i) * 4 + 2] = static_cast<std::uint8_t>((state_[i] >> 8) & 0xFF);
            out[static_cast<std::size_t>(i) * 4 + 3] = static_cast<std::uint8_t>(state_[i] & 0xFF);
        }
        return out;
    }

    static std::uint32_t rotr(std::uint32_t x, std::uint32_t n) {
        return (x >> n) | (x << (32 - n));
    }

    void transform(const std::uint8_t* chunk) {
        static const std::uint32_t k[64] = {
            0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
            0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
            0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
            0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
            0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
            0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
            0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
            0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2};

        std::uint32_t w[64];
        for (int i = 0; i < 16; ++i) {
            w[i] = (static_cast<std::uint32_t>(chunk[i * 4]) << 24) |
                   (static_cast<std::uint32_t>(chunk[i * 4 + 1]) << 16) |
                   (static_cast<std::uint32_t>(chunk[i * 4 + 2]) << 8) |
                   (static_cast<std::uint32_t>(chunk[i * 4 + 3]));
        }
        for (int i = 16; i < 64; ++i) {
            std::uint32_t s0 = rotr(w[i-15], 7) ^ rotr(w[i-15], 18) ^ (w[i-15] >> 3);
            std::uint32_t s1 = rotr(w[i-2], 17) ^ rotr(w[i-2], 19) ^ (w[i-2] >> 10);
            w[i] = w[i-16] + s0 + w[i-7] + s1;
        }
        std::uint32_t a = state_[0], b = state_[1], c = state_[2], d = state_[3];
        std::uint32_t e = state_[4], f = state_[5], g = state_[6], h = state_[7];
        for (int i = 0; i < 64; ++i) {
            std::uint32_t S1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
            std::uint32_t ch = (e & f) ^ (~e & g);
            std::uint32_t t1 = h + S1 + ch + k[i] + w[i];
            std::uint32_t S0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
            std::uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
            std::uint32_t t2 = S0 + maj;
            h = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
        }
        state_[0]+=a; state_[1]+=b; state_[2]+=c; state_[3]+=d;
        state_[4]+=e; state_[5]+=f; state_[6]+=g; state_[7]+=h;
    }

    std::array<std::uint8_t, 64> buffer_{};
    std::size_t buffer_len_ = 0;
    std::uint64_t bit_len_ = 0;
    std::uint32_t state_[8] = {
        0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,
        0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19};
};

/* ---- Session store ---- */
class SessionStore {
public:
    explicit SessionStore(int ttl_minutes, int token_length = 48)
        : ttl_seconds_(ttl_minutes * 60),
          token_length_(token_length > 0 ? token_length : 48) {}

    std::string create(const std::string& user) {
        std::lock_guard<std::mutex> lk(mtx_);
        std::string token = make_token(token_length_);
        sessions_[token] = Session{user, now() + ttl_seconds_};
        return token;
    }

    bool valid(const std::string& token, std::string& user_out) {
        if (token.empty()) return false;
        std::lock_guard<std::mutex> lk(mtx_);
        auto it = sessions_.find(token);
        if (it == sessions_.end()) return false;
        if (now() > it->second.expires_at) { sessions_.erase(it); return false; }
        it->second.expires_at = now() + ttl_seconds_;   /* sliding expiry */
        user_out = it->second.user;
        return true;
    }

    void destroy(const std::string& token) {
        std::lock_guard<std::mutex> lk(mtx_);
        sessions_.erase(token);
    }

    std::size_t active() {
        std::lock_guard<std::mutex> lk(mtx_);
        return sessions_.size();
    }

private:
    struct Session { std::string user; std::int64_t expires_at; };

    static std::int64_t now() {
        using namespace std::chrono;
        return duration_cast<seconds>(system_clock::now().time_since_epoch()).count();
    }

    static std::string make_token(int length) {
        std::random_device rd;
        std::mt19937_64 gen(((static_cast<std::uint64_t>(rd()) << 32) ^ rd()) ^
                            static_cast<std::uint64_t>(now()));
        std::uniform_int_distribution<int> dist(0, 15);
        static const char* d = "0123456789abcdef";
        std::string t;
        t.reserve(static_cast<std::size_t>(length));
        for (int i = 0; i < length; ++i) t.push_back(d[dist(gen)]);
        return t;
    }

    std::mutex mtx_;
    std::map<std::string, Session> sessions_;
    std::int64_t ttl_seconds_;
    int token_length_;
};

} /* namespace metis::codeanalyzer */

#endif /* METIS_CODE_ANALYZER_AUTH_HPP */
