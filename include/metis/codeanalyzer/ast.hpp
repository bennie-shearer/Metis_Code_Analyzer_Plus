/* Metis Code Analyzer Plus - tree-sitter AST metrics
 * Compiled only when METIS_ENABLE_AST is defined. Provides accurate
 * per-function cyclomatic complexity, function counts, and max nesting
 * depth for six languages: C, Python, JavaScript, Go, Java, Rust.
 *
 * For all other languages the caller falls back to the heuristic metrics
 * from metrics.hpp. Header-only, C++20, no external runtime dependencies.
 *
 * Grammar coverage (vendored in vendor/tree-sitter-grammars/):
 *   c         - C (.c files, and .h used as C)
 *   python    - Python (.py)
 *   javascript - JavaScript (.js, .jsx, .mjs)
 *   go        - Go (.go)
 *   java      - Java (.java)
 *   rust      - Rust (.rs)
 */
#ifndef METIS_CODE_ANALYZER_AST_HPP
#define METIS_CODE_ANALYZER_AST_HPP

#ifdef METIS_ENABLE_AST

#include <cstddef>
#include <string>

// Tree-sitter C API
extern "C" {
#include "tree_sitter/api.h"

// Grammar declarations
const TSLanguage* tree_sitter_c(void);
const TSLanguage* tree_sitter_python(void);
const TSLanguage* tree_sitter_javascript(void);
const TSLanguage* tree_sitter_go(void);
const TSLanguage* tree_sitter_java(void);
const TSLanguage* tree_sitter_rust(void);
} // extern "C"

#include "metis/codeanalyzer/metrics.hpp"
#include "metis/codeanalyzer/scanner.hpp"

namespace metis::codeanalyzer::ast {

/* Selects the tree-sitter language for a given Language enum value.
 * Returns nullptr for languages without a vendored grammar. */
inline const TSLanguage* ts_language_for(Language l) {
    switch (l) {
    case Language::C:
    case Language::Header:     return tree_sitter_c();
    case Language::Python:     return tree_sitter_python();
    case Language::JavaScript:
    case Language::TypeScript: return tree_sitter_javascript();
    case Language::Go:         return tree_sitter_go();
    case Language::Java:       return tree_sitter_java();
    case Language::Rust:       return tree_sitter_rust();
    default:                   return nullptr;
    }
}

/* Node-type predicates: a decision point adds one to cyclomatic complexity. */
inline bool is_decision_node(const char* type) {
    return (
        /* Shared across C, Java, Rust, Go, JS */
        std::string_view(type) == "if_statement"         ||
        std::string_view(type) == "else_clause"          ||
        std::string_view(type) == "for_statement"        ||
        std::string_view(type) == "while_statement"      ||
        std::string_view(type) == "do_statement"         ||
        std::string_view(type) == "case_clause"          ||
        std::string_view(type) == "switch_case"          ||
        std::string_view(type) == "match_arm"            ||  /* Rust */
        std::string_view(type) == "catch_clause"         ||
        std::string_view(type) == "ternary_expression"   ||
        std::string_view(type) == "conditional_expression" ||
        std::string_view(type) == "binary_expression"    ||  /* && || */
        /* Python */
        std::string_view(type) == "if_statement"         ||
        std::string_view(type) == "for_statement"        ||
        std::string_view(type) == "while_statement"      ||
        std::string_view(type) == "except_clause"        ||
        std::string_view(type) == "conditional_expression" ||
        /* Go */
        std::string_view(type) == "select_statement"     ||
        std::string_view(type) == "type_case_clause"     ||
        std::string_view(type) == "expression_case"      ||
        std::string_view(type) == "default_case"
    );
}

inline bool is_function_node(const char* type) {
    return (
        std::string_view(type) == "function_definition"  ||  /* C, Python */
        std::string_view(type) == "function_declaration" ||  /* Java, Go, TS */
        std::string_view(type) == "method_declaration"   ||  /* Java */
        std::string_view(type) == "method_definition"    ||  /* JS */
        std::string_view(type) == "function_item"        ||  /* Rust */
        std::string_view(type) == "function_expression"  ||  /* JS */
        std::string_view(type) == "arrow_function"       ||  /* JS */
        std::string_view(type) == "fn_item"              ||  /* Rust alt */
        std::string_view(type) == "func_literal"         ||  /* Go */
        std::string_view(type) == "func_declaration"     ||  /* Go */
        std::string_view(type) == "function"             ||  /* JS legacy */
        std::string_view(type) == "impl_item"            ||  /* Rust impl */
        std::string_view(type) == "constructor_declaration" /* Java */
    );
}

/* Walk the AST tree counting decision points, function boundaries, and
 * maximum nesting depth. Uses tree-sitter's cursor API for efficiency. */
struct AstWalker {
    std::size_t functions  = 0;
    std::size_t cyclomatic = 1;   /* baseline 1 */
    std::size_t max_nesting = 0;
    std::size_t cur_depth  = 0;

    void walk(TSNode node) {
        const char* type = ts_node_type(node);
        if (is_function_node(type)) {
            ++functions;
            std::size_t saved = cur_depth;
            cur_depth = 0;
            walk_children(node);
            cur_depth = saved;
            return;
        }
        if (is_decision_node(type)) {
            ++cyclomatic;
        }
        walk_children(node);
    }

    void walk_children(TSNode node) {
        std::uint32_t count = ts_node_child_count(node);
        if (count == 0) return;
        ++cur_depth;
        if (cur_depth > max_nesting) max_nesting = cur_depth;
        for (std::uint32_t i = 0; i < count; ++i)
            walk(ts_node_child(node, i));
        --cur_depth;
    }
};

/* Compute AST-accurate FileMetrics for a source file.
 * Returns false if no grammar is available for the language (caller should
 * use heuristic metrics instead). */
inline bool compute_ast_metrics(const SourceFile& f, FileMetrics& out) {
    const TSLanguage* lang = ts_language_for(f.language);
    if (!lang) return false;

    TSParser* parser = ts_parser_new();
    ts_parser_set_language(parser, lang);

    const char* src = f.content.data();
    std::uint32_t len = static_cast<std::uint32_t>(f.content.size());
    TSTree* tree = ts_parser_parse_string(parser, nullptr, src, len);

    if (!tree) { ts_parser_delete(parser); return false; }

    TSNode root = ts_tree_root_node(tree);
    AstWalker walker;
    walker.walk(root);

    /* AST fills complexity and function count; preserve line counts from
     * heuristic pass (which is always run first). */
    out.functions  = (walker.functions > 0) ? walker.functions : 1;
    out.cyclomatic = walker.cyclomatic;
    out.max_nesting = walker.max_nesting;

    ts_tree_delete(tree);
    ts_parser_delete(parser);
    return true;
}

} /* namespace metis::codeanalyzer::ast */

#endif /* METIS_ENABLE_AST */
#endif /* METIS_CODE_ANALYZER_AST_HPP */
