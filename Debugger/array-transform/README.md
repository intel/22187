# DPC++ Sample for the Debugger

This is a small DPC++ example that accompanies the
[Get Started Guide](https://software.intel.com/en-us/get-started-with-debugging-dpcpp)
of the debugger.

| Optimized for                   | Description
|---------------------------------|--------------
| OS                              | Linux Ubuntu 18.04, Windows 10
| Hardware                        | Kaby Lake with GEN9 (on GPU) or newer (on CPU)
| Software                        | Intel&reg; oneAPI DPC++ Compiler (beta) 
| What you will learn             | Essential debugger features for effective debugging of DPC++ on CPU, GPU, and FPGA emulator
| Time to complete                | 20 minutes for CPU, 30 minutes for GPU

## License

This code sample is licensed under MIT license.

## How to build and run

### On Linux

To build:

```
$ mkdir build
$ cd build
$ cmake ..
$ make
```

*Note:* The cmake configuration enforces the `Debug` build type.

To run the program:

```
$ make run-cpu
```

This will execute the program by offloading to the CPU device.
Use the `run-gpu` and `run-fpga` targets of `make` to run the kernel
on the GPU and FPGA emulation devices, respectively.

To start a debugging session:

```
$ make debug-session
```

To clean up:

```
$ make clean
```

### On Windows

#### Command line using MSBuild

* `set CL_CONFIG_USE_NATIVE_DEBUGGER=1`
* `MSBuild array-transform.sln /t:Rebuild /p:Configuration="debug"`

#### Visual Studio IDE

* Open Visual Studio 2017 or Visual Studio 2019

* Open in Visual Studio "Tools > Options > Debugging > General" and
  ensure that "Require source files to exactly match the original
  version" Debugging option is **not** checked.  
  ![](vs-debugger-option.png)

* Select Menu "File > Open > Project/Solution", browse to the
  "array-transform" folder, and select "array-transform.sln".

* Select Menu "Build > Build Solution" to build the selected configuration.

* Select Menu "Debug > Start Debugging" to run the program.
