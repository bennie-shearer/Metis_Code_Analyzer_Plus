/* Metis Code Analyzer Plus - server entry point
 * Wires configuration, the analysis engine, the REST API, Prometheus
 * metrics, and the static dashboard.
 * 7-bit ASCII, C++20.
 */
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <map>
#include <mutex>
#include <sstream>
#include <string>
#include <ctime>
#include <fstream>
#include <filesystem>
#include <thread>
#include <vector>
#ifdef _WIN32
#  include <direct.h>   /* _mkdir */
#else
#  include <sys/stat.h> /* mkdir */
#endif

#include "metis/codeanalyzer/analyzer.hpp"
#include "metis/codeanalyzer/ai.hpp"
#include "metis/codeanalyzer/license.hpp"
#include "metis/codeanalyzer/technologies.hpp"
#include "metis/codeanalyzer/auth.hpp"
#include "metis/codeanalyzer/http.hpp"
#include "metis/codeanalyzer/json.hpp"
#include "metis/codeanalyzer/log.hpp"
#include "metis/codeanalyzer/pdf.hpp"
#include "metis/codeanalyzer/pson.hpp"
#include "metis/codeanalyzer/version.hpp"

namespace mc = metis::codeanalyzer;

namespace {

mc::http::Server* g_server = nullptr;

void on_signal(int) {
    if (g_server) g_server->stop();
}

} /* namespace */

namespace {
using json = mc::json::Value;

std::string cookie_value(const mc::http::Request& req, const std::string& name) {
    auto it = req.headers.find("cookie");
    if (it == req.headers.end()) return "";
    const std::string& c = it->second;
    std::string key = name + "=";
    std::size_t p = c.find(key);
    while (p != std::string::npos) {
        bool at_start = (p == 0) || c[p - 1] == ' ' || c[p - 1] == ';';
        if (at_start) {
            std::size_t v = p + key.size();
            std::size_t e = c.find(';', v);
            return c.substr(v, e == std::string::npos ? std::string::npos : e - v);
        }
        p = c.find(key, p + 1);
    }
    return "";
}

std::string url_decode(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (std::size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '%' && i + 2 < s.size()) {
            auto hex = [](char c) -> int {
                if (c >= '0' && c <= '9') return c - '0';
                if (c >= 'a' && c <= 'f') return c - 'a' + 10;
                if (c >= 'A' && c <= 'F') return c - 'A' + 10;
                return -1;
            };
            int hi = hex(s[i + 1]), lo = hex(s[i + 2]);
            if (hi >= 0 && lo >= 0) { out.push_back(static_cast<char>(hi * 16 + lo)); i += 2; continue; }
        }
        if (s[i] == '+') out.push_back(' ');
        else out.push_back(s[i]);
    }
    return out;
}

std::string json_string_field(const std::string& body, const std::string& field) {
    std::string key = "\"" + field + "\"";
    std::size_t p = body.find(key);
    if (p == std::string::npos) return "";
    std::size_t colon = body.find(':', p + key.size());
    if (colon == std::string::npos) return "";
    std::size_t q1 = body.find('"', colon + 1);
    if (q1 == std::string::npos) return "";
    std::string out;
    for (std::size_t i = q1 + 1; i < body.size(); ++i) {
        if (body[i] == '\\' && i + 1 < body.size()) { out.push_back(body[i + 1]); ++i; }
        else if (body[i] == '"') break;
        else out.push_back(body[i]);
    }
    return out;
}

/* ---- JSON serialization helpers ----------------------------------------- */

/* Wrap a pre-built JSON value in an HTTP 200 response. Used to avoid
 * repeating the same three-line tail across every array-returning route. */
static mc::http::Response json_response(const json& value) {
    mc::http::Response res;
    res.body = value.dump();
    return res;
}

json health_json(const mc::HealthScores& h, const mc::ScoreConfig& sc) {
    json o = json::object();
    o.set("robustness", h.robustness);
    o.set("security", h.security);
    o.set("efficiency", h.efficiency);
    o.set("transferability", h.transferability);
    o.set("changeability", h.changeability);
    o.set("portability", h.portability);
    o.set("tqi", h.tqi);
    std::string g(1, mc::grade_for(h.tqi, sc));
    o.set("grade", g);
    return o;
}

json report_json(const mc::AnalysisResult& r, const mc::ScoreConfig& sc) {
    const mc::ProjectReport& rep = r.report;
    json o = json::object();
    o.set("ran", r.ran);
    o.set("scanned_root", r.scanned_root);
    o.set("file_count", rep.file_count);
    o.set("total_lines", rep.total_lines);
    o.set("code_lines", rep.code_lines);
    o.set("comment_lines", rep.comment_lines);
    o.set("blank_lines", rep.blank_lines);
    o.set("comment_ratio", rep.comment_ratio);
    o.set("total_functions", rep.total_functions);
    o.set("avg_complexity", rep.avg_complexity);
    o.set("max_complexity", rep.max_complexity);
    o.set("duplicate_blocks", rep.duplicate_blocks);
    json dups = json::array();
    for (const auto& group : rep.duplicates) {
        json g = json::array();
        for (const std::string& loc : group) g.push_back(loc);
        dups.push_back(g);
    }
    o.set("duplicates", dups);
    o.set("issue_count", rep.issue_count);

    json langs = json::object();
    for (const auto& [name, lines] : rep.lines_by_language) {
        langs.set(name, lines);
    }
    o.set("lines_by_language", langs);

    o.set("health", health_json(rep.health, sc));

    json debt = json::object();
    debt.set("total_minutes", static_cast<long long>(rep.debt.total_minutes));
    debt.set("total_hours", rep.debt.total_hours);
    debt.set("cost", rep.debt.cost);
    debt.set("debt_ratio", rep.debt.debt_ratio);
    json sev = json::object();
    for (const auto& [k, v] : rep.debt.severity_counts) {
        sev.set(k, static_cast<long long>(v));
    }
    debt.set("severity_counts", sev);
    o.set("debt", debt);
    return o;
}

json file_to_json(const mc::FileRecord& f) {
    json o = json::object();
    o.set("path", f.path);
    o.set("language", f.language);
    o.set("total_lines", f.metrics.total_lines);
    o.set("code_lines", f.metrics.code_lines);
    o.set("comment_lines", f.metrics.comment_lines);
    o.set("blank_lines", f.metrics.blank_lines);
    o.set("functions", f.metrics.functions);
    o.set("cyclomatic", f.metrics.cyclomatic);
    o.set("max_nesting", f.metrics.max_nesting);
    o.set("long_lines", f.metrics.long_lines);
    o.set("issues", f.issue_count);
    json deps = json::array();
    for (const std::string& d : f.dependencies) deps.push_back(d);
    o.set("dependencies", deps);
    return o;
}

json issue_to_json(const mc::Issue& i) {
    json o = json::object();
    o.set("rule_id", i.rule_id);
    o.set("title", i.title);
    o.set("cwe", i.cwe);
    o.set("severity", mc::severity_name(i.severity));
    o.set("factor", mc::health_name(i.factor));
    o.set("remediation_minutes", i.remediation_minutes);
    o.set("file", i.file);
    o.set("line", static_cast<long long>(i.line));
    o.set("snippet", i.snippet);
    return o;
}

json log_to_json(const mc::LogEntry& e) {
    json o = json::object();
    o.set("timestamp", e.timestamp);
    o.set("level", mc::log_level_name(e.level));
    o.set("message", e.message);
    return o;
}

void grade_rgb(char g, double& r, double& gn, double& b) {
    switch (g) {
    case 'A': r = 0.21; gn = 0.79; b = 0.55; break;
    case 'B': r = 0.48; gn = 0.84; b = 0.41; break;
    case 'C': r = 0.91; gn = 0.78; b = 0.29; break;
    case 'D': r = 0.94; gn = 0.61; b = 0.29; break;
    default:  r = 0.88; gn = 0.35; b = 0.42; break;
    }
}

/* ---- Export helpers ----------------------------------------------------- */

static std::string csv_escape(const std::string& s) {
    if (s.find_first_of(",\"\n\r") == std::string::npos) return s;
    std::string out = "\"";
    for (char c : s) { if (c == '"') out += "\"\""; else out += c; }
    return out + "\"";
}

/* Derive a download-filename stem from a scanned path. Takes the last path
   component (splitting on both '/' and '\'), keeps [A-Za-z0-9._-], maps any
   other byte to a single '_', and trims leading/trailing separator chars.
   Falls back to "report" when nothing usable remains (empty root, ".", or a
   bare drive such as "C:/"). Pure 7-bit ASCII; safe in an HTTP filename and
   on disk. */
static std::string download_stem(const std::string& path) {
    std::string p = path;
    while (!p.empty() && (p.back() == '/' || p.back() == '\\')) p.pop_back();
    std::size_t cut = p.find_last_of("/\\");
    std::string base = (cut == std::string::npos) ? p : p.substr(cut + 1);
    std::string out;
    bool last_us = false;
    for (char c : base) {
        bool ok = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                  (c >= '0' && c <= '9') || c == '.' || c == '-' || c == '_';
        if (ok) { out += c; last_us = false; }
        else if (!last_us) { out += '_'; last_us = true; }
    }
    std::size_t b = out.find_first_not_of("_.-");
    if (b == std::string::npos) return "report";
    std::size_t e = out.find_last_not_of("_.-");
    out = out.substr(b, e - b + 1);
    return out.empty() ? "report" : out;
}



std::string build_report_pdf(const mc::AnalysisResult& res, const mc::ScoreConfig& sc,
                             std::size_t pdf_max_issues = 40) {
    const mc::ProjectReport& rep = res.report;
    mc::pdf::PdfBuilder pdf;
    pdf.heading(std::string(mc::product) + "  " + mc::version);
    pdf.line("Code quality report");
    pdf.line("Scanned root: " + res.scanned_root);
    pdf.gap(8.0);

    char g = mc::grade_for(rep.health.tqi, sc);
    double r, gn, b;
    grade_rgb(g, r, gn, b);
    std::ostringstream score;
    score.precision(1);
    score << std::fixed << rep.health.tqi << " / 100  (Grade " << g << ")";
    pdf.grade_block(std::string(1, g), score.str(), r, gn, b);
    pdf.gap(6.0);

    pdf.subheading("Health Factors");
    auto factor_bar = [&](const char* name, double v) {
        double fr, fg, fb;
        grade_rgb(mc::grade_for(v, sc), fr, fg, fb);
        pdf.bar(name, v, fr, fg, fb);
    };
    factor_bar("Robustness", rep.health.robustness);
    factor_bar("Security", rep.health.security);
    factor_bar("Efficiency", rep.health.efficiency);
    factor_bar("Transferability", rep.health.transferability);
    factor_bar("Changeability", rep.health.changeability);
    factor_bar("Portability", rep.health.portability);
    pdf.gap(8.0);

    pdf.subheading("Summary");
    pdf.kv("Files analyzed", std::to_string(rep.file_count));
    pdf.kv("Lines of code", std::to_string(rep.code_lines));
    pdf.kv("Functions", std::to_string(rep.total_functions));
    {
        std::ostringstream c; c.precision(1); c << std::fixed << rep.avg_complexity;
        pdf.kv("Average complexity", c.str());
    }
    pdf.kv("Duplicate blocks", std::to_string(rep.duplicate_blocks));
    pdf.kv("Issues", std::to_string(rep.issue_count));
    {
        std::ostringstream h; h.precision(1); h << std::fixed << rep.debt.total_hours;
        pdf.kv("Technical debt (hours)", h.str());
    }
    {
        std::ostringstream c; c.precision(0); c << std::fixed << rep.debt.cost;
        pdf.kv("Estimated remediation cost", "$" + c.str());
    }
    pdf.gap(8.0);

    pdf.subheading("Top Issues");
    std::size_t shown = 0;
    for (const mc::Issue& i : res.issues) {
        if (shown >= pdf_max_issues) break;
        std::string cwe = i.cwe.empty() ? "" : (" [" + i.cwe + "]");
        pdf.line(std::string(mc::severity_name(i.severity)) + "  " + i.rule_id +
                 cwe + "  " + i.file + ":" + std::to_string(i.line));
        ++shown;
    }
    if (res.issues.empty()) pdf.line("No issues detected.");
    return pdf.build();
}

/* ---- Prometheus text-format metrics ------------------------------------- */

std::string prometheus_metrics(const mc::AnalysisResult& r) {
    const mc::ProjectReport& rep = r.report;
    std::ostringstream os;
    os << "# HELP metis_code_analyzer_files_total Number of source files analyzed.\n";
    os << "# TYPE metis_code_analyzer_files_total gauge\n";
    os << "metis_code_analyzer_files_total " << rep.file_count << "\n";
    os << "# HELP metis_code_analyzer_code_lines_total Lines of code analyzed.\n";
    os << "# TYPE metis_code_analyzer_code_lines_total gauge\n";
    os << "metis_code_analyzer_code_lines_total " << rep.code_lines << "\n";
    os << "# HELP metis_code_analyzer_issues_total Detected issues.\n";
    os << "# TYPE metis_code_analyzer_issues_total gauge\n";
    os << "metis_code_analyzer_issues_total " << rep.issue_count << "\n";
    os << "# HELP metis_code_analyzer_debt_minutes Estimated remediation minutes.\n";
    os << "# TYPE metis_code_analyzer_debt_minutes gauge\n";
    os << "metis_code_analyzer_debt_minutes " << rep.debt.total_minutes << "\n";
    os << "# HELP metis_code_analyzer_tqi Total Quality Index (0-100).\n";
    os << "# TYPE metis_code_analyzer_tqi gauge\n";
    os << "metis_code_analyzer_tqi " << rep.health.tqi << "\n";
    auto factor = [&](const char* name, double v) {
        os << "metis_code_analyzer_health{factor=\"" << name << "\"} " << v << "\n";
    };
    os << "# HELP metis_code_analyzer_health Health factor scores (0-100).\n";
    os << "# TYPE metis_code_analyzer_health gauge\n";
    factor("robustness", rep.health.robustness);
    factor("security", rep.health.security);
    factor("efficiency", rep.health.efficiency);
    factor("transferability", rep.health.transferability);
    factor("changeability", rep.health.changeability);
    return os.str();
}

} /* namespace */

/* ---- Entry point --------------------------------------------------------
 * Loads code_analyzer.pson (or the path given as argv[1]), configures every
 * subsystem from config, runs the initial scan when scan.on_start = true,
 * registers all REST endpoints, and starts the HTTP/HTTPS server loop.
 * The server blocks until a POST /api/shutdown is received or SIGINT/SIGTERM.
 * --------------------------------------------------------------------- */
int main(int argc, char** argv) {
    std::string config_path = "code_analyzer.pson";
    if (argc > 1) config_path = argv[1];

    mc::Pson cfg;
    if (!cfg.load(config_path)) {
        std::cerr << "[metis-code-analyzer] config not found: " << config_path
                  << " (using defaults)\n";
    }

    const std::string host = cfg.get_str("server.host", "0.0.0.0");
    const std::string static_dir = cfg.get_str("server.static_dir", "web");
    const int port = static_cast<int>(cfg.get_int("port", 8080));
    const int max_header_bytes = static_cast<int>(cfg.get_int("server.max_header_bytes", 1048576));
    const int listen_backlog = static_cast<int>(cfg.get_int("server.listen_backlog", 64));

    /* TLS / post-quantum config (single port serves HTTP or HTTPS). */
    mc::http::TlsOptions tls;
    tls.enabled = cfg.get_bool("tls.enabled", true);
    tls.cert_file = cfg.get_str("tls.cert_file", "");
    tls.key_file = cfg.get_str("tls.key_file", "");
    tls.groups = cfg.get_str("tls.groups", "X25519MLKEM768:X25519:secp256r1:secp384r1");
    tls.min_version = cfg.get_str("tls.min_version", "1.2");
    tls.cert_days = static_cast<int>(cfg.get_int("tls.cert_days", 365));
    tls.key_bits = static_cast<int>(cfg.get_int("tls.key_bits", 2048));
    tls.cert_cn = cfg.get_str("tls.cert_cn", "Metis Code Analyzer Plus");
    const std::string scan_root = cfg.get_str("scan.root", ".");
    const bool scan_on_start = cfg.get_bool("scan.on_start", true);

    /* Multi-project: optional named scan roots (project.N.name / project.N.root).
     * If no project.N entries are found, one implicit project is created using
     * scan.root.  The active project is selected by POST /api/project/select. */
    struct ProjectDef { std::string name; std::string root; };
    std::vector<ProjectDef> project_defs;
    for (int n = 1; ; ++n) {
        std::string base = "project." + std::to_string(n) + ".";
        if (!cfg.has(base + "name") && !cfg.has(base + "root")) break;
        ProjectDef pd;
        pd.name = cfg.get_str(base + "name", "project" + std::to_string(n));
        pd.root = cfg.get_str(base + "root", scan_root);
        project_defs.push_back(pd);
    }
    if (project_defs.empty()) {
        project_defs.push_back({"default", scan_root});
    }
    /* Resolve every project root to an absolute path now so that
     * scanned_root is always an absolute path in results and exports.
     * This ensures download_stem() derives the correct directory name
     * even when scan.root is "." or a relative path. */
    {
        namespace fs = std::filesystem;
        for (auto& pd : project_defs) {
            std::error_code ec;
            fs::path abs = fs::weakly_canonical(fs::path(pd.root), ec);
            if (!ec && !abs.empty()) pd.root = abs.string();
        }
    }

    /* AI summary config (outbound HTTPS; requires a TLS-enabled build). */
    mc::ai::AiOptions ai;
    ai.enabled = cfg.get_bool("ai.enabled", false);
    ai.provider = cfg.get_str("ai.provider", "");
    ai.endpoint = cfg.get_str("ai.endpoint", "");
    ai.model = cfg.get_str("ai.model", "");
    ai.api_key = cfg.get_str("ai.api_key", "");
    ai.system_prompt = cfg.get_str("ai.system_prompt",
        "You are a senior code reviewer. Summarize the report and suggest fixes.");
    ai.timeout_seconds = static_cast<int>(cfg.get_int("ai.timeout_seconds", 30));
    ai.max_output_tokens = static_cast<int>(cfg.get_int("ai.max_output_tokens", 1024));

    std::vector<std::string> excludes = cfg.get_array("scan.exclude");
    if (excludes.empty()) {
        excludes = {".git", "build", "node_modules", "cmake-build-debug",
                    "cmake-build-release", "dist", "vendor", "third_party"};
    }
    {
        std::string ex_list;
        for (std::size_t i = 0; i < excludes.size(); ++i) {
            if (i) ex_list += ", ";
            ex_list += "\"" + excludes[i] + "\"";
        }
        mc::Logger::instance().info("Scan excludes active (" +
            std::to_string(excludes.size()) + "): " + ex_list);
    }

    const double hourly_rate = cfg.get_double("debt.hourly_rate", 75.0);
    const double dev_min_per_line = cfg.get_double("debt.dev_minutes_per_line", 0.5);

    /* Logging */
    mc::Logger& logger = mc::Logger::instance();
    logger.configure(mc::log_level_from(cfg.get_str("log.level", "info")),
                     static_cast<std::size_t>(cfg.get_int("log.max_entries", 1000)));

    /* Authentication */
    const bool auth_enabled = cfg.get_bool("auth.enabled", true);
    const std::string auth_user = cfg.get_str("auth.username", "admin");
    const std::string auth_hash = cfg.get_str("auth.password_sha256", "");
    const bool api_key_enabled = cfg.get_bool("auth.api_key_enabled", false);
    const std::string api_key_hash = cfg.get_str("auth.api_key_sha256", "");

    /* Issue lifecycle store: maps "rule_id:file:line" -> status string.
     * Statuses: "open" (default), "accepted", "wontfix", "false_positive".
     * v2.5.0: Persisted to a dedicated NDJSON file so status survives restarts. */
    struct IssueStatus { std::string key; std::string status; std::string note; };
    std::map<std::string, IssueStatus> issue_statuses;
    std::mutex issue_status_mtx;

    /* Persistent storage path for issue status. Derived from ui.history_file or
     * a default "data/issue_status.ndjson" next to the executable. */
    const std::string status_file = [&]() -> std::string {
        std::string hf = cfg.get_str("ui.issue_status_file", "");
        if (hf.empty()) {
            /* Place in data/ subdirectory next to executable. */
            hf = "data/issue_status.ndjson";
        }
        return hf;
    }();

    /* Load persisted issue statuses from file. Each line:
     * {"key":"...","status":"...","note":"..."} */
    auto load_issue_statuses = [&]() {
        std::ifstream f(status_file);
        if (!f.is_open()) return;
        std::string line;
        std::lock_guard<std::mutex> lk(issue_status_mtx);
        issue_statuses.clear();
        while (std::getline(f, line)) {
            if (line.empty() || line[0] != '{') continue;
            /* Minimal parse: extract key, status, note from JSON line. */
            auto extract = [&](const std::string& field) -> std::string {
                std::string needle = "\"" + field + "\":\"";
                auto pos = line.find(needle);
                if (pos == std::string::npos) return "";
                pos += needle.size();
                auto end = line.find('"', pos);
                if (end == std::string::npos) return "";
                return line.substr(pos, end - pos);
            };
            std::string k = extract("key");
            std::string s = extract("status");
            std::string n = extract("note");
            if (!k.empty() && !s.empty()) {
                issue_statuses[k] = { k, s, n };
            }
        }
        logger.info(std::string("Loaded ") + std::to_string(issue_statuses.size()) +
                  " issue status record(s) from " + status_file);
    };

    /* Save all issue statuses to file (called after every change). */
    auto save_issue_statuses = [&]() {
        /* Ensure data/ directory exists. */
        {
            std::string dir = status_file;
            auto slash = dir.find_last_of("/\\");
            if (slash != std::string::npos) {
                dir = dir.substr(0, slash);
#ifdef _WIN32
                _mkdir(dir.c_str());
#else
                mkdir(dir.c_str(), 0755);
#endif
            }
        }
        std::ofstream f(status_file, std::ios::trunc);
        if (!f.is_open()) {
            logger.warn(std::string("Cannot write issue status file: ") + status_file);
            return;
        }
        std::lock_guard<std::mutex> lk(issue_status_mtx);
        for (auto& [k, v] : issue_statuses) {
            /* Escape quotes in key/note (keys from rule:file:line rarely need it). */
            auto esc = [](const std::string& s) {
                std::string r; r.reserve(s.size());
                for (char c : s) { if (c == '"' || c == '\\') r += '\\'; r += c; }
                return r;
            };
            f << "{\"key\":\"" << esc(v.key) << "\","
              << "\"status\":\"" << esc(v.status) << "\","
              << "\"note\":\"" << esc(v.note) << "\"}\n";
        }
    };

    /* Load on startup. */
    load_issue_statuses();

    /* Quality gate config. */
    const bool gate_enabled      = cfg.get_bool("gate.enabled", false);
    const double gate_min_tqi    = cfg.get_double("gate.min_tqi", 80.0);
    const int gate_max_issues    = static_cast<int>(cfg.get_int("gate.max_issues", -1));
    const int gate_max_critical  = static_cast<int>(cfg.get_int("gate.max_critical", 0));
    const bool gate_exit_nonzero = cfg.get_bool("gate.exit_nonzero", false);

    struct GateResult {
        bool passed = true;
        std::string reason;
        double tqi = 0.0;
        int issues = 0;
        int critical = 0;
    };

    auto eval_gate = [&](const mc::AnalysisResult& r) -> GateResult {
        GateResult g;
        g.tqi    = r.report.health.tqi;
        g.issues = static_cast<int>(r.report.issue_count);
        int crit = 0;
        for (const mc::Issue& i : r.issues)
            if (i.severity == mc::Severity::Critical) ++crit;
        g.critical = crit;
        if (gate_min_tqi > 0.0 && g.tqi < gate_min_tqi) {
            g.passed = false;
            g.reason = "TQI " + std::to_string(g.tqi) + " < " + std::to_string(gate_min_tqi);
        }
        if (gate_max_issues >= 0 && g.issues > gate_max_issues) {
            g.passed = false;
            g.reason = "Issues " + std::to_string(g.issues) + " > " + std::to_string(gate_max_issues);
        }
        if (gate_max_critical >= 0 && crit > gate_max_critical) {
            g.passed = false;
            g.reason = "Critical issues: " + std::to_string(crit);
        }
        return g;
    };
    const int session_minutes = static_cast<int>(cfg.get_int("auth.session_minutes", 60));
    const int session_token_length = static_cast<int>(cfg.get_int("auth.session_token_length", 48));
    mc::SessionStore sessions(session_minutes, session_token_length);

    /* Login brute-force throttle: per-username failures within a sliding window. */
    const int login_max_attempts = static_cast<int>(cfg.get_int("auth.login_max_attempts", 5));
    const long login_window = static_cast<long>(cfg.get_int("auth.login_window_seconds", 60));
    struct LoginThrottle {
        int max_attempts; long window;
        std::mutex m;
        std::map<std::string, std::pair<int, long>> hits;  /* key -> (count, window_start) */
        bool blocked(const std::string& key, long now) {
            if (max_attempts <= 0) return false;
            std::lock_guard<std::mutex> lk(m);
            auto& e = hits[key];
            if (now - e.second >= window) e = {0, now};
            return e.first >= max_attempts;
        }
        void fail(const std::string& key, long now) {
            if (max_attempts <= 0) return;
            std::lock_guard<std::mutex> lk(m);
            auto& e = hits[key];
            if (now - e.second >= window) e = {0, now};
            e.first += 1;
        }
        void reset(const std::string& key) {
            std::lock_guard<std::mutex> lk(m);
            hits.erase(key);
        }
    };
    LoginThrottle throttle{login_max_attempts, login_window, {}, {}};

    /* Warn loudly if the shipped default administrator password is still in use. */
    static const char* kDefaultHash =
        "b5af2beb9b7fd5aa4eb21b61925bd3856793e54e0a39052fbcfaf5b477fd07f3";
    if (auth_enabled && auth_hash == kDefaultHash) {
        logger.warn("Default administrator password is in use (admin / Admin#2026). "
                    "Change auth.password_sha256 before deployment - see SUPPORT.md.");
    }

    /* Internationalization */
    const std::string default_locale = cfg.get_str("i18n.default_locale", "en-US");
    std::vector<std::string> locales = cfg.get_array("i18n.locales");
    if (locales.empty()) {
        locales = {"en-US", "en-GB", "fr-FR", "de-DE",
                   "es-ES", "it-IT", "pt-BR", "ja-JP"};
    }

    /* Scoring model (all tunables from PSON, defaults match the baseline) */
    mc::ScoreConfig sc;
    sc.w_critical = cfg.get_double("score.weight.critical", sc.w_critical);
    sc.w_major    = cfg.get_double("score.weight.major", sc.w_major);
    sc.w_minor    = cfg.get_double("score.weight.minor", sc.w_minor);
    sc.w_info     = cfg.get_double("score.weight.info", sc.w_info);
    sc.scale_security       = cfg.get_double("score.scale.security", sc.scale_security);
    sc.scale_robustness     = cfg.get_double("score.scale.robustness", sc.scale_robustness);
    sc.scale_efficiency     = cfg.get_double("score.scale.efficiency", sc.scale_efficiency);
    sc.scale_transferability = cfg.get_double("score.scale.transferability", sc.scale_transferability);
    sc.scale_changeability  = cfg.get_double("score.scale.changeability", sc.scale_changeability);
    sc.scale_portability    = cfg.get_double("score.scale.portability",   sc.scale_portability);
    sc.density_cap = cfg.get_double("score.density_cap", sc.density_cap);
    sc.grade_a = cfg.get_double("score.grade.a", sc.grade_a);
    sc.grade_b = cfg.get_double("score.grade.b", sc.grade_b);
    sc.grade_c = cfg.get_double("score.grade.c", sc.grade_c);
    sc.grade_d = cfg.get_double("score.grade.d", sc.grade_d);
    sc.complexity_threshold = cfg.get_double("score.complexity.threshold", sc.complexity_threshold);

    /* Per-language threshold overrides: score.complexity.threshold.<lang> */
    for (const char* lang : {"cpp","c","python","javascript","typescript","java",
                             "csharp","go","rust","ruby","php","cobol","shell","sql",
                             "kotlin","swift","scala","dart","r","lua","perl","objc",
                             "fortran","hlasm","rexx","jcl","pli","haskell","elixir"}) {
        std::string key = std::string("score.complexity.threshold.") + lang;
        if (cfg.has(key))
            sc.complexity_threshold_lang[lang] = cfg.get_double(key, sc.complexity_threshold);
    }
    sc.complexity_rob_mult  = cfg.get_double("score.complexity.robustness_mult", sc.complexity_rob_mult);
    sc.complexity_rob_cap   = cfg.get_double("score.complexity.robustness_cap", sc.complexity_rob_cap);
    sc.complexity_chg_mult  = cfg.get_double("score.complexity.changeability_mult", sc.complexity_chg_mult);
    sc.complexity_chg_cap   = cfg.get_double("score.complexity.changeability_cap", sc.complexity_chg_cap);
    sc.comment_min            = cfg.get_double("score.comment.min_ratio", sc.comment_min);
    sc.comment_transfer_mult  = cfg.get_double("score.comment.transferability_mult", sc.comment_transfer_mult);
    sc.comment_transfer_cap   = cfg.get_double("score.comment.transferability_cap", sc.comment_transfer_cap);
    sc.comment_change_mult    = cfg.get_double("score.comment.changeability_mult", sc.comment_change_mult);
    sc.comment_change_cap     = cfg.get_double("score.comment.changeability_cap", sc.comment_change_cap);
    sc.dup_change_mult   = cfg.get_double("score.duplication.changeability_mult", sc.dup_change_mult);
    sc.dup_change_cap    = cfg.get_double("score.duplication.changeability_cap", sc.dup_change_cap);
    sc.dup_transfer_mult = cfg.get_double("score.duplication.transferability_mult", sc.dup_transfer_mult);
    sc.dup_transfer_cap  = cfg.get_double("score.duplication.transferability_cap", sc.dup_transfer_cap);

    /* Analysis sizing (from PSON) */
    const std::size_t max_file_bytes = static_cast<std::size_t>(
        cfg.get_int("analysis.max_file_bytes", 4 * 1024 * 1024));
    const std::size_t dup_block_lines = static_cast<std::size_t>(
        cfg.get_int("analysis.duplication_block_lines", 6));
    const std::size_t long_line_columns = static_cast<std::size_t>(
        cfg.get_int("analysis.long_line_columns", 120));

    /* Rule catalog: load from PSON (rule.N.*) if present, else built-in. */
    mc::RuleSet rules;
    int loaded_rules = 0;
    for (int n = 1; ; ++n) {
        std::string base = "rule." + std::to_string(n) + ".";
        if (!cfg.has(base + "id")) break;
        if (!cfg.get_bool(base + "enabled", true)) { ++loaded_rules; continue; }
        rules.add(
            cfg.get_str(base + "id", ""),
            cfg.get_str(base + "title", ""),
            cfg.get_str(base + "cwe", ""),
            mc::severity_from(cfg.get_str(base + "severity", "info")),
            mc::factor_from(cfg.get_str(base + "factor", "robustness")),
            static_cast<int>(cfg.get_int(base + "minutes", 10)),
            cfg.get_str(base + "pattern", ""));
        ++loaded_rules;
    }
    if (loaded_rules == 0) rules = mc::default_rules();
    rules.set_max_line_length(static_cast<std::size_t>(
        cfg.get_int("analysis.max_rule_line_length", 2000)));
    /* rule_count = total defined (including disabled).
     * rules.size() = active only. Both are surfaced via /api/version. */
    const std::size_t rule_count = static_cast<std::size_t>(loaded_rules);

    mc::HealthModel model(hourly_rate, dev_min_per_line, sc);
    mc::Analyzer analyzer(rules, model, excludes, max_file_bytes,
                          dup_block_lines, long_line_columns);
    analyzer.set_incremental(cfg.get_bool("analysis.incremental", true));

    /* Snapshot: one data point per scan, stored in the project history ring. */
    struct Snapshot {
        std::string timestamp;
        std::size_t file_count = 0;
        std::size_t issue_count = 0;
        double tqi = 0.0;
        char grade = 'F';
        std::size_t duplicate_blocks = 0;
    };

    /* Per-project state: each project has its own result, license summary,
     * history ring, and mutex. The active_project index is atomically updated
     * by POST /api/project/select. */
    struct ProjectState {
        std::string name;
        std::string root;
        mc::AnalysisResult latest;
        mc::license::LicenseSummary latest_licenses;
        std::mutex mtx;
        std::vector<Snapshot> history;
        /* non-copyable due to mutex */
        ProjectState() = default;
        ProjectState(const ProjectState&) = delete;
        ProjectState& operator=(const ProjectState&) = delete;
        ProjectState(ProjectState&&) = default;
        ProjectState& operator=(ProjectState&&) = default;
    };

    std::vector<std::unique_ptr<ProjectState>> projects;
    for (const auto& pd : project_defs) {
        auto p = std::make_unique<ProjectState>();
        p->name = pd.name;
        p->root = pd.root;
        projects.push_back(std::move(p));
    }
    std::atomic<std::size_t> active_project{0};

    /* Convenience accessor: returns reference to the currently active project. */
    auto get_project = [&]() -> ProjectState& {
        return *projects[active_project.load()];
    };

    /* Scan history ring: records the summary of each scan for trend display.
     * max_history is configurable (ui.history_max); issues/files are not stored
     * per-snapshot to keep memory bounded. */
    const std::size_t max_history = static_cast<std::size_t>(
        cfg.get_int("ui.history_max", 50));
    const std::string history_file = cfg.get_str("ui.history_file", "");

    /* Snapshot to single JSON line (NDJSON). */
    auto snapshot_to_ndjson = [](const Snapshot& s) -> std::string {
        std::ostringstream o;
        o << "{\"timestamp\":\"" << s.timestamp << "\","
          << "\"file_count\":" << s.file_count << ","
          << "\"issue_count\":" << s.issue_count << ","
          << "\"tqi\":" << s.tqi << ","
          << "\"grade\":\"" << std::string(1, s.grade) << "\","
          << "\"duplicate_blocks\":" << s.duplicate_blocks << "}";
        return o.str();
    };

    /* Load persisted history from file on startup. */
    if (!history_file.empty()) {
        std::ifstream hf(history_file);
        std::string ln;
        while (std::getline(hf, ln) && get_project().history.size() < max_history) {
            if (ln.empty()) continue;
            auto get = [&](const std::string& key) -> std::string {
                std::string k = "\"" + key + "\":";
                std::size_t p = ln.find(k);
                if (p == std::string::npos) return "";
                p += k.size();
                if (ln[p] == '"') {
                    std::size_t e = ln.find('"', p + 1);
                    return (e != std::string::npos) ? ln.substr(p + 1, e - p - 1) : "";
                }
                std::size_t e = ln.find_first_of(",}", p);
                return (e != std::string::npos) ? ln.substr(p, e - p) : "";
            };
            Snapshot s;
            s.timestamp = get("timestamp");
            try { s.file_count = static_cast<std::size_t>(std::stoul(get("file_count"))); } catch (...) { /* malformed NDJSON field */ }
            try { s.issue_count = static_cast<std::size_t>(std::stoul(get("issue_count"))); } catch (...) { /* malformed NDJSON field */ }
            try { s.tqi = std::stod(get("tqi")); } catch (...) { /* malformed NDJSON field */ }
            std::string g = get("grade");
            s.grade = g.empty() ? 'F' : g[0];
            try { s.duplicate_blocks = static_cast<std::size_t>(std::stoul(get("duplicate_blocks"))); } catch (...) { /* malformed NDJSON field */ }
            if (!s.timestamp.empty()) get_project().history.push_back(s);
        }
        if (!get_project().history.empty())
            logger.info("Loaded " + std::to_string(get_project().history.size()) + " history entries from " + history_file);
    }

    /* Webhook config. */
    const bool webhook_enabled = cfg.get_bool("webhook.enabled", false);
    const std::string webhook_url = cfg.get_str("webhook.url", "");
    const std::string webhook_grade = cfg.get_str("webhook.on_grade_below", "");
    const int webhook_timeout = static_cast<int>(cfg.get_int("webhook.timeout_seconds", 10));
    (void)webhook_timeout;

    /* Watch mode config. */
    const bool scan_watch = cfg.get_bool("scan.watch", false);
    const int watch_interval = static_cast<int>(cfg.get_int("scan.watch_interval_seconds", 30));

    auto push_snapshot = [&](const mc::AnalysisResult& r) {
        Snapshot s;
        auto now = std::chrono::system_clock::now();
        std::time_t t = std::chrono::system_clock::to_time_t(now);
        std::tm tmv{};
#ifdef _WIN32
        gmtime_s(&tmv, &t);
#else
        gmtime_r(&t, &tmv);
#endif
        std::ostringstream ts;
        ts << (tmv.tm_year + 1900) << '-'
           << (tmv.tm_mon + 1 < 10 ? "0" : "") << (tmv.tm_mon + 1) << '-'
           << (tmv.tm_mday   < 10 ? "0" : "") << tmv.tm_mday   << 'T'
           << (tmv.tm_hour   < 10 ? "0" : "") << tmv.tm_hour   << ':'
           << (tmv.tm_min    < 10 ? "0" : "") << tmv.tm_min    << ':'
           << (tmv.tm_sec    < 10 ? "0" : "") << tmv.tm_sec    << 'Z';
        s.timestamp = ts.str();
        s.file_count = r.report.file_count;
        s.issue_count = r.report.issue_count;
        s.tqi = r.report.health.tqi;
        s.grade = mc::grade_for(r.report.health.tqi, model.config());
        s.duplicate_blocks = r.report.duplicate_blocks;
        if (get_project().history.size() >= max_history) get_project().history.erase(get_project().history.begin());
        get_project().history.push_back(s);
        /* Persist to NDJSON file if configured. */
        if (!history_file.empty()) {
            std::ofstream hf(history_file, std::ios::app);
            hf << snapshot_to_ndjson(s) << '\n';
        }
        /* Fire webhook if configured and grade threshold is met. */
        if (webhook_enabled && !webhook_url.empty()) {
            bool fire = true;
            if (!webhook_grade.empty() && !webhook_grade.empty()) {
                /* "below" means numerically worse: F<D<C<B<A. */
                static const char* order = "FDCBA";
                const char* pg = std::strchr(order, s.grade);
                const char* tg = std::strchr(order, webhook_grade[0]);
                fire = (pg && tg && pg <= tg);
            }
            if (fire) {
                std::string body = "{\"grade\":\"" + std::string(1, s.grade) +
                    "\",\"tqi\":" + std::to_string(s.tqi) +
                    ",\"issues\":" + std::to_string(s.issue_count) +
                    ",\"files\":" + std::to_string(s.file_count) + "}";
                mc::ai::AiOptions wo;
                wo.enabled = true;
                wo.endpoint = webhook_url;
                wo.api_key = "";
                wo.model = "webhook";
                wo.system_prompt = "";
                wo.max_output_tokens = 64;
                wo.timeout_seconds = webhook_timeout;
                /* Fire and forget on a detached thread. */
                std::thread([wo, body]() mutable {
                    wo.system_prompt = body;
                    mc::ai::summarize(wo, body);
                }).detach();
            }
        }
    };

    logger.info(std::string(mc::product) + " " + mc::version + " starting");
    if (auth_enabled && auth_hash.empty()) {
        logger.warn("auth.enabled is true but auth.password_sha256 is empty; "
                    "login will reject all credentials");
    }

    if (scan_on_start) {
        for (std::size_t i = 0; i < projects.size(); ++i) {
            logger.info("Initial scan of '" + projects[i]->root +
                        "' (project: " + projects[i]->name + ")");
            mc::AnalysisResult r = analyzer.analyze(projects[i]->root);
            {
                std::lock_guard<std::mutex> lk(projects[i]->mtx);
                /* push_snapshot reads get_project() so temporarily set active */
                active_project.store(i);
                push_snapshot(r);
                projects[i]->latest_licenses = mc::license::scan(r.files, projects[i]->root);
                projects[i]->latest = std::move(r);
            }
            logger.info("Scanned " + std::to_string(projects[i]->latest.report.file_count) +
                        " files, " + std::to_string(projects[i]->latest.report.issue_count) +
                        " issues, " + std::to_string(projects[i]->latest.report.duplicate_blocks) +
                        " duplicate blocks");
            if (gate_enabled && i == 0) {
                GateResult gr = eval_gate(projects[i]->latest);
                logger.info(std::string("Gate: ") + (gr.passed ? "PASS" : "FAIL") +
                            (gr.passed ? "" : " -- " + gr.reason));
                if (!gr.passed && gate_exit_nonzero) {
                    logger.warn("Exiting with code 1 due to gate failure");
                    return 1;
                }
            }
        }
        active_project.store(0);
    }

    mc::http::Server server(host, port, static_dir, tls);
    server.set_limits(max_header_bytes, listen_backlog,
                      static_cast<int>(cfg.get_int("server.recv_chunk_bytes", 8192)));
    server.set_index_document(cfg.get_str("server.index_doc", "index.html"));
    g_server = &server;

    /* Returns true if the request is authorized (or auth is disabled).
     * Accepts either a valid session cookie or an X-API-Key header when
     * api_key_enabled is true. */
    auto authorized = [&](const mc::http::Request& req) -> bool {
        if (!auth_enabled) return true;
        /* Session cookie path. */
        std::string user;
        if (sessions.valid(cookie_value(req, "mcsid"), user)) return true;
        /* X-API-Key path (headless/CI use). */
        if (api_key_enabled && !api_key_hash.empty()) {
            auto it = req.headers.find("x-api-key");
            if (it != req.headers.end() &&
                mc::Sha256::hex(it->second) == api_key_hash) return true;
        }
        return false;
    };
    auto unauthorized = [&](const mc::http::Request& req) {
        logger.warn("Unauthorized request: " + req.method + " " + req.path);
        mc::http::Response res;
        res.status = 401;
        res.body = R"({"error":"authentication required"})";
        return res;
    };

    server.route("GET", "/api/version", [rule_count, &rules](const mc::http::Request&) {
        mc::json::Value o = mc::json::Value::object();
        o.set("product", mc::product);
        o.set("version", mc::version);
        o.set("rules",        static_cast<long long>(rule_count));
        o.set("rules_active", static_cast<long long>(rules.size()));
        return json_response(o);
    });

    /* Serve the OpenAPI spec from docs/openapi.yaml (open, no auth needed). */
    server.route("GET", "/docs/openapi.yaml", [](const mc::http::Request&) {
        mc::http::Response res;
        res.content_type = "application/yaml";
        std::ifstream in(std::filesystem::path("docs") / "openapi.yaml");
        if (!in) { res.status = 404; res.body = "Not found"; return res; }
        std::ostringstream ss; ss << in.rdbuf();
        res.body = ss.str();
        return res;
    });

    server.route("GET", "/api/health", [](const mc::http::Request&) {
        mc::http::Response res;
        res.body = R"({"status":"ok"})";
        return res;
    });

    /* Directory browser for the scan-root picker.
     * GET /api/browse?path=<dir>  -- lists immediate subdirectories.
     * Returns: {"path":"...","parent":"...","dirs":["name",...]}
     * On Windows also returns {"drives":["C:/","D:/",...]} at the root. */
    server.route("GET", "/api/browse", [&](const mc::http::Request& req) {
        if (!authorized(req)) return unauthorized(req);
        namespace fs = std::filesystem;
        std::string raw;
        std::size_t p = req.query.find("path=");
        if (p != std::string::npos) {
            raw = url_decode(req.query.substr(p + 5));
            std::size_t amp = raw.find('&'); if (amp != std::string::npos) raw.resize(amp);
        }
        /* Default to filesystem root or home directory. */
        if (raw.empty() || raw == "/") {
#ifdef _WIN32
            /* On Windows enumerate drive letters. */
            mc::json::Value o = mc::json::Value::object();
            o.set("path",   "");
            o.set("parent", "");
            mc::json::Value drives = mc::json::Value::array();
            for (char c = 'A'; c <= 'Z'; ++c) {
                std::string dr; dr += c; dr += ":/";
                std::error_code ec;
                if (fs::exists(dr, ec)) drives.push_back(mc::json::Value(dr));
            }
            o.set("drives", drives);
            o.set("dirs", mc::json::Value::array());
            return json_response(o);
#else
            raw = "/";
#endif
        }
        fs::path dir(raw);
        std::error_code ec;
        if (!fs::exists(dir, ec) || !fs::is_directory(dir, ec)) {
            mc::http::Response res; res.status = 404;
            res.body = R"({"error":"not a directory"})"; return res;
        }
        /* Build parent path. */
        std::string parent = dir.parent_path().generic_string();
        if (parent == raw) parent = ""; /* already at root */
        mc::json::Value o = mc::json::Value::object();
        o.set("path",   dir.generic_string());
        o.set("parent", parent);
        mc::json::Value dirs = mc::json::Value::array();
        for (const auto& entry : fs::directory_iterator(dir, ec)) {
            if (ec) { ec.clear(); continue; }
            std::error_code ec2;
            if (!entry.is_directory(ec2)) continue;
            std::string name = entry.path().filename().generic_string();
            if (name.empty() || name[0] == '.') continue; /* skip hidden */
            dirs.push_back(mc::json::Value(name));
        }
        o.set("dirs", dirs);
        return json_response(o);
    });

    /* Password hash utility.
     * POST /api/hash-password  body: {"value":"your-password"}  (recommended)
     * GET  /api/hash-password?value=<text>                       (note: '#' in
     *   value must be URL-encoded as %23; browsers strip '#' and everything after
     *   it as a URL fragment, so GET will silently truncate passwords containing '#').
     * Both forms return {"hash":"<sha256-hex>"} and require no authentication. */
    server.route("POST", "/api/hash-password", [](const mc::http::Request& req) {
        std::string val = json_string_field(req.body, "value");
        mc::json::Value o = mc::json::Value::object();
        o.set("hash", mc::Sha256::hex(val));
        mc::http::Response res; res.body = o.dump(); return res;
    });
    server.route("GET", "/api/hash-password", [](const mc::http::Request& req) {
        std::size_t p = req.query.find("value=");
        std::string val = (p != std::string::npos) ? url_decode(req.query.substr(p + 6)) : "";
        mc::json::Value o = mc::json::Value::object();
        o.set("hash", mc::Sha256::hex(val));
        o.set("note", "If your password contains '#', URL-encode it as %23, "
                      "or use POST /api/hash-password with a JSON body instead.");
        mc::http::Response res; res.body = o.dump(); return res;
    });

    server.route("GET", "/api/security", [&](const mc::http::Request& req) {
        mc::http::Server::TlsStatus ts = server.tls_status();
        std::string g = req.tls_group;
        std::string c = req.tls_cipher;
        /* Log cipher+group once so it's visible in the activity log. */
        if (req.tls && !c.empty()) {
            static bool logged = false;
            if (!logged) {
                logged = true;
                logger.info("TLS cipher: " + c + "  group: " + g);
            }
        }
        bool pqc = (g.find("MLKEM") != std::string::npos) ||
                   (g.find("mlkem") != std::string::npos) ||
                   (g.find("Kyber") != std::string::npos) ||
                   (g.find("KYBER") != std::string::npos);
        std::string cu = c;
        for (char& ch : cu) ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
        bool aes256gcm = req.tls && (cu.find("AES_256_GCM") != std::string::npos ||
                                     cu.find("AES256-GCM") != std::string::npos ||
                                     cu.find("AES-256-GCM") != std::string::npos);
        mc::json::Value o = mc::json::Value::object();
        o.set("tls", req.tls);
        o.set("tls_configured", ts.enabled);
        o.set("group", g);
        o.set("cipher", c);
        o.set("post_quantum", pqc);
        o.set("aes_256_gcm", aes256gcm);
        o.set("detail", ts.detail);
        /* v2.2.0: protocol label and cert CN for the TLS detail panel. */
        o.set("protocol", req.tls ? std::string("TLS 1.3") : std::string(""));
        o.set("cert_cn", cfg.get_str("tls.cert_cn", "Metis Code Analyzer Plus"));
        return json_response(o);
    });

    server.route("GET", "/api/languages", [](const mc::http::Request&) {
        mc::json::Value arr = mc::json::Value::array();
        for (const mc::LanguageInfo& li : mc::supported_languages()) {
            mc::json::Value o = mc::json::Value::object();
            o.set("name", li.name);
            mc::json::Value ex = mc::json::Value::array();
            for (const std::string& e : li.extensions) ex.push_back(e);
            o.set("extensions", ex);
            arr.push_back(o);
        }
        return json_response(arr);
    });

    /* Session state: open so the dashboard can decide login vs. content. */
    server.route("GET", "/api/session", [&](const mc::http::Request& req) {
        std::string user;
        bool ok = !auth_enabled ||
                  sessions.valid(cookie_value(req, "mcsid"), user);
        mc::json::Value o = mc::json::Value::object();
        o.set("authenticated", ok);
        o.set("auth_required", auth_enabled);
        o.set("username", auth_enabled ? user : std::string("anonymous"));
        return json_response(o);
    });

    server.route("POST", "/api/login", [&](const mc::http::Request& req) {
        mc::http::Response res;
        std::string u = json_string_field(req.body, "username");
        std::string p = json_string_field(req.body, "password");
        std::string key = u.empty() ? "_" : u;
        long now = static_cast<long>(std::time(nullptr));
        if (throttle.blocked(key, now)) {
            logger.warn("Login throttled for user '" + u + "'");
            res.status = 429;
            res.extra_headers["Retry-After"] = std::to_string(login_window);
            res.body = R"({"ok":false,"error":"too many attempts; try again later"})";
            return res;
        }
        bool ok = (u == auth_user) && !auth_hash.empty() &&
                  (mc::Sha256::hex(p) == auth_hash);
        if (!ok) {
            throttle.fail(key, now);
            logger.warn("Failed login attempt for user '" + u + "'");
            res.status = 401;
            res.body = R"({"ok":false,"error":"invalid credentials"})";
            return res;
        }
        throttle.reset(key);
        std::string token = sessions.create(u);
        logger.info("User '" + u + "' signed in");
        std::string cookie = "mcsid=" + token +
            "; Path=/; HttpOnly; SameSite=Lax; Max-Age=" +
            std::to_string(session_minutes * 60);
        if (req.tls) cookie += "; Secure";
        res.extra_headers["Set-Cookie"] = cookie;
        res.body = R"({"ok":true})";
        return res;
    });

    server.route("POST", "/api/logout", [&](const mc::http::Request& req) {
        std::string token = cookie_value(req, "mcsid");
        sessions.destroy(token);
        logger.info("User signed out");
        mc::http::Response res;
        res.extra_headers["Set-Cookie"] =
            "mcsid=; Path=/; HttpOnly; SameSite=Strict; Max-Age=0";
        res.body = R"({"ok":true})";
        return res;
    });

    /* List all configured projects with their name, root, and latest grade. */
    /* Quality gate evaluation. */
    server.route("GET", "/api/gate", [&](const mc::http::Request& req) {
        if (!authorized(req)) return unauthorized(req);
        std::lock_guard<std::mutex> lk(get_project().mtx);
        GateResult gr = eval_gate(get_project().latest);
        mc::json::Value o = mc::json::Value::object();
        o.set("enabled", gate_enabled);
        o.set("passed",  gr.passed);
        o.set("reason",  gr.reason);
        o.set("tqi",     gr.tqi);
        o.set("issues",  static_cast<long long>(gr.issues));
        o.set("critical",static_cast<long long>(gr.critical));
        o.set("min_tqi", gate_min_tqi);
        o.set("max_issues",   static_cast<long long>(gate_max_issues));
        o.set("max_critical", static_cast<long long>(gate_max_critical));
        return json_response(o);
    });

    /* Technology / framework inventory. */
    server.route("GET", "/api/technologies", [&](const mc::http::Request& req) {
        if (!authorized(req)) return unauthorized(req);
        std::lock_guard<std::mutex> lk(get_project().mtx);
        auto techs = mc::tech::detect(get_project().latest);
        mc::json::Value arr = mc::json::Value::array();
        for (const auto& t : techs) {
            mc::json::Value o = mc::json::Value::object();
            o.set("name",       t.name);
            o.set("category",   t.category);
            o.set("file_count", static_cast<long long>(t.file_count));
            arr.push_back(o);
        }
        return json_response(arr);
    });

    /* Issue lifecycle: GET status for an issue key (rule_id:file:line). */
    server.route("GET", "/api/issue/status", [&](const mc::http::Request& req) {
        if (!authorized(req)) return unauthorized(req);
        std::size_t kp = req.query.find("key=");
        if (kp == std::string::npos) {
            mc::http::Response res; res.status = 400;
            res.body = R"({"error":"key parameter required"})"; return res;
        }
        std::string key = req.query.substr(kp + 4);
        std::size_t amp = key.find('&'); if (amp != std::string::npos) key.resize(amp);
        std::lock_guard<std::mutex> lk(issue_status_mtx);
        auto it = issue_statuses.find(key);
        mc::json::Value o = mc::json::Value::object();
        o.set("key",    key);
        o.set("status", it == issue_statuses.end() ? "open" : it->second.status);
        o.set("note",   it == issue_statuses.end() ? ""     : it->second.note);
        return json_response(o);
    });

    /* Issue lifecycle: set status. Body: {"key":"...","status":"...","note":"..."} */
    server.route("POST", "/api/issue/status", [&](const mc::http::Request& req) {
        if (!authorized(req)) return unauthorized(req);
        std::string key    = json_string_field(req.body, "key");
        std::string status = json_string_field(req.body, "status");
        std::string note   = json_string_field(req.body, "note");
        static const char* valid[] = {"open","accepted","wontfix","false_positive",nullptr};
        bool ok = false;
        for (const char** v = valid; *v; ++v) if (status == *v) { ok = true; break; }
        if (key.empty() || !ok) {
            mc::http::Response res; res.status = 400;
            res.body = R"({"error":"key and valid status required"})"; return res;
        }
        {
            std::lock_guard<std::mutex> lk(issue_status_mtx);
            issue_statuses[key] = {key, status, note};
        }
        save_issue_statuses(); /* v2.5.0: persist immediately */
        logger.info("Issue status set: " + key + " -> " + status);
        mc::http::Response res; res.body = R"({"ok":true})"; return res;
    });

    server.route("GET", "/api/projects", [&](const mc::http::Request& req) {
        if (!authorized(req)) return unauthorized(req);
        mc::json::Value arr = mc::json::Value::array();
        for (std::size_t i = 0; i < projects.size(); ++i) {
            std::lock_guard<std::mutex> lk(projects[i]->mtx);
            mc::json::Value o = mc::json::Value::object();
            o.set("name", projects[i]->name);
            o.set("root", projects[i]->root);
            o.set("active", (i == active_project.load()));
            char g = mc::grade_for(projects[i]->latest.report.health.tqi, model.config());
            o.set("grade", std::string(1, g));
            o.set("tqi", projects[i]->latest.report.health.tqi);
            o.set("issues", static_cast<long long>(projects[i]->latest.report.issue_count));
            arr.push_back(o);
        }
        return json_response(arr);
    });

    /* Switch the active project. POST body: {"name":"..."} */
    server.route("POST", "/api/project/select", [&](const mc::http::Request& req) {
        if (!authorized(req)) return unauthorized(req);
        std::string name = json_string_field(req.body, "name");
        mc::http::Response res;
        for (std::size_t i = 0; i < projects.size(); ++i) {
            if (projects[i]->name == name) {
                active_project.store(i);
                logger.info("Active project switched to '" + name + "'");
                res.body = R"({"ok":true})";
                return res;
            }
        }
        res.status = 404;
        res.body = R"({"ok":false,"error":"project not found"})";
        return res;
    });

    server.route("GET", "/api/config", [&](const mc::http::Request&) {
        mc::json::Value o = mc::json::Value::object();
        o.set("scan_root", get_project().root);
        o.set("static_dir", static_dir);
        o.set("hourly_rate", hourly_rate);
        o.set("dev_minutes_per_line", dev_min_per_line);
        o.set("rules", static_cast<long long>(rule_count));
        o.set("auth_required", auth_enabled);
        o.set("default_locale", default_locale);
        mc::json::Value gt = mc::json::Value::object();
        gt.set("a", model.config().grade_a);
        gt.set("b", model.config().grade_b);
        gt.set("c", model.config().grade_c);
        gt.set("d", model.config().grade_d);
        o.set("grade_thresholds", gt);
        mc::json::Value loc = mc::json::Value::array();
        for (const std::string& l : locales) loc.push_back(l);
        o.set("locales", loc);
        mc::json::Value ex = mc::json::Value::array();
        for (const std::string& e : excludes) ex.push_back(e);
        o.set("exclude", ex);
        o.set("max_table_rows", cfg.get_int("ui.max_table_rows", 500));
        o.set("ai_enabled", ai.enabled);
        /* v2.2.0: expose ai sub-object for the AI Configuration dashboard panel. */
        mc::json::Value ai_obj = mc::json::Value::object();
        ai_obj.set("enabled", ai.enabled);
        ai_obj.set("provider", ai.provider);
        ai_obj.set("model", ai.model);
        ai_obj.set("max_output_tokens", static_cast<long long>(cfg.get_int("ai.max_output_tokens", 1024)));
        ai_obj.set("timeout_seconds",   static_cast<long long>(cfg.get_int("ai.timeout_seconds", 30)));
        ai_obj.set("max_report_issues", static_cast<long long>(cfg.get_int("ai.max_report_issues", 15)));
        o.set("ai", ai_obj);
        return json_response(o);
    });

    server.route("POST", "/api/scan", [&](const mc::http::Request& req) {
        if (!authorized(req)) return unauthorized(req);
        std::string root = get_project().root;
        std::size_t rp = req.body.find("\"root\"");
        if (rp != std::string::npos) {
            std::size_t q1 = req.body.find('"', req.body.find(':', rp) + 1);
            std::size_t q2 = (q1 == std::string::npos)
                                 ? std::string::npos : req.body.find('"', q1 + 1);
            if (q1 != std::string::npos && q2 != std::string::npos) {
                root = req.body.substr(q1 + 1, q2 - q1 - 1);
            }
        }
        logger.info("Scan requested for '" + root + "'");
        mc::AnalysisResult r = analyzer.analyze(root);
        {
            std::lock_guard<std::mutex> lk(get_project().mtx);
            push_snapshot(r);
            get_project().latest_licenses = mc::license::scan(r.files, get_project().root);
            get_project().latest = std::move(r);
        }
        std::lock_guard<std::mutex> lk(get_project().mtx);
        logger.info("Scan complete: " + std::to_string(get_project().latest.report.file_count) +
                    " files, " + std::to_string(get_project().latest.report.issue_count) +
                    " issues, " + std::to_string(get_project().latest.report.duplicate_blocks) +
                    " duplicate blocks");
        mc::http::Response res;
        res.body = report_json(get_project().latest, model.config()).dump();
        return res;
    });

    server.route("GET", "/api/results", [&](const mc::http::Request& req) {
        if (!authorized(req)) return unauthorized(req);
        std::lock_guard<std::mutex> lk(get_project().mtx);
        mc::http::Response res;
        res.body = report_json(get_project().latest, model.config()).dump();
        return res;
    });

    server.route("GET", "/api/history", [&](const mc::http::Request& req) {
        if (!authorized(req)) return unauthorized(req);
        std::lock_guard<std::mutex> lk(get_project().mtx);
        mc::json::Value arr = mc::json::Value::array();
        for (const Snapshot& s : get_project().history) {
            mc::json::Value o = mc::json::Value::object();
            o.set("timestamp", s.timestamp);
            o.set("file_count", static_cast<long long>(s.file_count));
            o.set("issue_count", static_cast<long long>(s.issue_count));
            o.set("tqi", s.tqi);
            o.set("grade", std::string(1, s.grade));
            o.set("duplicate_blocks", static_cast<long long>(s.duplicate_blocks));
            arr.push_back(o);
        }
        return json_response(arr);
    });

    server.route("GET", "/api/licenses", [&](const mc::http::Request& req) {
        if (!authorized(req)) return unauthorized(req);
        std::lock_guard<std::mutex> lk(get_project().mtx);
        mc::json::Value o = mc::json::Value::object();
        o.set("files_with_header",    static_cast<long long>(get_project().latest_licenses.files_with_header));
        o.set("files_without_header", static_cast<long long>(get_project().latest_licenses.files_without_header));
        mc::json::Value spdx = mc::json::Value::object();
        for (const auto& kv : get_project().latest_licenses.spdx_counts)
            spdx.set(kv.first, static_cast<long long>(kv.second));
        o.set("spdx", spdx);
        mc::json::Value files = mc::json::Value::array();
        for (const auto& f : get_project().latest_licenses.files) {
            mc::json::Value fo = mc::json::Value::object();
            fo.set("path", f.path);
            fo.set("spdx", f.spdx);
            fo.set("has_copyright", f.has_copyright);
            fo.set("has_header", f.has_header);
            files.push_back(fo);
        }
        o.set("files", files);
        return json_response(o);
    });

    server.route("GET", "/api/files", [&](const mc::http::Request& req) {
        if (!authorized(req)) return unauthorized(req);
        std::lock_guard<std::mutex> lk(get_project().mtx);
        mc::json::Value arr = mc::json::Value::array();
        for (const mc::FileRecord& f : get_project().latest.files) arr.push_back(file_to_json(f));
        return json_response(arr);
    });

    /* Inline source preview: a window of lines around a finding, confined to
     * the scan root (no path traversal). GET /api/source?path=...&line=N */
    server.route("GET", "/api/source", [&](const mc::http::Request& req) {
        if (!authorized(req)) return unauthorized(req);
        auto qp = [&](const std::string& k) -> std::string {
            std::string key = k + "=";
            std::size_t p = req.query.find(key);
            if (p == std::string::npos) return "";
            std::size_t s = p + key.size();
            std::size_t e = req.query.find('&', s);
            return url_decode(req.query.substr(s, e == std::string::npos ? std::string::npos : e - s));
        };
        mc::http::Response res;
        std::string relpath = qp("path");
        int line = 0;
        try { line = std::stoi(qp("line")); } catch (...) { line = 0; }
        if (relpath.empty()) { res.status = 400; res.body = R"({"error":"path required"})"; return res; }

        namespace fs = std::filesystem;
        std::error_code ec;
        fs::path root = fs::weakly_canonical(fs::path(get_project().root), ec);
        if (ec) root = fs::absolute(fs::path(get_project().root));
        fs::path target = fs::weakly_canonical(root / fs::path(relpath), ec);
        std::string rs = root.string(), ts = target.string();
        if (ts.size() < rs.size() || ts.compare(0, rs.size(), rs) != 0) {
            res.status = 403; res.body = R"({"error":"outside scan root"})"; return res;
        }
        std::ifstream in(target);
        if (!in) { res.status = 404; res.body = R"({"error":"not found"})"; return res; }
        std::vector<std::string> lines;
        std::string ln;
        while (std::getline(in, ln)) lines.push_back(ln);
        int total = static_cast<int>(lines.size());
        int ctx = 4;
        int start = (line > 0) ? std::max(1, line - ctx) : 1;
        int end = (line > 0) ? std::min(total, line + ctx) : std::min(total, 9);
        mc::json::Value arr = mc::json::Value::array();
        for (int i = start; i <= end; ++i) {
            mc::json::Value o = mc::json::Value::object();
            o.set("n", static_cast<long long>(i));
            o.set("text", lines[static_cast<std::size_t>(i - 1)]);
            arr.push_back(o);
        }
        mc::json::Value out = mc::json::Value::object();
        out.set("path", relpath);
        out.set("line", static_cast<long long>(line));
        out.set("lines", arr);
        res.body = out.dump();
        return res;
    });

    server.route("GET", "/api/issues", [&](const mc::http::Request& req) {
        if (!authorized(req)) return unauthorized(req);
        std::lock_guard<std::mutex> lk(get_project().mtx);
        mc::json::Value arr = mc::json::Value::array();
        for (const mc::Issue& i : get_project().latest.issues) {
            mc::json::Value o = issue_to_json(i);
            std::string key = i.rule_id + ":" + i.file + ":" + std::to_string(i.line);
            std::lock_guard<std::mutex> slk(issue_status_mtx);
            auto it = issue_statuses.find(key);
            o.set("status", it == issue_statuses.end() ? "open" : it->second.status);
            o.set("note",   it == issue_statuses.end() ? ""     : it->second.note);
            arr.push_back(o);
        }
        return json_response(arr);
    });

    /* AI-assisted summary of the latest report (outbound HTTPS to a configured
     * provider). Requires a TLS-enabled build and ai.enabled = true. */
    server.route("POST", "/api/ai/summarize", [&](const mc::http::Request& req) {
        if (!authorized(req)) return unauthorized(req);
        std::string text;
        {
            std::lock_guard<std::mutex> lk(get_project().mtx);
            const mc::HealthScores& h = get_project().latest.report.health;
            char g = mc::grade_for(h.tqi, model.config());
            text = "Code quality report.\n";
            text += "Files: " + std::to_string(get_project().latest.report.file_count) +
                    ", Issues: " + std::to_string(get_project().latest.report.issue_count) + "\n";
            text += "Total Quality Index: " + std::to_string(static_cast<int>(h.tqi)) +
                    " (grade " + std::string(1, g) + ")\n";
            text += "Security " + std::to_string(static_cast<int>(h.security)) +
                    ", Robustness " + std::to_string(static_cast<int>(h.robustness)) +
                    ", Efficiency " + std::to_string(static_cast<int>(h.efficiency)) +
                    ", Transferability " + std::to_string(static_cast<int>(h.transferability)) +
                    ", Changeability " + std::to_string(static_cast<int>(h.changeability)) +
                    ", Portability " + std::to_string(static_cast<int>(h.portability)) + "\n";
            text += "Top issues:\n";
            const std::size_t ai_max_issues = static_cast<std::size_t>(
                cfg.get_int("ai.max_report_issues", 15));
            std::size_t n = 0;
            for (const mc::Issue& i : get_project().latest.issues) {
                if (n++ >= ai_max_issues) break;
                text += "- [" + std::string(mc::severity_name(i.severity)) + "] " +
                        i.rule_id + " " + i.file + ":" +
                        std::to_string(static_cast<long long>(i.line)) + "\n";
            }
        }
        mc::ai::Result ar = mc::ai::summarize(ai, text);
        mc::json::Value o = mc::json::Value::object();
        o.set("ok", ar.ok);
        o.set("status", static_cast<long long>(ar.status));
        o.set("error", ar.error);
        o.set("response", ar.body);
        mc::http::Response res;
        if (!ar.ok && ar.error.find("disabled") == std::string::npos &&
            ar.error.find("not fully configured") == std::string::npos) {
            res.status = 502;
        }
        res.body = o.dump();
        return res;
    });

    /* ------------------------------------------------------------------ */
    /* v2.5.0: Infrastructure probing - GPU, Kubernetes, Containers.      */
    /* Pure C++20, Windows/Linux/macOS, no Docker, no external deps.      */
    /* ------------------------------------------------------------------ */

    /* GPU detection: probe for CUDA (nvcc/nvidia-smi) and OpenCL (cl.h). */
    auto probe_gpu = [&]() -> mc::json::Value {
        mc::json::Value o = mc::json::Value::object();
        o.set("subsystem", std::string("gpu"));
        bool gpu_enabled = cfg.get_bool("gpu.enabled", false);
        o.set("enabled", gpu_enabled);
        std::string status = "unavailable";
        std::string detail = "No GPU runtime detected.";
        if (gpu_enabled) {
            /* Probe for nvidia-smi on PATH (Windows/Linux/macOS). */
#ifdef _WIN32
            int rc = std::system("nvidia-smi --query-gpu=name --format=csv,noheader >nul 2>&1");
#else
            int rc = std::system("nvidia-smi --query-gpu=name --format=csv,noheader >/dev/null 2>&1");
#endif
            if (rc == 0) {
                status = "available";
                detail = "NVIDIA GPU detected via nvidia-smi.";
            } else {
                /* Probe for cl_platform_id in system headers (OpenCL). */
                std::ifstream clh;
#ifdef _WIN32
                clh.open("C:/Windows/System32/OpenCL.dll", std::ios::binary);
#elif defined(__APPLE__)
                clh.open("/System/Library/Frameworks/OpenCL.framework/OpenCL", std::ios::binary);
#else
                clh.open("/usr/lib/x86_64-linux-gnu/libOpenCL.so.1", std::ios::binary);
                if (!clh.is_open())
                    clh.open("/usr/lib/libOpenCL.so.1", std::ios::binary);
#endif
                if (clh.is_open()) {
                    status = "available";
                    detail = "OpenCL runtime detected.";
                    clh.close();
                } else {
                    status = "planned";
                    detail = "GPU enabled in PSON but no GPU runtime found on this host.";
                }
            }
        } else {
            status = "disabled";
            detail = cfg.get_str("gpu.note",
                "Set gpu.enabled = true to probe for GPU runtimes (NVIDIA / OpenCL).");
        }
        o.set("status", status);
        o.set("note", detail);
        return o;
    };

    /* Kubernetes detection: look for kubeconfig or in-cluster service account. */
    auto probe_kubernetes = [&]() -> mc::json::Value {
        mc::json::Value o = mc::json::Value::object();
        o.set("subsystem", std::string("kubernetes"));
        bool k8s_enabled = cfg.get_bool("kubernetes.enabled", false);
        o.set("enabled", k8s_enabled);
        std::string status = "unavailable";
        std::string detail = "Kubernetes not detected.";
        if (k8s_enabled) {
            bool found = false;
            /* In-cluster: /var/run/secrets/kubernetes.io/serviceaccount/token */
            std::ifstream sa("/var/run/secrets/kubernetes.io/serviceaccount/token");
            if (sa.is_open()) { found = true; detail = "In-cluster: service account token found."; sa.close(); }
            if (!found) {
                /* kubeconfig: KUBECONFIG env or ~/.kube/config */
                const char* kc = std::getenv("KUBECONFIG");
                std::string kcp = kc ? std::string(kc) : "";
                if (kcp.empty()) {
                    const char* home = std::getenv(
#ifdef _WIN32
                        "USERPROFILE"
#else
                        "HOME"
#endif
                    );
                    if (home) kcp = std::string(home) +
#ifdef _WIN32
                        "\\.kube\\config";
#else
                        "/.kube/config";
#endif
                }
                std::ifstream kf(kcp);
                if (kf.is_open()) { found = true; detail = "kubeconfig found at: " + kcp; kf.close(); }
            }
            if (found) { status = "available"; }
            else { status = "unavailable"; detail = "Kubernetes enabled in PSON but no kubeconfig or in-cluster token found."; }
        } else {
            status = "disabled";
            detail = cfg.get_str("kubernetes.note",
                "Set kubernetes.enabled = true to detect Kubernetes kubeconfig.");
        }
        o.set("status", status);
        o.set("note", detail);
        return o;
    };

    /* Container detection: probe for Docker/Podman socket or containerd. */
    auto probe_containers = [&]() -> mc::json::Value {
        mc::json::Value o = mc::json::Value::object();
        o.set("subsystem", std::string("containers"));
        bool ctr_enabled = cfg.get_bool("containers.enabled", false);
        o.set("enabled", ctr_enabled);
        std::string status = "unavailable";
        std::string detail = "No container runtime detected.";
        if (ctr_enabled) {
            bool found = false;
            std::string runtime;
            /* Check for Docker/Podman socket (POSIX) or named pipe (Windows). */
#ifdef _WIN32
            std::ifstream pipe("\\\\.\\pipe\\docker_engine", std::ios::binary); /* metis-suppress SEC-PATH-TRAVERSAL: Windows named pipe path, not a traversal */
            if (pipe.is_open()) { found = true; runtime = "Docker (Windows named pipe)"; pipe.close(); }
#else
            for (const char* path : {"/var/run/docker.sock",
                                     "/run/docker.sock",
                                     "/run/podman/podman.sock",
                                     "/var/run/containerd/containerd.sock"}) {
                std::ifstream s(path);
                if (s.is_open()) { found = true; runtime = path; s.close(); break; }
            }
#endif
            if (found) { status = "available"; detail = "Container runtime socket: " + runtime; }
            else { status = "unavailable"; detail = "Containers enabled in PSON but no container runtime socket found."; }
        } else {
            status = "disabled";
            detail = cfg.get_str("containers.note",
                "Set containers.enabled = true to detect Docker/Podman/containerd.");
        }
        o.set("status", status);
        o.set("note", detail);
        return o;
    };

    /* Pre-probe at startup and cache results (re-probed on each API call). */
    server.route("GET", "/api/gpu",        [&](const mc::http::Request& req) {
        if (!authorized(req)) return unauthorized(req);
        return json_response(probe_gpu());
    });
    server.route("GET", "/api/kubernetes", [&](const mc::http::Request& req) {
        if (!authorized(req)) return unauthorized(req);
        return json_response(probe_kubernetes());
    });
    server.route("GET", "/api/containers", [&](const mc::http::Request& req) {
        if (!authorized(req)) return unauthorized(req);
        return json_response(probe_containers());
    });

    /* ------------------------------------------------------------------ */
    /* v2.5.0: WebSocket endpoint for live scan progress.                  */
    /* ws[s]://localhost:8080/api/scan/ws                                  */
    /* Messages: {"event":"start","root":"..."}                            */
    /*           {"event":"progress","file":"...","n":N,"total":N}         */
    /*           {"event":"done","files":N,"issues":N,"tqi":N}             */
    /* ------------------------------------------------------------------ */
    server.ws_route("/api/scan/ws", [&](mc::http::detail::Conn& conn, const mc::http::Request& req) {
        /* Authenticate via cookie in the upgrade request. */
        auto cook = req.headers.find("cookie");
        bool authed = false;
        std::string user;
        if (cook != req.headers.end()) {
            const std::string& cv = cook->second;
            std::size_t pos = cv.find("mcsid=");
            if (pos != std::string::npos) {
                std::string sid = cv.substr(pos + 6);
                std::size_t semi = sid.find(';');
                if (semi != std::string::npos) sid = sid.substr(0, semi);
                authed = sessions.valid(sid, user);
            }
        }
        if (!authed) {
            mc::http::Server::ws_close(conn);
            return;
        }

        std::string root = get_project().root;
        auto qp = req.query;
        auto rp = qp.find("root=");
        if (rp != std::string::npos) {
            root = qp.substr(rp + 5);
            auto amp = root.find('&');
            if (amp != std::string::npos) root = root.substr(0, amp);
            root = url_decode(root);
        }

        {
            mc::json::Value start_msg = mc::json::Value::object();
            start_msg.set("event", std::string("start"));
            start_msg.set("root", root);
            mc::http::Server::ws_send_text(conn, start_msg.dump());
        }

        logger.info("WebSocket scan started for '" + root + "'");

        mc::AnalysisResult r = analyzer.analyze(root);

        /* Send completion event. */
        mc::json::Value done_obj = mc::json::Value::object();
        done_obj.set("event", std::string("done"));
        done_obj.set("files", static_cast<long long>(r.report.file_count));
        done_obj.set("issues", static_cast<long long>(r.report.issue_count));
        {
            std::ostringstream s; s << std::fixed; s.precision(1); s << r.report.health.tqi;
            double rounded_tqi = 0.0;
            try { rounded_tqi = std::stod(s.str()); } catch (...) { rounded_tqi = r.report.health.tqi; }
            done_obj.set("tqi", rounded_tqi);
        }
        std::string done_msg = done_obj.dump();
        mc::http::Server::ws_send_text(conn, done_msg);

        /* Commit scan results. */
        {
            std::lock_guard<std::mutex> lk(get_project().mtx);
            push_snapshot(r);
            get_project().latest_licenses = mc::license::scan(r.files, root);
            get_project().latest = std::move(r);
        }
        logger.info("WebSocket scan done: " +
            std::to_string(get_project().latest.report.file_count) + " files, " +
            std::to_string(get_project().latest.report.duplicate_blocks) + " duplicate blocks");

        mc::http::Server::ws_close(conn);
    });

    server.route("GET", "/api/report.pdf", [&](const mc::http::Request& req) {
        if (!authorized(req)) return unauthorized(req);
        const std::size_t pdf_max = static_cast<std::size_t>(cfg.get_int("ui.pdf_max_issues", 40));
        std::string pdf_bytes;
        std::string stem;
        {
            std::lock_guard<std::mutex> lk(get_project().mtx);
            pdf_bytes = build_report_pdf(get_project().latest, model.config(), pdf_max);
            stem = download_stem(get_project().latest.scanned_root);
        }
        logger.info("PDF report exported");
        mc::http::Response res;
        res.content_type = "application/pdf";
        res.extra_headers["Content-Disposition"] =
            "attachment; filename=\"" + stem + "-report.pdf\"";
        res.body = std::move(pdf_bytes);
        return res;
    });

    /* Full structured JSON export: all metrics, issues, and file records. */
    server.route("GET", "/api/report.json", [&](const mc::http::Request& req) {
        if (!authorized(req)) return unauthorized(req);
        std::lock_guard<std::mutex> lk(get_project().mtx);
        mc::json::Value root = mc::json::Value::object();
        root.set("report", report_json(get_project().latest, model.config()));
        mc::json::Value files = mc::json::Value::array();
        for (const auto& f : get_project().latest.files) files.push_back(file_to_json(f));
        root.set("files", files);
        mc::json::Value issues = mc::json::Value::array();
        for (const auto& i : get_project().latest.issues) issues.push_back(issue_to_json(i));
        root.set("issues", issues);
        mc::http::Response res;
        res.extra_headers["Content-Disposition"] =
            "attachment; filename=\"" +
            download_stem(get_project().latest.scanned_root) + "-report.json\"";
        res.body = root.dump();
        return res;
    });

    /* Issues as CSV: severity, rule_id, cwe, file, line, title, minutes. */
    server.route("GET", "/api/report.csv", [&](const mc::http::Request& req) {
        if (!authorized(req)) return unauthorized(req);
        std::lock_guard<std::mutex> lk(get_project().mtx);
        std::ostringstream csv;
        /* Provenance preamble: identify the scanned source, matching the
           PDF "Scanned root:" line and the JSON "scanned_root" field. The
           leading '#' marks these as comments so the issues table below
           remains a clean, single-table CSV for tools that honor it. */
        csv << "# " << mc::product << " " << mc::version
            << " - code quality report\n";
        csv << "# Scanned root: " << get_project().latest.scanned_root << "\n";
        csv << "severity,rule_id,cwe,file,line,title,remediation_minutes\n";
        for (const auto& i : get_project().latest.issues) {
            csv << csv_escape(mc::severity_name(i.severity)) << ","
                << csv_escape(i.rule_id) << "," << csv_escape(i.cwe) << ","
                << csv_escape(i.file) << "," << i.line << ","
                << csv_escape(i.title) << "," << i.remediation_minutes << "\n";
        }
        mc::http::Response res;
        res.content_type = "text/csv; charset=utf-8";
        res.extra_headers["Content-Disposition"] =
            "attachment; filename=\"" +
            download_stem(get_project().latest.scanned_root) + "-issues.csv\"";
        res.body = csv.str();
        return res;
    });

    server.route("GET", "/api/logs", [&](const mc::http::Request& req) {
        if (!authorized(req)) return unauthorized(req);
        std::size_t deflim = static_cast<std::size_t>(cfg.get_int("log.default_limit", 200));
        std::size_t limit = deflim;
        std::size_t lp = req.query.find("limit=");
        if (lp != std::string::npos) {
            try { limit = static_cast<std::size_t>(std::stoul(req.query.substr(lp + 6))); }
            catch (...) { limit = deflim; }
        }
        mc::json::Value arr = mc::json::Value::array();
        for (const mc::LogEntry& e : logger.recent(limit)) arr.push_back(log_to_json(e));
        return json_response(arr);
    });

    server.route("POST", "/api/logs/clear", [&](const mc::http::Request& req) {
        if (!authorized(req)) return unauthorized(req);
        std::size_t n = logger.clear();
        logger.info("Log buffer cleared (" + std::to_string(n) + " entries removed)");
        mc::http::Response res;
        res.body = std::string(R"({"ok":true,"cleared":)") + std::to_string(n) + "}";
        return res;
    });

    server.route("GET", "/metrics", [&](const mc::http::Request&) {
        std::lock_guard<std::mutex> lk(get_project().mtx);
        mc::http::Response res;
        res.content_type = "text/plain; version=0.0.4";
        res.body = prometheus_metrics(get_project().latest);
        return res;
    });

    server.route("POST", "/api/shutdown", [&](const mc::http::Request& req) {
        if (!authorized(req)) return unauthorized(req);
        logger.warn("Shutdown requested via API");
        mc::http::Response res;
        res.body = R"({"status":"shutting down"})";
        std::thread([&] {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            server.stop();
        }).detach();
        return res;
    });

    std::signal(SIGINT, on_signal);
    std::signal(SIGTERM, on_signal);
#ifndef _WIN32
    /* A client disconnecting mid-response makes send() raise SIGPIPE, whose
     * default action terminates the process. Ignore it on POSIX (Linux/macOS);
     * send() then returns an error we already handle. */
    std::signal(SIGPIPE, SIG_IGN);
#endif

    if (!server.start()) {
        logger.error("Failed to start on " + host + ":" + std::to_string(port) +
                     " (" + server.tls_status().detail + ")");
        return 1;
    }
    {
        mc::http::Server::TlsStatus ts = server.tls_status();
        const char* eff_scheme = (ts.enabled && ts.active) ? "https" : "http";
        if (ts.enabled && ts.active) {
            logger.info("TLS enabled (" + ts.detail + ")");
        } else if (ts.enabled) {
            logger.warn("TLS requested but not active: " + ts.detail);
        } else {
            logger.info("TLS disabled (plain HTTP)");
        }
        logger.info(std::string(mc::product) + " " + mc::version +
                    " listening on " + eff_scheme + "://" + host + ":" + std::to_string(port));
    }

    /* Watch thread: polls the scan root for file changes and triggers a
     * rescan when any file modification time changes. */
    std::thread watch_thread;
    if (scan_watch && watch_interval > 0) {
        logger.info("Watch mode enabled (interval " + std::to_string(watch_interval) + "s)");
        watch_thread = std::thread([&]() {
            namespace fs = std::filesystem;
            /* Snapshot mtime map: path -> last_write_time. */
            std::map<std::string, fs::file_time_type> mtimes;
            auto collect = [&]() {
                std::map<std::string, fs::file_time_type> cur;
                std::error_code ec;
                for (auto it = fs::recursive_directory_iterator(
                         fs::path(get_project().root),
                         fs::directory_options::skip_permission_denied, ec);
                     it != fs::recursive_directory_iterator();
                     it.increment(ec)) {
                    if (ec) { ec.clear(); continue; }
                    if (!it->is_regular_file(ec)) continue;
                    auto mtime = it->last_write_time(ec);
                    if (!ec) cur[it->path().string()] = mtime;
                }
                return cur;
            };
            mtimes = collect();
            while (g_server) {
                std::this_thread::sleep_for(
                    std::chrono::seconds(watch_interval));
                if (!g_server) break;
                auto cur = collect();
                bool changed = (cur != mtimes);
                if (changed) {
                    mtimes = cur;
                    logger.info("Watch: changes detected, rescanning");
                    mc::AnalysisResult r = analyzer.analyze(get_project().root);
                    {
                        std::lock_guard<std::mutex> lk(get_project().mtx);
                        push_snapshot(r);
                        get_project().latest = std::move(r);
                    }
                    logger.info("Watch rescan complete: " +
                                std::to_string(get_project().latest.report.file_count) +
                                " files, " +
                                std::to_string(get_project().latest.report.issue_count) + " issues");
                }
            }
        });
        watch_thread.detach();
    }

    server.run();
    logger.info("Server stopped");
    return 0;
}
