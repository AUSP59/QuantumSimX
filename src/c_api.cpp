// SPDX-License-Identifier: MIT

#include "quantum/c_api.h"
#include "quantum/circuit.hpp"
#include "quantum/optimize.hpp"
#include <string>
#include <sstream>
#include <optional>
#include <cstring>

extern "C" {

static std::string get_kv(const std::string& js, const std::string& k){
  auto p = js.find(k);
  if (p==std::string::npos) return "";
  p = js.find(':', p);
  if (p==std::string::npos) return "";
  auto q = js.find_first_not_of(" \t\r\n", p+1);
  if (q==std::string::npos) return "";
  if (js[q]=='"'){ auto e = js.find('"', q+1); return js.substr(q+1, e-q-1); }
  auto e = js.find_first_of(",}\n", q);
  return js.substr(q, e-q);
}

int qsx_run_string(const char* circuit_text, const char* options_json, char** out_json){
  if (!circuit_text || !out_json) return 2;
  std::string txt(circuit_text);
  std::string err;
  std::optional<qsx::Circuit> circ_opt;
  // crude autodetect
  std::string trimmed = txt; trimmed.erase(0, trimmed.find_first_not_of(" \t\r\n"));
  if (trimmed.rfind("OPENQASM", 0) == 0){
    circ_opt = parse_qasm_string(txt, err);
  } else {
    circ_opt = parse_circuit_string(txt, err);
  }
  if (!circ_opt) return 3;
  auto circ = *circ_opt;
  std::string opts = options_json? options_json : "";
  std::string be = get_kv(opts, ""backend"");
  int shots = 1;
  uint64_t seed = 12345;
  if (auto s = get_kv(opts, ""shots""); !s.empty()) shots = std::max(1, std::stoi(s));
  if (auto s = get_kv(opts, ""seed""); !s.empty()) seed = std::stoull(s);
  bool use_density = (be=="density");
  std::vector<std::vector<int>> outcomes; outcomes.reserve(shots);
  std::map<std::string,int> counts;
  std::vector<double> probs;
  for (int s=0;s<shots;++s){
    auto r = qsx::run(circ, seed + s, use_density);
    if (s==0) probs = r.probabilities;
    outcomes.push_back(r.outcome);
    // helper: bitstring msb..lsb
    std::string key; key.reserve(r.outcome.size());
    for (int i=int(r.outcome.size())-1;i>=0;--i) key.push_back(r.outcome[i]?'1':'0');
    counts[key] += 1;
  }
  std::ostringstream os;
  os << "{\n  \"nqubits\": " << circ.nqubits << ",\n  \"probabilities\": [";
  for (size_t i=0;i<probs.size();++i){ os<<probs[i]; if (i+1<probs.size()) os<<", "; }
  os << "],\n  \"counts\": {\n";
  size_t k=0; for (auto it=counts.begin(); it!=counts.end(); ++it,++k){ os << "    \"" << it->first << "\": " << it->second << (std::next(it)!=counts.end() ? "," : "") << "\n"; }
  os << "  },\n  \"outcomes\": [\n";
  for (size_t s=0; s<outcomes.size(); ++s){ os << "    ["; for (size_t i=0;i<outcomes[s].size(); ++i){ os << outcomes[s][i]; if (i+1<outcomes[s].size()) os << ", "; } os << "]" << (s+1<outcomes.size()? ",":"") << "\n"; }
  os << "  ]\n}\n";
  std::string js = os.str();
  char* buf = (char*)std::malloc(js.size()+1);
  std::memcpy(buf, js.data(), js.size()); buf[js.size()]='\0';
  *out_json = buf;
  return 0;
}

void qsx_free(char* p){ if (p) std::free(p); }

} // extern "C"


extern "C" {
int qsx_run_file(const char* filepath, const char* options_json, char** out_json){
  if (!filepath) return 2;
  std::ifstream in(filepath, std::ios::binary);
  if (!in) return 3;
  std::string txt((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  return qsx_run_string(txt.c_str(), options_json, out_json);
}
}


extern "C" {
const char* qsx_version(void){
#ifdef QSX_VERSION
  return QSX_VERSION;
#else
  return "unknown";
#endif
}
}
