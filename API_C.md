
# C API

```c
#include <quantum/c_api.h>

char* out = NULL;
const char* qasm = "OPENQASM 2.0; qreg q[2]; h q[0]; cx q[0], q[1]; measure q[0] -> c[0]; measure q[1] -> c[1];";
int rc = qsx_run_string(qasm, "{"backend":"state","shots":1000,"seed":7}", &out);
if (rc==0) { puts(out); }
qsx_free(out);
```
