/* Metis Code Analyzer Plus - optional AI summary client.
 *
 * Performs an outbound HTTPS POST to an operator-configured, OpenAI-compatible
 * chat/completions endpoint and returns the raw provider response. All settings
 * come from the [ai] section of the PSON config; nothing is hardcoded.
 *
 * This path requires a build with METIS_ENABLE_TLS (it reuses OpenSSL for the
 * outbound TLS connection). When built without TLS, calls return a clear
 * "unavailable" result instead of failing to compile.
 *
 * 7-bit ASCII, C++20.
 */
#ifndef METIS_CODE_ANALYZER_AI_HPP
#define METIS_CODE_ANALYZER_AI_HPP

#include <string>

#ifdef METIS_ENABLE_TLS
#  include <openssl/ssl.h>
#  include <openssl/bio.h>
#  include <openssl/err.h>
#  ifndef _WIN32
#    include <sys/socket.h>
#    include <sys/time.h>
#  endif
#endif

namespace metis::codeanalyzer::ai {

struct AiOptions {
    bool enabled = false;
    std::string provider;
    std::string endpoint;          /* full https URL */
    std::string model;
    std::string api_key;
    std::string system_prompt;
    int timeout_seconds = 30;
    int max_output_tokens = 1024;
};

struct Result {
    bool ok = false;
    int status = 0;
    std::string body;              /* raw provider response body */
    std::string error;
};

/* JSON string escape for embedding prompt text in the request body. */
inline std::string json_escape(const std::string& s) {
    std::string o;
    o.reserve(s.size() + 16);
    for (char c : s) {
        switch (c) {
            case '"':  o += "\\\""; break;
            case '\\': o += "\\\\"; break;
            case '\n': o += "\\n"; break;
            case '\r': o += "\\r"; break;
            case '\t': o += "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    static const char* hex = "0123456789abcdef";
                    o += "\\u00";
                    o += hex[(c >> 4) & 0xF];
                    o += hex[c & 0xF];
                } else {
                    o += c;
                }
        }
    }
    return o;
}

/* Build an OpenAI-compatible chat/completions request body. */
inline std::string build_body(const AiOptions& o, const std::string& user_text) {
    std::string b = "{";
    b += "\"model\":\"" + json_escape(o.model) + "\",";
    b += "\"max_tokens\":" + std::to_string(o.max_output_tokens) + ",";
    b += "\"messages\":[";
    b += "{\"role\":\"system\",\"content\":\"" + json_escape(o.system_prompt) + "\"},";
    b += "{\"role\":\"user\",\"content\":\"" + json_escape(user_text) + "\"}";
    b += "]}";
    return b;
}

#ifdef METIS_ENABLE_TLS

inline bool parse_https_url(const std::string& url, std::string& host,
                            std::string& port, std::string& path) {
    const std::string pfx = "https://";
    if (url.size() <= pfx.size() || url.compare(0, pfx.size(), pfx) != 0) return false;
    std::string rest = url.substr(pfx.size());
    std::string::size_type slash = rest.find('/');
    std::string hostport = (slash == std::string::npos) ? rest : rest.substr(0, slash);
    path = (slash == std::string::npos) ? "/" : rest.substr(slash);
    std::string::size_type colon = hostport.find(':');
    if (colon == std::string::npos) { host = hostport; port = "443"; }
    else { host = hostport.substr(0, colon); port = hostport.substr(colon + 1); }
    return !host.empty();
}

inline Result summarize(const AiOptions& o, const std::string& user_text) {
    Result r;
    if (!o.enabled) { r.error = "AI is disabled (set ai.enabled = true)."; return r; }
    if (o.endpoint.empty() || o.model.empty() || o.api_key.empty()) {
        r.error = "AI is not fully configured (ai.endpoint, ai.model, ai.api_key)."; return r;
    }
    std::string host, port, path;
    if (!parse_https_url(o.endpoint, host, port, path)) {
        r.error = "ai.endpoint must be an https:// URL."; return r;
    }

    SSL_CTX* ctx = SSL_CTX_new(TLS_client_method());
    if (!ctx) { r.error = "TLS context creation failed."; return r; }
    SSL_CTX_set_default_verify_paths(ctx);
    SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, nullptr);

    BIO* bio = BIO_new_ssl_connect(ctx);
    if (!bio) { SSL_CTX_free(ctx); r.error = "BIO creation failed."; return r; }
    SSL* ssl = nullptr;
    BIO_get_ssl(bio, &ssl);
    if (!ssl) { BIO_free_all(bio); SSL_CTX_free(ctx); r.error = "SSL handle unavailable."; return r; }
    SSL_set_tlsext_host_name(ssl, host.c_str());
    /* SSL_set1_host is deprecated in OpenSSL 4.0 but still functional.
     * Suppress the deprecation warning until a replacement API is adopted. */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    SSL_set1_host(ssl, host.c_str());
#pragma GCC diagnostic pop
    BIO_set_conn_hostname(bio, (host + ":" + port).c_str());

    if (BIO_do_connect(bio) <= 0 || BIO_do_handshake(bio) <= 0) {
        BIO_free_all(bio); SSL_CTX_free(ctx);
        r.error = "TLS connection or handshake to provider failed.";
        return r;
    }

    /* Apply the configured timeout to the underlying socket. */
    if (o.timeout_seconds > 0) {
        int fd = -1;
        BIO_get_fd(bio, &fd);
        if (fd >= 0) {
#ifdef _WIN32
            DWORD ms = static_cast<DWORD>(o.timeout_seconds) * 1000;
            setsockopt(static_cast<SOCKET>(fd), SOL_SOCKET, SO_RCVTIMEO,
                       reinterpret_cast<const char*>(&ms), sizeof(ms));
            setsockopt(static_cast<SOCKET>(fd), SOL_SOCKET, SO_SNDTIMEO,
                       reinterpret_cast<const char*>(&ms), sizeof(ms));
#else
            struct timeval tv; tv.tv_sec = o.timeout_seconds; tv.tv_usec = 0;
            setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
            setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
#endif
        }
    }

    std::string body = build_body(o, user_text);
    std::string req = "POST " + path + " HTTP/1.1\r\n";
    req += "Host: " + host + "\r\n";
    req += "Authorization: Bearer " + o.api_key + "\r\n";
    req += "Content-Type: application/json\r\n";
    req += "Content-Length: " + std::to_string(body.size()) + "\r\n";
    req += "Connection: close\r\n\r\n";
    req += body;

    if (BIO_write(bio, req.data(), static_cast<int>(req.size())) <= 0) {
        BIO_free_all(bio); SSL_CTX_free(ctx);
        r.error = "Failed to send request to provider.";
        return r;
    }

    std::string raw;
    char buf[4096];
    for (;;) {
        int n = BIO_read(bio, buf, static_cast<int>(sizeof(buf)));
        if (n <= 0) break;
        raw.append(buf, static_cast<std::size_t>(n));
        if (raw.size() > (4u << 20)) break;   /* 4 MB ceiling */
    }
    BIO_free_all(bio);
    SSL_CTX_free(ctx);

    std::string::size_type he = raw.find("\r\n\r\n");
    std::string head = (he == std::string::npos) ? raw : raw.substr(0, he);
    r.body = (he == std::string::npos) ? std::string() : raw.substr(he + 4);
    std::string::size_type sp = head.find(' ');
    if (sp != std::string::npos) {
        try { r.status = std::stoi(head.substr(sp + 1, 3)); } catch (...) { r.status = 0; }
    }
    r.ok = (r.status >= 200 && r.status < 300);
    if (!r.ok && r.error.empty()) {
        r.error = "Provider returned HTTP " + std::to_string(r.status);
    }
    return r;
}

#else  /* no TLS build */

inline Result summarize(const AiOptions& o, const std::string&) {
    Result r;
    r.error = o.enabled
        ? "AI requires a TLS-enabled build (compile with METIS_ENABLE_TLS)."
        : "AI is disabled (set ai.enabled = true).";
    return r;
}

#endif

} /* namespace metis::codeanalyzer::ai */

#endif /* METIS_CODE_ANALYZER_AI_HPP */
