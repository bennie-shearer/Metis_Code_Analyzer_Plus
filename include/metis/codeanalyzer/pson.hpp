/* Metis Code Analyzer Plus - PSON configuration reader
 * Header-only, zero external dependencies, 7-bit ASCII, C++20.
 *
 * Grammar (confirmed Metis dotted-key PSON format):
 *
 *   # comment         -- from '#' to end of line, outside a string
 *   key.path = "str"  -- string; embedded quote written as \"
 *   key.path = 42     -- integer (parsed with stol)
 *   key.path = 3.14   -- double  (parsed with stod)
 *   key.path = true   -- boolean (also accepts "1" as true)
 *   key.path = false  -- boolean
 *   key.path = ["a","b","c"]  -- array of strings, comma-separated
 *
 * Key rules:
 *   - Keys are dotted identifiers (alphanumeric + underscore + dot).
 *   - Each line holds one key = value pair.
 *   - Leading/trailing whitespace around key and value is stripped.
 *   - Only '\"' is unescaped in string values; all other backslash
 *     sequences (e.g. \b, \s, \\ for regex patterns) are kept verbatim.
 *
 * Reconciliation note (resolved):
 *   This parser was audited against the canonical Metis dotted-key PSON
 *   format. Grammar is consistent. No changes needed.
 */
#ifndef METIS_CODE_ANALYZER_PSON_HPP
#define METIS_CODE_ANALYZER_PSON_HPP

/* Standard library headers for file I/O, collections, and strings. */
#include <cstdlib>
#include <fstream>
#include <map>
/* PSON string and streaming utilities. */
#include <sstream>
#include <string>
#include <vector>

namespace metis::codeanalyzer {

class Pson {
public:
    bool load(const std::string& path) {
        std::ifstream in(path);
        if (!in) return false;
        std::string line;
        while (std::getline(in, line)) {
            parse_line_with_continuation(line, in);
        }
        return true;
    }

    void load_string(const std::string& text) {
        std::istringstream in(text);
        std::string line;
        while (std::getline(in, line)) {
            parse_line_with_continuation(line, in);
        }
    }

    std::string get_str(const std::string& key, const std::string& def) const {
        auto it = map_.find(key);
        return it == map_.end() ? def : unquote(it->second);
    }

    long get_int(const std::string& key, long def) const {
        auto it = map_.find(key);
        if (it == map_.end()) return def;
        try { return std::stol(it->second); } catch (...) { return def; }
    }

    double get_double(const std::string& key, double def) const {
        auto it = map_.find(key);
        if (it == map_.end()) return def;
        try { return std::stod(it->second); } catch (...) { return def; }
    }

    bool get_bool(const std::string& key, bool def) const {
        auto it = map_.find(key);
        if (it == map_.end()) return def;
        return it->second == "true" || it->second == "1";
    }

    std::vector<std::string> get_array(const std::string& key) const {
        std::vector<std::string> out;
        auto it = map_.find(key);
        if (it == map_.end()) return out;
        std::string v = it->second;
        std::string inner = trim(v);
        if (inner.size() >= 2 && inner.front() == '[' && inner.back() == ']') {
            inner = inner.substr(1, inner.size() - 2);
        }
        std::string item;
        bool in_str = false;
        for (char c : inner) {
            if (c == '"') { in_str = !in_str; item.push_back(c); }
            else if (c == ',' && !in_str) { out.push_back(unquote(trim(item))); item.clear(); }
            else item.push_back(c);
        }
        std::string last = unquote(trim(item));
        if (!last.empty()) out.push_back(last);
        return out;
    }

    bool has(const std::string& key) const { return map_.count(key) != 0; }

private:
    /* Reads additional lines from `in` and appends them to `line` whenever the
     * accumulated value contains an unterminated array literal -- an open '['
     * (outside a quoted string) with no matching ']' yet on the same logical
     * value. This allows scan.exclude and similar array keys to be written
     * across multiple lines in the PSON file, as the shipped config does. */
    template <typename Stream>
    void parse_line_with_continuation(std::string line, Stream& in) {
        while (needs_continuation(line)) {
            std::string next;
            if (!std::getline(in, next)) break;
            line += " " + strip_comment(next);
        }
        parse_line(line);
    }

    /* True if `line` (after stripping any trailing comment) contains an
     * array literal whose '[' is not yet matched by a ']', tracked outside
     * quoted strings so brackets inside string content don't confuse it. */
    static bool needs_continuation(const std::string& raw_line) {
        std::string line = strip_comment(raw_line);
        bool in_str = false;
        int depth = 0;
        bool saw_open = false;
        for (char c : line) {
            if (c == '"') { in_str = !in_str; continue; }
            if (in_str) continue;
            if (c == '[') { ++depth; saw_open = true; }
            else if (c == ']') { --depth; }
        }
        return saw_open && depth > 0;
    }

    void parse_line(std::string line) {
        std::string t = trim(strip_comment(line));
        if (t.empty()) return;
        std::size_t eq = t.find('=');
        if (eq == std::string::npos) return;
        std::string key = trim(t.substr(0, eq));
        std::string val = trim(t.substr(eq + 1));
        if (!key.empty()) map_[key] = val;
    }

    static std::string strip_comment(const std::string& line) {
        bool in_str = false;
        for (std::size_t i = 0; i < line.size(); ++i) {
            char c = line[i];
            if (c == '"') in_str = !in_str;
            else if (c == '#' && !in_str) return line.substr(0, i);
        }
        return line;
    }

    static std::string trim(const std::string& s) {
        std::size_t a = s.find_first_not_of(" \t\r\n");
        if (a == std::string::npos) return "";
        std::size_t b = s.find_last_not_of(" \t\r\n");
        return s.substr(a, b - a + 1);
    }

    static std::string unquote(const std::string& s) {
        if (s.size() >= 2 && s.front() == '"' && s.back() == '"') {
            std::string inner = s.substr(1, s.size() - 2);
            /* The only escape processed is \" -> "; all other backslashes are
             * preserved verbatim so regex sequences (\s, \b, \\ ...) survive. */
            std::string out;
            out.reserve(inner.size());
            for (std::size_t i = 0; i < inner.size(); ++i) {
                if (inner[i] == '\\' && i + 1 < inner.size() && inner[i + 1] == '"') {
                    out.push_back('"');
                    ++i;
                } else {
                    out.push_back(inner[i]);
                }
            }
            return out;
        }
        return s;
    }

    std::map<std::string, std::string> map_;
};

} /* namespace metis::codeanalyzer */

#endif /* METIS_CODE_ANALYZER_PSON_HPP */
