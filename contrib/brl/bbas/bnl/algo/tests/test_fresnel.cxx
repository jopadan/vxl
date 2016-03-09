#include <testlib/testlib_test.h>
#include <bnl/algo/bnl_fresnel.h>
#include <iostream>
#include <vcl_compiler.h>
#include <iostream>
#include <cmath>


//: Compare the values of Fresnel integral (cosin and sin) computed
// against those computed using Matlab.
MAIN( test_fresnel )
{
  START ("Fresnel Integral");

  std::cout << "Test Fresnel integral:" << std::endl;
  double tol = 1e-8;
  double x[] = {
   -1.000000000000000e+030,
   -1.000000000000000e+010,
   -1.000000000000000e+005,
   -1.000000000000000e+002,
   -1.000000000000000e+001,
   -5.000000000000000e+000,
   -2.000000000000000e+000,
   -1.000000000000000e+000,
   -5.000000000000000e-001,
   -1.000000000000000e-001,
   -1.000000000000000e-003,
   -1.000000000000000e-005,
   -1.000000000000000e-010,
                         0,
    1.000000000000000e+030,
    1.000000000000000e+010,
    1.000000000000000e+005,
    1.000000000000000e+002,
    1.000000000000000e+001,
    5.000000000000000e+000,
    2.000000000000000e+000,
    1.000000000000000e+000,
    5.000000000000000e-001,
    1.000000000000000e-001,
    1.000000000000000e-003,
    1.000000000000000e-005,
    1.000000000000000e-010
  };
  // Fresnel integrals of x obtained from Matlab
  double matlab_fresnel_cos_x[] = {
   -5.000000000000000e-001,
   -5.000000000000000e-001,
   -4.999999999999999e-001,
   -4.999998986788179e-001,
   -4.998986942055157e-001,
   -5.636311887040122e-001,
   -4.882534060753408e-001,
   -7.798934003768228e-001,
   -4.923442258714464e-001,
   -9.999753262708507e-002,
   -9.999999999997533e-004,
   -1.000000000000000e-005,
   -1.000000000000000e-010,
                         0,
    5.000000000000000e-001,
    5.000000000000000e-001,
    4.999999999999999e-001,
    4.999998986788179e-001,
    4.998986942055157e-001,
    5.636311887040122e-001,
    4.882534060753408e-001,
    7.798934003768228e-001,
    4.923442258714464e-001,
    9.999753262708507e-002,
    9.999999999997533e-004,
    1.000000000000000e-005,
    1.000000000000000e-010

  };

  double matlab_fresnel_sin_x[] = {
   -5.000000000000000e-001,
   -4.999999999681690e-001,
   -4.999968169011382e-001,
   -4.968169011478376e-001,
   -4.681699785848822e-001,
   -4.991913819171169e-001,
   -3.434156783636982e-001,
   -4.382591473903548e-001,
   -6.473243285999929e-002,
   -5.235895476122106e-004,
   -5.235987755982066e-010,
   -5.235987755982989e-016,
   -5.235987755982989e-031,
                         0,
    5.000000000000000e-001,
    4.999999999681690e-001,
    4.999968169011382e-001,
    4.968169011478376e-001,
    4.681699785848822e-001,
    4.991913819171169e-001,
    3.434156783636982e-001,
    4.382591473903548e-001,
    6.473243285999929e-002,
    5.235895476122106e-004,
    5.235987755982066e-010,
    5.235987755982989e-016,
    5.235987755982989e-031,
  };

  double fs[27];
  double fc[27];
  // compute fresnel integrals
  for (int i = 0; i < 27; ++i)
  {
    bnl_fresnel_integral(x[i], &fc[i], &fs[i]);
  }

  for (int i = 0; i < 27; ++i)
  {
    std::cout << "x = " << x[i] << std::endl;
    TEST_NEAR("Test fresnel cosine", fc[i], matlab_fresnel_cos_x[i], tol);
    TEST_NEAR("Test fresnel sine", fs[i], matlab_fresnel_sin_x[i], tol);
  }

  SUMMARY();
}
