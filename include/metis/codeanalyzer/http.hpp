/* Metis Code Analyzer Plus - minimal cross-platform HTTP/1.1 server
 * Zero external dependencies, 7-bit ASCII, C++20.
 * Static file serving is confined to the web root (no path traversal).
 */
#ifndef METIS_CODE_ANALYZER_HTTP_HPP
#define METIS_CODE_ANALYZER_HTTP_HPP

#include <atomic>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <map>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#  include <winsock2.h>
#  include <ws2tcpip.h>
#  ifdef _MSC_VER
#    pragma comment(lib, "Ws2_32.lib")
#  endif
using socket_t = SOCKET;
static constexpr socket_t kInvalidSocket = INVALID_SOCKET;
#else
#  include <arpa/inet.h>
#  include <netinet/in.h>
#  include <sys/socket.h>
#  include <unistd.h>
using socket_t = int;
static constexpr socket_t kInvalidSocket = -1;
#endif

#ifdef METIS_ENABLE_TLS
#  include <openssl/ssl.h>
#  include <openssl/err.h>
#  include <openssl/x509.h>
#  include <openssl/x509v3.h>
#  include <openssl/pem.h>
#  include <openssl/evp.h>
#endif

namespace metis::codeanalyzer::http {

struct Request {
    std::string method;
    std::string path;
    std::string query;
    std::string body;
    std::map<std::string, std::string> headers;
    bool tls = false;            /* connection used TLS */
    std::string tls_group;       /* negotiated key-exchange group, if known */
    std::string tls_cipher;      /* negotiated cipher suite, if known */
};

struct TlsOptions {
    bool enabled = false;
    std::string cert_file;       /* PEM; empty -> ephemeral self-signed */
    std::string key_file;        /* PEM */
    std::string groups = "X25519MLKEM768:X25519:secp256r1:secp384r1";
    std::string min_version = "1.2";   /* "1.2" or "1.3" */
    int cert_days = 365;         /* self-signed validity */
    int key_bits = 2048;         /* self-signed RSA key size */
    std::string cert_cn = "Metis Code Analyzer Plus";
};

struct Response {
    int status = 200;
    std::string content_type = "application/json";
    std::string body;
    std::map<std::string, std::string> extra_headers;
};

namespace detail {

inline void close_socket(socket_t s) {
#ifdef _WIN32
    closesocket(s);
#else
    ::close(s);
#endif
}

inline std::string status_text(int code) {
    switch (code) {
    case 200: return "OK";
    case 201: return "Created";
    case 204: return "No Content";
    case 400: return "Bad Request";
    case 403: return "Forbidden";
    case 404: return "Not Found";
    case 405: return "Method Not Allowed";
    case 500: return "Internal Server Error";
    default:  return "OK";
    }
}

inline std::string content_type_for(const std::filesystem::path& p) {
    std::string e = p.extension().string();
    for (char& c : e) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    if (e == ".html" || e == ".htm") return "text/html; charset=utf-8";
    if (e == ".js")  return "application/javascript; charset=utf-8";
    if (e == ".css") return "text/css; charset=utf-8";
    if (e == ".json") return "application/json";
    if (e == ".svg") return "image/svg+xml";
    if (e == ".png") return "image/png";
    if (e == ".ico") return "image/x-icon";
    return "application/octet-stream";
}

inline std::string read_file(const std::filesystem::path& p) {
    std::ifstream in(p, std::ios::binary);
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

/* Connection abstraction: plain socket or, when built with TLS, an SSL stream. */
struct Conn {
    socket_t fd = kInvalidSocket;
    bool tls_active = false;
#ifdef METIS_ENABLE_TLS
    SSL* ssl = nullptr;
#endif

    long recv_some(char* buf, std::size_t n) {
#ifdef METIS_ENABLE_TLS
        if (ssl) return SSL_read(ssl, buf, static_cast<int>(n));
#endif
#ifdef _WIN32
        return ::recv(fd, buf, static_cast<int>(n), 0);
#else
        return static_cast<long>(::recv(fd, buf, n, 0));
#endif
    }

    long send_some(const char* buf, std::size_t n) {
#ifdef METIS_ENABLE_TLS
        if (ssl) return SSL_write(ssl, buf, static_cast<int>(n));
#endif
#ifdef _WIN32
        return ::send(fd, buf, static_cast<int>(n), 0);
#else
        return static_cast<long>(::send(fd, buf, n, 0));
#endif
    }

    void close() {
#ifdef METIS_ENABLE_TLS
        if (ssl) { SSL_shutdown(ssl); SSL_free(ssl); ssl = nullptr; }
#endif
        if (fd != kInvalidSocket) { close_socket(fd); fd = kInvalidSocket; }
    }
};

} /* namespace detail */

class Server {
public:
    using Handler = std::function<Response(const Request&)>;

    Server(std::string host, int port, std::string static_dir,
           TlsOptions tls = TlsOptions{})
        : host_(std::move(host)), port_(port), static_dir_(std::move(static_dir)),
          tls_(std::move(tls)) {}

    ~Server() {
#ifdef METIS_ENABLE_TLS
        if (ssl_ctx_) { SSL_CTX_free(ssl_ctx_); ssl_ctx_ = nullptr; }
#endif
    }

    /* Result of TLS setup, for the startup banner. Empty status when disabled. */
    struct TlsStatus { bool enabled = false; bool active = false; std::string detail; };
    TlsStatus tls_status() const { return tls_state_; }

    void route(const std::string& method, const std::string& path, Handler h) {
        routes_[method + " " + path] = std::move(h);
    }

    void set_index_document(const std::string& doc) {
        index_doc_ = doc.empty() ? std::string("index.html") : doc;
    }

    void set_limits(int max_header_bytes, int listen_backlog, int recv_chunk_bytes) {
        if (max_header_bytes > 0) max_header_bytes_ = static_cast<std::size_t>(max_header_bytes);
        if (listen_backlog > 0) listen_backlog_ = listen_backlog;
        if (recv_chunk_bytes > 0) recv_chunk_bytes_ = static_cast<std::size_t>(recv_chunk_bytes);
    }

    bool start() {
#ifdef _WIN32
        WSADATA wsa;
        if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) return false;
#endif
        listen_fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
        if (listen_fd_ == kInvalidSocket) return false;

        int opt = 1;
        ::setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR,
                     reinterpret_cast<const char*>(&opt), sizeof(opt));

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(static_cast<std::uint16_t>(port_));
        if (host_ == "0.0.0.0" || host_.empty()) {
            addr.sin_addr.s_addr = INADDR_ANY;
        } else {
            inet_pton(AF_INET, host_.c_str(), &addr.sin_addr);
        }

        if (::bind(listen_fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
            detail::close_socket(listen_fd_);
            return false;
        }
        if (::listen(listen_fd_, listen_backlog_) != 0) {
            detail::close_socket(listen_fd_);
            return false;
        }
        if (!init_tls()) {
            detail::close_socket(listen_fd_);
            return false;
        }
        running_ = true;
        return true;
    }

    bool init_tls() {
#ifdef METIS_ENABLE_TLS
        if (!tls_.enabled) { tls_state_ = {false, false, "disabled"}; return true; }
        const SSL_METHOD* method = TLS_server_method();
        ssl_ctx_ = SSL_CTX_new(method);
        if (!ssl_ctx_) { tls_state_ = {true, false, "context creation failed"}; return false; }
        SSL_CTX_set_min_proto_version(ssl_ctx_,
            tls_.min_version == "1.3" ? TLS1_3_VERSION : TLS1_2_VERSION);

        /* Prefer AES-256-GCM so the security badge lights up on every modern
         * browser. SSL_OP_CIPHER_SERVER_PREFERENCE forces OpenSSL to follow
         * the SERVER's list order instead of the client's; without it, TLS 1.3
         * follows the browser's order and Edge sends AES-128 first. */
        SSL_CTX_set_options(ssl_ctx_, SSL_OP_CIPHER_SERVER_PREFERENCE);
        SSL_CTX_set_ciphersuites(ssl_ctx_,
            "TLS_AES_256_GCM_SHA384:"
            "TLS_CHACHA20_POLY1305_SHA256:"
            "TLS_AES_128_GCM_SHA256");

        /* Prefer the post-quantum hybrid group; fall back to classical if the
         * linked OpenSSL does not know it (pre-3.5 has no ML-KEM). */
        std::string detail_msg;
        if (SSL_CTX_set1_groups_list(ssl_ctx_, tls_.groups.c_str()) != 1) {
            const char* classical = "X25519:secp256r1:secp384r1";
            SSL_CTX_set1_groups_list(ssl_ctx_, classical);
            detail_msg = "classical groups (PQC group not supported by this OpenSSL)";
        } else {
            detail_msg = "groups: " + tls_.groups;
        }

        bool ok;
        if (!tls_.cert_file.empty() && !tls_.key_file.empty()) {
            /* Use chain-file loader so mkcert bundles (leaf + CA in one PEM)
             * are accepted -- SSL_CTX_use_certificate_file only reads the first
             * cert, which causes a load failure on chain files. */
            ok = (SSL_CTX_use_certificate_chain_file(ssl_ctx_, tls_.cert_file.c_str()) == 1)
              && (SSL_CTX_use_PrivateKey_file(ssl_ctx_, tls_.key_file.c_str(), SSL_FILETYPE_PEM) == 1);
            if (ok) {
                detail_msg += "; cert: file";
                load_cert_der_from_ctx();
            } else {
                char ossl_err[256] = {};
                ERR_error_string_n(ERR_get_error(), ossl_err, sizeof(ossl_err));
                detail_msg += std::string("; cert file load failed (")
                            + tls_.cert_file + "): " + ossl_err;
            }
        } else {
            /* Persist the ephemeral cert so the user only needs to import it
             * once into the OS trust store.  Files sit next to the executable.
             * If they already exist, reload them instead of generating new ones. */
            std::string cfile = "metis-cert.pem";
            std::string kfile = "metis-key.pem";
            bool loaded = try_load_persisted(cfile, kfile);
            if (loaded) {
                detail_msg += "; cert: persisted (metis-cert.pem)";
            } else {
                ok = make_self_signed(cfile, kfile);
                detail_msg += ok ? "; cert: self-signed (saved to metis-cert.pem)"
                                 : "; self-signed generation failed";
                if (!ok) { tls_state_ = {true, false, detail_msg}; return false; }
            }
            load_cert_der_from_ctx();
        }
        tls_state_ = {true, true, detail_msg};
        return true;
#else
        if (tls_.enabled) {
            tls_state_ = {true, false, "built without TLS support; serving plain HTTP"};
            return true;
        }
        tls_state_ = {false, false, "disabled"};
        return true;
#endif
    }

    void run() {
        while (running_) {
            socket_t client = ::accept(listen_fd_, nullptr, nullptr);
            if (client == kInvalidSocket) {
                if (!running_) break;
                continue;
            }
            std::thread(&Server::handle_client, this, client).detach();
        }
    }

    void stop() {
        running_ = false;
        if (listen_fd_ != kInvalidSocket) {
            detail::close_socket(listen_fd_);
            listen_fd_ = kInvalidSocket;
        }
    }

private:
    void handle_client(socket_t client) {
        detail::Conn conn;
        conn.fd = client;
        std::string group_label;
        std::string cipher_label;

#ifdef METIS_ENABLE_TLS
        if (ssl_ctx_) {
            conn.ssl = SSL_new(ssl_ctx_);
            if (!conn.ssl) { conn.close(); return; }
            SSL_set_fd(conn.ssl, static_cast<int>(client));
            if (SSL_accept(conn.ssl) != 1) { conn.close(); return; }
            conn.tls_active = true;
            conn.tls_active = true;
            group_label  = negotiated_group(conn.ssl);
            const char* c = SSL_get_cipher_name(conn.ssl);
            cipher_label = c ? std::string(c) : std::string();
        }
#endif

        std::string raw;
        char buf[8192];
        std::size_t chunk = recv_chunk_bytes_ < sizeof(buf) ? recv_chunk_bytes_ : sizeof(buf);
        std::size_t header_end = std::string::npos;

        while (header_end == std::string::npos) {
            long n = conn.recv_some(buf, chunk);
            if (n <= 0) { conn.close(); return; }
            raw.append(buf, static_cast<std::size_t>(n));
            header_end = raw.find("\r\n\r\n");
            if (raw.size() > max_header_bytes_) break;
        }
        if (header_end == std::string::npos) { conn.close(); return; }

        Request req = parse_request(raw.substr(0, header_end));
        req.tls = conn.tls_active;
        req.tls_group = group_label;
        req.tls_cipher = cipher_label;
        std::string body = raw.substr(header_end + 4);

        std::size_t content_len = 0;
        auto it = req.headers.find("content-length");
        if (it != req.headers.end()) {
            try { content_len = static_cast<std::size_t>(std::stoul(it->second)); }
            catch (...) {
                /* Malformed Content-Length header: treat as zero-length body. */
                content_len = 0;
            }
        }
        while (body.size() < content_len) {
            long n = conn.recv_some(buf, chunk);
            if (n <= 0) break;
            body.append(buf, static_cast<std::size_t>(n));
        }
        req.body = body;

        Response res = dispatch(req);
        send_response(conn, res);
        conn.close();
    }

    Request parse_request(const std::string& head) {
        Request req;
        std::istringstream ss(head);
        std::string line;
        std::getline(ss, line);
        if (!line.empty() && line.back() == '\r') line.pop_back();

        std::istringstream rl(line);
        std::string target;
        rl >> req.method >> target;
        std::size_t q = target.find('?');
        if (q == std::string::npos) {
            req.path = target;
        } else {
            req.path = target.substr(0, q);
            req.query = target.substr(q + 1);
        }

        while (std::getline(ss, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (line.empty()) continue;
            std::size_t c = line.find(':');
            if (c == std::string::npos) continue;
            std::string key = line.substr(0, c);
            std::string val = line.substr(c + 1);
            for (char& ch : key) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
            std::size_t a = val.find_first_not_of(" \t");
            if (a != std::string::npos) val = val.substr(a);
            req.headers[key] = val;
        }
        return req;
    }

    Response dispatch(const Request& req) {
        auto it = routes_.find(req.method + " " + req.path);
        if (it != routes_.end()) return it->second(req);
        if (req.method == "GET") return serve_static(req.path);
        Response res;
        res.status = 404;
        res.body = R"({"error":"not found"})";
        return res;
    }

    /* Static serving confined to the web root via canonicalization. */
    Response serve_static(const std::string& url_path) {
        Response res;
        namespace fs = std::filesystem;

        std::string rel = url_path;
        if (rel == "/" || rel.empty()) rel = "/" + index_doc_;
        if (!rel.empty() && rel.front() == '/') rel = rel.substr(1);

        std::error_code ec;
        fs::path root = fs::weakly_canonical(fs::path(static_dir_), ec);
        if (ec) root = fs::absolute(fs::path(static_dir_));
        fs::path target = fs::weakly_canonical(root / rel, ec);
        if (ec) { res.status = 404; res.body = R"({"error":"not found"})"; return res; }

        /* Confinement check: target must live under root. */
        std::string root_s = root.string();
        std::string tgt_s = target.string();
        if (tgt_s.size() < root_s.size() ||
            tgt_s.compare(0, root_s.size(), root_s) != 0) {
            res.status = 403;
            res.content_type = "application/json";
            res.body = R"({"error":"forbidden"})";
            return res;
        }
        if (!fs::exists(target) || !fs::is_regular_file(target)) {
            res.status = 404;
            res.content_type = "application/json";
            res.body = R"({"error":"not found"})";
            return res;
        }
        res.status = 200;
        res.content_type = detail::content_type_for(target);
        res.body = detail::read_file(target);
        return res;
    }

    void send_response(detail::Conn& conn, const Response& res) {
        std::ostringstream os;
        os << "HTTP/1.1 " << res.status << ' ' << detail::status_text(res.status) << "\r\n";
        os << "Content-Type: " << res.content_type << "\r\n";
        os << "Content-Length: " << res.body.size() << "\r\n";
        os << "Connection: close\r\n";
        os << "X-Content-Type-Options: nosniff\r\n";
        os << "X-Frame-Options: DENY\r\n";
        os << "Referrer-Policy: no-referrer\r\n";
        /* Content-Security-Policy: all assets are local; no inline scripts
         * or external resources are used in the dashboard. */
        os << "Content-Security-Policy: default-src 'self'; "
              "script-src 'self'; style-src 'self' 'unsafe-inline'; img-src 'self' data:\r\n";
        if (conn.tls_active) {
            /* HSTS: tell browsers to only contact this host over HTTPS.
             * max-age of one year; no includeSubDomains since host is local. */
            os << "Strict-Transport-Security: max-age=31536000\r\n";
        }
        for (const auto& [k, v] : res.extra_headers) {
            os << k << ": " << v << "\r\n";
        }
        os << "\r\n";
        std::string head = os.str();
        send_all(conn, head);
        send_all(conn, res.body);
    }

    void send_all(detail::Conn& conn, const std::string& data) {
        std::size_t sent = 0;
        while (sent < data.size()) {
            long n = conn.send_some(data.data() + sent, data.size() - sent);
            if (n <= 0) break;
            sent += static_cast<std::size_t>(n);
        }
    }

#ifdef METIS_ENABLE_TLS
    static std::string negotiated_group(SSL* ssl) {
#  if OPENSSL_VERSION_NUMBER >= 0x30200000L
        const char* g = SSL_get0_group_name(ssl);
        return g ? std::string(g) : std::string("unknown");
#  else
        /* Pre-3.2 has no SSL_get0_group_name; report the protocol instead.
         * (Such OpenSSL also has no ML-KEM, so the group is classical.) */
        const char* v = SSL_get_version(ssl);
        return std::string(v ? v : "TLS") + " (classical)";
#  endif
    }

    /* Attempt to reload previously persisted cert + key PEM files. */
    bool try_load_persisted(const std::string& cfile, const std::string& kfile) {
        std::ifstream cf(cfile); std::ifstream kf(kfile);
        if (!cf.good() || !kf.good()) return false;
        return SSL_CTX_use_certificate_file(ssl_ctx_, cfile.c_str(), SSL_FILETYPE_PEM) == 1
            && SSL_CTX_use_PrivateKey_file(ssl_ctx_, kfile.c_str(), SSL_FILETYPE_PEM) == 1;
    }

    /* Cache the current context certificate as DER bytes for /tls/cert.pem. */
    void load_cert_der_from_ctx() {
        X509* c = SSL_CTX_get0_certificate(ssl_ctx_);
        if (!c) return;
        unsigned char* buf = nullptr;
        int len = i2d_X509(c, &buf);
        if (len > 0 && buf) {
            cert_der_.assign(reinterpret_cast<char*>(buf), static_cast<std::size_t>(len));
            OPENSSL_free(buf);
        }
    }

    bool make_self_signed(const std::string& save_cert = "",
                          const std::string& save_key  = "") {
        EVP_PKEY* pkey = EVP_RSA_gen(static_cast<unsigned int>(tls_.key_bits > 0 ? tls_.key_bits : 2048));
        if (!pkey) return false;
        X509* x509 = X509_new();
        if (!x509) { EVP_PKEY_free(pkey); return false; }
        ASN1_INTEGER_set(X509_get_serialNumber(x509), 1);
        X509_gmtime_adj(X509_getm_notBefore(x509), 0);
        X509_gmtime_adj(X509_getm_notAfter(x509),
                        60L * 60 * 24 * (tls_.cert_days > 0 ? tls_.cert_days : 3650));
        X509_set_pubkey(x509, pkey);
        /* X509_get_subject_name returns const X509_NAME* in OpenSSL 4.0 */
        X509_NAME* name = const_cast<X509_NAME*>(X509_get_subject_name(x509));
        std::string cn = tls_.cert_cn.empty() ? std::string("Metis Code Analyzer Plus") : tls_.cert_cn;
        X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC,
            reinterpret_cast<const unsigned char*>(cn.c_str()), -1, -1, 0);
        X509_set_issuer_name(x509, name);

        /* SAN required by Chrome/Edge -- without it the OS trust-store import
         * has no effect and the browser still shows "Not secure". */
        X509V3_CTX ext_ctx;
        X509V3_set_ctx_nodb(&ext_ctx);
        X509V3_set_ctx(&ext_ctx, x509, x509, nullptr, nullptr, 0);

        X509_EXTENSION* san = X509V3_EXT_conf_nid(nullptr, &ext_ctx,
            NID_subject_alt_name,
            "DNS:localhost,DNS:127.0.0.1,IP:127.0.0.1,IP:::1");
        if (san) { X509_add_ext(x509, san, -1); X509_EXTENSION_free(san); }

        X509_EXTENSION* eku = X509V3_EXT_conf_nid(nullptr, &ext_ctx,
            NID_ext_key_usage, "serverAuth");
        if (eku) { X509_add_ext(x509, eku, -1); X509_EXTENSION_free(eku); }

        X509_EXTENSION* bc = X509V3_EXT_conf_nid(nullptr, &ext_ctx,
            NID_basic_constraints, "critical,CA:TRUE");
        if (bc) { X509_add_ext(x509, bc, -1); X509_EXTENSION_free(bc); }

        bool ok = X509_sign(x509, pkey, EVP_sha256()) != 0
               && SSL_CTX_use_certificate(ssl_ctx_, x509) == 1
               && SSL_CTX_use_PrivateKey(ssl_ctx_, pkey) == 1;

        /* Persist to disk so the same cert survives restarts. */
        if (ok && !save_cert.empty() && !save_key.empty()) {
            if (FILE* fc = std::fopen(save_cert.c_str(), "wb")) {
                PEM_write_X509(fc, x509); std::fclose(fc);
            }
            if (FILE* fk = std::fopen(save_key.c_str(), "wb")) {
                PEM_write_PrivateKey(fk, pkey, nullptr, nullptr, 0, nullptr, nullptr);
                std::fclose(fk);
            }
        }

        X509_free(x509);
        EVP_PKEY_free(pkey);
        return ok;
    }
#endif

    std::string host_;
    int port_;
    std::string static_dir_;
    socket_t listen_fd_ = kInvalidSocket;
    std::atomic<bool> running_{false};
    std::map<std::string, Handler> routes_;
    std::size_t max_header_bytes_ = 1048576;
    int listen_backlog_ = 64;
    std::size_t recv_chunk_bytes_ = 8192;
    std::string index_doc_ = "index.html";
    TlsOptions tls_;
    TlsStatus tls_state_;
#ifdef METIS_ENABLE_TLS
    SSL_CTX* ssl_ctx_ = nullptr;
    std::string cert_der_;   /* DER bytes of the current cert, served at /tls/cert.pem */
#endif
};

} /* namespace metis::codeanalyzer::http */

#endif /* METIS_CODE_ANALYZER_HTTP_HPP */
