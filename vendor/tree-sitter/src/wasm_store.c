/* Stub WASM store -- WASM not used in Metis; all functions are no-ops. */
#include "tree_sitter/api.h"
#include "./parser.h"
#include "./wasm_store.h"
bool ts_wasm_store_start(TSWasmStore *s, TSLexer *l, const TSLanguage *g) { (void)s;(void)l;(void)g;return false; }
void ts_wasm_store_reset(TSWasmStore *s)                                   { (void)s; }
bool ts_wasm_store_has_error(const TSWasmStore *s)                         { (void)s;return false; }
bool ts_wasm_store_call_lex_main(TSWasmStore *s, TSStateId st)             { (void)s;(void)st;return false; }
bool ts_wasm_store_call_lex_keyword(TSWasmStore *s, TSStateId st)          { (void)s;(void)st;return false; }
uint32_t ts_wasm_store_call_scanner_create(TSWasmStore *s)                 { (void)s;return 0; }
void ts_wasm_store_call_scanner_destroy(TSWasmStore *s, uint32_t a)        { (void)s;(void)a; }
bool ts_wasm_store_call_scanner_scan(TSWasmStore *s, uint32_t a, uint32_t v) { (void)s;(void)a;(void)v;return false; }
uint32_t ts_wasm_store_call_scanner_serialize(TSWasmStore *s, uint32_t a, char *b) { (void)s;(void)a;(void)b;return 0; }
void ts_wasm_store_call_scanner_deserialize(TSWasmStore *s, uint32_t a, const char *b, unsigned l) { (void)s;(void)a;(void)b;(void)l; }
void ts_wasm_language_retain(const TSLanguage *l)                          { (void)l; }
void ts_wasm_language_release(const TSLanguage *l)                         { (void)l; }
bool ts_language_is_wasm(const TSLanguage *l)                              { (void)l;return false; }
