/* Metis Code Analyzer Plus - code metrics
 * Computes line breakdown, cyclomatic complexity (decision-point method),
 * maximum nesting depth, and a heuristic function count per file.
 * Header-only, 7-bit ASCII, C++20. 30 languages.
 *
 * Heuristic-based analysis; not a full AST parse. The planned AST tier
 * (tree-sitter) is tracked in BACKGROUND.md and docs/ARCHITECTURE.md.
 */
#ifndef METIS_CODE_ANALYZER_METRICS_HPP
#define METIS_CODE_ANALYZER_METRICS_HPP

#include <cctype>
#include <sstream>
#include <string>
#include <vector>

#include "metis/codeanalyzer/scanner.hpp"

namespace metis::codeanalyzer {

struct FileMetrics {
    std::size_t total_lines    = 0;
    std::size_t code_lines     = 0;
    std::size_t comment_lines  = 0;
    std::size_t blank_lines    = 0;
    std::size_t functions      = 0;
    std::size_t cyclomatic     = 1;  /* decision points + 1 */
    std::size_t max_nesting    = 0;
    std::size_t long_lines     = 0;  /* lines exceeding the column threshold */
};

namespace detail {

// Returns true for languages whose primary line-comment marker is '#'.
inline bool uses_hash_comment(Language l) {
    return l == Language::Python     || l == Language::Ruby    ||
           l == Language::Shell      || l == Language::R       ||
           l == Language::Perl       || l == Language::Elixir  ||
           l == Language::PowerShell || l == Language::Crystal ||
           l == Language::Nim;
}

// Returns true for languages whose primary line-comment marker is '--'.
inline bool uses_dash_comment(Language l) {
    return l == Language::Sql  || l == Language::Haskell ||
           l == Language::Lua  || l == Language::Ada     ||
           l == Language::Vhdl;
}

// Returns true for languages whose line-comment marker is '%'.
inline bool uses_percent_comment(Language l) {
    return l == Language::Erlang || l == Language::Matlab;
}

// Returns true for languages whose line-comment marker is ';'.
inline bool uses_semicolon_comment(Language l) {
    return l == Language::Clojure;
}

// Returns true for Fortran, which uses '!' for line comments.
inline bool uses_bang_comment(Language l) { return l == Language::Fortran; }

// Returns true for COBOL (fixed-format: col-7 '*' is a comment line).
inline bool uses_cobol_comment(Language l) { return l == Language::Cobol; }

// Returns true for HLASM/ABAP where '*' in column 1 is a comment.
inline bool uses_asterisk_comment(Language l) {
    return l == Language::Hlasm || l == Language::Abap;
}

// Returns true for Visual Basic 6, where "'" begins a line comment.
inline bool uses_apostrophe_comment(Language l) {
    return l == Language::Vb6;
}

// Returns true for JCL, where comment lines start with "//*".
inline bool uses_jcl_comment(Language l) { return l == Language::Jcl; }

// Returns true for REXX, PL/I, OCaml, F# -- block-comment only.
inline bool uses_block_only(Language l) {
    return l == Language::Rexx  || l == Language::Pli   ||
           l == Language::OCaml || l == Language::FSharp;
}

inline bool is_word_boundary(char c) {
    return !(std::isalnum(static_cast<unsigned char>(c)) || c == '_');
}

// Count non-overlapping whole-word occurrences of needle in haystack.
inline std::size_t count_keyword(const std::string& s, const std::string& kw) {
    std::size_t count = 0;
    std::size_t pos   = 0;
    while ((pos = s.find(kw, pos)) != std::string::npos) {
        bool left  = (pos == 0) || is_word_boundary(s[pos - 1]);
        std::size_t end = pos + kw.size();
        bool right = (end >= s.size()) || is_word_boundary(s[end]);
        if (left && right) ++count;
        pos = end;
    }
    return count;
}

} // namespace detail

inline FileMetrics compute_metrics(const SourceFile& f,
                                   std::size_t long_line_columns = 120) {
    FileMetrics m;
    bool in_block_comment = false;

    // Classify comment style once, not per line.
    const bool hash_lang      = detail::uses_hash_comment(f.language);
    const bool dash_lang      = detail::uses_dash_comment(f.language);
    const bool bang_lang      = detail::uses_bang_comment(f.language);
    const bool cobol_lang     = detail::uses_cobol_comment(f.language);
    const bool asterisk_lang  = detail::uses_asterisk_comment(f.language);
    const bool jcl_lang       = detail::uses_jcl_comment(f.language);
    const bool block_only     = detail::uses_block_only(f.language);
    const bool pct_lang       = detail::uses_percent_comment(f.language);
    const bool apostrophe_lang= detail::uses_apostrophe_comment(f.language);
    const bool semi_lang     = detail::uses_semicolon_comment(f.language);
    // All languages that use slash-slash or slash-star block comments.
    const bool slash_lang    = !hash_lang && !dash_lang && !bang_lang &&
                               !cobol_lang && !asterisk_lang &&
                               !pct_lang   && !semi_lang && !apostrophe_lang;

    auto flush_line = [&](const std::string& ln) {
        ++m.total_lines;
        std::string t;
        for (char c : ln) if (c != '\r') t.push_back(c);
        if (t.size() > long_line_columns) ++m.long_lines;

        std::size_t a = t.find_first_not_of(" \t");
        if (a == std::string::npos) { ++m.blank_lines; return; }

        const std::string tr = t.substr(a);
        bool is_comment = false;

        if (in_block_comment) {
            // Every line inside an open block comment is a comment line.
            is_comment = true;
            if (tr.find("*/") != std::string::npos) in_block_comment = false;
        } else if (cobol_lang) {
            // COBOL fixed-format: position 6 (0-indexed) is the indicator.
            if (t.size() >= 7 && t[6] == '*') is_comment = true;
        } else if (asterisk_lang) {
            // HLASM: any line with '*' as first non-blank char is a comment.
            if (tr.front() == '*') is_comment = true;
        } else if (jcl_lang) {
            // JCL: lines beginning with the three-char sequence //asterisk.
            if (tr.size() >= 3 && tr[0] == '/' && tr[1] == '/' && tr[2] == '*')
                is_comment = true;
            // JCL block comments are not standard; skip block tracking.
        } else if (hash_lang && tr.front() == '#') {
            is_comment = true;
        } else if (dash_lang && tr.size() >= 2 &&
                   tr[0] == '-' && tr[1] == '-') {
            is_comment = true;
        } else if (dash_lang && f.language == Language::Haskell &&
                   tr.size() >= 2 && tr[0] == '{' && tr[1] == '-') {
            // Haskell block comment open.
            is_comment = true;
            if (tr.find("-}") == std::string::npos) in_block_comment = true;
        } else if (dash_lang && f.language == Language::Lua &&
                   tr.size() >= 4 && tr.rfind("--[[", 0) == 0) {
            // Lua long-string block comment open.
            is_comment = true;
            if (tr.find("]]") == std::string::npos) in_block_comment = true;
        } else if (bang_lang && tr.front() == '!') {
            // Fortran free-form: '!' begins a comment.
            is_comment = true;
        } else if (apostrophe_lang && tr.front() == '\'') {
            // Visual Basic 6: "'" begins a line comment.
            is_comment = true;
        } else if (pct_lang && tr.front() == '%') {
            // Erlang / MATLAB: '%' begins a line comment.
            is_comment = true;
        } else if (pct_lang && f.language == Language::Matlab &&
                   tr.size() >= 2 && tr[0] == '%' && tr[1] == '{') {
            // MATLAB block comment %{ ... %}.
            is_comment = true;
            if (tr.find("%}") == std::string::npos) in_block_comment = true;
        } else if (semi_lang && tr.front() == ';') {
            // Clojure: ';' begins a line comment.
            is_comment = true;
        } else if ((slash_lang || block_only) &&
                   tr.size() >= 2 && tr[0] == '/' && tr[1] == '*') {
            // Slash-star block comment open (C-family, REXX, PL/I).
            is_comment = true;
            if (tr.find("*/") == std::string::npos) in_block_comment = true;
        } else if (slash_lang && !block_only &&
                   tr.size() >= 2 && tr[0] == '/' && tr[1] == '/') {
            // Slash-slash line comment.
            is_comment = true;
        } else if (slash_lang && !block_only &&
                   tr.size() >= 2 && tr[0] == '*' && tr[1] == ' ') {
            // Doc-comment continuation line (e.g. " * description").
            is_comment = true;
        }

        if (is_comment) { ++m.comment_lines; return; }
        ++m.code_lines;
    };

    // Process line by line using a stream.
    {
        std::istringstream ss(f.content);
        std::string ln;
        while (std::getline(ss, ln)) flush_line(ln);
        if (!f.content.empty() && f.content.back() != '\n') flush_line(std::string{});
    }

    // --- Cyclomatic complexity (decision-point method) ---
    const std::string& src = f.content;

    auto count_substr = [&](const char* needle, std::size_t len) {
        std::size_t n = 0, p = 0;
        while ((p = src.find(needle, p, len)) != std::string::npos) { ++n; p += len; }
        return n;
    };

    std::size_t decisions = 0;
    // Universal keywords present in most languages.
    for (const char* kw : {"if", "for", "while", "case", "catch"})
        decisions += detail::count_keyword(src, kw);

    // Language-specific additional decision keywords.
    switch (f.language) {
    case Language::Kotlin:
        decisions += detail::count_keyword(src, "when");
        decisions += detail::count_keyword(src, "else");
        break;
    case Language::Swift:
        decisions += detail::count_keyword(src, "guard");
        decisions += detail::count_keyword(src, "switch");
        break;
    case Language::Scala:
        decisions += detail::count_keyword(src, "match");
        decisions += detail::count_keyword(src, "yield");
        break;
    case Language::Haskell:
        decisions += detail::count_keyword(src, "otherwise");
        decisions += detail::count_keyword(src, "where");
        break;
    case Language::Elixir:
        decisions += detail::count_keyword(src, "cond");
        decisions += detail::count_keyword(src, "with");
        break;
    case Language::Fortran:
        decisions += detail::count_keyword(src, "IF");
        decisions += detail::count_keyword(src, "DO");
        decisions += detail::count_keyword(src, "SELECT");
        break;
    case Language::Rexx:
    case Language::Pli:
        decisions += detail::count_keyword(src, "THEN");
        decisions += detail::count_keyword(src, "DO");
        decisions += detail::count_keyword(src, "SELECT");
        break;
    case Language::Cobol:
        decisions += detail::count_keyword(src, "IF");
        decisions += detail::count_keyword(src, "EVALUATE");
        decisions += detail::count_keyword(src, "PERFORM");
        break;
    case Language::R:
        decisions += detail::count_keyword(src, "else");
        decisions += detail::count_keyword(src, "repeat");
        break;
    case Language::Lua:
        decisions += detail::count_keyword(src, "elseif");
        decisions += detail::count_keyword(src, "repeat");
        decisions += detail::count_keyword(src, "until");
        break;
    case Language::Perl:
        decisions += detail::count_keyword(src, "unless");
        decisions += detail::count_keyword(src, "until");
        decisions += detail::count_keyword(src, "foreach");
        break;
    /* v2.1.4 additions */
    case Language::Ada:
        decisions += detail::count_keyword(src, "elsif");
        decisions += detail::count_keyword(src, "loop");
        decisions += detail::count_keyword(src, "when");
        break;
    case Language::PowerShell:
        decisions += detail::count_keyword(src, "elseif");
        decisions += detail::count_keyword(src, "foreach");
        decisions += detail::count_keyword(src, "switch");
        break;
    case Language::Matlab:
        decisions += detail::count_keyword(src, "elseif");
        decisions += detail::count_keyword(src, "parfor");
        break;
    case Language::Groovy:
        decisions += detail::count_keyword(src, "else");
        decisions += detail::count_keyword(src, "each");
        break;
    case Language::Zig:
        decisions += detail::count_keyword(src, "else");
        decisions += detail::count_keyword(src, "orelse");
        break;
    case Language::Abap:
        decisions += detail::count_keyword(src, "IF");
        decisions += detail::count_keyword(src, "CASE");
        decisions += detail::count_keyword(src, "LOOP");
        decisions += detail::count_keyword(src, "DO");
        decisions += detail::count_keyword(src, "WHILE");
        decisions += detail::count_keyword(src, "CHECK");
        break;
    case Language::FSharp:
        decisions += detail::count_keyword(src, "match");
        decisions += detail::count_keyword(src, "elif");
        break;
    case Language::Erlang:
        decisions += detail::count_keyword(src, "receive");
        decisions += detail::count_keyword(src, "case");
        decisions += detail::count_keyword(src, "when");
        break;
    case Language::Clojure:
        decisions += detail::count_keyword(src, "cond");
        decisions += detail::count_keyword(src, "when");
        decisions += detail::count_keyword(src, "loop");
        break;
    case Language::Crystal:
        decisions += detail::count_keyword(src, "elsif");
        decisions += detail::count_keyword(src, "unless");
        decisions += detail::count_keyword(src, "until");
        break;
    case Language::Nim:
        decisions += detail::count_keyword(src, "elif");
        decisions += detail::count_keyword(src, "of");
        break;
    case Language::Dlang:
        decisions += detail::count_keyword(src, "foreach");
        decisions += detail::count_keyword(src, "else");
        break;
    case Language::OCaml:
        decisions += detail::count_keyword(src, "match");
        decisions += detail::count_keyword(src, "when");
        break;
    case Language::Vhdl:
        decisions += detail::count_keyword(src, "IF");
        decisions += detail::count_keyword(src, "CASE");
        decisions += detail::count_keyword(src, "LOOP");
        decisions += detail::count_keyword(src, "GENERATE");
        break;
    case Language::Verilog:
        decisions += detail::count_keyword(src, "else");
        decisions += detail::count_keyword(src, "casex");
        decisions += detail::count_keyword(src, "casez");
        decisions += detail::count_keyword(src, "generate");
        break;
    case Language::Vb6:
        decisions += detail::count_keyword(src, "ElseIf");
        decisions += detail::count_keyword(src, "Else");
        decisions += detail::count_keyword(src, "For");
        decisions += detail::count_keyword(src, "Do");
        decisions += detail::count_keyword(src, "While");
        decisions += detail::count_keyword(src, "Select");
        decisions += detail::count_keyword(src, "Case");
        decisions += detail::count_keyword(src, "On Error");
        break;
    default:
        break;
    }

    // Logical-operator symbols.
    decisions += count_substr("&&", 2);
    decisions += count_substr("||", 2);
    if (f.language == Language::Haskell || f.language == Language::Elixir ||
        f.language == Language::R        || f.language == Language::Erlang ||
        f.language == Language::Ada      || f.language == Language::OCaml  ||
        f.language == Language::FSharp   || f.language == Language::Nim) {
        decisions += detail::count_keyword(src, "and");
        decisions += detail::count_keyword(src, "or");
    }
    m.cyclomatic = decisions + 1;

    // --- Nesting depth ---
    const bool uses_braces =
        f.language != Language::Python  &&
        f.language != Language::Haskell &&
        f.language != Language::Fortran &&
        f.language != Language::Cobol   &&
        f.language != Language::Hlasm   &&
        f.language != Language::Rexx    &&
        f.language != Language::Jcl     &&
        f.language != Language::Pli     &&
        f.language != Language::Ada     &&
        f.language != Language::Vhdl    &&
        f.language != Language::Abap    &&
        f.language != Language::OCaml   &&
        f.language != Language::FSharp  &&
        f.language != Language::Erlang  &&
        f.language != Language::Clojure;

    if (uses_braces) {
        std::size_t depth = 0;
        for (char c : src) {
            if      (c == '{') { ++depth; if (depth > m.max_nesting) m.max_nesting = depth; }
            else if (c == '}') { if (depth > 0) --depth; }
        }
    } else if (f.language == Language::Python) {
        // Approximate nesting depth by counting leading spaces per line.
        std::istringstream ss2(src);
        std::string ln;
        while (std::getline(ss2, ln)) {
            std::size_t indent = 0;
            for (char c : ln) { if (c == ' ') ++indent; else if (c == '\t') indent += 4; else break; }
            std::size_t d = indent / 4;
            if (d > m.max_nesting) m.max_nesting = d;
        }
    }

    // --- Function count ---
    switch (f.language) {
    case Language::Python:
    case Language::Scala:
        m.functions = detail::count_keyword(src, "def"); break;
    case Language::Go:
    case Language::Swift:
        m.functions = detail::count_keyword(src, "func"); break;
    case Language::JavaScript:
    case Language::TypeScript:
    case Language::Lua:
    case Language::R:
        m.functions = detail::count_keyword(src, "function"); break;
    case Language::Kotlin:
        m.functions = detail::count_keyword(src, "fun"); break;
    case Language::Ruby:
        m.functions = detail::count_keyword(src, "def"); break;
    case Language::Perl:
        m.functions = detail::count_keyword(src, "sub"); break;
    case Language::Haskell:
        m.functions = detail::count_keyword(src, "where") +
                      detail::count_keyword(src, "let") + 1; break;
    case Language::Elixir:
        m.functions = detail::count_keyword(src, "def") +
                      detail::count_keyword(src, "defp"); break;
    case Language::Fortran:
        m.functions = detail::count_keyword(src, "SUBROUTINE") +
                      detail::count_keyword(src, "FUNCTION")   +
                      detail::count_keyword(src, "subroutine") +
                      detail::count_keyword(src, "function"); break;
    case Language::Cobol:
        m.functions = detail::count_keyword(src, "SECTION") +
                      detail::count_keyword(src, "PARAGRAPH"); break;
    case Language::Hlasm:
        m.functions = detail::count_keyword(src, "CSECT") +
                      detail::count_keyword(src, "DSECT") +
                      detail::count_keyword(src, "PROC"); break;
    case Language::Rexx:
    case Language::Pli:
        m.functions = detail::count_keyword(src, "PROCEDURE") +
                      detail::count_keyword(src, "PROC"); break;
    case Language::Jcl:
        m.functions = detail::count_keyword(src, "EXEC"); break;
    /* v2.1.4 additions */
    case Language::Ada:
        m.functions = detail::count_keyword(src, "procedure") +
                      detail::count_keyword(src, "PROCEDURE") +
                      detail::count_keyword(src, "function") +
                      detail::count_keyword(src, "FUNCTION") +
                      detail::count_keyword(src, "task") +
                      detail::count_keyword(src, "TASK"); break;
    case Language::PowerShell:
        m.functions = detail::count_keyword(src, "function") +
                      detail::count_keyword(src, "filter"); break;
    case Language::Matlab:
        m.functions = detail::count_keyword(src, "function"); break;
    case Language::Groovy:
        m.functions = detail::count_keyword(src, "def"); break;
    case Language::Zig:
        m.functions = detail::count_keyword(src, "fn"); break;
    case Language::Abap:
        m.functions = detail::count_keyword(src, "FORM") +
                      detail::count_keyword(src, "METHOD") +
                      detail::count_keyword(src, "FUNCTION"); break;
    case Language::FSharp:
        m.functions = detail::count_keyword(src, "let") +
                      detail::count_keyword(src, "member"); break;
    case Language::Erlang:
        m.functions = detail::count_keyword(src, "->"); break;
    case Language::Clojure:
        m.functions = detail::count_keyword(src, "defn") +
                      detail::count_keyword(src, "defmacro") +
                      detail::count_keyword(src, "defun"); break;
    case Language::Crystal:
        m.functions = detail::count_keyword(src, "def"); break;
    case Language::Nim:
        m.functions = detail::count_keyword(src, "proc") +
                      detail::count_keyword(src, "func") +
                      detail::count_keyword(src, "method"); break;
    case Language::Dlang:
        /* D uses C-family syntax; fall through to C heuristic */ break;
    case Language::OCaml:
        m.functions = detail::count_keyword(src, "let") +
                      detail::count_keyword(src, "fun"); break;
    case Language::Vhdl:
        m.functions = detail::count_keyword(src, "architecture") +
                      detail::count_keyword(src, "ARCHITECTURE") +
                      detail::count_keyword(src, "procedure") +
                      detail::count_keyword(src, "PROCEDURE") +
                      detail::count_keyword(src, "function") +
                      detail::count_keyword(src, "FUNCTION"); break;
    case Language::Verilog:
        m.functions = detail::count_keyword(src, "module") +
                      detail::count_keyword(src, "task") +
                      detail::count_keyword(src, "function"); break;
    case Language::Vb6:
        m.functions = detail::count_keyword(src, "Sub") +
                      detail::count_keyword(src, "Function") +
                      detail::count_keyword(src, "Property Get") +
                      detail::count_keyword(src, "Property Let") +
                      detail::count_keyword(src, "Property Set"); break;
    default: {
        // C-family heuristic: closing paren followed by opening brace.
        std::size_t fn = 0, p = 0;
        while ((p = src.find('(', p)) != std::string::npos) {
            std::size_t close = src.find(')', p);
            if (close == std::string::npos) break;
            std::size_t b = src.find_first_not_of(" \t\r\n", close + 1);
            if (b != std::string::npos && src[b] == '{') ++fn;
            p = close + 1;
        }
        m.functions = fn;
        break;
    }
    }
    if (m.functions == 0 && m.code_lines > 0) m.functions = 1;
    return m;
}

} /* namespace metis::codeanalyzer */

#endif /* METIS_CODE_ANALYZER_METRICS_HPP */
