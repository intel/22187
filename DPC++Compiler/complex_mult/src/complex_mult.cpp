//==============================================================
// Copyright � 2019 Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#include <CL/sycl.hpp>
#include <iomanip>
#include <vector>

#include "Complex.hpp"

using namespace cl::sycl;
using namespace std;

// Number of complex numbers passing to the DPC++ code
static const int NumElements = 100;

// in_vect1 and in_vect2 are the vectors with NumElements complex nubers and are inputs to
// the parallel function
void DpcppParallel(queue &q, std::vector<Complex2> &in_vect1,
                   std::vector<Complex2> &in_vect2, std::vector<Complex2> &out_vect) {
  // Setup input buffers
  buffer<Complex2, 1> bufin_vect1(in_vect1.data(), range<1>(NumElements));
  buffer<Complex2, 1> bufin_vect2(in_vect2.data(), range<1>(NumElements));

  // Setup Output buffers
  buffer<Complex2, 1> bufout_vect(out_vect.data(), range<1>(NumElements));

  std::cout << "Target Device: "
            << q.get_device().get_info<info::device::name>() << "\n";
  // Submit Command group function object to the queue
  q.submit([&](handler &h) {
    // Accessors set as read mode
    auto V1 = bufin_vect1.get_access<access::mode::read>(h);
    auto V2 = bufin_vect2.get_access<access::mode::read>(h);
    // Accessor set to Write mode
    auto V3 = bufout_vect.get_access<access::mode::write>(h);
    h.parallel_for(range<1>(NumElements), [=](id<1> i) {
      // Kernel code. Call the complex_mul function here.
      V3[i] = V1[i].complex_mul(V2[i]);
    });
  });
  q.wait_and_throw();
}
void DpcppScalar(std::vector<Complex2> &in_vect1, std::vector<Complex2> &in_vect2,
                 std::vector<Complex2> &out_vect) {
  for (int i = 0; i < NumElements; i++) {
    out_vect[i] = in_vect1[i].complex_mul(in_vect2[i]);
  }
}
// Compare the results of the two output vectors from parallel and scalar. They
// should be equal
int Compare(std::vector<Complex2> &v1, std::vector<Complex2> &v2) {
  int ret_code = 1;
  for (int i = 0; i < NumElements; i++) {
    if (v1[i] != v2[i]) {
      ret_code = -1;
      break;
    }
  }
  return ret_code;
}
int main() {
  // Declare your Input and Output vectors of the Complex2 class
  vector<Complex2> input_vect1;
  vector<Complex2> input_vect2;
  vector<Complex2> out_vect_parallel;
  vector<Complex2> out_vect_scalar;

  for (int i = 0; i < NumElements; i++) {
    input_vect1.push_back(Complex2(i + 2, i + 4));
    input_vect2.push_back(Complex2(i + 4, i + 6));
    out_vect_parallel.push_back(Complex2(0, 0));
    out_vect_scalar.push_back(Complex2(0, 0));
  }

  // this exception handler with catch async exceptions
  auto exception_handler = [&](cl::sycl::exception_list eList) {
    for (std::exception_ptr const &e : eList) {
      try {
        std::rethrow_exception(e);
      } catch (cl::sycl::exception const &e) {
        std::cout << "Failure" << std::endl;
        std::terminate();
      }
    }
  };

  // Initialize your Input and Output Vectors. Inputs are initialized as below.
  // Outputs are initialized with 0
  try {
    // queue constructor passed exception handler
    queue q(default_selector{}, exception_handler);
    // Call the DpcppParallel with the required inputs and outputs
    DpcppParallel(q, input_vect1, input_vect2, out_vect_parallel);
  } catch (...) {
    // some other exception detected
    std::cout << "Failure" << std::endl;
    std::terminate();
  }

  cout << "****************************************Multiplying Complex numbers "
          "in Parallel********************************************************"
       << std::endl;
  // Print the outputs of the Parallel function
  for (int i = 0; i < NumElements; i++) {
    cout << out_vect_parallel[i] << ' ';
    if (i == NumElements - 1) {
      cout << "\n\n";
    }
  }
  cout << "****************************************Multiplying Complex numbers "
          "in Serial***********************************************************"
       << std::endl;
  // Call the DpcppScalar function with the required input and outputs
  DpcppScalar(input_vect1, input_vect2, out_vect_scalar);
  for (auto it = out_vect_scalar.begin(); it != out_vect_scalar.end(); it++) {
    cout << *it << ' ';
    if (it == out_vect_scalar.end() - 1) {
      cout << "\n\n";
    }
  }

  // Compare the outputs from the parallel and the scalar functions. They should
  // be equal

  int ret_code = Compare(out_vect_parallel, out_vect_scalar);
  if (ret_code == 1) {
    cout << "********************************************Success. Results are "
            "matched******************************"
         << "\n";
  } else
    cout << "*********************************************Failed. Results are "
            "not matched**************************"
         << "\n";

  return 0;
}
