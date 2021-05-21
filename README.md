# um-tracer - Usermode Virtual Instruction Tracer Demo

um-trace is a demo project/repo which contains an example of how to create a runtime trace of a VMProtect'ed binary (VMProtect 2). The binaries in this repo are not packed for simplicity sake, however in later demo's I will be applying packing to the executable. 

#### Contents

* dependencies/ - this project is dependent on `vmtracer`. 
* refbuilds/ - binaries protected with `ultra virtualization` and no packing. These bins are for you to mess with!
* src/ - source code for the usermode tracer. 
    * vmptest/ - source code for the the test bins
    * um-tracer/ - source code for usermode tracer, includes a hook on `LCONSTBZX`.