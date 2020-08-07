# Code Samples of Intel DPC++ compiler
We are in the process of moving all Intel oneAPI code samples to an open source repository:  https://github.com/oneapi-src/oneAPI-samples. This repository will be deprecated in the near future.

| Code sample name                          | Supported Intel(r) Architecture(s) | Description
|:---                                       |:---                                |:---
| FPGATutorials/BestPractices/double_buffering| FPGA, CPU                 | See details under FPGATutorials
| FPGATutorials/BestPractices/local_memory_cache| FPGA, CPU               | See details under FPGATutorials
| FPGATutorials/BestPractices/n_way_buffering| FPGA, CPU                  | See details under FPGATutorials
| FPGATutorials/BestPractices/remove_loop_carried_dependency| FPGA, CPU                  | See details under FPGATutorials
| FPGATutorials/BestPractices/triangular_loop| FPGA, CPU                  | See details under FPGATutorials
| FPGATutorials/Compilation/compile_flow| FPGA, CPU                 | See details under FPGATutorials
| FPGATutorials/Compilation/device_link| FPGA, CPU                 | See details under FPGATutorials
| FPGATutorials/Compilation/use_library| FPGA, CPU                 | See details under FPGATutorials
| FPGATutorials/FPGAExtensions/LoopAttributes/loop_ivdep| FPGA, CPU                 | See details under FPGATutorials
| FPGATutorials/FPGAExtensions/LoopAttributes/loop_unroll| FPGA, CPU                 | See details under FPGATutorials
| FPGATutorials/FPGAExtensions/LoopAttributes/max_concurrency| FPGA, CPU                 | See details under FPGATutorials
| FPGATutorials/FPGAExtensions/Other/fpga_register| FPGA, CPU                 | See details under FPGATutorials
| FPGATutorials/FPGAExtensions/Other/no_accessor_aliasing| FPGA, CPU                 | See details under FPGA Tutorials
| FPGATutorials/FPGAExtensions/Other/system_profiling| FPGA, CPU                 | See details under FPGATutorials
| FPGATutorials/FPGAExtensions/MemoryAttributes/memory_attributes_overview| FPGA, CPU               | See details under FPGATutorials
| FPGATutorials/FPGAExtensions/Pipes/pipe_array| FPGA                           | See details under FPGATutorials
| FPGATutorials/FPGAExtensions/Pipes/pipes| FPGA                           | See details under FPGATutorials
| FPGAExampleDesigns/crr| FPGA, CPU                        | See details under FPGAExampleDesigns
| FPGAExampleDesigns/gzip| FPGA                       | See details under FPGAExampleDesigns
| FPGAExampleDesigns/grd| FPGA, CPU                        | See details under FPGAExampleDesigns
| Debugger/array-transform                              | GPU, CPU                     | Array transform
| ThreadingBuildingBlocks/tbb-async-sycl             | GPU, CPU  | The calculations are split between TBB Flow Graph asynchronous node that calls SYCL kernel on GPU while TBB functional node does CPU part of calculations.
| ThreadingBuildingBlocks/tbb-task-sycl              | GPU, CPU  | One TBB task executes SYCL code on GPU while another TBB task performs calculations using TBB parallel_for.
| VideoProcessingLibrary/hello-decode  | CPU | shows how to use oneVPL to perform a simple video decode
| VideoProcessingLibrary/hello-encode  | CPU | shows how to use oneVPL to perform a simple video encode


## License  
The code samples are licensed under MIT license 

## Known issues or limitations 
### On Windows Platform 


1.  If you are using Visual Studio 2019, Visual Studio 2019 version 16.3.0 or newer is required. 
2.  To build samples on Windows, the required Windows SDK is ver. 10.0.17763.0. 
	1.  If the SDK is not installed, use the following instructions below to avoid build failure: 
		1.  Open the "code sample's" .sln from within Visual Studio 2017 or 2019, 
		2.  Right-click on the project name in "Solution Explorer" and select "Properties"
	2.  The project property dialog opens.
		1.  Select the "General" tab on the left, 
		2.  Select on the right side of the dialog box "Windows SDK Version"(2nd Item". 
		3.  Click on the drop-down icon to select a version that is installed on your system. 
		4.  click on [Ok] to save. 
3.  Now you should be able to build the code sample. 
3.  For beta, FPGA samples support Windows through FPGA-emulator. 
4.  If you encounter a compilation error like below when building a sample program, one reason is that the directory path of the sample is too long; the work around is to move the sample to a directory like "c:\temp\sample_name".
  
> Error MSB6003 The specified task executable "dpcpp-cl.exe" could not be run ...... 

