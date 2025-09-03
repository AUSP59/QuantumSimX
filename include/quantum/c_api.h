// SPDX-License-Identifier: MIT

#pragma once
#ifdef __cplusplus
extern "C" {
#endif

// Runs a circuit given in QASM 2.0 subset or QSX text (autodetected by first non-space char 'O' for OPENQASM).
// options_json supports keys: backend ("state"|"density"), shots (int), seed (uint64).
// Returns 0 on success; *out_json must be freed with qsx_free().
int qsx_run_string(const char* circuit_text, const char* options_json, char** out_json);

// Frees buffers allocated by the library.
void qsx_free(char* p);

#ifdef __cplusplus
}
#endif


#ifdef __cplusplus
extern "C" {
#endif

// Runs a circuit from a file path (auto-detects QASM/QSX by header).
// Returns 0 on success; *out_json must be freed with qsx_free().
int qsx_run_file(const char* filepath, const char* options_json, char** out_json);

#ifdef __cplusplus
}
#endif


#ifdef __cplusplus
extern "C" {
#endif

// Returns the compiled library version string.
const char* qsx_version(void);

#ifdef __cplusplus
}
#endif
