/* Metis Code Analyzer Plus - license header detection
 * Scans the first N lines of each source file for SPDX license identifiers
 * and copyright notices. Header-only, zero external dependencies, C++20.
 *
 * Recognized markers:
 *   SPDX-License-Identifier: <expression>  -- machine-readable license
 *   Copyright (c) / Copyright (C)          -- copyright notice
 *   @license / @copyright                  -- JSDoc-style annotations
 */
#ifndef METIS_CODE_ANALYZER_LICENSE_HPP
#define METIS_CODE_ANALYZER_LICENSE_HPP

#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "metis/codeanalyzer/analyzer.hpp"

namespace metis::codeanalyzer::license {

struct FileLicense {
    std::string path;
    std::string spdx;        /* SPDX expression, empty if not found */
    bool has_copyright = false;
    bool has_header    = false;
};

struct LicenseSummary {
    std::vector<FileLicense> files;
    std::map<std::string, int> spdx_counts;
    int files_with_header    = 0;
    int files_without_header = 0;
};

static constexpr int kScanLines = 20;

inline std::string to_upper(const std::string& s) {
    std::string out; out.reserve(s.size());
    for (char c : s)
        out.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(c))));
    return out;
}

inline FileLicense scan_file(const std::string& path,
                             const std::string& scan_root) {
    FileLicense fl;
    fl.path = path;

    std::string full = scan_root.empty() ? path : (scan_root + "/" + path);
    std::ifstream in(full);
    if (!in) return fl;

    std::string line;
    int n = 0;
    while (std::getline(in, line) && n < kScanLines) {
        ++n;
        std::string u = to_upper(line);

        /* SPDX-License-Identifier: <expr> */
        std::size_t p = u.find("SPDX-LICENSE-IDENTIFIER:");
        if (p != std::string::npos && fl.spdx.empty()) {
            std::size_t start = line.find(':', p) + 1;
            while (start < line.size() &&
                   (line[start] == ' ' || line[start] == '\t')) ++start;
            std::size_t end = line.size();
            while (end > start && (line[end-1] == '\r' || line[end-1] == '\n' ||
                                   line[end-1] == ' '  || line[end-1] == '*' ||
                                   line[end-1] == '/')) --end;
            if (end > start) fl.spdx = line.substr(start, end - start);
        }

        /* Copyright / @license / @copyright */
        if (!fl.has_copyright &&
            (u.find("COPYRIGHT") != std::string::npos ||
             u.find("@LICENSE")  != std::string::npos ||
             u.find("@COPYRIGHT")!= std::string::npos))
            fl.has_copyright = true;
    }

    fl.has_header = !fl.spdx.empty() || fl.has_copyright;
    return fl;
}

inline LicenseSummary scan(const std::vector<FileRecord>& files,
                           const std::string& scan_root) {
    LicenseSummary summary;
    for (const FileRecord& f : files) {
        FileLicense fl = scan_file(f.path, scan_root);
        if (!fl.spdx.empty()) summary.spdx_counts[fl.spdx]++;
        if (fl.has_header) ++summary.files_with_header;
        else               ++summary.files_without_header;
        summary.files.push_back(std::move(fl));
    }
    return summary;
}

} /* namespace metis::codeanalyzer::license */

#endif /* METIS_CODE_ANALYZER_LICENSE_HPP */
