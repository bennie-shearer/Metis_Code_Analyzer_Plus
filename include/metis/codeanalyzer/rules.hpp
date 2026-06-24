/* Metis Code Analyzer Plus - issue detection rule engine
 * Regex-based rules, each mapped to a CWE id, severity, health factor, and
 * remediation effort (minutes). Rules can be toggled via PSON config.
 * Header-only, 7-bit ASCII, C++20.
 */
#ifndef METIS_CODE_ANALYZER_RULES_HPP
#define METIS_CODE_ANALYZER_RULES_HPP

#include <regex>
#include <string>
#include <vector>

#include "metis/codeanalyzer/scanner.hpp"

namespace metis::codeanalyzer {

enum class Severity { Info, Minor, Major, Critical };

inline const char* severity_name(Severity s) {
    switch (s) {
    case Severity::Info:     return "info";
    case Severity::Minor:    return "minor";
    case Severity::Major:    return "major";
    case Severity::Critical: return "critical";
    }
    return "info";
}

enum class HealthFactor { Robustness, Security, Efficiency, Transferability, Changeability, Portability };

inline Severity severity_from(const std::string& s) {
    if (s == "critical") return Severity::Critical;
    if (s == "major")    return Severity::Major;
    if (s == "minor")    return Severity::Minor;
    return Severity::Info;
}

inline const char* health_name(HealthFactor h) {
    switch (h) {
    case HealthFactor::Robustness:      return "robustness";
    case HealthFactor::Security:        return "security";
    case HealthFactor::Efficiency:      return "efficiency";
    case HealthFactor::Transferability: return "transferability";
    case HealthFactor::Changeability:   return "changeability";
    case HealthFactor::Portability:     return "portability";
    }
    return "robustness";
}

inline HealthFactor factor_from(const std::string& s) {
    if (s == "security")        return HealthFactor::Security;
    if (s == "efficiency")      return HealthFactor::Efficiency;
    if (s == "transferability") return HealthFactor::Transferability;
    if (s == "changeability")   return HealthFactor::Changeability;
    if (s == "portability")     return HealthFactor::Portability;
    return HealthFactor::Robustness;
}

struct Rule {
    std::string id;
    std::string title;
    std::string cwe;             /* e.g. "CWE-798"; empty if not applicable */
    Severity severity;
    HealthFactor factor;
    int remediation_minutes;
    std::regex pattern;
    std::string raw_pattern;
};

struct Issue {
    std::string rule_id;
    std::string title;
    std::string cwe;
    Severity severity;
    HealthFactor factor;
    int remediation_minutes;
    std::string file;
    std::size_t line;
    std::string snippet;
};

class RuleSet {
public:
    void add(const std::string& id, const std::string& title, const std::string& cwe,
             Severity sev, HealthFactor factor, int minutes, const std::string& pattern) {
        Rule r;
        r.id = id;
        r.title = title;
        r.cwe = cwe;
        r.severity = sev;
        r.factor = factor;
        r.remediation_minutes = minutes;
        r.raw_pattern = pattern;
        try {
            r.pattern = std::regex(pattern, std::regex::ECMAScript);
            rules_.push_back(std::move(r));
        } catch (const std::regex_error&) {
            /* Skip malformed rule patterns rather than abort the scan. */
        }
    }

    std::vector<Issue> apply(const SourceFile& f) const {
        std::vector<Issue> issues;
        std::size_t line_no = 0;
        std::string line;
        /* Preprocessor guard stack for portability rules. Each frame is one of:
         * 0 = neutral, 1 = inside a _WIN32-true region, 2 = inside a _WIN32-false
         * region. PORT-WIN-* are suppressed inside a _WIN32-true region and
         * PORT-POSIX-* inside a _WIN32-false region, since correctly guarded
         * platform code is portable and should not be flagged.
         *
         * platform_guard_depth counts nesting inside any #if/#ifdef/#ifndef that
         * references a known platform token (_WIN32, __APPLE__, __linux__,
         * defined(, _MSC_VER, __GNUC__). CHG-MAGIC-PATH and SEC-SYSTEM-CALL are
         * suppressed inside such guards because platform-probe code legitimately
         * references OS-specific paths and shell commands. */
        std::vector<int> guard;
        int platform_guard_depth = 0;
        auto scan_line = [&](const std::string& ln) {
            ++line_no;
            /* Track #if/#ifdef/#ifndef/#elif/#else/#endif for _WIN32 guards. */
            {
                std::size_t h = ln.find('#');
                bool only_ws = h != std::string::npos;
                for (std::size_t i = 0; i < h && only_ws; ++i)
                    if (ln[i] != ' ' && ln[i] != '\t') only_ws = false;
                if (only_ws) {
                    std::string d = ln.substr(h + 1);
                    std::size_t s = d.find_first_not_of(" \t");
                    if (s != std::string::npos) d = d.substr(s);
                    auto starts = [&](const char* k) { return d.rfind(k, 0) == 0; };
                    bool win32 = d.find("_WIN32") != std::string::npos;
                    /* Platform token: any well-known compiler/OS macro. */
                    bool is_platform = win32 ||
                        d.find("__APPLE__") != std::string::npos ||
                        d.find("__linux__") != std::string::npos ||
                        d.find("_MSC_VER")  != std::string::npos ||
                        d.find("__GNUC__")  != std::string::npos ||
                        d.find("defined(")  != std::string::npos;
                    if (starts("ifdef")) {
                        guard.push_back(win32 ? 1 : 0);
                        if (is_platform) ++platform_guard_depth;
                    } else if (starts("ifndef")) {
                        guard.push_back(win32 ? 2 : 0);
                        if (is_platform) ++platform_guard_depth;
                    } else if (starts("if")) {
                        bool neg = d.find('!') != std::string::npos;
                        guard.push_back(win32 ? (neg ? 2 : 1) : 0);
                        if (is_platform) ++platform_guard_depth;
                    } else if (starts("elif")) {
                        if (!guard.empty()) guard.back() = 0;
                        /* elif stays within the same platform guard depth */
                    } else if (starts("else")) {
                        if (!guard.empty()) {
                            if (guard.back() == 1) guard.back() = 2;
                            else if (guard.back() == 2) guard.back() = 1;
                        }
                    } else if (starts("endif")) {
                        if (!guard.empty()) {
                            /* Decrement platform depth if we were tracking one. */
                            if (platform_guard_depth > 0) --platform_guard_depth;
                            guard.pop_back();
                        }
                    }
                }
            }
            if (ln.size() > max_line_len_) return;
            bool in_win = false, in_posix = false;
            for (int g : guard) { if (g == 1) in_win = true; else if (g == 2) in_posix = true; }
            for (const Rule& r : rules_) {
                if (in_win   && r.id.rfind("PORT-WIN",   0) == 0) continue;
                if (in_posix && r.id.rfind("PORT-POSIX", 0) == 0) continue;
                /* Suppress path and shell-command rules inside platform guards:
                 * probe code legitimately references OS paths and utilities. */
                if (platform_guard_depth > 0 && r.id == "CHG-MAGIC-PATH")  continue;
                if (platform_guard_depth > 0 && r.id == "SEC-SYSTEM-CALL") continue;
                if (std::regex_search(ln, r.pattern)) {
                    Issue iss;
                    iss.rule_id = r.id;
                    iss.title = r.title;
                    iss.cwe = r.cwe;
                    iss.severity = r.severity;
                    iss.factor = r.factor;
                    iss.remediation_minutes = r.remediation_minutes;
                    iss.file = f.path;
                    iss.line = line_no;
                    iss.snippet = trim_snippet(ln);
                    issues.push_back(std::move(iss));
                }
            }
        };
        for (char c : f.content) {
            if (c == '\n') { scan_line(line); line.clear(); }
            else if (c != '\r') line.push_back(c);
        }
        if (!line.empty()) scan_line(line);
        return issues;
    }

    std::size_t size() const { return rules_.size(); }

    /* Lines longer than this are skipped for regex matching, to avoid
     * catastrophic backtracking on minified/generated files. */
    void set_max_line_length(std::size_t n) { max_line_len_ = (n == 0 ? 1 : n); }

private:
    static std::string trim_snippet(const std::string& s) {
        std::size_t a = s.find_first_not_of(" \t");
        std::string t = (a == std::string::npos) ? s : s.substr(a);
        if (t.size() > 160) t = t.substr(0, 160);
        return t;
    }

    std::vector<Rule> rules_;
    std::size_t max_line_len_ = 2000;
};

/* Built-in default rules covering common cross-language patterns. */
inline RuleSet default_rules() {
    RuleSet rs;
    rs.add("SEC-HARDCODED-SECRET", "Possible hardcoded credential or secret",
           "CWE-798", Severity::Critical, HealthFactor::Security, 30,
           R"((password|passwd|secret|api[_-]?key|token)\s*[:=]\s*["'](?![/+\s])[^"']{4,}["'])");
    rs.add("SEC-EVAL", "Dynamic code execution (eval/exec)",
           "CWE-95", Severity::Major, HealthFactor::Security, 25,
           R"(\beval\s*\(|\bexec\s*\(\s*(?!["'R)]|const\b))");
    rs.add("SEC-WEAK-HASH", "Weak hashing algorithm",
           "CWE-327", Severity::Major, HealthFactor::Security, 20,
           R"(\b(md5|sha1)\b)");
    rs.add("SEC-WEAK-CIPHER", "Weak or broken cipher",
           "CWE-327", Severity::Major, HealthFactor::Security, 30,
           R"(\b(des|rc4|ecb)\b)");
    rs.add("SEC-SQL-CONCAT", "Possible SQL built by string concatenation",
           "CWE-89", Severity::Critical, HealthFactor::Security, 40,
           R"((select|insert|update|delete)\b.*["'].*\+)");
    rs.add("SEC-SYSTEM-CALL", "Shell command execution",
           "CWE-78", Severity::Major, HealthFactor::Security, 25,
           R"((^|[^.\w/])(system|popen)\s*\(\s*[^\"'L]|\bos\.system\s*\(\s*[^\"']|\bsubprocess\.call\s*\(\s*[^\"'[])"); /* literal-arg exempt */
    rs.add("ROB-EMPTY-CATCH", "Empty or swallowed exception handler",
           "CWE-1069", Severity::Major, HealthFactor::Robustness, 15,
           R"(catch\s*\([^)]*\)\s*\{\s*\})");
    rs.add("ROB-TODO", "Unresolved TODO / FIXME marker",
           "", Severity::Minor, HealthFactor::Changeability, 10,
           R"(\b(todo|fixme|hack|xxx)\b)");
    rs.add("EFF-SELECT-STAR", "SELECT * may fetch unused columns",
           "", Severity::Minor, HealthFactor::Efficiency, 10,
           R"(select\s+\*\s+from)");
    rs.add("CHG-MAGIC-PATH", "Hardcoded absolute filesystem path",
           "", Severity::Minor, HealthFactor::Changeability, 10,
           R"((["'])((?:/(?:etc|usr|var|home|opt|tmp|root|srv|proc|sys|mnt)(?:/[^\s"']+)+)|(?:/(?!api/|metrics|docs/|\.well-known)[a-z0-9_.\-]+(?:/[a-z0-9_.\-]+)+\.[a-z0-9]{1,6})|(?:[a-z]:\\\\[^"'\s]+))\1)");
    rs.add("SEC-INSECURE-RANDOM", "Insecure random source",
           "CWE-330", Severity::Minor, HealthFactor::Security, 15,
           R"(\b(rand\s*\(|math\.random|random\.random)\b)");
    return rs;
}

} /* namespace metis::codeanalyzer */

#endif /* METIS_CODE_ANALYZER_RULES_HPP */
