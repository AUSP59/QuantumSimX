
OPENQASM 2.0;
qreg q[2];
h q[0];
cx q[0], q[1];
measure q[0] -> c0;
measure q[1] -> c1;
