/* Metis Code Analyzer Plus - technology inventory
 * Scans dependency imports extracted by analyzer.hpp to identify known
 * frameworks and libraries. Header-only, zero external dependencies, C++20.
 */
#ifndef METIS_CODE_ANALYZER_TECHNOLOGIES_HPP
#define METIS_CODE_ANALYZER_TECHNOLOGIES_HPP

#include <algorithm>
#include <map>
#include <string>
#include <vector>

#include "metis/codeanalyzer/analyzer.hpp"

namespace metis::codeanalyzer::tech {

struct TechEntry {
    std::string name;       /* e.g. "React" */
    std::string category;   /* e.g. "Frontend framework" */
    std::string language;   /* e.g. "JavaScript" or "" for multi-language */
    int file_count = 0;     /* number of files importing this technology */
};

/* Mapping from import token prefix/contains to tech name + category.
 * Order matters: more specific patterns must come first. */
struct TechPattern {
    const char* token;      /* substring of import path */
    const char* name;
    const char* category;
};

static const TechPattern kPatterns[] = {
    /* JavaScript / TypeScript */
    {"react",             "React",               "Frontend framework"},
    {"vue",               "Vue",                 "Frontend framework"},
    {"@angular/",         "Angular",             "Frontend framework"},
    {"svelte",            "Svelte",              "Frontend framework"},
    {"next/",             "Next.js",             "Full-stack framework"},
    {"nuxt",              "Nuxt",                "Full-stack framework"},
    {"express",           "Express",             "Web framework"},
    {"fastify",           "Fastify",             "Web framework"},
    {"koa",               "Koa",                 "Web framework"},
    {"mongoose",          "Mongoose",            "Database / ODM"},
    {"sequelize",         "Sequelize",           "ORM"},
    {"typeorm",           "TypeORM",             "ORM"},
    {"prisma",            "Prisma",              "ORM"},
    {"graphql",           "GraphQL",             "API layer"},
    {"axios",             "Axios",               "HTTP client"},
    {"lodash",            "Lodash",              "Utility library"},
    {"redux",             "Redux",               "State management"},
    {"mobx",              "MobX",                "State management"},
    {"jest",              "Jest",                "Testing framework"},
    {"mocha",             "Mocha",               "Testing framework"},
    {"vitest",            "Vitest",              "Testing framework"},
    {"webpack",           "Webpack",             "Bundler"},
    {"vite",              "Vite",                "Bundler"},
    {"rollup",            "Rollup",              "Bundler"},
    /* Python */
    {"django",            "Django",              "Web framework"},
    {"flask",             "Flask",               "Web framework"},
    {"fastapi",           "FastAPI",             "Web framework"},
    {"tornado",           "Tornado",             "Web framework"},
    {"sqlalchemy",        "SQLAlchemy",          "ORM"},
    {"pandas",            "Pandas",              "Data analysis"},
    {"numpy",             "NumPy",               "Numerical computing"},
    {"scipy",             "SciPy",               "Scientific computing"},
    {"sklearn",           "scikit-learn",        "Machine learning"},
    {"tensorflow",        "TensorFlow",          "Machine learning"},
    {"torch",             "PyTorch",             "Machine learning"},
    {"keras",             "Keras",               "Machine learning"},
    {"celery",            "Celery",              "Task queue"},
    {"redis",             "Redis",               "Cache / Message broker"},
    {"pytest",            "pytest",              "Testing framework"},
    /* Java / Kotlin */
    {"org.springframework","Spring",             "Enterprise framework"},
    {"javax.servlet",     "Java Servlet",        "Web API"},
    {"jakarta.servlet",   "Jakarta EE",          "Enterprise framework"},
    {"hibernate",         "Hibernate",           "ORM"},
    {"junit",             "JUnit",               "Testing framework"},
    {"org.slf4j",         "SLF4J",               "Logging"},
    {"com.fasterxml.jackson","Jackson",          "JSON serialization"},
    {"io.grpc",           "gRPC",                "RPC framework"},
    /* Go */
    {"github.com/gin-gonic","Gin",               "Web framework"},
    {"github.com/gorilla", "Gorilla",            "Web toolkit"},
    {"github.com/labstack/echo","Echo",          "Web framework"},
    {"go.uber.org/zap",   "Zap",                 "Logging"},
    {"gorm.io",           "GORM",                "ORM"},
    {"github.com/stretchr/testify","Testify",    "Testing framework"},
    /* Rust */
    {"tokio",             "Tokio",               "Async runtime"},
    {"actix",             "Actix",               "Web framework"},
    {"axum",              "Axum",                "Web framework"},
    {"serde",             "Serde",               "Serialization"},
    {"reqwest",           "Reqwest",             "HTTP client"},
    {"sqlx",              "SQLx",                "Database"},
    {"diesel",            "Diesel",              "ORM"},
    {"tracing",           "Tracing",             "Telemetry"},
    /* C / C++ */
    {"boost/",            "Boost",               "C++ library suite"},
    {"openssl",           "OpenSSL",             "TLS / Crypto"},
    {"sqlite",            "SQLite",              "Database"},
    {"curl",              "libcurl",             "HTTP client"},
    {"gtest",             "Google Test",         "Testing framework"},
    {"catch2",            "Catch2",              "Testing framework"},
    {"nlohmann",          "nlohmann/json",       "JSON library"},
    {"spdlog",            "spdlog",              "Logging"},
    {"fmt/",              "fmtlib",              "Formatting"},
    /* General */
    {"kafka",             "Apache Kafka",        "Message broker"},
    {"rabbitmq",          "RabbitMQ",            "Message broker"},
    {"elasticsearch",     "Elasticsearch",       "Search engine"},
    {"prometheus",        "Prometheus",          "Monitoring"},
    {"opentelemetry",     "OpenTelemetry",       "Telemetry"},
    {"grpc",              "gRPC",                "RPC framework"},
    {nullptr,             nullptr,               nullptr}
};

inline std::string to_lower(const std::string& s) {
    std::string out; out.reserve(s.size());
    for (char c : s)
        out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    return out;
}

/* Build a technology inventory from the dependency lists in a result set. */
inline std::vector<TechEntry> detect(const AnalysisResult& result) {
    /* Accumulate file counts per technology name. */
    std::map<std::string, TechEntry> found;

    for (const FileRecord& fr : result.files) {
        std::string already;   /* prevent counting same tech twice per file */
        for (const std::string& dep : fr.dependencies) {
            std::string low = to_lower(dep);
            for (const TechPattern* p = kPatterns; p->token; ++p) {
                if (low.find(p->token) != std::string::npos) {
                    std::string key = p->name;
                    if (already.find(key + ";") != std::string::npos) continue;
                    already += key + ";";
                    auto& e = found[key];
                    e.name     = p->name;
                    e.category = p->category;
                    ++e.file_count;
                    break; /* first matching pattern wins for this dep */
                }
            }
        }
    }

    std::vector<TechEntry> out;
    out.reserve(found.size());
    for (auto& kv : found) out.push_back(std::move(kv.second));
    std::sort(out.begin(), out.end(),
              [](const TechEntry& a, const TechEntry& b) {
                  return a.file_count > b.file_count ||
                         (a.file_count == b.file_count && a.name < b.name);
              });
    return out;
}

} /* namespace metis::codeanalyzer::tech */

#endif /* METIS_CODE_ANALYZER_TECHNOLOGIES_HPP */
