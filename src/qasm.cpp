// SPDX-License-Identifier: MIT

#include "quantum/qasm.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>

namespace qsx {
static std::string trim(const std::string& s){
  auto l = std::find_if(s.begin(), s.end(), [](unsigned char c){return !std::isspace(c);} );
  auto r = std::find_if(s.rbegin(), s.rend(), [](unsigned char c){return !std::isspace(c);} ).base();
  if (l>=r) return "";
  return std::string(l,r);
}
std::optional<Circuit> parse_qasm_file(const std::string& path, std::string& err){
  std::ifstream in(path);
  if (!in){ err="Cannot open QASM file"; return std::nullopt; }
  Circuit c; c.nqubits=0;
  std::string line;
  size_t qcount=0;
  while (std::getline(in,line)){
    line = trim(line);
    if (line.empty() || line[0]=='/' || line[0]=='#') continue;
    if (line.rfind("OPENQASM",0)==0 || line.rfind("include",0)==0) continue;
    if (line.rfind("qreg",0)==0){
      auto l = line.find('['), r = line.find(']');
      if (l==std::string::npos || r==std::string::npos){ err="Invalid qreg"; return std::nullopt; }
      qcount = std::stoull(line.substr(l+1, r-l-1));
      c.nqubits = qcount;
      continue;
    }
    if (line.rfind("measure",0)==0){ c.ops.push_back({OpType::MEASURE,{},{0.0}}); continue; }
    auto par = line.find('(');
    std::string op = line.substr(0, par==std::string::npos? line.find(' '): par);
    std::transform(op.begin(), op.end(), op.begin(), ::tolower);
    auto argstr = line.substr(line.find(' ')+1);
    auto q1p = argstr.find('['); auto q1e = argstr.find(']');
    if (q1p==std::string::npos) { err="No target qubit"; return std::nullopt; }
    size_t q1 = std::stoull(argstr.substr(q1p+1, q1e-q1p-1));
    if (op=="h"||op=="x"||op=="y"||op=="z"||op=="s"){
      c.ops.push_back({ op=="h"?OpType::H: op=="x"?OpType::X: op=="y"?OpType::Y: op=="z"?OpType::Z: OpType::S, {q1}, 0.0 });
    } else if (op=="rz"||op=="rx"||op=="ry"){
      auto lp = line.find('('), rp = line.find(')');
      double ang = std::stod(line.substr(lp+1, rp-lp-1));
      c.ops.push_back({ op=="rz"?OpType::RZ: op=="rx"?OpType::RX: OpType::RY, {q1}, ang });
    } else if (op=="cx"){
      auto comma = argstr.find(',');
      auto q2p = argstr.find('[', comma); auto q2e = argstr.find(']', q2p);
      size_t q2 = std::stoull(argstr.substr(q2p+1, q2e-q2p-1));
      c.ops.push_back({OpType::CNOT,{q1,q2},0.0});
    } else {
      err = "Unsupported op: " + op;
      return std::nullopt;
    }
    c.nqubits = std::max(c.nqubits, std::max(q1+1, qcount));
  }
  return c;
}
} // namespace qsx
