/* Metis Code Analyzer Plus - version constants
 * Application intelligence and static code analysis platform.
 * Pure 7-bit ASCII. C++20.
 */
#ifndef METIS_CODE_ANALYZER_VERSION_HPP
#define METIS_CODE_ANALYZER_VERSION_HPP

#define METIS_CODE_ANALYZER_VERSION_MAJOR 2
#define METIS_CODE_ANALYZER_VERSION_MINOR 4
#define METIS_CODE_ANALYZER_VERSION_PATCH 2
#define METIS_CODE_ANALYZER_VERSION_STRING "2.4.2"
#define METIS_CODE_ANALYZER_PRODUCT_NAME "Metis Code Analyzer Plus"

namespace metis::codeanalyzer {

inline constexpr int    version_major  = METIS_CODE_ANALYZER_VERSION_MAJOR;
inline constexpr int    version_minor  = METIS_CODE_ANALYZER_VERSION_MINOR;
inline constexpr int    version_patch  = METIS_CODE_ANALYZER_VERSION_PATCH;
inline constexpr const char* version   = METIS_CODE_ANALYZER_VERSION_STRING;
inline constexpr const char* product   = METIS_CODE_ANALYZER_PRODUCT_NAME;

} /* namespace metis::codeanalyzer */

#endif /* METIS_CODE_ANALYZER_VERSION_HPP */
