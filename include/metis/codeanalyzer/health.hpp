/* Metis Code Analyzer Plus - technical debt and health factors
 * Aggregates per-file metrics and issues into the five health factors
 * (Robustness, Security, Efficiency, Transferability, Changeability), a Total
 * Quality Index, technical-debt minutes/cost, and a debt ratio.
 *
 * All scoring weights, density scales, grade thresholds, and penalty
 * coefficients are supplied via ScoreConfig (loaded from PSON); nothing is
 * hardcoded here. Header-only, 7-bit ASCII, C++20.
 */
#ifndef METIS_CODE_ANALYZER_HEALTH_HPP
#define METIS_CODE_ANALYZER_HEALTH_HPP

#include <algorithm>
#include <array>
#include <cmath>
#include <map>
#include <string>
#include <vector>

#include "metis/codeanalyzer/metrics.hpp"
#include "metis/codeanalyzer/rules.hpp"

namespace metis::codeanalyzer {

/* Every tunable used by the scoring model. Defaults match the documented
 * baseline; all are overridable from code_analyzer.pson. */
struct ScoreConfig {
    double w_critical = 10.0;
    double w_major = 5.0;
    double w_minor = 2.0;
    double w_info = 1.0;
    double scale_security = 12.0;
    double scale_robustness = 10.0;
    double scale_efficiency = 8.0;
    double scale_transferability = 6.0;
    double scale_changeability = 6.0;
    double scale_portability = 8.0;
    double density_cap = 60.0;
    double grade_a = 90.0;
    double grade_b = 80.0;
    double grade_c = 65.0;
    double grade_d = 50.0;
    double complexity_threshold = 10.0;
    /* Per-language threshold overrides. Key is lowercase language name
     * with spaces replaced by underscores (e.g. "c__", "objective_c"). */
    std::map<std::string, double> complexity_threshold_lang;
    double complexity_rob_mult = 1.5;
    double complexity_rob_cap = 25.0;
    double complexity_chg_mult = 1.2;
    double complexity_chg_cap = 20.0;
    double comment_min = 0.10;
    double comment_transfer_mult = 1.5;
    double comment_transfer_cap = 20.0;
    double comment_change_mult = 1.0;
    double comment_change_cap = 15.0;
    double dup_change_mult = 2.0;
    double dup_change_cap = 20.0;
    double dup_transfer_mult = 1.5;
    double dup_transfer_cap = 15.0;
};

inline char grade_for(double score, const ScoreConfig& c) {
    if (score >= c.grade_a) return 'A';
    if (score >= c.grade_b) return 'B';
    if (score >= c.grade_c) return 'C';
    if (score >= c.grade_d) return 'D';
    return 'F';
}

struct HealthScores {
    double robustness = 100.0;
    double security = 100.0;
    double efficiency = 100.0;
    double transferability = 100.0;
    double changeability = 100.0;
    double portability = 100.0;
    double tqi = 100.0;

    double factor(HealthFactor f) const {
        switch (f) {
        case HealthFactor::Robustness:      return robustness;
        case HealthFactor::Security:        return security;
        case HealthFactor::Efficiency:      return efficiency;
        case HealthFactor::Transferability: return transferability;
        case HealthFactor::Changeability:   return changeability;
        case HealthFactor::Portability:     return portability;
        }
        return 100.0;
    }
};

struct DebtSummary {
    long total_minutes = 0;
    double total_hours = 0.0;
    double cost = 0.0;
    double debt_ratio = 0.0;
    std::map<std::string, long> severity_counts;
};

struct ProjectReport {
    std::size_t file_count = 0;
    std::size_t total_lines = 0;
    std::size_t code_lines = 0;
    std::size_t comment_lines = 0;
    std::size_t blank_lines = 0;
    double comment_ratio = 0.0;
    std::size_t total_functions = 0;
    std::size_t total_cyclomatic = 0;
    double avg_complexity = 0.0;
    std::size_t max_complexity = 0;
    std::size_t duplicate_blocks = 0;
    /* Each entry is a group of "file:line" locations that share an identical
     * normalized block (the actual duplicated locations, not just a count). */
    std::vector<std::vector<std::string>> duplicates;
    std::map<std::string, std::size_t> lines_by_language;
    HealthScores health;
    DebtSummary debt;
    std::size_t issue_count = 0;
};

class HealthModel {
public:
    HealthModel(double hourly_rate, double dev_minutes_per_line, ScoreConfig cfg)
        : hourly_rate_(hourly_rate),
          dev_minutes_per_line_(dev_minutes_per_line),
          cfg_(cfg) {}

    const ScoreConfig& config() const { return cfg_; }

    HealthScores score(const ProjectReport& base,
                       const std::vector<Issue>& issues) const {
        HealthScores h;
        double kloc = static_cast<double>(base.code_lines) / 1000.0;

        std::array<double, 6> weighted{0, 0, 0, 0, 0, 0};
        for (const Issue& i : issues) {
            weighted[static_cast<std::size_t>(i.factor)] += weight(i.severity);
        }

        h.security        = 100.0 - density(weighted[1], kloc, cfg_.scale_security);
        h.robustness      = 100.0 - density(weighted[0], kloc, cfg_.scale_robustness);
        h.efficiency      = 100.0 - density(weighted[2], kloc, cfg_.scale_efficiency);
        h.transferability = 100.0 - density(weighted[3], kloc, cfg_.scale_transferability);
        h.changeability   = 100.0 - density(weighted[4], kloc, cfg_.scale_changeability);
        h.portability     = 100.0 - density(weighted[5], kloc, cfg_.scale_portability);

        if (base.avg_complexity > cfg_.complexity_threshold) {
            double over = base.avg_complexity - cfg_.complexity_threshold;
            h.robustness -= std::min(cfg_.complexity_rob_cap, over * cfg_.complexity_rob_mult);
            h.changeability -= std::min(cfg_.complexity_chg_cap, over * cfg_.complexity_chg_mult);
        }
        if (base.comment_ratio < cfg_.comment_min) {
            double gap = (cfg_.comment_min - base.comment_ratio) * 100.0;
            h.transferability -= std::min(cfg_.comment_transfer_cap, gap * cfg_.comment_transfer_mult);
            h.changeability -= std::min(cfg_.comment_change_cap, gap * cfg_.comment_change_mult);
        }
        if (base.duplicate_blocks > 0 && base.code_lines > 0) {
            double dup = static_cast<double>(base.duplicate_blocks) /
                         static_cast<double>(base.code_lines) * 1000.0;
            h.changeability -= std::min(cfg_.dup_change_cap, dup * cfg_.dup_change_mult);
            h.transferability -= std::min(cfg_.dup_transfer_cap, dup * cfg_.dup_transfer_mult);
        }

        clamp(h.robustness); clamp(h.security); clamp(h.efficiency);
        clamp(h.transferability); clamp(h.changeability); clamp(h.portability);

        h.tqi = (h.robustness + h.security + h.efficiency +
                 h.transferability + h.changeability + h.portability) / 6.0;
        return h;
    }

    DebtSummary debt(const ProjectReport& base,
                     const std::vector<Issue>& issues) const {
        DebtSummary d;
        for (const Issue& i : issues) {
            d.total_minutes += i.remediation_minutes;
            ++d.severity_counts[severity_name(i.severity)];
        }
        d.total_hours = static_cast<double>(d.total_minutes) / 60.0;
        d.cost = d.total_hours * hourly_rate_;
        double dev_minutes =
            static_cast<double>(base.code_lines) * dev_minutes_per_line_;
        if (dev_minutes > 0.0) {
            d.debt_ratio = static_cast<double>(d.total_minutes) / dev_minutes;
        }
        return d;
    }

private:
    double weight(Severity s) const {
        switch (s) {
        case Severity::Critical: return cfg_.w_critical;
        case Severity::Major:    return cfg_.w_major;
        case Severity::Minor:    return cfg_.w_minor;
        case Severity::Info:     return cfg_.w_info;
        }
        return cfg_.w_info;
    }

    double density(double weighted, double kloc, double scale) const {
        if (kloc <= 0.0) return 0.0;
        return std::min(cfg_.density_cap, (weighted / kloc) * scale);
    }

    static void clamp(double& v) { v = std::max(0.0, std::min(100.0, v)); }

    double hourly_rate_;
    double dev_minutes_per_line_;
    ScoreConfig cfg_;
};

} /* namespace metis::codeanalyzer */

#endif /* METIS_CODE_ANALYZER_HEALTH_HPP */
