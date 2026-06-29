cmake_minimum_required(VERSION 3.20)

# Version is maintained in version.hpp (pre-generated, bumped manually).
project(Metis_Code_Analyzer_Plus
  VERSION 2.7.4
  DESCRIPTION "Metis Code Analyzer Plus"
  LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Prevent Ninja dirty-manifest regeneration loops (suite convention).
set(CMAKE_SUPPRESS_REGENERATION TRUE)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

add_executable(metis_code_analyzer_plus src/main.cpp)

target_include_directories(metis_code_analyzer_plus PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

# Opt-in post-quantum TLS. OFF keeps the build dependency-free.
# ON links OpenSSL (>= 3.5 for the ML-KEM hybrid; older falls back to classical).
# On Windows the prebuilt static libs in openssl-prebuilt/windows/ are used
# automatically; no external install is required.
# On Linux/macOS CMake finds the system OpenSSL via find_package.
option(METIS_ENABLE_TLS "Enable post-quantum TLS (requires OpenSSL)" ON)
if(METIS_ENABLE_TLS)
  if(WIN32)
    set(_OSSL_PRE ${CMAKE_CURRENT_SOURCE_DIR}/openssl-prebuilt/windows)
    if(NOT EXISTS ${_OSSL_PRE}/lib/libssl.a)
      message(FATAL_ERROR "openssl-prebuilt/windows/lib/libssl.a not found. "
              "Ensure the openssl-prebuilt/ directory is present in the project root.")
    endif()
    # Expose prebuilt headers and libs as imported targets so the rest of the
    # CMakeLists can reference OpenSSL::SSL / OpenSSL::Crypto identically on
    # all platforms.
    add_library(OpenSSL::Crypto STATIC IMPORTED GLOBAL)
    set_target_properties(OpenSSL::Crypto PROPERTIES
      IMPORTED_LOCATION ${_OSSL_PRE}/lib/libcrypto.a
      INTERFACE_INCLUDE_DIRECTORIES ${_OSSL_PRE}/include)
    add_library(OpenSSL::SSL STATIC IMPORTED GLOBAL)
    set_target_properties(OpenSSL::SSL PROPERTIES
      IMPORTED_LOCATION ${_OSSL_PRE}/lib/libssl.a
      INTERFACE_INCLUDE_DIRECTORIES ${_OSSL_PRE}/include
      INTERFACE_LINK_LIBRARIES OpenSSL::Crypto)
    message(STATUS "Using prebuilt OpenSSL 4.0.0 from openssl-prebuilt/windows/")
  else()
    find_package(OpenSSL REQUIRED)
  endif()
  target_compile_definitions(metis_code_analyzer_plus PRIVATE METIS_ENABLE_TLS)
  target_link_libraries(metis_code_analyzer_plus PRIVATE OpenSSL::SSL OpenSSL::Crypto)
  if(WIN32)
    # OpenSSL on Windows requires these system libraries.
    target_link_libraries(metis_code_analyzer_plus PRIVATE crypt32 ws2_32)
  endif()
endif()

# Opt-in AST analysis via vendored tree-sitter. OFF keeps the build zero-dep.
# ON compiles the tree-sitter runtime and six language grammars (C, Python,
# JavaScript, Go, Java, Rust) and enables accurate per-function complexity,
# function counts, and nesting depth for those languages.
option(METIS_ENABLE_AST "Enable AST-accurate metrics via tree-sitter" OFF)
if(METIS_ENABLE_AST)
  set(TS_SRC ${CMAKE_CURRENT_SOURCE_DIR}/vendor/tree-sitter/src)
  set(TS_INC ${CMAKE_CURRENT_SOURCE_DIR}/vendor/tree-sitter/include)
  set(TS_GRAMMARS ${CMAKE_CURRENT_SOURCE_DIR}/vendor/tree-sitter-grammars)

  # Compile tree-sitter runtime as a C library.
  add_library(tree_sitter_runtime STATIC ${TS_SRC}/lib.c)
  target_compile_options(tree_sitter_runtime PRIVATE -O2 -std=c11)
  target_include_directories(tree_sitter_runtime PRIVATE ${TS_INC} ${TS_SRC})

  # Compile each grammar (some have an external scanner alongside parser.c).
  foreach(LANG c python javascript go java rust)
    set(GRAMMAR_SRCS ${TS_GRAMMARS}/${LANG}/parser.c)
    if(EXISTS ${TS_GRAMMARS}/${LANG}/scanner.c)
      list(APPEND GRAMMAR_SRCS ${TS_GRAMMARS}/${LANG}/scanner.c)
    endif()
    add_library(ts_grammar_${LANG} STATIC ${GRAMMAR_SRCS})
    target_compile_options(ts_grammar_${LANG} PRIVATE -O2 -std=c11)
    target_include_directories(ts_grammar_${LANG} PRIVATE ${TS_INC})
    target_link_libraries(metis_code_analyzer_plus PRIVATE ts_grammar_${LANG})
  endforeach()

  target_compile_definitions(metis_code_analyzer_plus PRIVATE METIS_ENABLE_AST)
  target_include_directories(metis_code_analyzer_plus PRIVATE ${TS_INC})
  target_link_libraries(metis_code_analyzer_plus PRIVATE tree_sitter_runtime)
endif()

if(MSVC)
  target_compile_options(metis_code_analyzer_plus PRIVATE /W4 /permissive-)
else()
  target_compile_options(metis_code_analyzer_plus PRIVATE -Wall -Wextra -Wpedantic)
endif()

find_package(Threads REQUIRED)
target_link_libraries(metis_code_analyzer_plus PRIVATE Threads::Threads)

if(WIN32)
  target_link_libraries(metis_code_analyzer_plus PRIVATE ws2_32)
endif()

# Stage runtime assets next to the binary.
add_custom_command(TARGET metis_code_analyzer_plus POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          ${CMAKE_CURRENT_SOURCE_DIR}/web $<TARGET_FILE_DIR:metis_code_analyzer_plus>/web
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          ${CMAKE_CURRENT_SOURCE_DIR}/code_analyzer.pson $<TARGET_FILE_DIR:metis_code_analyzer_plus>/code_analyzer.pson
  # Copy certs/ directory (server.crt + server.key) if it exists.
  # Generate them once with: powershell -ExecutionPolicy Bypass -File certs\gen_certs.ps1
  COMMAND ${CMAKE_COMMAND} -E make_directory
          $<TARGET_FILE_DIR:metis_code_analyzer_plus>/certs
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          ${CMAKE_CURRENT_SOURCE_DIR}/certs $<TARGET_FILE_DIR:metis_code_analyzer_plus>/certs
  COMMENT "Staging web assets, config, and certs")
