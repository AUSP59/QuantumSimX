
# simple completion
_quantum_simx_complete() {
  COMPREPLY=( $(compgen -W "run mrun grad unitary pauli gen dot sweep bench qaoa check stats export-qasm report lint xcheck zne selftest" -- "${COMP_WORDS[1]}") )
}
complete -F _quantum_simx_complete quantum-simx
