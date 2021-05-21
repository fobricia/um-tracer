<div align="center">
    <div>
        <img src="https://githacks.org/uploads/-/system/project/avatar/374/icon-5.png"/>
    </div>
</div>

# um-tracer - Usermode Virtual Instruction Tracer Demo

um-trace is a demo project/repo which contains an example of how to create a runtime trace of a VMProtect'ed binary (VMProtect 2). The binaries in this repo are not packed for simplicity sake, however in later demo's I will be applying packing to the executable. 

#### Contents

* dependencies/ - this project is dependent on `vmtracer`. 
* refbuilds/ - binaries protected with `ultra virtualization` and no packing. These bins are for you to mess with!
* src/ - source code for the usermode tracer. 
    * vmptest/ - source code for the the test bins
    * um-tracer/ - source code for usermode tracer, includes a hook on `LCONSTBZX`.

### Usage

First download the repo with `git clone --recursive https://githacks.org/vmp2/um-tracer.git`, then compile the `um-tracer` by opening `demo.sln` inside of `src/`. There should be an executable called `um-tracer.exe` in `x64/Release`. This tracer program is compiled for the first `vmptest` binary in the `refbuilds` directly.

To create a trace file simply run the following:

```
um-tracer.exe --bin vmptest.vmp.exe --table 0x6473 --base 0x140000000 --out test.vmp2
```