/* Metis Code Analyzer Plus - analysis orchestrator
 * Drives a full scan: per-file metrics, issue detection, duplication, and
 * dependency extraction, then aggregates into a ProjectReport plus per-file
 * detail records.
 * Header-only, 7-bit ASCII, C++20.
 */
#ifndef METIS_CODE_ANALYZER_ANALYZER_HPP
#define METIS_CODE_ANALYZER_ANALYZER_HPP

#include <algorithm>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

#include "metis/codeanalyzer/health.hpp"
#include "metis/codeanalyzer/metrics.hpp"
#include "metis/codeanalyzer/rules.hpp"
#include "metis/codeanalyzer/scanner.hpp"
#include "metis/codeanalyzer/ast.hpp"

namespace metis::codeanalyzer {

struct FileRecord {
    std::string path;
    std::string language;
    FileMetrics metrics;
    std::size_t issue_count = 0;
    std::vector<std::string> dependencies;
};

struct AnalysisResult {
    ProjectReport report;
    std::vector<FileRecord> files;
    std::vector<Issue> issues;
    bool ran = false;
    std::string scanned_root;
};

namespace detail {

inline std::vector<std::string> extract_dependencies(const SourceFile& f) {
    std::vector<std::string> deps;
    std::string line;
    auto handle = [&](const std::string& ln) {
        std::size_t a = ln.find_first_not_of(" \t");
        if (a == std::string::npos) return;
        std::string t = ln.substr(a);
        auto grab_between = [&](char open, char close) {
            std::size_t o = t.find(open);
            std::size_t c = (o == std::string::npos) ? std::string::npos
                                                      : t.find(close, o + 1);
            if (o != std::string::npos && c != std::string::npos && c > o + 1) {
                deps.push_back(t.substr(o + 1, c - o - 1));
            }
        };
        if (t.rfind("#include", 0) == 0) {
            if (t.find('<') != std::string::npos) grab_between('<', '>');
            else grab_between('"', '"');
        } else if (t.rfind("import ", 0) == 0 || t.rfind("from ", 0) == 0) {
            std::string rest = t.substr(t.find(' ') + 1);
            std::size_t e = rest.find_first_of(" \t;");
            deps.push_back(e == std::string::npos ? rest : rest.substr(0, e));
        } else if (t.find("require(") != std::string::npos) {
            grab_between('\'', '\'');
            if (deps.empty() || deps.back().empty()) grab_between('"', '"');
        }
    };
    for (char c : f.content) {
        if (c == '\n') { handle(line); line.clear(); }
        else if (c != '\r') line.push_back(c);
    }
    if (!line.empty()) handle(line);
    std::sort(deps.begin(), deps.end());
    deps.erase(std::unique(deps.begin(), deps.end()), deps.end());
    return deps;
}

/* FNV-1a 64-bit hash of a normalized line block for duplication detection. */
inline std::uint64_t fnv1a(const std::string& s) {
    std::uint64_t h = 1469598103934665603ULL;
    for (char c : s) {
        h ^= static_cast<std::uint64_t>(static_cast<unsigned char>(c));
        h *= 1099511628211ULL;
    }
    return h;
}

inline std::string normalize_line(const std::string& ln) {
    std::string out;
    bool last_space = false;
    for (char c : ln) {
        if (c == ' ' || c == '\t' || c == '\r') {
            if (!last_space && !out.empty()) out.push_back(' ');
            last_space = true;
        } else {
            out.push_back(c);
            last_space = false;
        }
    }
    while (!out.empty() && out.back() == ' ') out.pop_back();
    return out;
}

} /* namespace detail */

class Analyzer {
public:
    Analyzer(RuleSet rules, HealthModel model, std::vector<std::string> excludes,
             std::size_t max_file_bytes = 4u * 1024u * 1024u,
             std::size_t duplication_block_lines = 6,
             std::size_t long_line_columns = 120)
        : rules_(std::move(rules)), model_(std::move(model)),
          scanner_(std::move(excludes), max_file_bytes),
          dup_block_(duplication_block_lines == 0 ? 1 : duplication_block_lines),
          long_line_columns_(long_line_columns) {}

    /* Enable or disable the incremental cache. When enabled, files whose
     * modification time has not changed since the last scan are not
     * re-analysed; cached metrics and issues are reused instead. The
     * per-scan duplicate-block calculation always runs on all files so that
     * cross-file duplication is not missed when one file changes. */
    void set_incremental(bool enabled) { incremental_ = enabled; }

    AnalysisResult analyze(const std::string& root) {
        namespace fs = std::filesystem;
        AnalysisResult result;
        result.scanned_root = root;
        std::vector<SourceFile> files = scanner_.scan(root);

        ProjectReport& rep = result.report;
        std::unordered_map<std::uint64_t, std::vector<std::pair<std::string, int>>> block_hashes;
        const std::size_t kBlock = dup_block_;

        /* Build absolute root path for mtime lookups. */
        std::error_code ec;
        fs::path abs_root = fs::weakly_canonical(fs::path(root), ec);
        if (ec) abs_root = fs::absolute(fs::path(root));

        /* Helper: run full analysis on a single file (no cache). */
        auto analyse_file = [&](const SourceFile& f,
                                FileMetrics& m_out,
                                std::vector<Issue>& issues_out,
                                std::vector<std::string>& deps_out) {
            m_out      = compute_metrics(f, long_line_columns_);
            issues_out = rules_.apply(f);
            deps_out   = detail::extract_dependencies(f);
#ifdef METIS_ENABLE_AST
            /* Overwrite heuristic complexity/function/nesting with
             * AST-accurate values when a grammar is available. Line counts,
             * comment ratio, and long-line count are kept from the heuristic
             * pass since they do not depend on the parse tree. */
            (void)ast::compute_ast_metrics(f, m_out);
#endif
        };

        for (const SourceFile& f : files) {
            FileMetrics m;
            std::vector<Issue> file_issues;
            std::vector<std::string> deps;

            bool cache_hit = false;
            if (incremental_) {
                fs::path abs_path = fs::weakly_canonical(abs_root / fs::path(f.path), ec);
                auto mtime = fs::last_write_time(abs_path, ec);
                if (!ec) {
                    auto it = cache_.find(f.path);
                    if (it != cache_.end() && it->second.mtime == mtime) {
                        m          = it->second.metrics;
                        file_issues = it->second.issues;
                        deps       = it->second.deps;
                        cache_hit  = true;
                    }
                    if (!cache_hit) {
                        analyse_file(f, m, file_issues, deps);
                        CacheEntry entry;
                        entry.mtime   = mtime;
                        entry.metrics = m;
                        entry.issues  = file_issues;
                        entry.deps    = deps;
                        cache_[f.path] = std::move(entry);
                    }
                } else {
                    analyse_file(f, m, file_issues, deps);
                }
            } else {
                analyse_file(f, m, file_issues, deps);
            }

            FileRecord fr;
            fr.path = f.path;
            fr.language = language_name(f.language);
            fr.metrics = m;
            fr.issue_count = file_issues.size();
            fr.dependencies = deps;

            rep.file_count += 1;
            rep.total_lines += m.total_lines;
            rep.code_lines += m.code_lines;
            rep.comment_lines += m.comment_lines;
            rep.blank_lines += m.blank_lines;
            rep.total_functions += m.functions;
            rep.total_cyclomatic += m.cyclomatic;
            rep.max_complexity = std::max(rep.max_complexity, m.cyclomatic);
            rep.lines_by_language[language_name(f.language)] += m.code_lines;

            for (Issue& iss : file_issues) result.issues.push_back(std::move(iss));
            result.files.push_back(std::move(fr));

            /* Duplication: sliding window over normalized non-empty lines,
             * tracking the source line where each kept line occurs. */
            std::vector<std::pair<std::string, int>> norm;
            std::string line;
            int srcline = 1;
            auto add = [&](const std::string& ln, int lno) {
                std::string n = detail::normalize_line(ln);
                if (!n.empty()) norm.push_back({n, lno});
            };
            for (char c : f.content) {
                if (c == '\n') { add(line, srcline); ++srcline; line.clear(); }
                else line.push_back(c);
            }
            if (!line.empty()) add(line, srcline);

            if (norm.size() >= kBlock) {
                for (std::size_t i = 0; i + kBlock <= norm.size(); ++i) {
                    std::string blk;
                    for (std::size_t j = 0; j < kBlock; ++j) { blk += norm[i + j].first; blk += '\n'; }
                    std::uint64_t h = detail::fnv1a(blk);
                    auto& locs = block_hashes[h];
                    locs.push_back({f.path, norm[i].second});
                    if (locs.size() == 2) rep.duplicate_blocks += 1;
                }
            }
        }

        /* Collect the actual duplicated locations (groups sharing a block). */
        for (auto& kv : block_hashes) {
            if (kv.second.size() < 2) continue;
            if (rep.duplicates.size() >= 100) break;
            std::vector<std::string> group;
            for (auto& loc : kv.second) {
                group.push_back(loc.first + ":" + std::to_string(loc.second));
                if (group.size() >= 20) break;
            }
            rep.duplicates.push_back(std::move(group));
        }

        rep.issue_count = result.issues.size();
        if (rep.total_lines > 0) {
            rep.comment_ratio = static_cast<double>(rep.comment_lines) /
                                static_cast<double>(rep.total_lines);
        }
        if (rep.file_count > 0) {
            rep.avg_complexity = static_cast<double>(rep.total_cyclomatic) /
                                 static_cast<double>(std::max(rep.total_functions,
                                                     static_cast<std::size_t>(1)));
        }

        rep.health = model_.score(rep, result.issues);
        rep.debt = model_.debt(rep, result.issues);
        result.ran = true;
        return result;
    }

private:
    struct CacheEntry {
        std::filesystem::file_time_type mtime;
        FileMetrics metrics;
        std::vector<Issue> issues;
        std::vector<std::string> deps;
    };

    RuleSet rules_;
    HealthModel model_;
    Scanner scanner_;
    std::size_t dup_block_;
    std::size_t long_line_columns_;
    bool incremental_ = true;
    std::unordered_map<std::string, CacheEntry> cache_;
};

} /* namespace metis::codeanalyzer */

#endif /* METIS_CODE_ANALYZER_ANALYZER_HPP */
