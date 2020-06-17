//==============================================================
// Copyright © 2019 Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================
#include <CL/sycl.hpp>
#include <CL/sycl/intel/fpga_extensions.hpp>
#include <iomanip>
#include <random>
#include <thread>

using namespace std::chrono;
using namespace cl::sycl;

constexpr int kLocalN = 5;  // N-way buffering. N must be >= 1.

#if defined(FPGA_EMULATOR) || defined(CPU_HOST)
constexpr int kTimes =
    20;  // # times to execute the kernel. kTimes must be >= kLocalN
#else
constexpr int kTimes =
    100;  // # times to execute the kernel. kTimes must be >= kLocalN
#endif

#if defined(FPGA_EMULATOR) || defined(CPU_HOST)
constexpr int kSize = 4096;
#else
constexpr int kSize =
    2621440;  // # of floats to process on each kernel execution. ~10MB
#endif

constexpr int kPow = 20;  // Kernel executes a power function (base^kPow). Must be
                         // >= 2. Can increase this to increase kernel execution
                         // time, but ProcessOutput() time will also increase.
constexpr int kNumRuns = 4;  // Number of iterations through the main loop

bool pass = true;

class SimpleVpow;

/*  Kernel function.
    Performs buffer_b[i] = buffer_a[i] ** pow
    Only supports pow >= 2.
    This kernel is not meant to be an optimal implementation of the power
   operation -- it's just a sample kernel for this tutorial whose execution time
   is easily controlled via the pow parameter. SYCL buffers are created
   externally and passed in by reference to control (external to this function)
   when the buffers are destructed. The destructor causes a blocking buffer
   transfer from device to host and N-way buffering requires us to not block
   here (because we need to queue more kernels). So we only want this transfer
   to occur at the end of overall execution, not at the end of each individual
   kernel execution.
*/
void SimplePow(std::unique_ptr<queue> &q, buffer<cl_float, 1> &buffer_a,
               buffer<cl_float, 1> &buffer_b, event &e) {
  // Submit to the queue and execute the kernel
  e = q->submit([&](handler &h) {
    // Get kernel access to the buffers
    auto accessor_a = buffer_a.get_access<access::mode::read>(h);
    auto accessor_b = buffer_b.get_access<access::mode::discard_read_write>(h);

    const int num = kSize;
    const int p = kPow - 1;  // Assumes pow >= 2;
    assert(kPow >= 2);

    h.single_task<class SimpleVpow>([=]() [[intel::kernel_args_restrict]] {
      for (int j = 0; j < p; j++) {
        if (j == 0) {
          for (int i = 0; i < num; i++) {
            accessor_b[i] = accessor_a[i] * accessor_a[i];
          }
        } else {
          for (int i = 0; i < num; i++) {
            accessor_b[i] = accessor_b[i] * accessor_a[i];
          }
        }
      }
    });
  });

  event update_host_event;
  update_host_event = q->submit([&](handler &h) {
    auto accessor_b = buffer_b.get_access<access::mode::read>(h);

    /*
      Explicitly instruct the SYCL runtime to copy the kernel's output buffer
      back to the host upon kernel completion. This is not required for
      functionality since the buffer access in ProcessOutput() also implicitly
      instructs the runtime to copy the data back. But it should be noted that
      this buffer access blocks ProcessOutput() until the kernel is complete
      and the data is copied. In contrast, update_host() instructs the runtime
      to perform the copy earlier. This allows ProcessOutput() to optionally
      perform more useful work *before* making the blocking buffer access. Said
      another way, this allows ProcessOutput() to potentially perform more work
      in parallel with the runtime's copy operation.
    */
    h.update_host(accessor_b);
  });

  q->throw_asynchronous();
}

// Returns kernel execution time for a given SYCL event from a queue.
cl_ulong SyclGetExecTimeNs(event e) {
  cl_ulong start_time =
      e.get_profiling_info<info::event_profiling::command_start>();
  cl_ulong end_time =
      e.get_profiling_info<info::event_profiling::command_end>();
  return (end_time - start_time);
}

// Local pow function for verifying results
cl_float MyPow(cl_float input, int pow) {
  return (pow == 0) ? 1 : input * MyPow(input, pow - 1);
}

/*  Compares kernel output against expected output.
    Grabs kernel output data from its SYCL buffer. Reading from this buffer is a
   blocking operation that will block on the kernel completing. Grabs expected
   output from a host-side copy of the input data. A copy is used to allow for
   parallel generation of the input data for the next execution. Queries and
   records execution time of the kernel that just completed. This is a natural
   place to do this because ProcessOutput() is blocked on kernel completion.
*/
void ProcessOutput(buffer<cl_float, 1> &output_buf,
                   std::vector<cl_float> &input_copy, int exec_number, event e,
                   cl_ulong &total_kernel_time_per_slot) {
  auto output_buf_acc = output_buf.get_access<access::mode::read>();
  int num_errors = 0;
  int num_errors_to_print = 10;
  /*  The use of update_host() in the kernel function allows for additional
     host-side operations to be performed here, in parallel with the buffer copy
     operation from device to host, before the blocking access to the output
     buffer is made via output_buf_acc[]. To be clear, no real operations are
     done here and this is just a note that this is the place
      where you *could* do it. */
  for (int i = 0; i < kSize; i++) {
    if ((num_errors < num_errors_to_print) &&
        (MyPow(input_copy.data()[i], kPow) != output_buf_acc[i])) {
      if (num_errors == 0) {
        pass = false;
        std::cout << "Verification failed on kernel execution # " << exec_number
                  << ". Showing up to " << num_errors_to_print << " mismatches.\n";
      }
      std::cout << "Verification failed on kernel execution # " << exec_number
                << ", at element " << i << ". Expected " << std::fixed
                << std::setprecision(16) << MyPow(input_copy.data()[i], kPow)
                << " but got " << output_buf_acc[i] << "\n";
      num_errors++;
    }
  }

  // At this point we know the kernel has completed, so can query the profiling
  // data.
  total_kernel_time_per_slot += SyclGetExecTimeNs(e);
}

/*
    Generates input data for the next kernel execution.
    Writes the data into the associated SYCL buffer. The write will block until
   the previous kernel execution, that is using this buffer, completes. Writes a
   copy of the data into a host-side buffer that will later be used by
   ProcessOutput().
*/
void ProcessInput(buffer<cl_float, 1> &buf, std::vector<cl_float> &copy) {
  auto buf_acc = buf.get_access<
      access::mode::discard_write>();  // We are generating completely new input
                                       // data, so can use discard_write() here
                                       // to indicate we don't care about the
                                       // SYCL buffer's current contents.
  auto seed =
      std::chrono::system_clock::now().time_since_epoch().count();  // seed
  std::default_random_engine dre(seed);                             // engine
  std::uniform_real_distribution<float> di(1.0f,
                                           2.0f);  // Values between 1 and 2

  float start_val =
      di(dre);  // Randomly generate a start value and increment from there.
                // Compared to randomly generating every value, this is done to
                // speed up this function a bit.

  for (int i = 0; i < kSize; i++) {
    buf_acc[i] = start_val;
    copy.data()[i] = start_val;
    start_val++;
  }
}

auto exception_handler = [](exception_list exceptions) {
  for (std::exception_ptr const &e : exceptions) {
    try {
      std::rethrow_exception(e);
    } catch (exception const &e) {
      std::cout << "Caught asynchronous SYCL exception:\n";
      std::terminate();
    }
  }
};

int main() {
  try {
// Create queue, get platform and device
#if defined(FPGA_EMULATOR)
    intel::fpga_emulator_selector device_selector;
    std::cout << "\nEmulator output does not demonstrate true hardware "
                 "performance. The design may need to run on actual hardware "
                 "to observe the performance benefit of the optimization "
                 "exemplified in this tutorial.\n\n";
#elif defined(CPU_HOST)
    host_selector device_selector;
    std::cout << "\nCPU Host target does not accurately measure kernel execution "
                 "time. The design must run on actual hardware to observe the "
                 "benefit of the optimization exemplified in this tutorial.\n\n";
#else
    intel::fpga_selector device_selector;
#endif

    auto property_list =
        cl::sycl::property_list{property::queue::enable_profiling()};
    std::unique_ptr<queue> q;

    try {
      // queue q(device_selector, property_list);
      q.reset(new queue(device_selector, exception_handler, property_list));
    } catch (exception const &e) {
      std::cout << "Caught a synchronous SYCL exception:\n"
                << e.what() << "\n";
      std::cout << "If you are targeting an FPGA hardware, please "
                   "ensure that your system is plugged to an FPGA board that "
                   "is set up correctly.\n";
      std::cout << "If you are targeting the FPGA emulator, compile with "
                   "-DFPGA_EMULATOR.\n";
      std::cout
          << "If you are targeting a CPU host device, compile with -DCPU_HOST.\n";
      return 1;
    }

    platform platform = q->get_context().get_platform();
    device device = q->get_device();
    std::cout << "Platform name: "
              << platform.get_info<info::platform::name>().c_str() << "\n";
    std::cout << "Device name: "
              << device.get_info<info::device::name>().c_str() << "\n\n\n";

    std::cout << "Executing kernel " << kTimes << " times in each round.\n\n";

    // Create a vector to store the input/output SYCL buffers
    std::vector<buffer<cl_float, 1>> input_buf;
    std::vector<buffer<cl_float, 1>> output_buf;

    std::vector<cl_float> input_buf_copy
        [2 * kLocalN];  // For every execution slot, we need 2 host-side buffers
                        // to store copies of the input data. One is used to
                        // verify the previous kernel's output. The other stores
                        // the new data for the next kernel execution.
    event sycl_events[kLocalN];  // SYCL events for each kernel launch.
    cl_ulong total_kernel_time_per_slot[kLocalN];  // In nanoseconds. Total
                                                   // execution time of kernels
                                                   // in a given slot.
    cl_ulong total_kernel_time = 0;  // Total execution time of all kernels.
    std::thread t_process_output[kLocalN];  // Threads to process the output
                                            // from each kernel
    std::thread t_process_input[kLocalN];   // Threads to process the input data
                                            // for the next kernel
    int N;  // Demonstrate with 1-way buffering first, then N-way buffering.
    bool st = true;  // st = "single threaded". Used to enable multi-threading
                     // in subsequent runs.

    // Allocate vectors to store the host-side copies of the input data
    for (int i = 0; i < 2 * kLocalN; i++) {
      input_buf_copy[i] = std::vector<cl_float>(kSize);
    }

    // Create and allocate the SYCL buffers
    for (int i = 0; i < kLocalN; i++) {
      input_buf.push_back(buffer<cl_float, 1>(range<1>(kSize)));
      output_buf.push_back(buffer<cl_float, 1>(range<1>(kSize)));
    }

    /*
      Main loop.
      This loop runs multiple times to demonstrate how performance can be
      improved by increasing the number of buffers as well as multi-threading
      the host-side operations. The first iteration is a base run, demonstrating
      the performance with none of these optimizations (ie. 1-way buffering,
      single-threaded).
    */
    for (int i = 0; i < kNumRuns; i++) {
      for (int i = 0; i < kLocalN; i++) {
        total_kernel_time_per_slot[i] = 0;  // Initialize timers to zero.
      }

      switch (i) {
        case 0: {
          std::cout << "*** Beginning execution, 1-way buffering, "
                       "single-threaded host operations\n";
          N = 1;
          st = true;
          break;
        }
        case 1: {
          std::cout << "*** Beginning execution, 1-way buffering, "
                       "multi-threaded host operations.\n";
          N = 1;
          st = false;
          break;
        }
        case 2: {
          std::cout << "*** Beginning execution, 2-way buffering, "
                       "multi-threaded host operationss\n";
          N = 2;
          st = false;
          break;
        }
        case 3: {
          std::cout << "*** Beginning execution, N=" << kLocalN
                    << "-way buffering, multi-threaded host operations\n";
          N = kLocalN;
          st = false;
          break;
        }
        default:
          std::cout << "*** Beginning execution.\n";
      }

      high_resolution_clock::time_point t1 =
          high_resolution_clock::now();  // Start the timer. This will include
                                         // the time to process the input data
                                         // for the first N kernel executions.

      // Process the input data for first N kernel executions. For
      // multi-threaded runs, this is done in parallel.
      for (int i = 0; i < N; i++) {
        t_process_input[i] = std::thread(ProcessInput, std::ref(input_buf[i]),
                                         std::ref(input_buf_copy[i]));
        if (st) {
          t_process_input[i].join();
        }
      }

      /*
        It's useful to think of the kernel execution space as having N slots.
        Conceptually, the slots are executed chronologically sequentially on the
        device (i.e. slot 0 to N-1). Each slot has its own buffering on both the
        host and device. Before launching a kernel in a given slot, we must
        process output data from the previous execution that occurred in that
        slot and process new input data for the upcoming new execution in that
        slot.
      */
      for (int i = 0; i < kTimes; i++) {
        // The current slot is i%N.
        // Before each kernel launch, the ProcessOutput() must have completed
        // for the last execution in this slot. The ProcessInput() must also
        // have completed for the upcoming new execution for this slot. Block on
        // both of these.
        if (!st) {
          if (i >= N) {  // ProcessOutput() is only relevant after the first N
                         // kernels have been launched.
            t_process_output[i % N].join();
          }

          t_process_input[i % N].join();
        }

        // Launch the kernel. This is non-blocking with respect to main().
        if (i % 10 == 0) {
          std::cout << "Launching kernel #" << i << "\n";
        }  // Only print every few iterations, just to limit the prints.

        SimplePow(q, input_buf[i % N], output_buf[i % N], sycl_events[i % N]);

        // Immediately launch threads for the ProcessOutput() and
        // ProcessInput() for *this* slot. These are non-blocking with respect
        // to main(), but they will individually be blocked until the
        // corresponding kernel execution is complete. The ProcessOutput()
        // compares the kernel output data against the input data. But
        // ProcessInput() will be overwriting that input data in parallel.
        // Therefore ProcessOutput() must compare against an older copy of the
        // data. We ping-pong between host-side copies of the input data.
        t_process_output[i % N] = std::thread(
            ProcessOutput, std::ref(output_buf[i % N]),
            std::ref(input_buf_copy[i % (2 * N)]), i, sycl_events[i % N],
            std::ref(total_kernel_time_per_slot[i % N]));
        if (st) {
          t_process_output[i % N].join();
        }  // For single-threaded runs, force single-threaded operation by
           // blocking here immediately.

        if (i <
            kTimes - N) {  // For the final N kernel launches, no need to process
                          // input data because there will be no more launches.
          t_process_input[i % N] = std::thread(
              ProcessInput, std::ref(input_buf[i % N]),
              std::ref(
                  input_buf_copy[(i + N) %
                                 (2 *
                                  N)]));  // The indexes for the input_buf_copy
                                          // used by ProcessOutput() and
                                          // ProcessInput() are spaced N apart.
          if (st) {
            t_process_input[i % N].join();
          }
        }
      }

      // Wait for the final N threads to finish and add up the overall kernel
      // execution time.
      total_kernel_time = 0;
      for (int i = 0; i < N; i++) {
        if (!st) {
          t_process_output[i].join();
        }
        total_kernel_time += total_kernel_time_per_slot[i];
      }

      high_resolution_clock::time_point t2 =
          high_resolution_clock::now();  // Stop the timer.

      duration<double> time_span = duration_cast<duration<double>>(t2 - t1);

      std::cout << "\nOverall execution time "
                << ((i == kNumRuns - 1) ? ("with N-way buffering ") : "")
                << "= " << (unsigned)(time_span.count() * 1000) << " ms\n";
      std::cout << "Total kernel-only execution time "
                << ((i == kNumRuns - 1) ? ("with N-way buffering ") : "")
                << "= " << (unsigned)(total_kernel_time / 1000000) << " ms\n";
      std::cout << "Throughput = " << std::setprecision(8)
                << (float)kSize * (float)kTimes * (float)sizeof(cl_float) /
                       (float)time_span.count() / 1000000
                << " MB/s\n\n\n";
    }
    if (pass) {
      std::cout << "Verification PASSED\n";
    } else {
      std::cout << "Verification FAILED\n";
      return 1;
    }
  } catch (exception const &e) {
    std::cout << "Caught synchronous SYCL exception:\n" << e.what() << "\n";
    return 1;
  }
  return 0;
}
