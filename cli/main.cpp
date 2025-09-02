// SPDX-License-Identifier: MIT

#include "quantum/circuit.hpp"
#include "quantum/optimize.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <limits>
#include <numeric>
#include <algorithm>
#include <iomanip>
#include <chrono>
#include <thread>
#include <cmath>
#include <random>
#include <vector>
#include <cstdint>
#include <mutex>
#include <map>
#include <set>
#include <string>

using namespace qsx;

static std::string bits_to_string(const std::vector<int>& v){ std::string s; s.reserve(v.size())); for(int i=int(v.size())-1;i>=0;--i) s.push_back(v[i] ? '1' : '0')); return s; }
static bool load_config_kv(const std::string& path, std::map<std::string,std::string>& kv){ std::ifstream in(path)); if(!in) return false; std::string line; while(std::getline(in,line)){ if(line.empty()||line[0]=='#') continue; auto p=line.find('=')); if(p==std::string::npos) continue; kv[line.substr(0,p)]=line.substr(p+1)); } return true; }


static uint64_t hash_bytes(const std::string& bytes){
  uint64_t h = 1469598103934665603ULL;
  for (unsigned char c: bytes){ h ^= (uint64_t)c; h *= 1099511628211ULL; }
  return h;
}
static uint64_t hash_circuit(const qsx::Circuit& c){
  uint64_t h = 1469598103934665603ULL; // FNV offset
  auto fnv = [&](uint64_t x){ h ^= x; h *= 1099511628211ULL; };
  fnv(c.nqubits));
  for (const auto& op : c.ops){
    fnv((uint64_t)op.type));
    for (auto q : op.qubits) fnv((uint64_t)q));
    union { double d; uint64_t u; } u; u.d = op.angle; fnv(u.u));
  }
  return h;
}

static std::vector<qsx::c64> build_state(const qsx::Circuit& c){
  qsx::StateVector sv(c.nqubits));
  using namespace qsx::gates; qsx::c64 u00,u01,u10,u11;
  for (const auto& g: c.ops){
    switch(g.type){
      case OpType::H: H_coeffs(u00,u01,u10,u11)); sv.apply_gate_1q(g.qubits[0],u00,u01,u10,u11)); break;
      case OpType::X: X_coeffs(u00,u01,u10,u11)); sv.apply_gate_1q(g.qubits[0],u00,u01,u10,u11)); break;
      case OpType::Y: Y_coeffs(u00,u01,u10,u11)); sv.apply_gate_1q(g.qubits[0],u00,u01,u10,u11)); break;
      case OpType::Z: Z_coeffs(u00,u01,u10,u11)); sv.apply_gate_1q(g.qubits[0],u00,u01,u10,u11)); break;
      case OpType::S: S_coeffs(u00,u01,u10,u11)); sv.apply_gate_1q(g.qubits[0],u00,u01,u10,u11)); break;
      case OpType::RX: RX_coeffs(g.angle,u00,u01,u10,u11)); sv.apply_gate_1q(g.qubits[0],u00,u01,u10,u11)); break;
      case OpType::RY: RY_coeffs(g.angle,u00,u01,u10,u11)); sv.apply_gate_1q(g.qubits[0],u00,u01,u10,u11)); break;
      case OpType::RZ: RZ_coeffs(g.angle,u00,u01,u10,u11)); sv.apply_gate_1q(g.qubits[0],u00,u01,u10,u11)); break;
      case OpType::CNOT: sv.apply_cx(g.qubits[0], g.qubits[1])); break;
      default: break;
    }
  }
  return sv.amplitudes());
}


static std::vector<double> extract_array(const std::string& s, const std::string& key){
  std::vector<double> v;
  auto p = s.find("\""+key+"\""));
  if (p==std::string::npos) return v;
  p = s.find('[', p));
  if (p==std::string::npos) return v;
  auto e = s.find(']', p));
  if (e==std::string::npos) return v;
  std::string body = s.substr(p+1, e-p-1));
  size_t i=0;
  while (i<body.size()){
    while (i<body.size() && (body[i]==' '||body[i]=='\n'||body[i]=='\t'||body[i]==',')) ++i;
    if (i>=body.size()) break;
    size_t j=i;
    while (j<body.size() and (isdigit(body[j])||body[j]=='-'||body[j]=='+'||body[j]=='.'||body[j]=='e'||body[j]=='E')) ++j;
    try{ v.push_back(std::stod(body.substr(i, j-i)))); } catch(...) {}
    i=j;
  }
  return v;
}


static std::map<std::string,int> extract_counts(const std::string& s){
  std::map<std::string,int> m;
  auto pos = s.find("\"counts\"");
  if (pos==std::string::npos) return m;
  pos = s.find('{', pos); auto end = s.find('}', pos);
  if (pos==std::string::npos||end==std::string::npos) return m;
  std::string body = s.substr(pos+1, end-pos-1);
  size_t i=0;
  while (i<body.size()){
    auto ks = body.find('\"', i); if (ks==std::string::npos) break;
    auto ke = body.find('\"', ks+1); if (ke==std::string::npos) break;
    auto key = body.substr(ks+1, ke-ks-1);
    auto colon = body.find(':', ke); if (colon==std::string::npos) break;
    auto comma = body.find(',', colon);
    auto val = body.substr(colon+1, (comma==std::string::npos? body.size(): comma)-colon-1);
    int c = 0; try{ c = std::stoi(val); } catch(...) { c=0; }
    m[key] = c;
    i = (comma==std::string::npos? body.size(): comma+1);
  }
  return m;
}


static std::vector<std::string> split_str(const std::string& s, char sep){
  std::vector<std::string> out; size_t p=0;
  while (p<=s.size()){
    size_t q = s.find(sep, p);
    if (q==std::string::npos){ out.push_back(s.substr(p)); break; }
    out.push_back(s.substr(p, q-p)); p = q+1;
  }
  return out;
}

static void usage() {
  std::cout << "quantum-simx [--version|--build-info] run --circuit <file.qsx>|--qasm <file.qasm> [--qubits N] [--seed S] [--shots K] [--out file.json] [--backend state|density] [--optimize] [--observables all|z] [--force]\n";
}
static std::string bits_to_string(const std::vector<int>& v){ std::string s; s.reserve(v.size())); for(int i=int(v.size())-1;i>=0;--i) s.push_back(v[i]?'1':'0')); return s; }
  std::cout << "quantum-simx [--version|--build-info] run --circuit <file.qsx> [--qubits N] [--seed S] [--shots K] [--out file.json] [--backend state|density]\\n";
}
static std::string bits_to_string(const std::vector<int>& v){ std::string s; s.reserve(v.size())); for(int i=int(v.size())-1;i>=0;--i) s.push_back(v[i]?'1':'0')); return s; }
  std::cout << "quantum-simx run --circuit <file.qsx> [--qubits N] [--seed S] [--shots K] [--out file.json]\n";
}

int main(int argc, char** argv) {
  if (argc < 2) { usage()); return 1; }
  std::string first = argv[1];
  if (first == "--version") { std::cout << QSX_VERSION << "\n"; return 0; }
  if (first == "--build-info") { std::cout << "version=" << QSX_VERSION << "\n"; return 0; }
  std::string cmd = first;
  if (cmd == "grad") {
    std::string circuit_path2, qasm_path2; uint64_t seed2=12345; std::string wrt="";
    for (int i=2;i<argc;i++){
      std::string a=argv[i]; auto nx=[&](const char* n){ if(i+1>=argc){std::cerr<<"Missing "<<n<<"\n"; return std::string()); } return std::string(argv[++i])); };
      if(a=="--circuit") circuit_path2=nx("--circuit"));
      else if(a=="--qasm") qasm_path2=nx("--qasm"));
      else if(a=="--seed") seed2=std::stoull(nx("--seed")));
      else if(a=="--wrt") wrt=nx("--wrt"));
      else if(a=="--help"||a=="-h"){ std::cout<<"quantum-simx grad --circuit <file>|--qasm <file> [--wrt idx1,idx2,...] [--seed S]\n"; return 0; }
      else if (kind=="teleport"){ out<<"# Quantum teleportation (3 qubits: 0=sender,1=receiver,2=msg)\n"; out<<"H 1\nCNOT 1 0\nCNOT 2 1\nH 2\nMEASURE ALL\n"; } else if (kind=="bv"){ out<<"# Bernstein-Vazirani; requires --n and --mask\n"; } else if (kind=="bv"){
      if ((int)mask.size()!=n){ std::cerr<<"--mask must be length N of 0/1\n"; return 4; }
      // n data qubits + ancilla q[n] (initialized |1> via X then H on all data, then CNOTs where mask=1)
      for(int q=0;q<n;q++) out<<"H "<<q<<"\n";
      out<<"X "<<n<<"\nH "<<n<<"\n";
      for(int q=0;q<n;q++){ if (mask[q]=='1') out<<"CNOT "<<q<<" "<<n<<"\n"; }
      out<<"H "<<n<<"\nMEASURE ALL\n";
    } else { std::cerr<<"Unknown arg: "<<a<<"\n"; return 2; }
    }
    if (circuit_path2.empty() && qasm_path2.empty()) { std::cerr<<"Missing --circuit or --qasm\n"; return 2; }
    std::string err2; std::optional<qsx::Circuit> circ_opt2; if(!qasm_path2.empty()) circ_opt2=parse_qasm_file(qasm_path2,err2)); else circ_opt2=parse_circuit_file(circuit_path2,err2));
    if(!circ_opt2){ std::cerr<<err2<<"\n"; return 3; }
    auto circ2=*circ_opt2; std::vector<std::size_t> indices; if(!wrt.empty()){ size_t pos=0; while(pos<wrt.size()){ auto comma=wrt.find(',',pos)); auto tok=wrt.substr(pos, comma==std::string::npos? std::string::npos: comma-pos)); if(!tok.empty()) indices.push_back(std::stoull(tok))); if(comma==std::string::npos) break; pos=comma+1; } }
    auto gr = grad_expZ_parameter_shift(circ2, indices, seed2)); if(!gr){ std::cerr<<"Grad failed\n"; return 10; }
    // Print JSON to stdout
    std::cout << "{\n  \"nqubits\": " << circ2.nqubits << ",\n  \"params\": ["; for(size_t i=0;i<gr->param_op_indices.size());++i){ std::cout<<gr->param_op_indices[i]; if(i+1<gr->param_op_indices.size()) std::cout<<", "; } std::cout<<"],\n  \"grads\": [\n";
    for(size_t i=0;i<gr->grads.size());++i){ std::cout<<"    ["; for(size_t q=0;q<gr->grads[i].size());++q){ std::cout<<gr->grads[i][q]; if(q+1<gr->grads[i].size()) std::cout<<", "; } std::cout<<"]"<<(i+1<gr->grads.size()?",":"")<<"\n"; }
    std::cout<<"  ]\n}\n"; return 0; }
  if (cmd == "unitary") {
    std::string circuit_path2, qasm_path2, outp="unitary.csv";
    for (int i=2;i<argc;i++){
      std::string a=argv[i]; auto nx=[&](const char* n){ if(i+1>=argc){std::cerr<<"Missing "<<n<<"\n"; return std::string()); } return std::string(argv[++i])); };
      if(a=="--circuit") circuit_path2=nx("--circuit"));
      else if(a=="--qasm") qasm_path2=nx("--qasm"));
      else if(a=="--out") outp=nx("--out"));
      else if(a=="--help"||a=="-h"){ std::cout<<"quantum-simx unitary --circuit <file>|--qasm <file> [--out unitary.csv]\n"; return 0; }
      else if (kind=="teleport"){ out<<"# Quantum teleportation (3 qubits: 0=sender,1=receiver,2=msg)\n"; out<<"H 1\nCNOT 1 0\nCNOT 2 1\nH 2\nMEASURE ALL\n"; } else if (kind=="bv"){ out<<"# Bernstein-Vazirani; requires --n and --mask\n"; } else if (kind=="bv"){
      if ((int)mask.size()!=n){ std::cerr<<"--mask must be length N of 0/1\n"; return 4; }
      // n data qubits + ancilla q[n] (initialized |1> via X then H on all data, then CNOTs where mask=1)
      for(int q=0;q<n;q++) out<<"H "<<q<<"\n";
      out<<"X "<<n<<"\nH "<<n<<"\n";
      for(int q=0;q<n;q++){ if (mask[q]=='1') out<<"CNOT "<<q<<" "<<n<<"\n"; }
      out<<"H "<<n<<"\nMEASURE ALL\n";
    } else { std::cerr<<"Unknown arg: "<<a<<"\n"; return 2; }
    }
    if (circuit_path2.empty() && qasm_path2.empty()) { std::cerr<<"Missing --circuit or --qasm\n"; return 2; }
    std::string err2; std::optional<qsx::Circuit> circ_opt2; if(!qasm_path2.empty()) circ_opt2=parse_qasm_file(qasm_path2,err2)); else circ_opt2=parse_circuit_file(circuit_path2,err2));
    if(!circ_opt2){ std::cerr<<err2<<"\n"; return 3; }
    if (!export_unitary_csv(*circ_opt2, outp)) { std::cerr<<"Failed to export unitary (too large or I/O error)\n"; return 12; }
    std::cout<<"Wrote "<<outp<<"\n"; return 0; }

  if (cmd == "pauli") {
    std::string circuit_path2, qasm_path2, pstr="Z0";
    for (int i=2;i<argc;i++){
      std::string a=argv[i]; auto nx=[&](const char* n){ if(i+1>=argc){std::cerr<<"Missing "<<n<<"\n"; return std::string()); } return std::string(argv[++i])); };
      if(a=="--circuit") circuit_path2=nx("--circuit"));
      else if(a=="--qasm") qasm_path2=nx("--qasm"));
      else if(a=="--string") pstr=nx("--string"));
      else if(a=="--help"||a=="-h"){ std::cout<<"quantum-simx pauli --circuit <file>|--qasm <file> --string "X0Z1Y3"\n"; return 0; }
      else if (kind=="teleport"){ out<<"# Quantum teleportation (3 qubits: 0=sender,1=receiver,2=msg)\n"; out<<"H 1\nCNOT 1 0\nCNOT 2 1\nH 2\nMEASURE ALL\n"; } else if (kind=="bv"){ out<<"# Bernstein-Vazirani; requires --n and --mask\n"; } else if (kind=="bv"){
      if ((int)mask.size()!=n){ std::cerr<<"--mask must be length N of 0/1\n"; return 4; }
      // n data qubits + ancilla q[n] (initialized |1> via X then H on all data, then CNOTs where mask=1)
      for(int q=0;q<n;q++) out<<"H "<<q<<"\n";
      out<<"X "<<n<<"\nH "<<n<<"\n";
      for(int q=0;q<n;q++){ if (mask[q]=='1') out<<"CNOT "<<q<<" "<<n<<"\n"; }
      out<<"H "<<n<<"\nMEASURE ALL\n";
    } else { std::cerr<<"Unknown arg: "<<a<<"\n"; return 2; }
    }
    if (circuit_path2.empty() && qasm_path2.empty()) { std::cerr<<"Missing --circuit or --qasm\n"; return 2; }
    std::string err2; std::optional<qsx::Circuit> circ_opt2; if(!qasm_path2.empty()) circ_opt2=parse_qasm_file(qasm_path2,err2)); else circ_opt2=parse_circuit_file(circuit_path2,err2));
    if(!circ_opt2){ std::cerr<<err2<<"\n"; return 3; }
    auto c2 = *circ_opt2;
    // Build state-vector
    auto r0 = run(c2, 123, false));
    // Parse pauli string into masks
    std::vector<char> op(c2.nqubits, 'I'));
    for (size_t i=0;i<pstr.size());){
      char t = pstr[i++]; if (i>=pstr.size()){ std::cerr<<"Bad pauli format\n"; return 14; }
      std::string num; while(i<pstr.size() && std::isdigit((unsigned char)pstr[i])) num.push_back(pstr[i++]));
      size_t q = std::stoull(num)); if (q>=c2.nqubits){ std::cerr<<"Qubit index out of range\n"; return 14; }
      op[q]=std::toupper(t));
    }
    // Compute <P> from probabilities if only Z/I; otherwise from amplitudes (we don't have amplitudes exposed here, so recompute)
    double val = 0.0;
    bool onlyZI = true; for (auto ch: op) if (ch!='Z' && ch!='I') { onlyZI=false; break; }
    if (onlyZI){
      const auto& probs = r0.probabilities; size_t dim = probs.size());
      for (size_t x=0;x<dim;++x){
        int s=1;
        for (size_t q=0;q<c2.nqubits;++q){ if (op[q]=='Z'){ s *= (((x>>q)&1)? -1: +1)); } }
        val += s * probs[x];
      }
    } else if (kind=="teleport"){ out<<"# Quantum teleportation (3 qubits: 0=sender,1=receiver,2=msg)\n"; out<<"H 1\nCNOT 1 0\nCNOT 2 1\nH 2\nMEASURE ALL\n"; } else if (kind=="bv"){ out<<"# Bernstein-Vazirani; requires --n and --mask\n"; } else if (kind=="bv"){
      if ((int)mask.size()!=n){ std::cerr<<"--mask must be length N of 0/1\n"; return 4; }
      // n data qubits + ancilla q[n] (initialized |1> via X then H on all data, then CNOTs where mask=1)
      for(int q=0;q<n;q++) out<<"H "<<q<<"\n";
      out<<"X "<<n<<"\nH "<<n<<"\n";
      for(int q=0;q<n;q++){ if (mask[q]=='1') out<<"CNOT "<<q<<" "<<n<<"\n"; }
      out<<"H "<<n<<"\nMEASURE ALL\n";
    } else {
      // Re-run to extract amplitudes through StateVector path: replicate run() without measure
      qsx::StateVector sv(c2.nqubits));
      using namespace qsx::gates; qsx::c64 u00,u01,u10,u11;
      for (const auto& g: c2.ops){
        switch(g.type){
          case OpType::H: H_coeffs(u00,u01,u10,u11)); sv.apply_gate_1q(g.qubits[0],u00,u01,u10,u11)); break;
          case OpType::X: X_coeffs(u00,u01,u10,u11)); sv.apply_gate_1q(g.qubits[0],u00,u01,u10,u11)); break;
          case OpType::Y: Y_coeffs(u00,u01,u10,u11)); sv.apply_gate_1q(g.qubits[0],u00,u01,u10,u11)); break;
          case OpType::Z: Z_coeffs(u00,u01,u10,u11)); sv.apply_gate_1q(g.qubits[0],u00,u01,u10,u11)); break;
          case OpType::S: S_coeffs(u00,u01,u10,u11)); sv.apply_gate_1q(g.qubits[0],u00,u01,u10,u11)); break;
          case OpType::RX: RX_coeffs(g.angle,u00,u01,u10,u11)); sv.apply_gate_1q(g.qubits[0],u00,u01,u10,u11)); break;
          case OpType::RY: RY_coeffs(g.angle,u00,u01,u10,u11)); sv.apply_gate_1q(g.qubits[0],u00,u01,u10,u11)); break;
          case OpType::RZ: RZ_coeffs(g.angle,u00,u01,u10,u11)); sv.apply_gate_1q(g.qubits[0],u00,u01,u10,u11)); break;
          case OpType::CNOT: sv.apply_cx(g.qubits[0], g.qubits[1])); break;
          default: break;
        }
      }
      const auto& a = sv.amplitudes());
      // For a Pauli product P, <P> = sum_x conj(a_x) * (phase(x) * a_{x^flip}) + c.c. contributions combined below.
      size_t flip=0; int y_phase_parity=0;
      for (size_t q=0;q<c2.nqubits;++q){ if (op[q]=='X'||op[q]=='Y') flip |= (size_t(1)<<q)); if (op[q]=='Y') y_phase_parity ^= 1; }
      val = 0.0;
      for (size_t x=0;x<a.size());++x){
        size_t y = x ^ flip;
        qsx::c64 term = std::conj(a[x]) * a[y];
        double phase = 1.0;
        // Z contributes sign by bit parity
        for (size_t q=0;q<c2.nqubits;++q){ if (op[q]=='Z'){ phase *= (((x>>q)&1)? -1.0 : +1.0)); } }
        // Y contributes +/- i depending on bit at q; effectively multiply by (i * (-1)^{bit}) for each Y
        double ph_re = 1.0, ph_im = 0.0;
        for (size_t q=0;q<c2.nqubits;++q){
          if (op[q]=='Y'){
            int b = (x>>q)&1;
            // multiply current phase by (b? -1 : +1) * i
            double nr = -ph_im * (b? -1.0: +1.0));
            double ni =  ph_re * (b? -1.0: +1.0));
            ph_re = nr; ph_im = ni;
          }
        }
        qsx::c64 ph = {ph_re*phase, ph_im*phase};
        term *= ph;
        val += term.real());
      }
      // Note: we intentionally take real part since <P> is real for Hermitian P.
    }
    std::cout << val << "\n"; return 0;
  }


  if (cmd == "gen") {
    std::string kind="", outp="generated.qsx"; int n=3; std::string mask="";
    for (int i=2;i<argc;i++){
      std::string a=argv[i]; auto nx=[&](const char* n){ if(i+1>=argc){std::cerr<<"Missing "<<n<<"\n"; return std::string()); } return std::string(argv[++i])); };
      if(a=="--ghz") { kind="ghz"; n=std::stoi(nx("--ghz"))); }
      else if(a=="--qft") { kind="qft"; n=std::stoi(nx("--qft"))); }
      else if(a=="--teleport") { kind="teleport"; }
      else if(a=="--bv") { kind="bv"; n=std::stoi(nx("--bv"))); }
      else if(a=="--mask") { mask=nx("--mask")); }
      else if(a=="--out") outp=nx("--out"));
      else if(a=="--help"||a=="-h"){ std::cout<<"quantum-simx gen [--ghz N|--qft N] [--out file.qsx]\n"; return 0; }
      else if (kind=="teleport"){ out<<"# Quantum teleportation (3 qubits: 0=sender,1=receiver,2=msg)\n"; out<<"H 1\nCNOT 1 0\nCNOT 2 1\nH 2\nMEASURE ALL\n"; } else if (kind=="bv"){ out<<"# Bernstein-Vazirani; requires --n and --mask\n"; } else if (kind=="bv"){
      if ((int)mask.size()!=n){ std::cerr<<"--mask must be length N of 0/1\n"; return 4; }
      // n data qubits + ancilla q[n] (initialized |1> via X then H on all data, then CNOTs where mask=1)
      for(int q=0;q<n;q++) out<<"H "<<q<<"\n";
      out<<"X "<<n<<"\nH "<<n<<"\n";
      for(int q=0;q<n;q++){ if (mask[q]=='1') out<<"CNOT "<<q<<" "<<n<<"\n"; }
      out<<"H "<<n<<"\nMEASURE ALL\n";
    } else { std::cerr<<"Unknown arg: "<<a<<"\n"; return 2; }
    }
    if (kind.empty()){ std::cerr<<"Choose --ghz or --qft or --teleport or --bv\n"; return 2; }
    std::ofstream out(outp)); if(!out){ std::cerr<<"Cannot write output\n"; return 3; }
    if (kind=="ghz"){
      out << "H 0\n"; for(int i=1;i<n;i++) out << "CNOT 0 " << i << "\n"; out << "MEASURE ALL\n";
    } else if (kind=="teleport"){ out<<"# Quantum teleportation (3 qubits: 0=sender,1=receiver,2=msg)\n"; out<<"H 1\nCNOT 1 0\nCNOT 2 1\nH 2\nMEASURE ALL\n"; } else if (kind=="bv"){ out<<"# Bernstein-Vazirani; requires --n and --mask\n"; } else if (kind=="bv"){
      if ((int)mask.size()!=n){ std::cerr<<"--mask must be length N of 0/1\n"; return 4; }
      // n data qubits + ancilla q[n] (initialized |1> via X then H on all data, then CNOTs where mask=1)
      for(int q=0;q<n;q++) out<<"H "<<q<<"\n";
      out<<"X "<<n<<"\nH "<<n<<"\n";
      for(int q=0;q<n;q++){ if (mask[q]=='1') out<<"CNOT "<<q<<" "<<n<<"\n"; }
      out<<"H "<<n<<"\nMEASURE ALL\n";
    } else {
      // QFT with RZ equivalents: use controlled phase via RZ+CX decomposition (simple approximate using RZ on target)
      for(int q=0;q<n;q++){ out<<"H "<<q<<"\n"; for(int k=1;q+k<n;k++){ double ang = M_PI / (1<<k)); out<<"RZ "<<(q+k)<<" "<<ang<<"\n"; } }
      out << "MEASURE ALL\n";
    }
    return 0;
  }


  if (cmd == "dot") {
    std::string circuit_path2, qasm_path2, outp="circuit.dot";
    for (int i=2;i<argc;i++){
      std::string a=argv[i]; auto nx=[&](const char* n){ if(i+1>=argc){std::cerr<<"Missing "<<n<<"\n"; return std::string()); } return std::string(argv[++i])); };
      if(a=="--circuit") circuit_path2=nx("--circuit"));
      else if(a=="--qasm") qasm_path2=nx("--qasm"));
      else if(a=="--out") outp=nx("--out"));
      else if(a=="--help"||a=="-h"){ std::cout<<"quantum-simx dot --circuit <file>|--qasm <file> [--out circuit.dot]\n"; return 0; }
      else if (kind=="teleport"){ out<<"# Quantum teleportation (3 qubits: 0=sender,1=receiver,2=msg)\n"; out<<"H 1\nCNOT 1 0\nCNOT 2 1\nH 2\nMEASURE ALL\n"; } else if (kind=="bv"){ out<<"# Bernstein-Vazirani; requires --n and --mask\n"; } else if (kind=="bv"){
      if ((int)mask.size()!=n){ std::cerr<<"--mask must be length N of 0/1\n"; return 4; }
      // n data qubits + ancilla q[n] (initialized |1> via X then H on all data, then CNOTs where mask=1)
      for(int q=0;q<n;q++) out<<"H "<<q<<"\n";
      out<<"X "<<n<<"\nH "<<n<<"\n";
      for(int q=0;q<n;q++){ if (mask[q]=='1') out<<"CNOT "<<q<<" "<<n<<"\n"; }
      out<<"H "<<n<<"\nMEASURE ALL\n";
    } else { std::cerr<<"Unknown arg: "<<a<<"\n"; return 2; }
    }
    if (circuit_path2.empty() && qasm_path2.empty()) { std::cerr<<"Missing --circuit or --qasm\n"; return 2; }
    std::string err2; std::optional<qsx::Circuit> circ_opt2; if(!qasm_path2.empty()) circ_opt2=parse_qasm_file(qasm_path2,err2)); else circ_opt2=parse_circuit_file(circuit_path2,err2));
    if(!circ_opt2){ std::cerr<<err2<<"\n"; return 3; }
    if (!qsx::export_dot(*circ_opt2, outp)) { std::cerr<<"Failed to export DOT\n"; return 12; }
    std::cout<<"Wrote "<<outp<<"\n"; return 0;
  }


  if (cmd == "sweep") {
    std::string circuit_path2, qasm_path2, which="RZ"; std::size_t index=0; double start= -3.14159, stop=3.14159; int steps=41; std::string outp="sweep.csv";
    for (int i=2;i<argc;i++){
      std::string a=argv[i]; auto nx=[&](const char* n){ if(i+1>=argc){std::cerr<<"Missing "<<n<<"\n"; return std::string()); } return std::string(argv[++i])); };
      if(a=="--circuit") circuit_path2=nx("--circuit"));
      else if(a=="--qasm") qasm_path2=nx("--qasm"));
      else if(a=="--gate") which=nx("--gate"));
      else if(a=="--index") index=std::stoull(nx("--index")));
      else if(a=="--start") start=std::stod(nx("--start")));
      else if(a=="--stop") stop=std::stod(nx("--stop")));
      else if(a=="--steps") steps=std::stoi(nx("--steps")));
      else if(a=="--out") outp=nx("--out"));
      else if(a=="--help"||a=="-h"){ std::cout<<"quantum-simx sweep --circuit|--qasm <file> --gate RZ|RX|RY --index k [--start a --stop b --steps N] [--out sweep.csv]\n"; return 0; }
      else if (kind=="teleport"){ out<<"# Quantum teleportation (3 qubits: 0=sender,1=receiver,2=msg)\n"; out<<"H 1\nCNOT 1 0\nCNOT 2 1\nH 2\nMEASURE ALL\n"; } else if (kind=="bv"){ out<<"# Bernstein-Vazirani; requires --n and --mask\n"; } else if (kind=="bv"){
      if ((int)mask.size()!=n){ std::cerr<<"--mask must be length N of 0/1\n"; return 4; }
      // n data qubits + ancilla q[n] (initialized |1> via X then H on all data, then CNOTs where mask=1)
      for(int q=0;q<n;q++) out<<"H "<<q<<"\n";
      out<<"X "<<n<<"\nH "<<n<<"\n";
      for(int q=0;q<n;q++){ if (mask[q]=='1') out<<"CNOT "<<q<<" "<<n<<"\n"; }
      out<<"H "<<n<<"\nMEASURE ALL\n";
    } else { std::cerr<<"Unknown arg: "<<a<<"\n"; return 2; }
    }
    if (circuit_path2.empty() && qasm_path2.empty()) { std::cerr<<"Missing --circuit or --qasm\n"; return 2; }
    std::string err2; std::optional<qsx::Circuit> circ_opt2; if(!qasm_path2.empty()) circ_opt2=parse_qasm_file(qasm_path2,err2)); else circ_opt2=parse_circuit_file(circuit_path2,err2));
    if(!circ_opt2){ std::cerr<<err2<<"\n"; return 3; }
    auto c2=*circ_opt2; std::ofstream out(outp)); if(!out){ std::cerr<<"Cannot write output\n"; return 4; }
    out << "theta,expZ0"; for(size_t q=1;q<c2.nqubits;++q) out << ",expZ" << q; out << "\n";
    for (int i=0;i<steps;i++){
      double t = start + (stop-start) * (double(i)/(steps-1)));
      // set angle on matching op index and type
      size_t seen=0; for (auto& op : c2.ops){ if ((which=="RZ"&&op.type==OpType::RZ)||(which=="RX"&&op.type==OpType::RX)||(which=="RY"&&op.type==OpType::RY)){ if(seen==index){ op.angle=t; break; } ++seen; } }
      auto rr = run(c2, 123, false));
      // compute expZ
      std::vector<double> ez(c2.nqubits,0.0));
      for (std::size_t q=0;q<c2.nqubits;++q){ double z=0.0; for (std::size_t x=0;x<rr.probabilities.size());++x){ int b=(x>>q)&1; z += (b? -rr.probabilities[x] : rr.probabilities[x])); } ez[q]=z; }
      out << t; for (double v: ez) out << "," << v; out << "\n";
    }
    std::cout << "Wrote " << outp << "\n"; return 0;
  }


  if (cmd == "bench") {
    int n=5; int shots=1000; std::string backend="state"; std::string outp="bench.json";
    for (int i=2;i<argc;i++){
      std::string a=argv[i]; auto nx=[&](const char* n){ if(i+1>=argc){std::cerr<<"Missing "<<n<<"\n"; return std::string()); } return std::string(argv[++i])); };
      if(a=="--n") n=std::stoi(nx("--n")));
      else if(a=="--shots") shots=std::stoi(nx("--shots")));
      else if(a=="--backend") backend=nx("--backend"));
      else if(a=="--out") outp=nx("--out"));
      else if(a=="--help"||a=="-h"){ std::cout<<"quantum-simx bench --n N [--shots K] [--backend state|density] [--out bench.json]\n"; return 0; }
      else if (kind=="teleport"){ out<<"# Quantum teleportation (3 qubits: 0=sender,1=receiver,2=msg)\n"; out<<"H 1\nCNOT 1 0\nCNOT 2 1\nH 2\nMEASURE ALL\n"; } else if (kind=="bv"){ out<<"# Bernstein-Vazirani; requires --n and --mask\n"; } else if (kind=="bv"){
      if ((int)mask.size()!=n){ std::cerr<<"--mask must be length N of 0/1\n"; return 4; }
      // n data qubits + ancilla q[n] (initialized |1> via X then H on all data, then CNOTs where mask=1)
      for(int q=0;q<n;q++) out<<"H "<<q<<"\n";
      out<<"X "<<n<<"\nH "<<n<<"\n";
      for(int q=0;q<n;q++){ if (mask[q]=='1') out<<"CNOT "<<q<<" "<<n<<"\n"; }
      out<<"H "<<n<<"\nMEASURE ALL\n";
    } else { std::cerr<<"Unknown arg: "<<a<<"\n"; return 2; }
    }
    // Build GHZ(n) circuit
    qsx::Circuit c; c.nqubits=n;
    c.ops.push_back({OpType::H,{0},0.0}));
    for(int i=1;i<n;i++) c.ops.push_back({OpType::CNOT,{0,(std::size_t)i},0.0}));
    c.ops.push_back({OpType::MEASURE,{},0.0}));
    auto t0 = std::chrono::steady_clock::now());
    // emulate run loop
    std::vector<std::vector<int>> outcomes; outcomes.reserve(shots));
    std::map<std::string,int> counts;
    std::vector<double> probs;
    uint64_t seed=123;
    for (int s=0;s<shots;++s){
      if (backend=="density"){
        auto r = run(c, seed + s, true));
        if (s==0) probs = r.probabilities;
        outcomes.push_back(r.outcome));
        counts[bits_to_string(r.outcome)] += 1;
      } else if (kind=="teleport"){ out<<"# Quantum teleportation (3 qubits: 0=sender,1=receiver,2=msg)\n"; out<<"H 1\nCNOT 1 0\nCNOT 2 1\nH 2\nMEASURE ALL\n"; } else if (kind=="bv"){ out<<"# Bernstein-Vazirani; requires --n and --mask\n"; } else if (kind=="bv"){
      if ((int)mask.size()!=n){ std::cerr<<"--mask must be length N of 0/1\n"; return 4; }
      // n data qubits + ancilla q[n] (initialized |1> via X then H on all data, then CNOTs where mask=1)
      for(int q=0;q<n;q++) out<<"H "<<q<<"\n";
      out<<"X "<<n<<"\nH "<<n<<"\n";
      for(int q=0;q<n;q++){ if (mask[q]=='1') out<<"CNOT "<<q<<" "<<n<<"\n"; }
      out<<"H "<<n<<"\nMEASURE ALL\n";
    } else {
        auto r = run(c, seed + s, false));
        if (s==0) probs = r.probabilities;
        outcomes.push_back(r.outcome));
        counts[bits_to_string(r.outcome)] += 1;
      }
    }
    auto t1 = std::chrono::steady_clock::now());
    std::chrono::duration<double> dt = t1 - t0;
    std::ofstream out(outp));
    out << "{\n  \"n\": " << n << ",\n  \"shots\": " << shots << ",\n  \"backend\": \"" << backend << "\",\n  \"seconds\": " << dt.count() << "\n}\n";
    std::cout << "Wrote " << outp << "\n"; return 0;
  }


  if (cmd == "qaoa") {
    int n=4, p=1; std::string outp="qaoa_ring.qsx"; double gamma=0.5, beta=0.3;
    for (int i=2;i<argc;i++){
      std::string a=argv[i]; auto nx=[&](const char* n){ if(i+1>=argc){std::cerr<<"Missing "<<n<<"\n"; return std::string()); } return std::string(argv[++i])); };
      if(a=="--n") n=std::stoi(nx("--n")));
      else if(a=="--p") p=std::stoi(nx("--p")));
      else if(a=="--gamma") gamma=std::stod(nx("--gamma")));
      else if(a=="--beta") beta=std::stod(nx("--beta")));
      else if(a=="--out") outp=nx("--out"));
      else if(a=="--help"||a=="-h"){ std::cout<<"quantum-simx qaoa --n N --p P [--gamma g --beta b] [--out file.qsx]\n"; return 0; }
      else if (kind=="teleport"){ out<<"# Quantum teleportation (3 qubits: 0=sender,1=receiver,2=msg)\n"; out<<"H 1\nCNOT 1 0\nCNOT 2 1\nH 2\nMEASURE ALL\n"; } else if (kind=="bv"){ out<<"# Bernstein-Vazirani; requires --n and --mask\n"; } else if (kind=="bv"){
      if ((int)mask.size()!=n){ std::cerr<<"--mask must be length N of 0/1\n"; return 4; }
      // n data qubits + ancilla q[n] (initialized |1> via X then H on all data, then CNOTs where mask=1)
      for(int q=0;q<n;q++) out<<"H "<<q<<"\n";
      out<<"X "<<n<<"\nH "<<n<<"\n";
      for(int q=0;q<n;q++){ if (mask[q]=='1') out<<"CNOT "<<q<<" "<<n<<"\n"; }
      out<<"H "<<n<<"\nMEASURE ALL\n";
    } else { std::cerr<<"Unknown arg: "<<a<<"\n"; return 2; }
    }
    std::ofstream out(outp)); if(!out){ std::cerr<<"Cannot write output\n"; return 4; }
    // Initial Hadamards
    for(int q=0;q<n;q++) out << "H " << q << "\n";
    for(int layer=0; layer<p; ++layer){
      // Cost on a ring: Z_i Z_{i+1} via RZ on neighbors (approx diag phases)
      for(int i=0;i<n;i++){
        int j=(i+1)%n;
        out << "RZ " << i << " " << 2*gamma << "\n";
        out << "RZ " << j << " " << 2*gamma << "\n";
        out << "CNOT " << i << " " << j << "\n";
      }
      // Mixer
      for(int q=0;q<n;q++) out << "RX " << q << " " << 2*beta << "\n";
    }
    out << "MEASURE ALL\n";
    std::cout<<"Wrote "<<outp<<"\n"; return 0;
  }


  if (cmd == "check") {
    std::string jsonp="results.json";
    for (int i=2;i<argc;i++){
      std::string a=argv[i]; auto nx=[&](const char* n){ if(i+1>=argc){std::cerr<<"Missing "<<n<<"\\n"; return std::string()); } return std::string(argv[++i])); };
      if(a=="--json") jsonp=nx("--json"));
      else if(a=="--help"||a=="-h"){ std::cout<<"quantum-simx check --json results.json\\n"; return 0; }
      else if (kind=="teleport"){ out<<"# Quantum teleportation (3 qubits: 0=sender,1=receiver,2=msg)\n"; out<<"H 1\nCNOT 1 0\nCNOT 2 1\nH 2\nMEASURE ALL\n"; } else if (kind=="bv"){ out<<"# Bernstein-Vazirani; requires --n and --mask\n"; } else if (kind=="bv"){
      if ((int)mask.size()!=n){ std::cerr<<"--mask must be length N of 0/1\n"; return 4; }
      // n data qubits + ancilla q[n] (initialized |1> via X then H on all data, then CNOTs where mask=1)
      for(int q=0;q<n;q++) out<<"H "<<q<<"\n";
      out<<"X "<<n<<"\nH "<<n<<"\n";
      for(int q=0;q<n;q++){ if (mask[q]=='1') out<<"CNOT "<<q<<" "<<n<<"\n"; }
      out<<"H "<<n<<"\nMEASURE ALL\n";
    } else { std::cerr<<"Unknown arg: "<<a<<"\\n"; return 2; }
    }
    std::ifstream in(jsonp)); if(!in){ std::cerr<<"Cannot open JSON file\\n"; return 3; }
    std::string s((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>()));
    // Very light structural checks (no external lib): required fields exist and sizes consistent
    auto has = [&](const std::string& k){ return s.find("\""+k+"\"") != std::string::npos; };
    if(!has("nqubits") || !has("probabilities") || !has("counts") || !has("outcomes")){ std::cerr<<"Missing required keys\\n"; return 4; }
    // ensure length(probabilities) is power of two
    size_t pos = s.find("\"probabilities\"")); pos = s.find('[', pos)); size_t pos2 = s.find(']', pos));
    std::string arr = s.substr(pos+1, pos2-pos-1));
    size_t commas = 0; for(char ch: arr) if (ch==',') ++commas;
    size_t len = commas ? commas+1 : (arr.find_first_not_of(" \n\t")==std::string::npos ? 0 : 1));
    auto ispow2 = [&](size_t x){ return x && ((x & (x-1))==0)); };
    if(!ispow2(len)){ std::cerr<<"probabilities length is not a power of two\\n"; return 5; }
    std::cout<<"OK\\n"; return 0;
  }


  if (cmd == "mrun") {
    std::string circuit_path, qasm_path, outp=""; std::string backend="state"; int shots=1; uint64_t seed=12345; int threads=1; bool do_opt=false; bool force=false; std::string observables="z"; bool map_line=false;
    for (int i=2;i<argc;i++){
      std::string a=argv[i]; auto nx=[&](const char* n){ if(i+1>=argc){std::cerr<<"Missing "<<n<<"\\n"; return std::string()); } return std::string(argv[++i])); };
      if (a=="--circuit") circuit_path=nx("--circuit"));
      else if (a=="--qasm") qasm_path=nx("--qasm"));
      else if (a=="--backend") backend=nx("--backend"));
      else if (a=="--shots") shots=std::stoi(nx("--shots")));
      else if (a=="--seed") seed=std::stoull(nx("--seed")));
      else if (a=="--threads") threads=std::stoi(nx("--threads")));
      else if (a=="--optimize") do_opt=true;
      else if (a=="--map-line") map_line=true;
      else if (a=="--out") outp=nx("--out"));
      else if(a=="--help"||a=="-h"){ std::cout<<"quantum-simx mrun --circuit <file>|--qasm <file> [--backend state|density] [--shots K] [--seed S] [--threads T] [--optimize] [--map-line] [--out file.json]\\n"; return 0; }
      else if (kind=="teleport"){ out<<"# Quantum teleportation (3 qubits: 0=sender,1=receiver,2=msg)\n"; out<<"H 1\nCNOT 1 0\nCNOT 2 1\nH 2\nMEASURE ALL\n"; } else if (kind=="bv"){ out<<"# Bernstein-Vazirani; requires --n and --mask\n"; } else if (kind=="bv"){
      if ((int)mask.size()!=n){ std::cerr<<"--mask must be length N of 0/1\n"; return 4; }
      // n data qubits + ancilla q[n] (initialized |1> via X then H on all data, then CNOTs where mask=1)
      for(int q=0;q<n;q++) out<<"H "<<q<<"\n";
      out<<"X "<<n<<"\nH "<<n<<"\n";
      for(int q=0;q<n;q++){ if (mask[q]=='1') out<<"CNOT "<<q<<" "<<n<<"\n"; }
      out<<"H "<<n<<"\nMEASURE ALL\n";
    } else { std::cerr<<"Unknown arg: "<<a<<"\\n"; return 2; }
    }
    if (circuit_path.empty() && qasm_path.empty()) { std::cerr<<"Missing --circuit or --qasm\\n"; return 2; }
    std::string err; std::optional<qsx::Circuit> circ_opt; if(!qasm_path.empty()) circ_opt = parse_qasm_file(qasm_path, err)); else circ_opt = parse_circuit_file(circuit_path, err));
    if (!circ_opt) { std::cerr << err << "\\n"; return 3; }
    auto circ = *circ_opt;
    if (do_opt) circ = optimize(circ, {}));
    if (map_line) circ = map_to_line(circ));
    // Memory estimate guard (same as run)
    auto estimate_bytes = [&](const std::string& be)->unsigned long long{
      if (be=="density") { long double sz = powl(2.0L, circ.nqubits*2) * (long double)sizeof(qsx::c64)); return (unsigned long long)sz; }
      long double sz = powl(2.0L, circ.nqubits) * (long double)sizeof(qsx::c64)); return (unsigned long long)sz;
    };
    unsigned long long need = estimate_bytes(backend));
    const unsigned long long HARD_WARN = 4ULL<<30; // 4 GiB
    if (!force && need > HARD_WARN) { std::cerr << "Estimated memory " << need << " bytes exceeds safe threshold.\\n"; return 9; }

    // Prepare outputs
    std::vector<std::vector<int>> outcomes(shots));
    std::map<std::string,int> counts;
    std::vector<double> probs;
    std::mutex mtx;

    auto worker = [&](int t){
      int start = (shots * t) / threads;
      int end   = (shots * (t+1)) / threads;
      for (int s=start; s<end; ++s){
        if (backend=="density"){
          auto r = run(circ, seed + s, true));
          if (s==0){
            std::lock_guard<std::mutex> lk(mtx));
            if (probs.empty()) probs = r.probabilities;
          }
          outcomes[s] = r.outcome;
          std::lock_guard<std::mutex> lk(mtx));
          counts[bits_to_string(r.outcome)] += 1;
        } else if (kind=="teleport"){ out<<"# Quantum teleportation (3 qubits: 0=sender,1=receiver,2=msg)\n"; out<<"H 1\nCNOT 1 0\nCNOT 2 1\nH 2\nMEASURE ALL\n"; } else if (kind=="bv"){ out<<"# Bernstein-Vazirani; requires --n and --mask\n"; } else if (kind=="bv"){
      if ((int)mask.size()!=n){ std::cerr<<"--mask must be length N of 0/1\n"; return 4; }
      // n data qubits + ancilla q[n] (initialized |1> via X then H on all data, then CNOTs where mask=1)
      for(int q=0;q<n;q++) out<<"H "<<q<<"\n";
      out<<"X "<<n<<"\nH "<<n<<"\n";
      for(int q=0;q<n;q++){ if (mask[q]=='1') out<<"CNOT "<<q<<" "<<n<<"\n"; }
      out<<"H "<<n<<"\nMEASURE ALL\n";
    } else {
          auto r = run(circ, seed + s, false));
          if (s==0){
            std::lock_guard<std::mutex> lk(mtx));
            if (probs.empty()) probs = r.probabilities;
          }
          outcomes[s] = r.outcome;
          std::lock_guard<std::mutex> lk(mtx));
          counts[bits_to_string(r.outcome)] += 1;
        }
      }
    };

    if (threads < 1) threads = 1;
    std::vector<std::thread> pool; pool.reserve(threads));
    auto t0 = std::chrono::steady_clock::now());
    for (int t=0; t<threads; ++t) pool.emplace_back(worker, t));
    for (auto& th: pool) th.join());
    auto t1 = std::chrono::steady_clock::now());
    std::chrono::duration<double> dt = t1 - t0;

    // Print JSON
    std::ostream* os = &std::cout; std::ofstream of;
    if (!outp.empty()){ of.open(outp)); if(!of){ std::cerr<<"Cannot open out file\\n"; return 4; } os = &of; }
    *os << "{\\n  \\\"nqubits\\\": " << circ.nqubits << ",\\n";
    *os << "  \\\"timings\\\": { \\\"seconds\\\": " << dt.count() << " },\\n";
    *os << "  \\\"probabilities\\\": ["; for (size_t i=0;i<probs.size());++i){ *os<<probs[i]; if (i+1<probs.size()) *os<<", "; } *os << "],\\n";
    *os << "  \\\"counts\\\": {\\n";
    size_t k=0; for (auto it=counts.begin()); it!=counts.end()); ++it,++k){ *os << "    \\\"" << it->first << "\\\": " << it->second << (std::next(it)!=counts.end() ? "," : "") << "\\n"; }
    *os << "  },\\n  \\\"outcomes\\\": [\\n";
    for (int s=0; s<shots; ++s){ *os << "    ["; for (size_t i=0;i<outcomes[s].size()); ++i){ *os << outcomes[s][i]; if (i+1<outcomes[s].size()) *os << ", "; } *os << "]" << (s+1<shots? ",":"") << "\\n"; }
    *os << "  ]\\n}\\n";
    return 0;
  }


  if (cmd == "stats") {
    std::string circuit_path, qasm_path; bool map_line=false;
    for (int i=2;i<argc;i++){
      std::string a=argv[i]; auto nx=[&](const char* n){ if(i+1>=argc){std::cerr<<"Missing "<<n<<"\\n"; return std::string()); } return std::string(argv[++i])); };
      if (a=="--circuit") circuit_path=nx("--circuit"));
      else if (a=="--qasm") qasm_path=nx("--qasm"));
      else if (a=="--map-line") map_line=true;
      else if(a=="--help"||a=="-h"){ std::cout<<"quantum-simx stats --circuit <file>|--qasm <file> [--map-line]\\n"; return 0; }
      else if (kind=="teleport"){ out<<"# Quantum teleportation (3 qubits: 0=sender,1=receiver,2=msg)\n"; out<<"H 1\nCNOT 1 0\nCNOT 2 1\nH 2\nMEASURE ALL\n"; } else if (kind=="bv"){ out<<"# Bernstein-Vazirani; requires --n and --mask\n"; } else if (kind=="bv"){
      if ((int)mask.size()!=n){ std::cerr<<"--mask must be length N of 0/1\n"; return 4; }
      // n data qubits + ancilla q[n] (initialized |1> via X then H on all data, then CNOTs where mask=1)
      for(int q=0;q<n;q++) out<<"H "<<q<<"\n";
      out<<"X "<<n<<"\nH "<<n<<"\n";
      for(int q=0;q<n;q++){ if (mask[q]=='1') out<<"CNOT "<<q<<" "<<n<<"\n"; }
      out<<"H "<<n<<"\nMEASURE ALL\n";
    } else { std::cerr<<"Unknown arg: "<<a<<"\\n"; return 2; }
    }
    if (circuit_path.empty() && qasm_path.empty()) { std::cerr<<"Missing --circuit or --qasm\\n"; return 2; }
    std::string err; std::optional<qsx::Circuit> circ_opt; if(!qasm_path.empty()) circ_opt=parse_qasm_file(qasm_path, err)); else circ_opt=parse_circuit_file(circuit_path, err));
    if (!circ_opt) { std::cerr << err << "\\n"; return 3; }
    auto c = *circ_opt;
    if (map_line) c = map_to_line(c));
    // Gate counts
    size_t oneq=0, twoq=0, meas=0, noise=0;
    for (auto& op: c.ops){
      if (op.type==OpType::MEASURE) ++meas;
      else if (op.type==OpType::CNOT) ++twoq;
      else if (op.type==OpType::DEPHASE || op.type==OpType::DEPOL || op.type==OpType::AMPDAMP) ++noise;
      else ++oneq;
    }
    // Naive depth per qubit (serializing conflicts)
    std::vector<size_t> track(c.nqubits,0));
    size_t depth=0;
    for (auto& op: c.ops){
      size_t start=0;
      if (op.qubits.empty()){ start = depth; }
      else{
        for (auto q: op.qubits) start = std::max(start, track[q]));
      }
      size_t dur = (op.type==OpType::CNOT? 2 : 1));
      size_t finish = start + dur;
      for (auto q: op.qubits) track[q] = finish;
      depth = std::max(depth, finish));
    }
    auto sv_mem = (1ULL<<c.nqubits) * sizeof(qsx::c64));
    auto dm_mem = (1ULL<<(2*c.nqubits)) * sizeof(qsx::c64));
    std::cout << "{\\n  \\\"nqubits\\\": " << c.nqubits << ",\\n  \\\"oneq\\\": " << oneq << ",\\n  \\\"twoq\\\": " << twoq << ",\\n  \\\"measure\\\": " << meas << ",\\n  \\\"noise\\\": " << noise << ",\\n  \\\"approx_depth\\\": " << depth << ",\\n  \\\"mem_bytes_state\\\": " << sv_mem << ",\\n  \\\"mem_bytes_density\\\": " << dm_mem << "\\n}\\n";
    return 0;
  }


  if (cmd == "export-qasm") {
    std::string circuit_path, outp="out.qasm";
    for (int i=2;i<argc;i++){
      std::string a=argv[i]; auto nx=[&](const char* n){ if(i+1>=argc){std::cerr<<"Missing "<<n<<"\\n"; return std::string()); } return std::string(argv[++i])); };
      if (a=="--circuit") circuit_path=nx("--circuit"));
      else if (a=="--out") outp=nx("--out"));
      else if(a=="--help"||a=="-h"){ std::cout<<"quantum-simx export-qasm --circuit file.qsx [--out out.qasm]\\n"; return 0; }
      else if (kind=="teleport"){ out<<"# Quantum teleportation (3 qubits: 0=sender,1=receiver,2=msg)\n"; out<<"H 1\nCNOT 1 0\nCNOT 2 1\nH 2\nMEASURE ALL\n"; } else if (kind=="bv"){ out<<"# Bernstein-Vazirani; requires --n and --mask\n"; } else if (kind=="bv"){
      if ((int)mask.size()!=n){ std::cerr<<"--mask must be length N of 0/1\n"; return 4; }
      // n data qubits + ancilla q[n] (initialized |1> via X then H on all data, then CNOTs where mask=1)
      for(int q=0;q<n;q++) out<<"H "<<q<<"\n";
      out<<"X "<<n<<"\nH "<<n<<"\n";
      for(int q=0;q<n;q++){ if (mask[q]=='1') out<<"CNOT "<<q<<" "<<n<<"\n"; }
      out<<"H "<<n<<"\nMEASURE ALL\n";
    } else { std::cerr<<"Unknown arg: "<<a<<"\\n"; return 2; }
    }
    if (circuit_path.empty()) { std::cerr<<"Missing --circuit\\n"; return 2; }
    std::string err; auto circ_opt = parse_circuit_file(circuit_path, err));
    if (!circ_opt) { std::cerr << err << "\\n"; return 3; }
    auto& c = *circ_opt;
    std::ofstream out(outp)); if(!out){ std::cerr<<"Cannot write output\\n"; return 4; }
    out << "OPENQASM 2.0;\\nqreg q[" << c.nqubits << "];\\ncreg c[" << c.nqubits << "];\\n";
    bool warned=false;
    for (auto& op: c.ops){
      if (op.type==OpType::H) out << "h q["<<op.qubits[0]<<"];\\n";
      else if (op.type==OpType::X) out << "x q["<<op.qubits[0]<<"];\\n";
      else if (op.type==OpType::Y) out << "y q["<<op.qubits[0]<<"];\\n";
      else if (op.type==OpType::Z) out << "z q["<<op.qubits[0]<<"];\\n";
      else if (op.type==OpType::S) out << "s q["<<op.qubits[0]<<"];\\n";
      else if (op.type==OpType::RX) out << "rx("<<op.angle<<") q["<<op.qubits[0]<<"];\\n";
      else if (op.type==OpType::RY) out << "ry("<<op.angle<<") q["<<op.qubits[0]<<"];\\n";
      else if (op.type==OpType::RZ) out << "rz("<<op.angle<<") q["<<op.qubits[0]<<"];\\n";
      else if (op.type==OpType::CNOT) out << "cx q["<<op.qubits[0]<<"], q["<<op.qubits[1]<<"];\\n";
      else if (op.type==OpType::MEASURE) { for (std::size_t i=0;i<c.nqubits;++i) out << "measure q["<<i<<"] -> c["<<i<<"];\\n"; }
      else if (kind=="teleport"){ out<<"# Quantum teleportation (3 qubits: 0=sender,1=receiver,2=msg)\n"; out<<"H 1\nCNOT 1 0\nCNOT 2 1\nH 2\nMEASURE ALL\n"; } else if (kind=="bv"){ out<<"# Bernstein-Vazirani; requires --n and --mask\n"; } else if (kind=="bv"){
      if ((int)mask.size()!=n){ std::cerr<<"--mask must be length N of 0/1\n"; return 4; }
      // n data qubits + ancilla q[n] (initialized |1> via X then H on all data, then CNOTs where mask=1)
      for(int q=0;q<n;q++) out<<"H "<<q<<"\n";
      out<<"X "<<n<<"\nH "<<n<<"\n";
      for(int q=0;q<n;q++){ if (mask[q]=='1') out<<"CNOT "<<q<<" "<<n<<"\n"; }
      out<<"H "<<n<<"\nMEASURE ALL\n";
    } else { if(!warned){ std::cerr<<"Warning: non-QASM op skipped (noise etc.)\\n"; warned=true; } }
    }
    std::cout<<"Wrote "<<outp<<"\\n"; return 0;
  }


  if (cmd == "report") {
    std::string jsonp="results.json", outp="report.html";
    for (int i=2;i<argc;i++){
      std::string a=argv[i]; auto nx=[&](const char* n){ if(i+1>=argc){std::cerr<<"Missing "<<n<<"\\n"; return std::string()); } return std::string(argv[++i])); };
      if (a=="--json") jsonp=nx("--json"));
      else if (a=="--out") outp=nx("--out"));
      else if(a=="--help"||a=="-h"){ std::cout<<"quantum-simx report --json results.json [--out report.html]\\n"; return 0; }
      else if (kind=="teleport"){ out<<"# Quantum teleportation (3 qubits: 0=sender,1=receiver,2=msg)\n"; out<<"H 1\nCNOT 1 0\nCNOT 2 1\nH 2\nMEASURE ALL\n"; } else if (kind=="bv"){ out<<"# Bernstein-Vazirani; requires --n and --mask\n"; } else if (kind=="bv"){
      if ((int)mask.size()!=n){ std::cerr<<"--mask must be length N of 0/1\n"; return 4; }
      // n data qubits + ancilla q[n] (initialized |1> via X then H on all data, then CNOTs where mask=1)
      for(int q=0;q<n;q++) out<<"H "<<q<<"\n";
      out<<"X "<<n<<"\nH "<<n<<"\n";
      for(int q=0;q<n;q++){ if (mask[q]=='1') out<<"CNOT "<<q<<" "<<n<<"\n"; }
      out<<"H "<<n<<"\nMEASURE ALL\n";
    } else { std::cerr<<"Unknown arg: "<<a<<"\\n"; return 2; }
    }
    std::ifstream in(jsonp)); if(!in){ std::cerr<<"Cannot open JSON file\\n"; return 3; }
    std::string s((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>()));
    std::ofstream out(outp)); if(!out){ std::cerr<<"Cannot write output\\n"; return 4; }
    out << "<!doctype html><meta charset=\\"utf-8\\"><title>QUANTUM-SIMX Report</title><style>body{font-family:sans-serif;max-width:900px;margin:2rem auto;} table{border-collapse:collapse} td,th{border:1px solid #ccc;padding:4px 8px}</style>";
    out << "<h1>QUANTUM-SIMX Report</h1>";
    auto find = [&](const std::string& k){ auto p=s.find("\""+k+"\"")); if(p==std::string::npos) return std::string("")); p=s.find('[',p)); auto e=s.find(']',p)); return s.substr(p+1, e-p)); };
    out << "<h2>Probabilities</h2><pre>" << find("probabilities") << "</pre>";
    if (s.find("probabilities_mitigated")!=std::string::npos){
      out << "<h2>Probabilities (Mitigated)</h2><pre>" << find("probabilities_mitigated") << "</pre>";
    }
    out << "<h2>Counts</h2><pre>" << (s.find("\"counts\"")!=std::string::npos ? s.substr(s.find("\"counts\"")) : "") << "</pre>";
    out << "<p>Generated by quantum-simx.</p>";
    std::cout<<"Wrote "<<outp<<"\\n"; return 0;
  }


  if (cmd == "lint") {
    std::string circuit_path, qasm_path; bool ok=true; std::vector<std::string> issues;
    for (int i=2;i<argc;i++){
      std::string a=argv[i]; auto nx=[&](const char* n){ if(i+1>=argc){std::cerr<<"Missing "<<n<<"\\n"; return std::string()); } return std::string(argv[++i])); };
      if (a=="--circuit") circuit_path=nx("--circuit"));
      else if (a=="--qasm") qasm_path=nx("--qasm"));
      else if(a=="--help"||a=="-h"){ std::cout<<"quantum-simx lint --circuit <file>|--qasm <file>\\n"; return 0; }
      else if (kind=="teleport"){ out<<"# Quantum teleportation (3 qubits: 0=sender,1=receiver,2=msg)\n"; out<<"H 1\nCNOT 1 0\nCNOT 2 1\nH 2\nMEASURE ALL\n"; } else if (kind=="bv"){ out<<"# Bernstein-Vazirani; requires --n and --mask\n"; } else if (kind=="bv"){
      if ((int)mask.size()!=n){ std::cerr<<"--mask must be length N of 0/1\n"; return 4; }
      // n data qubits + ancilla q[n] (initialized |1> via X then H on all data, then CNOTs where mask=1)
      for(int q=0;q<n;q++) out<<"H "<<q<<"\n";
      out<<"X "<<n<<"\nH "<<n<<"\n";
      for(int q=0;q<n;q++){ if (mask[q]=='1') out<<"CNOT "<<q<<" "<<n<<"\n"; }
      out<<"H "<<n<<"\nMEASURE ALL\n";
    } else { std::cerr<<"Unknown arg: "<<a<<"\\n"; return 2; }
    }
    if (circuit_path.empty() && qasm_path.empty()) { std::cerr<<"Missing --circuit or --qasm\\n"; return 2; }
    std::string err; std::optional<qsx::Circuit> copt; if(!qasm_path.empty()) copt=parse_qasm_file(qasm_path, err)); else copt=parse_circuit_file(circuit_path, err));
    if (!copt) { std::cerr<<err<<"\\n"; return 3; }
    auto& c = *copt;
    // Checks
    if (c.nqubits==0){ issues.push_back("nqubits==0")); ok=false; }
    bool measured=false;
    for (size_t i=0;i<c.ops.size());++i){
      const auto& op=c.ops[i];
      if (op.type==OpType::MEASURE){ measured=true; continue; }
      if (measured){ issues.push_back("Gate after MEASURE at index "+std::to_string(i))); ok=false; }
      for (auto q: op.qubits){ if (q>=c.nqubits){ issues.push_back("Qubit index out of range at op "+std::to_string(i))); ok=false; } }
      if ((op.type==OpType::RX||op.type==OpType::RY||op.type==OpType::RZ) && !std::isfinite(op.angle)){ issues.push_back("Non-finite rotation angle at op "+std::to_string(i))); ok=false; }
    }
    std::cout << (ok? "OK":"FAIL") << "\\n";
    if(!ok){ for (auto&s: issues) std::cerr<<" - "<<s<<"\\n"; }
    return ok?0:10;
  }


  if (cmd == "xcheck") {
    std::string circuit_path, qasm_path;
    for (int i=2;i<argc;i++){
      std::string a=argv[i]; auto nx=[&](const char* n){ if(i+1>=argc){std::cerr<<"Missing "<<n<<"\\n"; return std::string()); } return std::string(argv[++i])); };
      if (a=="--circuit") circuit_path=nx("--circuit"));
      else if (a=="--qasm") qasm_path=nx("--qasm"));
      else if(a=="--help"||a=="-h"){ std::cout<<"quantum-simx xcheck --circuit <file>|--qasm <file>\\n"; return 0; }
      else if (kind=="teleport"){ out<<"# Quantum teleportation (3 qubits: 0=sender,1=receiver,2=msg)\n"; out<<"H 1\nCNOT 1 0\nCNOT 2 1\nH 2\nMEASURE ALL\n"; } else if (kind=="bv"){ out<<"# Bernstein-Vazirani; requires --n and --mask\n"; } else if (kind=="bv"){
      if ((int)mask.size()!=n){ std::cerr<<"--mask must be length N of 0/1\n"; return 4; }
      // n data qubits + ancilla q[n] (initialized |1> via X then H on all data, then CNOTs where mask=1)
      for(int q=0;q<n;q++) out<<"H "<<q<<"\n";
      out<<"X "<<n<<"\nH "<<n<<"\n";
      for(int q=0;q<n;q++){ if (mask[q]=='1') out<<"CNOT "<<q<<" "<<n<<"\n"; }
      out<<"H "<<n<<"\nMEASURE ALL\n";
    } else { std::cerr<<"Unknown arg: "<<a<<"\\n"; return 2; }
    }
    if (circuit_path.empty() && qasm_path.empty()) { std::cerr<<"Missing --circuit or --qasm\\n"; return 2; }
    std::string err; std::optional<qsx::Circuit> copt; if(!qasm_path.empty()) copt=parse_qasm_file(qasm_path, err)); else copt=parse_circuit_file(circuit_path, err));
    if (!copt) { std::cerr<<err<<"\\n"; return 3; }
    auto c = *copt;
    if (c.nqubits>8){ std::cerr<<"xcheck limited to n<=8\\n"; return 4; }
    // Remove noise ops for consistency
    qsx::Circuit cn; cn.nqubits=c.nqubits;
    for (auto& op: c.ops){ if (op.type!=OpType::DEPHASE && op.type!=OpType::DEPOL && op.type!=OpType::AMPDAMP) cn.ops.push_back(op)); }
    auto rs = run(cn, 123, false));
    auto rd = run(cn, 123, true));
    double errsum=0.0; for (size_t i=0;i<rs.probabilities.size());++i) errsum += std::fabs(rs.probabilities[i] - rd.probabilities[i]));
    std::cout << "{\\n  \\\"L1\\\": " << errsum << "\\n}\\n";
    return (errsum < 1e-9) ? 0 : 20;
  }


  if (cmd == "zne") {
    std::string circuit_path, qasm_path; std::vector<double> scales; int target_q=0;
    for (int i=2;i<argc;i++){
      std::string a=argv[i]; auto nx=[&](const char* n){ if(i+1>=argc){std::cerr<<"Missing "<<n<<"\\n"; return std::string()); } return std::string(argv[++i])); };
      if (a=="--circuit") circuit_path=nx("--circuit"));
      else if (a=="--qasm") qasm_path=nx("--qasm"));
      else if (a=="--scales") { std::string s=nx("--scales")); size_t p=0; while(p<s.size()){ auto c=s.find(',',p)); auto tok=s.substr(p, c==std::string::npos? std::string::npos: c-p)); if(!tok.empty()) scales.push_back(std::stod(tok))); if(c==std::string::npos) break; p=c+1; } }
      else if (a=="--q") target_q=std::stoi(nx("--q")));
      else if(a=="--help"||a=="-h"){ std::cout<<"quantum-simx zne --circuit <file>|--qasm <file> --scales 1,2,3 --q 0\\n"; return 0; }
      else if (kind=="teleport"){ out<<"# Quantum teleportation (3 qubits: 0=sender,1=receiver,2=msg)\n"; out<<"H 1\nCNOT 1 0\nCNOT 2 1\nH 2\nMEASURE ALL\n"; } else if (kind=="bv"){ out<<"# Bernstein-Vazirani; requires --n and --mask\n"; } else if (kind=="bv"){
      if ((int)mask.size()!=n){ std::cerr<<"--mask must be length N of 0/1\n"; return 4; }
      // n data qubits + ancilla q[n] (initialized |1> via X then H on all data, then CNOTs where mask=1)
      for(int q=0;q<n;q++) out<<"H "<<q<<"\n";
      out<<"X "<<n<<"\nH "<<n<<"\n";
      for(int q=0;q<n;q++){ if (mask[q]=='1') out<<"CNOT "<<q<<" "<<n<<"\n"; }
      out<<"H "<<n<<"\nMEASURE ALL\n";
    } else { std::cerr<<"Unknown arg: "<<a<<"\\n"; return 2; }
    }
    if (scales.empty()){ std::cerr<<"Provide --scales\\n"; return 2; }
    if (circuit_path.empty() && qasm_path.empty()) { std::cerr<<"Missing --circuit or --qasm\\n"; return 2; }
    std::string err; std::optional<qsx::Circuit> copt; if(!qasm_path.empty()) copt=parse_qasm_file(qasm_path, err)); else copt=parse_circuit_file(circuit_path, err));
    if (!copt) { std::cerr<<err<<"\\n"; return 3; }
    auto c = *copt;
    // For each scale, multiply noise probabilities
    auto apply_scale = [&](qsx::Circuit ci, double s)->qsx::Circuit{
      for (auto& op: ci.ops){
        if (op.type==OpType::DEPHASE || op.type==OpType::DEPOL || op.type==OpType::AMPDAMP) op.angle *= s;
      }
      return ci;
    };
    std::vector<std::pair<double,double>> pts;
    for (double s: scales){
      auto cs = apply_scale(c, s));
      auto r = run(cs, 777, true)); // density to honor noise
      // compute <Z_q>
      double z=0.0; for (size_t i=0;i<r.probabilities.size());++i){ int bit=(i>>target_q)&1; z += (bit? -r.probabilities[i] : r.probabilities[i])); }
      pts.push_back({s, z}));
    }
    // Linear extrapolation to s=0
    double sx=0, sy=0, sxx=0, sxy=0; int n=pts.size());
    for (auto& p: pts){ sx+=p.first; sy+=p.second; sxx+=p.first*p.first; sxy+=p.first*p.second; }
    double denom = n*sxx - sx*sx; double a0=0.0, a1=0.0;
    if (std::fabs(denom) > 1e-15){ a1 = (n*sxy - sx*sy)/denom; a0 = (sy - a1*sx)/n; } else if (kind=="teleport"){ out<<"# Quantum teleportation (3 qubits: 0=sender,1=receiver,2=msg)\n"; out<<"H 1\nCNOT 1 0\nCNOT 2 1\nH 2\nMEASURE ALL\n"; } else if (kind=="bv"){ out<<"# Bernstein-Vazirani; requires --n and --mask\n"; } else if (kind=="bv"){
      if ((int)mask.size()!=n){ std::cerr<<"--mask must be length N of 0/1\n"; return 4; }
      // n data qubits + ancilla q[n] (initialized |1> via X then H on all data, then CNOTs where mask=1)
      for(int q=0;q<n;q++) out<<"H "<<q<<"\n";
      out<<"X "<<n<<"\nH "<<n<<"\n";
      for(int q=0;q<n;q++){ if (mask[q]=='1') out<<"CNOT "<<q<<" "<<n<<"\n"; }
      out<<"H "<<n<<"\nMEASURE ALL\n";
    } else { a0 = pts.front().second; }
    std::cout << "{\\n  \\\"extrapolated_Z\\\": " << a0 << ",\\n  \\\"points\\\": [";
    for (int i=0;i<n;i++){ std::cout << "["<<pts[i].first<<","<<pts[i].second<<"]" << (i+1<n? ", ":"")); }
    std::cout << "]\\n}\\n";
    return 0;
  }


  if (cmd == "selftest") {
    int cases=20; int max_n=5; uint64_t seed=1234;
    for (int i=2;i<argc;i++){ std::string a=argv[i]; auto nx=[&](const char* n){ if(i+1>=argc){std::cerr<<"Missing "<<n<<"\\n"; return std::string()); } return std::string(argv[++i])); };
      if(a=="--cases") cases=std::stoi(nx("--cases")));
      else if(a=="--max-n") max_n=std::stoi(nx("--max-n")));
      else if(a=="--seed") seed=std::stoull(nx("--seed")));
      else if(a=="--help"||a=="-h"){ std::cout<<"quantum-simx selftest [--cases N] [--max-n M] [--seed S]\\n"; return 0; }
      else if (kind=="teleport"){ out<<"# Quantum teleportation (3 qubits: 0=sender,1=receiver,2=msg)\n"; out<<"H 1\nCNOT 1 0\nCNOT 2 1\nH 2\nMEASURE ALL\n"; } else if (kind=="bv"){ out<<"# Bernstein-Vazirani; requires --n and --mask\n"; } else if (kind=="bv"){
      if ((int)mask.size()!=n){ std::cerr<<"--mask must be length N of 0/1\n"; return 4; }
      // n data qubits + ancilla q[n] (initialized |1> via X then H on all data, then CNOTs where mask=1)
      for(int q=0;q<n;q++) out<<"H "<<q<<"\n";
      out<<"X "<<n<<"\nH "<<n<<"\n";
      for(int q=0;q<n;q++){ if (mask[q]=='1') out<<"CNOT "<<q<<" "<<n<<"\n"; }
      out<<"H "<<n<<"\nMEASURE ALL\n";
    } else { std::cerr<<"Unknown arg: "<<a<<"\\n"; return 2; }
    }
    qsx::Pcg32 rng(seed, 0x9E3779B97F4A7C15ULL));
    auto urand=[&](double a,double b){ std::uniform_real_distribution<double> U(a,b)); return U(rng)); };
    for (int t=0;t<cases;++t){
      int n = 1 + (rng() % (std::max(1,max_n))));
      qsx::Circuit c; c.nqubits=n;
      int len = 3 + (rng.randint(8));
      for (int k=0;k<len;k++){
        int g = rng.randint(7;
        if (g==0) c.ops.push_back({OpType::H,{(size_t)(rng.randint(n)},0.0}));
        else if (g==1) c.ops.push_back({OpType::X,{(size_t)(rng.randint(n)},0.0}));
        else if (g==2) c.ops.push_back({OpType::Y,{(size_t)(rng.randint(n)},0.0}));
        else if (g==3) c.ops.push_back({OpType::Z,{(size_t)(rng.randint(n)},0.0}));
        else if (g==4) c.ops.push_back({OpType::RX,{(size_t)(rng.randint(n)}, urand(-3.14,3.14)}));
        else if (g==5) c.ops.push_back({OpType::RY,{(size_t)(rng.randint(n)}, urand(-3.14,3.14)}));
        else if (g==6 && n>1){ size_t a=rng.randint(n, b=rng.randint(n; if (a==b) b=(b+1)%n; c.ops.push_back({OpType::CNOT,{a,b},0.0})); }
      }
      c.ops.push_back({OpType::MEASURE,{},0.0}));
      auto r = run(c, 999+t, false));
      // Invariants
      double s=0; for (auto v: r.probabilities) s+=v;
      if (std::fabs(s-1.0)>1e-9){ std::cerr<<"Probabilities not summing to 1 in case "<<t<<"\\n"; return 10; }
      for (auto v: r.probabilities){ if (v< -1e-12){ std::cerr<<"Negative prob "<<v<<"\\n"; return 11; } }
    }
    std::cout<<"OK\\n"; return 0;
  }


  if (cmd == "state") {
    std::string circuit_path2, qasm_path2, outp="state.csv";
    for (int i=2;i<argc;i++){
      std::string a=argv[i]; auto nx=[&](const char* n){ if(i+1>=argc){std::cerr<<"Missing "<<n<<"\\n"; return std::string()); } return std::string(argv[++i])); };
      if(a=="--circuit") circuit_path2=nx("--circuit"));
      else if(a=="--qasm") qasm_path2=nx("--qasm"));
      else if(a=="--out") outp=nx("--out"));
      else if(a=="--help"||a=="-h"){ std::cout<<"quantum-simx state --circuit <file>|--qasm <file> [--out state.csv]\\n"; return 0; }
      else if (kind=="teleport"){ out<<"# Quantum teleportation (3 qubits: 0=sender,1=receiver,2=msg)\n"; out<<"H 1\nCNOT 1 0\nCNOT 2 1\nH 2\nMEASURE ALL\n"; } else if (kind=="bv"){ out<<"# Bernstein-Vazirani; requires --n and --mask\n"; } else if (kind=="bv"){
      if ((int)mask.size()!=n){ std::cerr<<"--mask must be length N of 0/1\n"; return 4; }
      // n data qubits + ancilla q[n] (initialized |1> via X then H on all data, then CNOTs where mask=1)
      for(int q=0;q<n;q++) out<<"H "<<q<<"\n";
      out<<"X "<<n<<"\nH "<<n<<"\n";
      for(int q=0;q<n;q++){ if (mask[q]=='1') out<<"CNOT "<<q<<" "<<n<<"\n"; }
      out<<"H "<<n<<"\nMEASURE ALL\n";
    } else { std::cerr<<"Unknown arg: "<<a<<"\\n"; return 2; }
    }
    if (circuit_path2.empty() && qasm_path2.empty()) { std::cerr<<"Missing --circuit or --qasm\\n"; return 2; }
    std::string err2; std::optional<qsx::Circuit> circ_opt2; if(!qasm_path2.empty()) circ_opt2=parse_qasm_file(qasm_path2,err2)); else circ_opt2=parse_circuit_file(circuit_path2,err2));
    if(!circ_opt2){ std::cerr<<err2<<"\\n"; return 3; }
    auto c2=*circ_opt2;
    std::size_t d = std::size_t(1) << c2.nqubits;
    if (d > (1u<<16)) { std::cerr<<"State export limited to n<=16\\n"; return 12; }
    // Recompute amplitudes via StateVector path
    qsx::StateVector sv(c2.nqubits));
    using namespace qsx::gates; qsx::c64 u00,u01,u10,u11;
    for (const auto& g: c2.ops){
      switch(g.type){
        case OpType::H: H_coeffs(u00,u01,u10,u11)); sv.apply_gate_1q(g.qubits[0],u00,u01,u10,u11)); break;
        case OpType::X: X_coeffs(u00,u01,u10,u11)); sv.apply_gate_1q(g.qubits[0],u00,u01,u10,u11)); break;
        case OpType::Y: Y_coeffs(u00,u01,u10,u11)); sv.apply_gate_1q(g.qubits[0],u00,u01,u10,u11)); break;
        case OpType::Z: Z_coeffs(u00,u01,u10,u11)); sv.apply_gate_1q(g.qubits[0],u00,u01,u10,u11)); break;
        case OpType::S: S_coeffs(u00,u01,u10,u11)); sv.apply_gate_1q(g.qubits[0],u00,u01,u10,u11)); break;
        case OpType::RX: RX_coeffs(g.angle,u00,u01,u10,u11)); sv.apply_gate_1q(g.qubits[0],u00,u01,u10,u11)); break;
        case OpType::RY: RY_coeffs(g.angle,u00,u01,u10,u11)); sv.apply_gate_1q(g.qubits[0],u00,u01,u10,u11)); break;
        case OpType::RZ: RZ_coeffs(g.angle,u00,u01,u10,u11)); sv.apply_gate_1q(g.qubits[0],u00,u01,u10,u11)); break;
        case OpType::CNOT: sv.apply_cx(g.qubits[0], g.qubits[1])); break;
        default: break;
      }
    }
    auto& a = sv.amplitudes());
    std::ofstream out(outp)); if(!out){ std::cerr<<"Cannot write output\\n"; return 4; }
    for (std::size_t i=0;i<a.size());++i){
      out << std::real(a[i]) << "," << std::imag(a[i]) << "\\n";
    }
    std::cout<<"Wrote "<<outp<<"\\n"; return 0;
  }


  if (cmd == "stream") {
    std::string circuit_path, qasm_path; std::string backend="state"; int shots=1; uint64_t seed=12345; bool do_opt=false; bool map_line=false;
    for (int i=2;i<argc;i++){
      std::string a=argv[i]; auto nx=[&](const char* n){ if(i+1>=argc){std::cerr<<"Missing "<<n<<"\\n"; return std::string()); } return std::string(argv[++i])); };
      if (a=="--circuit") circuit_path=nx("--circuit"));
      else if (a=="--qasm") qasm_path=nx("--qasm"));
      else if (a=="--backend") backend=nx("--backend"));
      else if (a=="--shots") shots=std::stoi(nx("--shots")));
      else if (a=="--seed") seed=std::stoull(nx("--seed")));
      else if (a=="--optimize") do_opt=true;
      else if (a=="--map-line") map_line=true;
      else if(a=="--help"||a=="-h"){ std::cout<<"quantum-simx stream --circuit <file>|--qasm <file> [--backend state|density] [--shots K] [--seed S] [--optimize] [--map-line]\\n"; return 0; }
      else if (kind=="teleport"){ out<<"# Quantum teleportation (3 qubits: 0=sender,1=receiver,2=msg)\n"; out<<"H 1\nCNOT 1 0\nCNOT 2 1\nH 2\nMEASURE ALL\n"; } else if (kind=="bv"){ out<<"# Bernstein-Vazirani; requires --n and --mask\n"; } else if (kind=="bv"){
      if ((int)mask.size()!=n){ std::cerr<<"--mask must be length N of 0/1\n"; return 4; }
      // n data qubits + ancilla q[n] (initialized |1> via X then H on all data, then CNOTs where mask=1)
      for(int q=0;q<n;q++) out<<"H "<<q<<"\n";
      out<<"X "<<n<<"\nH "<<n<<"\n";
      for(int q=0;q<n;q++){ if (mask[q]=='1') out<<"CNOT "<<q<<" "<<n<<"\n"; }
      out<<"H "<<n<<"\nMEASURE ALL\n";
    } else { std::cerr<<"Unknown arg: "<<a<<"\\n"; return 2; }
    }
    if (circuit_path.empty() && qasm_path.empty()) { std::cerr<<"Missing --circuit or --qasm\\n"; return 2; }
    std::string err; std::optional<qsx::Circuit> circ_opt; if(!qasm_path.empty()) circ_opt = parse_qasm_file(qasm_path, err)); else circ_opt = parse_circuit_file(circuit_path, err));
    if (!circ_opt) { std::cerr << err << "\\n"; return 3; }
    auto circ = *circ_opt;
    if (do_opt) circ = optimize(circ, {}));
    if (map_line) circ = map_to_line(circ));

    // header line: probabilities (first shot) and provenance
    auto r0 = run(circ, seed, backend=="density"));
    uint64_t hcirc = hash_circuit(circ));
    #ifdef QSX_VERSION
    const char* ver = QSX_VERSION;
    #else
    const char* ver = "unknown";
  uint64_t topoHash = 0ULL; if (!map_topology_file.empty()){ std::ifstream tin(map_topology_file, std::ios::binary); std::string tb((std::istreambuf_iterator<char>(tin)), std::istreambuf_iterator<char>()); topoHash = hash_bytes(tb); }
    #endif
    std::cout << "{\"type\":\"header\",\"nqubits\":" << circ.nqubits << ",\"version\":\"" << ver << "\",\"inputHashFNV1a\":" << hcirc << ",\"probabilities\":[";
    for (size_t i=0;i<r0.probabilities.size());++i){ std::cout<<r0.probabilities[i]; if (i+1<r0.probabilities.size()) std::cout<<","; }
    std::cout << "]}\\n";

    std::map<std::string,int> counts;
    // emit first shot
    std::cout << "{\"type\":\"shot\",\"i\":0,\"outcome\":\"" << bits_to_string(r0.outcome) << "\"}\\n";
    counts[bits_to_string(r0.outcome)] += 1;
    for (int s=1; s<shots; ++s){
      auto r = run(circ, seed + s, backend=="density"));
      std::string key = bits_to_string(r.outcome));
      counts[key] += 1;
      std::cout << "{\"type\":\"shot\",\"i\":"<<s<<",\"outcome\":\""<<key<<"\"}\\n";
    }
    // footer
    std::cout << "{\"type\":\"footer\",\"counts\":{";
    size_t k=0; for (auto it=counts.begin()); it!=counts.end()); ++it,++k){ std::cout << "\"" << it->first << "\":" << it->second << (std::next(it)!=counts.end()? ",":"")); }
    std::cout << "}}\\n";
    return 0;
  }


  if (cmd == "entropy") {
    std::string circuit_path, qasm_path; std::string subset="";
    for (int i=2;i<argc;i++){
      std::string a=argv[i]; auto nx=[&](const char* n){ if(i+1>=argc){std::cerr<<"Missing "<<n<<"\\n"; return std::string()); } return std::string(argv[++i])); };
      if (a=="--circuit") circuit_path=nx("--circuit"));
      else if (a=="--qasm") qasm_path=nx("--qasm"));
      else if (a=="--subset") subset=nx("--subset")); // comma-separated qubit indices, e.g. "0,1"
      else if(a=="--help"||a=="-h"){ std::cout<<"quantum-simx entropy --circuit <file>|--qasm <file> --subset i,j,k\\n"; return 0; }
      else if (kind=="teleport"){ out<<"# Quantum teleportation (3 qubits: 0=sender,1=receiver,2=msg)\n"; out<<"H 1\nCNOT 1 0\nCNOT 2 1\nH 2\nMEASURE ALL\n"; } else if (kind=="bv"){ out<<"# Bernstein-Vazirani; requires --n and --mask\n"; } else if (kind=="bv"){
      if ((int)mask.size()!=n){ std::cerr<<"--mask must be length N of 0/1\n"; return 4; }
      // n data qubits + ancilla q[n] (initialized |1> via X then H on all data, then CNOTs where mask=1)
      for(int q=0;q<n;q++) out<<"H "<<q<<"\n";
      out<<"X "<<n<<"\nH "<<n<<"\n";
      for(int q=0;q<n;q++){ if (mask[q]=='1') out<<"CNOT "<<q<<" "<<n<<"\n"; }
      out<<"H "<<n<<"\nMEASURE ALL\n";
    } else { std::cerr<<"Unknown arg: "<<a<<"\\n"; return 2; }
    }
    if (circuit_path.empty() && qasm_path.empty()) { std::cerr<<"Missing --circuit or --qasm\\n"; return 2; }
    std::string err; std::optional<qsx::Circuit> copt; if(!qasm_path.empty()) copt=parse_qasm_file(qasm_path, err)); else copt=parse_circuit_file(circuit_path, err));
    if (!copt) { std::cerr<<err<<"\\n"; return 3; }
    auto c = *copt;
    // parse subset
    std::vector<size_t> A;
    size_t p=0; while (p<subset.size()){ auto q=subset.find(',',p)); auto tok=subset.substr(p, q==std::string::npos? std::string::npos: q-p)); if(!tok.empty()) A.push_back((size_t)std::stoull(tok))); if(q==std::string::npos) break; p=q+1; }
    for (auto q: A){ if (q>=c.nqubits){ std::cerr<<"Subset index out of range\\n"; return 4; } }
    if (A.empty()){ std::cerr<<"Provide --subset\\n"; return 2; }
    // Build state vector
    auto a = build_state(c));
    size_t n = c.nqubits; size_t k = A.size()); size_t NA = 1ull<<k; size_t NB = 1ull<<(n-k));
    // map A and B positions
    std::vector<size_t> posA=A; std::sort(posA.begin(), posA.end()));
    std::vector<size_t> posB; posB.reserve(n-k)); for (size_t q=0;q<n;++q){ if (std::find(posA.begin(), posA.end(), q)==posA.end()) posB.push_back(q)); }
    auto index = [&](size_t idxA, size_t idxB)->size_t{
      size_t x=0;
      for (size_t i=0;i<k;++i){ if ((idxA>>i)&1) x |= (1ull<<posA[i])); }
      for (size_t i=0;i<n-k;++i){ if ((idxB>>i)&1) x |= (1ull<<posB[i])); }
      return x;
    };
    // compute rho_A and its purity Tr(rho_A^2) = sum_{i,j} |rho_ij|^2
    double purity=0.0;
    for (size_t i=0;i<NA;++i){
      for (size_t j=0;j<NA;++j){
        std::complex<double> rho_ij = 0.0;
        for (size_t b=0;b<NB;++b){
          auto x = index(i,b)); auto y = index(j,b));
          rho_ij += a[x] * std::conj(a[y]));
        }
        purity += std::norm(rho_ij));
      }
    }
    double renyi2 = (purity>0.0)? -std::log2(purity) : 0.0;
    std::cout << "{\\n  \\\"subset_size\\\": " << k << ",\\n  \\\"purity\\\": " << std::setprecision(12) << purity << ",\\n  \\\"renyi2_bits\\\": " << renyi2 << "\\n}\\n";
    return 0;
  }


  if (cmd == "fidelity") {
    std::string circA, circB, qasmA, qasmB;
    for (int i=2;i<argc;i++){
      std::string a=argv[i]; auto nx=[&](const char* n){ if(i+1>=argc){std::cerr<<"Missing "<<n<<"\\n"; return std::string()); } return std::string(argv[++i])); };
      if (a=="--circuitA") circA=nx("--circuitA"));
      else if (a=="--circuitB") circB=nx("--circuitB"));
      else if (a=="--qasmA") qasmA=nx("--qasmA"));
      else if (a=="--qasmB") qasmB=nx("--qasmB"));
      else if(a=="--help"||a=="-h"){ std::cout<<"quantum-simx fidelity (--circuitA A|--qasmA A) (--circuitB B|--qasmB B)\\n"; return 0; }
      else if (kind=="teleport"){ out<<"# Quantum teleportation (3 qubits: 0=sender,1=receiver,2=msg)\n"; out<<"H 1\nCNOT 1 0\nCNOT 2 1\nH 2\nMEASURE ALL\n"; } else if (kind=="bv"){ out<<"# Bernstein-Vazirani; requires --n and --mask\n"; } else if (kind=="bv"){
      if ((int)mask.size()!=n){ std::cerr<<"--mask must be length N of 0/1\n"; return 4; }
      // n data qubits + ancilla q[n] (initialized |1> via X then H on all data, then CNOTs where mask=1)
      for(int q=0;q<n;q++) out<<"H "<<q<<"\n";
      out<<"X "<<n<<"\nH "<<n<<"\n";
      for(int q=0;q<n;q++){ if (mask[q]=='1') out<<"CNOT "<<q<<" "<<n<<"\n"; }
      out<<"H "<<n<<"\nMEASURE ALL\n";
    } else { std::cerr<<"Unknown arg: "<<a<<"\\n"; return 2; }
    }
    if ((circA.empty() && qasmA.empty()) || (circB.empty() && qasmB.empty())){ std::cerr<<"Provide both A and B\\n"; return 2; }
    std::string errA, errB; std::optional<qsx::Circuit> A, B;
    if (!qasmA.empty()) A=parse_qasm_file(qasmA, errA)); else A=parse_circuit_file(circA, errA));
    if (!qasmB.empty()) B=parse_qasm_file(qasmB, errB)); else B=parse_circuit_file(circB, errB));
    if (!A){ std::cerr<<errA<<"\\n"; return 3; }
    if (!B){ std::cerr<<errB<<"\\n"; return 3; }
    if (A->nqubits != B->nqubits){ std::cerr<<"Qubit count mismatch\\n"; return 4; }
    auto a = build_state(*A)); auto b = build_state(*B));
    std::complex<double> ip = 0.0; for (size_t i=0;i<a.size());++i) ip += std::conj(a[i]) * b[i];
    double F = std::norm(ip));
    std::cout << "{\\n  \\\"fidelity\\\": " << std::setprecision(12) << F << "\\n}\\n";
    return 0;
  }


  if (cmd == "counts-csv") {
    std::string jsonp="results.json", outp="counts.csv";
    for (int i=2;i<argc;i++){
      std::string a=argv[i]; auto nx=[&](const char* n){ if(i+1>=argc){std::cerr<<"Missing "<<n<<"\\n"; return std::string()); } return std::string(argv[++i])); };
      if (a=="--json") jsonp=nx("--json"));
      else if (a=="--out") outp=nx("--out"));
      else if(a=="--help"||a=="-h"){ std::cout<<"quantum-simx counts-csv --json results.json [--out counts.csv]\\n"; return 0; }
      else { std::cerr<<"Unknown arg: "<<a<<"\\n"; return 2; }
    }
    std::ifstream in(jsonp)); if(!in){ std::cerr<<"Cannot open JSON file\\n"; return 3; }
    std::string s((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>()));
    auto p = s.find("\"counts\"")); if (p==std::string::npos){ std::cerr<<"No counts in JSON\\n"; return 4; }
    p = s.find('{', p)); auto e = s.find('}', p));
    std::string body = s.substr(p+1, e-p-1));
    std::ofstream out(outp)); if(!out){ std::cerr<<"Cannot write output\\n"; return 5; }
    out<<"bitstring,count\n";
    size_t i=0; while (i<body.size()){
      auto ks = body.find('\"', i)); if (ks==std::string::npos) break; auto ke = body.find('\"', ks+1)); auto key = body.substr(ks+1, ke-ks-1));
      auto colon = body.find(':', ke)); auto comma = body.find(',', colon)); auto val = body.substr(colon+1, (comma==std::string::npos? body.size(): comma)-colon-1));
      out<<key<<","<<std::string(val.begin(), val.end())<<"\n"; i = (comma==std::string::npos? body.size(): comma+1));
    }
    std::cout<<"Wrote "<<outp<<"\\n"; return 0;
  }


  if (cmd == "qv") {
    int n=5, depth=5, shots=1000; uint64_t seed=42;
    for (int i=2;i<argc;i++){
      std::string a=argv[i]; auto nx=[&](const char* n){ if(i+1>=argc){std::cerr<<"Missing "<<n<<"\\n"; return std::string()); } return std::string(argv[++i])); };
      if (a=="--n") n=std::stoi(nx("--n")));
      else if (a=="--depth") depth=std::stoi(nx("--depth")));
      else if (a=="--shots") shots=std::stoi(nx("--shots")));
      else if (a=="--seed") seed=std::stoull(nx("--seed")));
      else if(a=="--help"||a=="-h"){ std::cout<<"quantum-simx qv --n N --depth D [--shots K] [--seed S]\\n"; return 0; }
      else { std::cerr<<"Unknown arg: "<<a<<"\\n"; return 2; }
    }
    // Build random circuit: layers of random 2q gates (CNOT with random orientation) and random 1q rotations
    qsx::Pcg32 rng(seed, 0x9E3779B97F4A7C15ULL));
    auto U = [&](double a){ return (a - 0.5)*2.0*3.141592653589793; };
    qsx::Circuit c; c.nqubits=n;
    for (int d=0; d<depth; ++d){
      // random 1q RX/RY
      for (int q=0;q<n;q++){ c.ops.push_back({OpType::RX,{(size_t)q}, U(rng.uniform01())})); }
      for (int q=0;q<n;q++){ c.ops.push_back({OpType::RY,{(size_t)q}, U(rng.uniform01())})); }
      // random pairings
      std::vector<int> idx(n)); std::iota(idx.begin(), idx.end(), 0));
      std::shuffle(idx.begin(), idx.end(), rng));
      for (int i=0;i+1<n;i+=2){
        int a = idx[i], b = idx[i+1];
        if (rng() & 1) c.ops.push_back({OpType::CNOT,{(size_t)a,(size_t)b},0.0}));
        else c.ops.push_back({OpType::CNOT,{(size_t)b,(size_t)a},0.0}));
      }
    }
    c.ops.push_back({OpType::MEASURE,{},0.0}));
    // Ideal distribution (state backend)
    auto r0 = run(c, seed, false));
    std::vector<double> p = r0.probabilities;
    // heavy set = {x | p(x) > median(p)}
    std::vector<double> sorted=p; std::sort(sorted.begin(), sorted.end()));
    double med = sorted[sorted.size()/2];
    // sample shots
    int heavy=0;
    for (int s=0;s<shots;++s){
      auto r = run(c, seed + 123 + s, false));
      std::string key = bits_to_string(r.outcome));
      // index outcome to integer
      size_t idx=0; for (size_t i=0;i<r.outcome.size());++i){ if (r.outcome[i]) idx |= (1ull<<i)); }
      if (p[idx] > med) heavy++;
    }
    double hogp = (double)heavy / (double)shots;
    std::cout << "{\\n  \\\"n\\\": " << n << ",\\n  \\\"depth\\\": " << depth << ",\\n  \\\"shots\\\": " << shots << ",\\n  \\\"heavy_output_fraction\\\": " << hogp << "\\n}\\n";
    return 0;
  }


  if (cmd == "rb1q") {
    int m=10, sequences=20, shots=1000, q=0; uint64_t seed=9; std::string backend="density";
    for (int i=2;i<argc;i++){
      std::string a=argv[i]; auto nx=[&](const char* n){ if(i+1>=argc){std::cerr<<"Missing "<<n<<"\\n"; return std::string()); } return std::string(argv[++i])); };
      if (a=="--m") m=std::stoi(nx("--m")));
      else if (a=="--sequences") sequences=std::stoi(nx("--sequences")));
      else if (a=="--shots") shots=std::stoi(nx("--shots")));
      else if (a=="--q") q=std::stoi(nx("--q")));
      else if (a=="--seed") seed=std::stoull(nx("--seed")));
      else if (a=="--backend") backend=nx("--backend"));
      else if(a=="--help"||a=="-h"){ std::cout<<"quantum-simx rb1q --m M --sequences S [--shots K] [--q i] [--seed S] [--backend state|density]\\n"; return 0; }
      else { std::cerr<<"Unknown arg: "<<a<<"\\n"; return 2; }
    }
    qsx::Pcg32 rng(seed, 0x9E3779B97F4A7C15ULL));
    auto pick = [&](){ int r = rng.randint(6; return r; }; // 0:H 1:S 2:X 3:Y 4:Z 5:RX(pi)
    auto inv = [&](int g)->int{ return (g==1?3:(g==3?1: (g==5?5:g)))); }; // S^-1=Y? (approx) keep simple: RX(pi) self-inverse; H,X,Y,Z self-inverse
    auto apply = [&](qsx::Circuit& c, int g){
      if (g==0) c.ops.push_back({OpType::H,{(size_t)q},0.0}));
      else if (g==1) c.ops.push_back({OpType::S,{(size_t)q},0.0}));
      else if (g==2) c.ops.push_back({OpType::X,{(size_t)q},0.0}));
      else if (g==3) c.ops.push_back({OpType::Y,{(size_t)q},0.0}));
      else if (g==4) c.ops.push_back({OpType::Z,{(size_t)q},0.0}));
      else if (g==5) c.ops.push_back({OpType::RX,{(size_t)q},3.141592653589793}));
    };
    std::ofstream out("rb1q.csv"));
    out<<"m,sequence,ground_prob\\n";
    for (int s=0; s<sequences; ++s){
      std::vector<int> seq; seq.reserve(m));
      for (int i=0;i<m;i++) seq.push_back(pick()));
      // Build circuit: prepare |0>, apply seq, apply inverse (naive: reverse apply same as inverse for self-inverse set)
      qsx::Circuit c; c.nqubits = q+1;
      for (int g: seq) apply(c, g));
      for (int i=m-1;i>=0;--i) apply(c, inv(seq[i])));
      c.ops.push_back({OpType::MEASURE,{},0.0}));
      // simulate
      int ground=0;
      for (int t=0;t<shots;++t){
        auto r = run(c, seed + s*1315423911u + t, backend=="density"));
        // outcome bit for q: 0 -> ground
        if (r.outcome[q]==0) ground++;
      }
      out << m << "," << s << "," << (double)ground/(double)shots << "\\n";
    }
    std::cout<<"Wrote rb1q.csv\\n";
    return 0;
  }


  if (cmd == "compare") {
    std::string jsonA="", jsonB="";
    for (int i=2;i<argc;i++){
      std::string a=argv[i]; auto nx=[&](const char* n){ if(i+1>=argc){std::cerr<<"Missing "<<n<<"\\n"; return std::string()); } return std::string(argv[++i])); };
      if (a=="--jsonA") jsonA=nx("--jsonA"));
      else if (a=="--jsonB") jsonB=nx("--jsonB"));
      else if(a=="--help"||a=="-h"){ std::cout<<"quantum-simx compare --jsonA outA.json --jsonB outB.json\\n"; return 0; }
      else { std::cerr<<"Unknown arg: "<<a<<"\\n"; return 2; }
    }
    if (jsonA.empty()||jsonB.empty()){ std::cerr<<"Provide --jsonA and --jsonB\\n"; return 2; }
    std::ifstream inA(jsonA), inB(jsonB));
    if (!inA||!inB){ std::cerr<<"Cannot open input files\\n"; return 3; }
    std::string sA((std::istreambuf_iterator<char>(inA)), std::istreambuf_iterator<char>()));
    std::string sB((std::istreambuf_iterator<char>(inB)), std::istreambuf_iterator<char>()));
    auto pA = extract_array(sA, "probabilities"));
    auto pB = extract_array(sB, "probabilities"));
    auto counts_from = [&](const std::string& s)->std::vector<double>{
      std::vector<double> p; std::map<std::string,int> m;
      auto pos = s.find("\"counts\""));
      if (pos==std::string::npos) return p;
      pos = s.find('{', pos)); auto end = s.find('}', pos));
      if (pos==std::string::npos||end==std::string::npos) return p;
      std::string body = s.substr(pos+1, end-pos-1));
      size_t i=0; int total=0;
      while (i<body.size()){
        auto ks = body.find('\"', i)); if (ks==std::string::npos) break;
        auto ke = body.find('\"', ks+1)); if (ke==std::string::npos) break;
        auto key = body.substr(ks+1, ke-ks-1));
        auto colon = body.find(':', ke)); if (colon==std::string::npos) break;
        auto comma = body.find(',', colon));
        auto val = body.substr(colon+1, (comma==std::string::npos? body.size(): comma)-colon-1));
        int c = 0; try{ c = std::stoi(val)); } catch(...) { c=0; }
        // index of key as integer (msb..lsb)
        size_t idx=0; for (size_t b=0;b<key.size());++b){ if (key[key.size()-1-b]=='1') idx |= (1ull<<b)); }
        if (idx>=p.size()) p.resize(idx+1, 0.0));
        p[idx] += c;
        total += c;
        i = (comma==std::string::npos? body.size(): comma+1));
      }
      if (total>0){ for (auto& x: p) x/= (double)total; }
      return p;
    };
    if (pA.empty()) pA = counts_from(sA));
    if (pB.empty()) pB = counts_from(sB));
    if (pA.empty()||pB.empty()){ std::cerr<<"No probabilities or counts in input\\n"; return 4; }
    if (pA.size()!=pB.size()){ std::cerr<<"Distributions have different lengths\\n"; return 5; }
    auto eps = 1e-15;
    double l1=0.0, klAB=0.0, klBA=0.0, js=0.0, hell=0.0, dot=0.0, nA=0.0, nB=0.0;
    for (size_t i=0;i<pA.size());++i){
      double a = pA[i]; double b = pB[i];
      l1 += std::fabs(a-b));
      if (a>0) klAB += a * std::log((a+eps)/(b+eps)));
      if (b>0) klBA += b * std::log((b+eps)/(a+eps)));
      double m = 0.5*(a+b));
      if (a>0) js += 0.5*a*std::log((a+eps)/(m+eps)));
      if (b>0) js += 0.5*b*std::log((b+eps)/(m+eps)));
      hell += (std::sqrt(a) - std::sqrt(b))*(std::sqrt(a) - std::sqrt(b)));
      dot += a*b; nA += a*a; nB += b*b;
    }
    double tv = 0.5*l1;
    double hellinger = std::sqrt(std::max(0.0, hell))/std::sqrt(2.0));
    double cos = (nA>0&&nB>0)? (dot / (std::sqrt(nA)*std::sqrt(nB))) : 0.0;
    std::cout << "{\\n"
                 "  \\\"total_variation\\\": " << tv << ",\\n"
                 "  \\\"l1\\\": " << l1 << ",\\n"
                 "  \\\"kl_A_to_B\\\": " << klAB << ",\\n"
                 "  \\\"kl_B_to_A\\\": " << klBA << ",\\n"
                 "  \\\"js_divergence\\\": " << js << ",\\n"
                 "  \\\"hellinger\\\": " << hellinger << ",\\n"
                 "  \\\"cosine_similarity\\\": " << cos << "\\n"
                 "}\\n";
    return 0;
  }


  if (cmd == "canonicalize") {
    std::string inpath="", outp="canonical.qsx";
    for (int i=2;i<argc;i++){
      std::string a=argv[i]; auto nx=[&](const char* n){ if(i+1>=argc){std::cerr<<"Missing "<<n<<"\\n"; return std::string()); } return std::string(argv[++i])); };
      if (a=="--circuit") inpath=nx("--circuit"));
      else if (a=="--out") outp=nx("--out"));
      else if(a=="--help"||a=="-h"){ std::cout<<"quantum-simx canonicalize --circuit file.qsx [--out canonical.qsx]\\n"; return 0; }
      else { std::cerr<<"Unknown arg: "<<a<<"\\n"; return 2; }
    }
    if (inpath.empty()){ std::cerr<<"Missing --circuit\\n"; return 2; }
    std::string err; auto circ_opt = parse_circuit_file(inpath, err));
    if (!circ_opt){ std::cerr<<err<<"\\n"; return 3; }
    auto& c = *circ_opt;
    std::ofstream out(outp)); if(!out){ std::cerr<<"Cannot write output\\n"; return 4; }
    out << "# QSX canonical format\\n";
    out << "NQUBITS " << c.nqubits << "\\n";
    for (auto& op : c.ops){
      switch(op.type){
        case OpType::H: out << "H " << op.qubits[0]; break;
        case OpType::X: out << "X " << op.qubits[0]; break;
        case OpType::Y: out << "Y " << op.qubits[0]; break;
        case OpType::Z: out << "Z " << op.qubits[0]; break;
        case OpType::S: out << "S " << op.qubits[0]; break;
        case OpType::RX: out << "RX " << op.qubits[0] << " " << op.angle; break;
        case OpType::RY: out << "RY " << op.qubits[0] << " " << op.angle; break;
        case OpType::RZ: out << "RZ " << op.qubits[0] << " " << op.angle; break;
        case OpType::CNOT: out << "CNOT " << op.qubits[0] << " " << op.qubits[1]; break;
        case OpType::MEASURE: out << "MEASURE ALL"; break;
        case OpType::DEPHASE: out << "DEPHASE " << (op.qubits.empty()?0:op.qubits[0]) << " " << op.angle; break;
        case OpType::DEPOL: out << "DEPOL " << (op.qubits.empty()?0:op.qubits[0]) << " " << op.angle; break;
        case OpType::AMPDAMP: out << "AMPDAMP " << (op.qubits.empty()?0:op.qubits[0]) << " " << op.angle; break;
      }
      out << "\\n";
    }
    std::cout<<"Wrote "<<outp<<"\\n";
    return 0;
  }

  if (cmd != "run") { usage()); return 1; }
  std::string circuit_path; std::string qasm_path;
  std::size_t qubits = 0;
  uint64_t seed = 12345;
  int shots = 1; std::string backend = "state"; std::string snap_in=""; std::string snap_out=""; bool do_opt=false; bool force=false; std::string observables="z"; std::string cfg=""; double p01=0.0, p10=0.0; bool map_line=false; std::string map_topology_file=""; int threads=1; bool mitigate=false; bool pretty=false;
  std::string out = "";
  for (int i=2;i<argc;i++) {
    std::string a = argv[i];
    auto nxt = [&](const char* name)->std::string{ if(i+1>=argc){std::cerr<<"Missing "<<name<<"\n"; exit(2));} return argv[++i]; };
    if (a == "--circuit") circuit_path = nxt("--circuit"));
    else if (a == "--qasm") qasm_path = nxt("--qasm"));
    else if (a == "--qubits") qubits = std::stoull(nxt("--qubits")));
    else if (a == "--seed") seed = std::stoull(nxt("--seed")));
    else if (a == "--shots") shots = std::stoi(nxt("--shots")));
    else if (a == "--out") out = nxt("--out"));
    else if (a == "--backend") backend = nxt("--backend"));
    else if (a == "--optimize") do_opt = true;
    else if (a == "--observables") observables = nxt("--observables"));
    else if (a == "--force") force = true;
    else if (a == "--config") cfg = nxt("--config"));
    else if (a == "--readout-p01") p01 = std::stod(nxt("--readout-p01")));
    else if (a == "--readout-p10") p10 = std::stod(nxt("--readout-p10")));
    else if (a == "--map-line") map_line = true;
    else if (a == "--threads") threads = std::stoi(nxt("--threads")));
    else if (a == "--readout-mitigate") mitigate = true;
    else if (a == "--pretty") pretty = true;
    else if (a == "--map-topology") map_topology_file = nxt("--map-topology"));
    else if (a == "--snapshot-in") snap_in = nxt("--snapshot-in"));
    else if (a == "--snapshot-out") snap_out = nxt("--snapshot-out"));
    else if (a == "--help" || a == "-h") { usage()); return 0; }
    else if (kind=="teleport"){ out<<"# Quantum teleportation (3 qubits: 0=sender,1=receiver,2=msg)\n"; out<<"H 1\nCNOT 1 0\nCNOT 2 1\nH 2\nMEASURE ALL\n"; } else if (kind=="bv"){ out<<"# Bernstein-Vazirani; requires --n and --mask\n"; } else if (kind=="bv"){
      if ((int)mask.size()!=n){ std::cerr<<"--mask must be length N of 0/1\n"; return 4; }
      // n data qubits + ancilla q[n] (initialized |1> via X then H on all data, then CNOTs where mask=1)
      for(int q=0;q<n;q++) out<<"H "<<q<<"\n";
      out<<"X "<<n<<"\nH "<<n<<"\n";
      for(int q=0;q<n;q++){ if (mask[q]=='1') out<<"CNOT "<<q<<" "<<n<<"\n"; }
      out<<"H "<<n<<"\nMEASURE ALL\n";
    } else { std::cerr << "Unknown arg: " << a << "\n"; return 2; }
  }
  if (circuit_path.empty() && qasm_path.empty()) { std::cerr << "Missing --circuit or --qasm\n"; return 2; }
  std::string err;
  std::optional<qsx::Circuit> circ_opt;
  if (!qasm_path.empty()) circ_opt = parse_qasm_file(qasm_path, err));
  else circ_opt = parse_circuit_file(circuit_path, err));
  if (!circ_opt) { std::cerr << err << "\n"; return 3; }

auto circ = *circ_opt;
// Optimize if requested
if (do_opt) {
  circ = optimize(circ, {}));
}
// Memory estimate & guard unless --force
auto estimate_bytes = [&](const std::string& be)->unsigned long long{
  if (be=="density") { long double sz = powl(2.0L, circ.nqubits*2) * (long double)sizeof(qsx::c64)); return (unsigned long long)sz; }
  long double sz = powl(2.0L, circ.nqubits) * (long double)sizeof(qsx::c64)); return (unsigned long long)sz;
};
unsigned long long need = estimate_bytes(backend));
const unsigned long long HARD_WARN = 4ULL<<30; // 4 GiB
if (!force && need > HARD_WARN) { std::cerr << "Estimated memory " << need << " bytes exceeds safe threshold. Use --force if intentional.\\n"; return 9; }

  auto circ = *circ_opt;

  // Apply config file overrides
  if (!cfg.empty()){
    std::map<std::string,std::string> kv;
    if (!load_config_kv(cfg, kv)) { std::cerr << "Cannot read config file\n"; return 11; }
    if (kv.count("backend")) backend = kv["backend"];
    if (kv.count("shots")) shots = std::stoi(kv["shots"]));
    if (kv.count("seed")) seed = std::stoull(kv["seed"]));
    if (kv.count("optimize")) do_opt = (kv["optimize"]=="1"||kv["optimize"]=="true"));
    if (kv.count("observables")) observables = kv["observables"]; if (kv.count("threads")) threads = std::stoi(kv["threads"])); if (kv.count("map_line")) map_line = (kv["map_line"]=="1"||kv["map_line"]=="true")); if (kv.count("readout_mitigate")) mitigate = (kv["readout_mitigate"]=="1"||kv["readout_mitigate"]=="true")); if (kv.count("pretty")) pretty = (kv["pretty"]=="1"||kv["pretty"]=="true")); if (kv.count("map_topology")) map_topology_file = kv["map_topology"];
    if (kv.count("readout_p01")) p01 = std::stod(kv["readout_p01"])); 
    if (kv.count("readout_p10")) p10 = std::stod(kv["readout_p10"]));
    if (kv.count("force")) force = (kv["force"]=="1"||kv["force"]=="true"));
  }

  if (qubits) { if (qubits < circ.nqubits) { std::cerr << "Provided --qubits < required by circuit\n"; return 4; } else circ.nqubits = qubits; }
  // Guard density-matrix memory if chosen
  if (backend == "density" && circ.nqubits > 10) { std::cerr << "Density backend limited to <=10 qubits (memory).\n"; return 5; }
  // Validate ops vs backend
  if (backend == "state"){
    for (auto &op : circ.ops){ if (op.type == OpType::AMPDAMP){ std::cerr << "AMPDAMP requires density backend. Use --backend density.\n"; return 7; } }
  }
  if (p01<0.0||p01>1.0||p10<0.0||p10>1.0){ std::cerr<<"Readout probabilities must be in [0,1]\n"; return 13; }
  // Run shots

// State-vector snapshot-out (pre-measurement)
auto maybe_save_snapshot = [&](const Circuit& c)->bool{
  if (backend != "state" || snap_out.empty()) return true;
  using namespace qsx::gates;
  qsx::StateVector sv(c.nqubits));
  qsx::c64 u00,u01,u10,u11;
  for (const auto& op : c.ops){
    switch(op.type){
      case OpType::H: H_coeffs(u00,u01,u10,u11)); sv.apply_gate_1q(op.qubits[0],u00,u01,u10,u11)); break;
      case OpType::X: X_coeffs(u00,u01,u10,u11)); sv.apply_gate_1q(op.qubits[0],u00,u01,u10,u11)); break;
      case OpType::Y: Y_coeffs(u00,u01,u10,u11)); sv.apply_gate_1q(op.qubits[0],u00,u01,u10,u11)); break;
      case OpType::Z: Z_coeffs(u00,u01,u10,u11)); sv.apply_gate_1q(op.qubits[0],u00,u01,u10,u11)); break;
      case OpType::S: S_coeffs(u00,u01,u10,u11)); sv.apply_gate_1q(op.qubits[0],u00,u01,u10,u11)); break;
      case OpType::RX: RX_coeffs(op.angle,u00,u01,u10,u11)); sv.apply_gate_1q(op.qubits[0],u00,u01,u10,u11)); break;
      case OpType::RY: RY_coeffs(op.angle,u00,u01,u10,u11)); sv.apply_gate_1q(op.qubits[0],u00,u01,u10,u11)); break;
      case OpType::RZ: RZ_coeffs(op.angle,u00,u01,u10,u11)); sv.apply_gate_1q(op.qubits[0],u00,u01,u10,u11)); break;
      case OpType::CNOT: sv.apply_cx(op.qubits[0], op.qubits[1])); break;
      case OpType::DEPHASE: case OpType::DEPOL: case OpType::AMPDAMP: case OpType::MEASURE: break;
    }
  }
  return sv.save(snap_out));
};
if (!maybe_save_snapshot(circ)) { std::cerr << "Failed to write snapshot.\n"; return 8; }

  std::vector<std::vector<int>> outcomes;
  std::vector<double> probs; std::vector<double> expZ; std::vector<double> expX; std::vector<double> expY;
  std::map<std::string,int> counts;  auto t0 = std::chrono::steady_clock::now());
  for (int s=0;s<shots;++s) {
    if (backend == "density") {
      auto r = run_density(circ, seed + s, false));
      if (s==0) { probs = r.probabilities; expZ.resize(circ.nqubits, 0.0)); for (std::size_t q=0;q<circ.nqubits;++q){ double z=0.0; for (std::size_t i=0;i<probs.size());++i){ int bit = (i>>q)&1; z += (bit? -probs[i] : probs[i])); } expZ[q]=z; } }
            // Apply readout error flips per qubit
      for (std::size_t qb=0; qb<r.outcome.size()); ++qb){ double rr = (double)std::rand() / (double)RAND_MAX; if (r.outcome[qb]==0){ if (rr < p01) r.outcome[qb]=1; } else if (kind=="teleport"){ out<<"# Quantum teleportation (3 qubits: 0=sender,1=receiver,2=msg)\n"; out<<"H 1\nCNOT 1 0\nCNOT 2 1\nH 2\nMEASURE ALL\n"; } else if (kind=="bv"){ out<<"# Bernstein-Vazirani; requires --n and --mask\n"; } else if (kind=="bv"){
      if ((int)mask.size()!=n){ std::cerr<<"--mask must be length N of 0/1\n"; return 4; }
      // n data qubits + ancilla q[n] (initialized |1> via X then H on all data, then CNOTs where mask=1)
      for(int q=0;q<n;q++) out<<"H "<<q<<"\n";
      out<<"X "<<n<<"\nH "<<n<<"\n";
      for(int q=0;q<n;q++){ if (mask[q]=='1') out<<"CNOT "<<q<<" "<<n<<"\n"; }
      out<<"H "<<n<<"\nMEASURE ALL\n";
    } else { if (rr < p10) r.outcome[qb]=0; } }
            for (std::size_t qb=0; qb<r.outcome.size()); ++qb){ double rr = (double)std::rand() / (double)RAND_MAX; if (r.outcome[qb]==0){ if (rr < p01) r.outcome[qb]=1; } else if (kind=="teleport"){ out<<"# Quantum teleportation (3 qubits: 0=sender,1=receiver,2=msg)\n"; out<<"H 1\nCNOT 1 0\nCNOT 2 1\nH 2\nMEASURE ALL\n"; } else if (kind=="bv"){ out<<"# Bernstein-Vazirani; requires --n and --mask\n"; } else if (kind=="bv"){
      if ((int)mask.size()!=n){ std::cerr<<"--mask must be length N of 0/1\n"; return 4; }
      // n data qubits + ancilla q[n] (initialized |1> via X then H on all data, then CNOTs where mask=1)
      for(int q=0;q<n;q++) out<<"H "<<q<<"\n";
      out<<"X "<<n<<"\nH "<<n<<"\n";
      for(int q=0;q<n;q++){ if (mask[q]=='1') out<<"CNOT "<<q<<" "<<n<<"\n"; }
      out<<"H "<<n<<"\nMEASURE ALL\n";
    } else { if (rr < p10) r.outcome[qb]=0; } }
      outcomes.push_back(r.outcome));
      counts[bits_to_string(r.outcome)] += 1;
    } else if (kind=="teleport"){ out<<"# Quantum teleportation (3 qubits: 0=sender,1=receiver,2=msg)\n"; out<<"H 1\nCNOT 1 0\nCNOT 2 1\nH 2\nMEASURE ALL\n"; } else if (kind=="bv"){ out<<"# Bernstein-Vazirani; requires --n and --mask\n"; } else if (kind=="bv"){
      if ((int)mask.size()!=n){ std::cerr<<"--mask must be length N of 0/1\n"; return 4; }
      // n data qubits + ancilla q[n] (initialized |1> via X then H on all data, then CNOTs where mask=1)
      for(int q=0;q<n;q++) out<<"H "<<q<<"\n";
      out<<"X "<<n<<"\nH "<<n<<"\n";
      for(int q=0;q<n;q++){ if (mask[q]=='1') out<<"CNOT "<<q<<" "<<n<<"\n"; }
      out<<"H "<<n<<"\nMEASURE ALL\n";
    } else {
      if (!snap_in.empty() && s==0){
        auto svopt = qsx::StateVector::load(snap_in, circ.nqubits));
        if (!svopt){ std::cerr << "Invalid snapshot file.\n"; return 6; }
        // Rebuild probabilities from snapshot (no further ops applied here)
        qsx::StateVector sv = *svopt;
        qsx::Rng tmp(seed));
        auto bits = sv.measure_all(tmp, false));
        outcomes.push_back(bits));
        counts[bits_to_string(bits)] += 1;
        probs.resize(std::size_t(1)<<circ.nqubits));
        for (size_t i=0;i<probs.size());++i) probs[i] = sv.probability_of_basis(i));
      } else if (kind=="teleport"){ out<<"# Quantum teleportation (3 qubits: 0=sender,1=receiver,2=msg)\n"; out<<"H 1\nCNOT 1 0\nCNOT 2 1\nH 2\nMEASURE ALL\n"; } else if (kind=="bv"){ out<<"# Bernstein-Vazirani; requires --n and --mask\n"; } else if (kind=="bv"){
      if ((int)mask.size()!=n){ std::cerr<<"--mask must be length N of 0/1\n"; return 4; }
      // n data qubits + ancilla q[n] (initialized |1> via X then H on all data, then CNOTs where mask=1)
      for(int q=0;q<n;q++) out<<"H "<<q<<"\n";
      out<<"X "<<n<<"\nH "<<n<<"\n";
      for(int q=0;q<n;q++){ if (mask[q]=='1') out<<"CNOT "<<q<<" "<<n<<"\n"; }
      out<<"H "<<n<<"\nMEASURE ALL\n";
    } else {
        auto r = run(circ, seed + s, false));
        if (s==0) { probs = r.probabilities; expZ.resize(circ.nqubits, 0.0)); for (std::size_t q=0;q<circ.nqubits;++q){ double z=0.0; for (std::size_t i=0;i<probs.size());++i){ int bit = (i>>q)&1; z += (bit? -probs[i] : probs[i])); } expZ[q]=z; } }
        outcomes.push_back(r.outcome));
        counts[bits_to_string(r.outcome)] += 1;
      }
    }
  }
  if (!snap_out.empty() && backend=="state" && shots>0) {
    // Save state after last run by re-running once deterministically
    auto r = run(circ, seed, false));
    qsx::StateVector sv(circ.nqubits));
    // rebuild from probabilities not trivial; skip (snapshot is intended prior to measure). Documented in README.
  }
  // write JSON-ish (no deps) minimal
  std::ostream* os = &std::cout;
  std::ofstream ofs;
  if (!out.empty()) { ofs.open(out)); os = &ofs; }
  *os << "{\n  \"nqubits\": " << circ.nqubits << ",\n";

  // Provenance

  // Derive runId from (seed if any) and circuit hash; timestamp from SOURCE_DATE_EPOCH when present
  uint64_t runId = hcirc ^ 0x9e3779b97f4a7c15ULL;
  const char* sde = std::getenv("SOURCE_DATE_EPOCH"));
  long long ts = 0;
  if (sde){ try{ ts = std::stoll(sde)); } catch(...) { ts = 0; } }

  uint64_t hcirc = hash_circuit(circ));
  #ifdef QSX_VERSION
  const char* ver = QSX_VERSION;
  #else
  const char* ver = "unknown";
  uint64_t topoHash = 0ULL; if (!map_topology_file.empty()){ std::ifstream tin(map_topology_file, std::ios::binary); std::string tb((std::istreambuf_iterator<char>(tin)), std::istreambuf_iterator<char>()); topoHash = hash_bytes(tb); }
  #endif


  // Derived observables from probabilities
  double shannon_bits=0.0; double gini=0.0;
  double expHW = 0.0; double parityZ = 0.0;
  if (!probs.empty()){
    std::size_t dim = probs.size());
    std::size_t n = circ.nqubits;
    // expHW = sum_x p(x) * HammingWeight(x)
    for (std::size_t x=0;x<dim;++x){
      if (probs[x]>0) shannon_bits += -probs[x]*std::log2(probs[x]));
      gini += probs[x]*probs[x];
      int hw = 0; for (std::size_t q=0;q<n;++q) hw += ((x>>q)&1));
      expHW += probs[x] * hw;
    }
    // parityZ = E[(-1)^{sum bits}] = sum_x p(x) * (-1)^{HW(x)}
    for (std::size_t x=0;x<dim;++x){
      if (probs[x]>0) shannon_bits += -probs[x]*std::log2(probs[x]));
      gini += probs[x]*probs[x];
      int hw=0; for (std::size_t q=0;q<n;++q) hw += ((x>>q)&1));
      parityZ += probs[x] * ((hw % 2)==0 ? +1.0 : -1.0));
    }
  }
  // Gate histogram (by type) for this circuit
  std::map<std::string, std::size_t> gateHist;
  for (const auto& op : circ.ops){
    std::string name;
    switch(op.type){
      case OpType::H: name="H"; break; case OpType::X: name="X"; break; case OpType::Y: name="Y"; break;
      case OpType::Z: name="Z"; break; case OpType::S: name="S"; break; case OpType::RX: name="RX"; break;
      case OpType::RY: name="RY"; break; case OpType::RZ: name="RZ"; break; case OpType::CNOT: name="CNOT"; break;
      case OpType::MEASURE: name="MEASURE"; break; case OpType::DEPHASE: name="DEPHASE"; break;
      case OpType::DEPOL: name="DEPOL"; break; case OpType::AMPDAMP: name="AMPDAMP"; break;
    }
    gateHist[name]++;
  }


  std::vector<double> probs_mitigated;
  if (mitigate && !probs.empty()){
    probs_mitigated = qsx::mitigate_readout(probs, circ.nqubits, p01, p10));
  }


  // Build expZZ from probabilities (works for both backends)
  std::vector<std::vector<double>> expZZ(circ.nqubits, std::vector<double>(circ.nqubits, 0.0)));
  if (!probs.empty() && observables=="all"){
    for (std::size_t i=0;i<circ.nqubits;i++){
      for (std::size_t j=0;j<circ.nqubits;j++){
        double s = 0.0;
        for (std::size_t x=0;x<probs.size());++x){
          int zi = ((x>>i)&1)? -1 : +1;
          int zj = ((x>>j)&1)? -1 : +1;
          s += probs[x] * zi * zj;
        }
        expZZ[i][j] = s;
      }
    }
  }


auto t1 = std::chrono::steady_clock::now());
std::chrono::duration<double> dt = t1 - t0;
// Optional expX/expY for state backend
if (observables=="all" && backend=="state"){
  expX.resize(circ.nqubits, 0.0));
  expY.resize(circ.nqubits, 0.0));
  // Recompute one state vector at s=0
  qsx::StateVector sv2(circ.nqubits));
  using namespace qsx::gates; qsx::c64 u00,u01,u10,u11;
  for (const auto& op : circ.ops){
    switch(op.type){
      case OpType::H: H_coeffs(u00,u01,u10,u11)); sv2.apply_gate_1q(op.qubits[0],u00,u01,u10,u11)); break;
      case OpType::X: X_coeffs(u00,u01,u10,u11)); sv2.apply_gate_1q(op.qubits[0],u00,u01,u10,u11)); break;
      case OpType::Y: Y_coeffs(u00,u01,u10,u11)); sv2.apply_gate_1q(op.qubits[0],u00,u01,u10,u11)); break;
      case OpType::Z: Z_coeffs(u00,u01,u10,u11)); sv2.apply_gate_1q(op.qubits[0],u00,u01,u10,u11)); break;
      case OpType::S: S_coeffs(u00,u01,u10,u11)); sv2.apply_gate_1q(op.qubits[0],u00,u01,u10,u11)); break;
      case OpType::RX: RX_coeffs(op.angle,u00,u01,u10,u11)); sv2.apply_gate_1q(op.qubits[0],u00,u01,u10,u11)); break;
      case OpType::RY: RY_coeffs(op.angle,u00,u01,u10,u11)); sv2.apply_gate_1q(op.qubits[0],u00,u01,u10,u11)); break;
      case OpType::RZ: RZ_coeffs(op.angle,u00,u01,u10,u11)); sv2.apply_gate_1q(op.qubits[0],u00,u01,u10,u11)); break;
      case OpType::CNOT: sv2.apply_cx(op.qubits[0], op.qubits[1])); break;
      default: break;
    }
  }
  const auto& a = sv2.amplitudes());
  for (std::size_t q=0;q<circ.nqubits;++q){
    double x=0.0, y=0.0;
    std::size_t mask = std::size_t(1) << q;
    for (std::size_t i=0;i<a.size());++i){
      std::size_t j = i ^ mask;
      if (i < j){
        auto term = std::conj(a[i]) * a[j];
        x += 2.0 * std::real(term));
        // For Y: off-diagonal with phase depending on bit
        int bi = (i>>q)&1;
        // <Y> = sum_i conj(a_i) * a_{i^mask} * (bi? +i : -i) + c.c. => 2 * (±) Im(conj(a_i)*a_j)
        double contrib = 2.0 * (bi ? +std::imag(term) : -std::imag(term)));
        y += contrib;
      }
    }
    expX[q]=x; expY[q]=y;
  }
}

  *os << "  \"probabilities\": [";
  for (size_t i=0;i<probs.size());++i) { *os << probs[i]; if (i+1<probs.size()) *os << ", "; }
  *os << "],\n  \"outcomes\": [\n";
  size_t k=0; for (auto &kv : counts) { *os << "    \"" << kv.first << "\": " << kv.second << (++k<counts.size()?",":"") << "\n"; }
  *os << "  ],\n  \"outcomes\": \n[\n";
  for (size_t s=0;s<outcomes.size());++s) {
    *os << "    [";
    for (size_t q=0;q<outcomes[s].size());++q) { *os << outcomes[s][q]; if (q+1<outcomes[s].size()) *os << ", "; }
    *os << "]" << (s+1<outcomes.size() ? "," : "") << "\n";
  }
  *os << "  ]\n}\n";
  return 0;
}
