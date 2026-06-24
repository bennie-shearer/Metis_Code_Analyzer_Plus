/* scanner.hpp - source file discovery and language classification */
/* Metis Code Analyzer Plus - source tree scanner
 * Walks a directory, detects language by extension, applies exclusions.
 * Header-only, zero external dependencies, 7-bit ASCII, C++20.
 * 30 languages supported; see supported_languages() for the complete list.
 */
#ifndef METIS_CODE_ANALYZER_SCANNER_HPP
#define METIS_CODE_ANALYZER_SCANNER_HPP

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <map>
/* Scanner string and streaming utilities. */
#include <sstream>
#include <string>
#include <vector>

namespace metis::codeanalyzer {

enum class Language {
    /* Original 15 */
    Cpp, C, Header, Python, JavaScript, TypeScript, Java, CSharp,
    Go, Rust, Ruby, Php, Cobol, Shell, Sql,
    /* v2.1.4: languages 16-30 */
    Kotlin, Swift, Scala, Dart, R,
    Lua, Perl, ObjC, Fortran, Hlasm,
    Rexx, Jcl, Pli, Haskell, Elixir,
    /* v2.1.4: languages 31-45 */
    Ada, PowerShell, Matlab, Groovy, Zig,
    Abap, FSharp, Erlang, Clojure, Crystal,
    Nim, Dlang, OCaml, Vhdl, Verilog,
    /* v2.1.4: language 46 */
    Vb6,
    Unknown
};

inline const char* language_name(Language l) {
    switch (l) {
    case Language::Cpp:        return "C++";
    case Language::C:          return "C";
    case Language::Header:     return "C/C++ Header";
    case Language::Python:     return "Python";
    case Language::JavaScript: return "JavaScript";
    case Language::TypeScript: return "TypeScript";
    case Language::Java:       return "Java";
    case Language::CSharp:     return "C#";
    case Language::Go:         return "Go";
    case Language::Rust:       return "Rust";
    case Language::Ruby:       return "Ruby";
    case Language::Php:        return "PHP";
    case Language::Cobol:      return "COBOL";
    case Language::Shell:      return "Shell";
    case Language::Sql:        return "SQL";
    case Language::Kotlin:     return "Kotlin";
    case Language::Swift:      return "Swift";
    case Language::Scala:      return "Scala";
    case Language::Dart:       return "Dart";
    case Language::R:          return "R";
    case Language::Lua:        return "Lua";
    case Language::Perl:       return "Perl";
    case Language::ObjC:       return "Objective-C";
    case Language::Fortran:    return "Fortran";
    case Language::Hlasm:      return "HLASM";
    case Language::Rexx:       return "REXX";
    case Language::Jcl:        return "JCL";
    case Language::Pli:        return "PL/I";
    case Language::Haskell:    return "Haskell";
    case Language::Elixir:     return "Elixir";
    case Language::Ada:        return "Ada";
    case Language::PowerShell: return "PowerShell";
    case Language::Matlab:     return "MATLAB";
    case Language::Groovy:     return "Groovy";
    case Language::Zig:        return "Zig";
    case Language::Abap:       return "ABAP";
    case Language::FSharp:     return "F#";
    case Language::Erlang:     return "Erlang";
    case Language::Clojure:    return "Clojure";
    case Language::Crystal:    return "Crystal";
    case Language::Nim:        return "Nim";
    case Language::Dlang:      return "D";
    case Language::OCaml:      return "OCaml";
    case Language::Vhdl:       return "VHDL";
    case Language::Verilog:    return "Verilog";
    case Language::Vb6:        return "Visual Basic 6";
    case Language::Unknown:    return "Other";
    }
    return "Other";
}

inline Language detect_language(const std::string& ext_in) {
    std::string e = ext_in;
    for (char& c : e) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

    /* Original 15 */
    if (e == ".cpp" || e == ".cc" || e == ".cxx") return Language::Cpp;
    if (e == ".c")   return Language::C;
    if (e == ".hpp" || e == ".h" || e == ".hh" || e == ".hxx") return Language::Header;
    if (e == ".py")  return Language::Python;
    if (e == ".js" || e == ".jsx" || e == ".mjs") return Language::JavaScript;
    if (e == ".ts" || e == ".tsx") return Language::TypeScript;
    if (e == ".java") return Language::Java;
    if (e == ".cs")  return Language::CSharp;
    if (e == ".go")  return Language::Go;
    if (e == ".rs")  return Language::Rust;
    if (e == ".rb")  return Language::Ruby;
    if (e == ".php" || e == ".phtml") return Language::Php;
    if (e == ".cob" || e == ".cbl" || e == ".cpy") return Language::Cobol;
    if (e == ".sh"  || e == ".bash" || e == ".zsh" || e == ".ksh") return Language::Shell;
    if (e == ".sql") return Language::Sql;

    /* Languages 16-30 */
    if (e == ".kt"  || e == ".kts")  return Language::Kotlin;
    if (e == ".swift")               return Language::Swift;
    if (e == ".scala" || e == ".sc") return Language::Scala;
    if (e == ".dart")                return Language::Dart;
    if (e == ".r")                   return Language::R;
    if (e == ".lua")                 return Language::Lua;
    if (e == ".pl"  || e == ".pm")   return Language::Perl;
    if (e == ".m"   || e == ".mm")   return Language::ObjC;
    if (e == ".f90" || e == ".f95" || e == ".f03" || e == ".f08" ||
        e == ".f"   || e == ".for" || e == ".f77") return Language::Fortran;
    if (e == ".asm" || e == ".hlasm" || e == ".mac") return Language::Hlasm;
    if (e == ".rexx" || e == ".rex" || e == ".rxp")  return Language::Rexx;
    if (e == ".jcl" || e == ".jcllib")               return Language::Jcl;
    if (e == ".pli" || e == ".pl1")                  return Language::Pli;
    if (e == ".hs"  || e == ".lhs")  return Language::Haskell;
    if (e == ".ex"  || e == ".exs")  return Language::Elixir;

    /* Languages 31-45 (v2.1.4) */
    if (e == ".adb" || e == ".ads")                    return Language::Ada;
    if (e == ".ps1" || e == ".psm1" || e == ".psd1")   return Language::PowerShell;
    if (e == ".mlx")                                    return Language::Matlab;
    if (e == ".groovy" || e == ".gvy" || e == ".gy")   return Language::Groovy;
    if (e == ".zig")                                    return Language::Zig;
    if (e == ".abap")                                   return Language::Abap;
    if (e == ".fs"  || e == ".fsi" || e == ".fsx")     return Language::FSharp;
    if (e == ".erl" || e == ".hrl")                     return Language::Erlang;
    if (e == ".clj" || e == ".cljs" || e == ".cljc")   return Language::Clojure;
    if (e == ".cr")                                     return Language::Crystal;
    if (e == ".nim" || e == ".nims")                    return Language::Nim;
    if (e == ".d")                                      return Language::Dlang;
    if (e == ".ml"  || e == ".mli")                     return Language::OCaml;
    if (e == ".vhd" || e == ".vhdl")                    return Language::Vhdl;
    if (e == ".v"   || e == ".sv" || e == ".svh")       return Language::Verilog;
    if (e == ".bas" || e == ".cls" || e == ".frm" ||
        e == ".ctl" || e == ".vbp")                      return Language::Vb6;

    return Language::Unknown;
}

struct LanguageInfo {
    std::string name;
    std::vector<std::string> extensions;
};

/* Single source of truth for recognized languages, mirroring detect_language().
 * Used by /api/languages and the dashboard language panel. */
inline std::vector<LanguageInfo> supported_languages() {
    return {
        /* Original 15 */
        {"C++",           {".cpp", ".cc", ".cxx"}},
        {"C",             {".c"}},
        {"C/C++ Header",  {".hpp", ".h", ".hh", ".hxx"}},
        {"Python",        {".py"}},
        {"JavaScript",    {".js", ".jsx", ".mjs"}},
        {"TypeScript",    {".ts", ".tsx"}},
        {"Java",          {".java"}},
        {"C#",            {".cs"}},
        {"Go",            {".go"}},
        {"Rust",          {".rs"}},
        {"Ruby",          {".rb"}},
        {"PHP",           {".php", ".phtml"}},
        {"COBOL",         {".cob", ".cbl", ".cpy"}},
        {"Shell",         {".sh", ".bash", ".zsh", ".ksh"}},
        {"SQL",           {".sql"}},
        /* Added in v2.1.4 */
        {"Kotlin",        {".kt", ".kts"}},
        {"Swift",         {".swift"}},
        {"Scala",         {".scala", ".sc"}},
        {"Dart",          {".dart"}},
        {"R",             {".r"}},
        {"Lua",           {".lua"}},
        {"Perl",          {".pl", ".pm"}},
        {"Objective-C",   {".m", ".mm"}},
        {"Fortran",       {".f90", ".f95", ".f03", ".f08", ".f", ".for", ".f77"}},
        {"HLASM",         {".asm", ".hlasm", ".mac"}},
        {"REXX",          {".rexx", ".rex", ".rxp"}},
        {"JCL",           {".jcl", ".jcllib"}},
        {"PL/I",          {".pli", ".pl1"}},
        {"Haskell",       {".hs", ".lhs"}},
        {"Elixir",        {".ex", ".exs"}},
        /* v2.1.4 */
        {"Ada",           {".adb", ".ads"}},
        {"PowerShell",    {".ps1", ".psm1", ".psd1"}},
        {"MATLAB",        {".mlx"}},
        {"Groovy",        {".groovy", ".gvy", ".gy"}},
        {"Zig",           {".zig"}},
        {"ABAP",          {".abap"}},
        {"F#",            {".fs", ".fsi", ".fsx"}},
        {"Erlang",        {".erl", ".hrl"}},
        {"Clojure",       {".clj", ".cljs", ".cljc"}},
        {"Crystal",       {".cr"}},
        {"Nim",           {".nim", ".nims"}},
        {"D",             {".d"}},
        {"OCaml",         {".ml", ".mli"}},
        {"VHDL",          {".vhd", ".vhdl"}},
        {"Verilog",       {".v", ".sv", ".svh"}},
        {"Visual Basic 6",{".bas", ".cls", ".frm", ".ctl", ".vbp"}}
    };
}

struct SourceFile {
    std::string path;       /* path relative to scan root */
    Language language = Language::Unknown;
    std::string content;
};

class Scanner {
public:
    explicit Scanner(std::vector<std::string> excludes,
                     std::size_t max_file_bytes = 4u * 1024u * 1024u)
        : excludes_(std::move(excludes)), max_file_bytes_(max_file_bytes) {}

    std::vector<SourceFile> scan(const std::string& root) const {
        namespace fs = std::filesystem;
        std::vector<SourceFile> files;
        std::error_code ec;
        fs::path base = fs::path(root);

        if (!fs::exists(base, ec)) return files;

        for (auto it = fs::recursive_directory_iterator(
                 base, fs::directory_options::skip_permission_denied, ec);
             it != fs::recursive_directory_iterator(); it.increment(ec)) {
            if (ec) { ec.clear(); continue; }
            const fs::directory_entry& entry = *it;
            std::string name = entry.path().filename().string();

            if (entry.is_directory(ec)) {
                if (is_excluded(name)) it.disable_recursion_pending();
                continue;
            }
            if (!entry.is_regular_file(ec)) continue;

            Language lang = detect_language(entry.path().extension().string());
            if (lang == Language::Unknown) continue;
            if (is_excluded(name)) continue;

            SourceFile sf;
            std::error_code rel_ec;
            fs::path rel = fs::relative(entry.path(), base, rel_ec);
            sf.path = rel_ec ? entry.path().string() : rel.generic_string();
            sf.language = lang;
            sf.content = read_file(entry.path());
            if (sf.content.size() > max_file_bytes_) {
                sf.content.resize(max_file_bytes_);
            }
            files.push_back(std::move(sf));
        }
        std::sort(files.begin(), files.end(),
                  [](const SourceFile& a, const SourceFile& b) { return a.path < b.path; });
        return files;
    }

private:
    bool is_excluded(const std::string& name) const {
        for (const std::string& ex : excludes_) {
            if (name == ex) return true;
        }
        return false;
    }

    static std::string read_file(const std::filesystem::path& p) {
        std::ifstream in(p, std::ios::binary);
        std::ostringstream ss;
        ss << in.rdbuf();
        return ss.str();
    }

    std::vector<std::string> excludes_;
    std::size_t max_file_bytes_;
};

} /* namespace metis::codeanalyzer */

#endif /* METIS_CODE_ANALYZER_SCANNER_HPP */
