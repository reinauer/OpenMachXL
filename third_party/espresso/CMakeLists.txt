add_library(machxl_espresso STATIC
  utility/cpu_time.c utility/prtime.c utility/strsav.c
  espresso/cofactor.c
  espresso/cols.c
  espresso/compl.c
  espresso/contain.c
  espresso/cubestr.c
  espresso/cvrin.c
  espresso/cvrm.c
  espresso/cvrmisc.c
  espresso/cvrout.c
  espresso/dominate.c
  espresso/equiv.c
  espresso/espresso.c
  espresso/essen.c
  espresso/exact.c
  espresso/expand.c
  espresso/gasp.c
  espresso/gimpel.c
  espresso/globals.c
  espresso/hack.c
  espresso/indep.c
  espresso/irred.c
  espresso/map.c
  espresso/matrix.c
  espresso/mincov.c
  espresso/opo.c
  espresso/pair.c
  espresso/part.c
  espresso/primes.c
  espresso/reduce.c
  espresso/rows.c
  espresso/set.c
  espresso/setc.c
  espresso/sharp.c
  espresso/sminterf.c
  espresso/solution.c
  espresso/sparse.c
  espresso/unate.c
  espresso/verify.c
  espresso/signature.c
  espresso/signature_exact.c
  espresso/util_signature.c
  espresso/black_white.c
  espresso/canonical.c
  espresso/essentiality.c
  espresso/sigma.c)
target_include_directories(machxl_espresso PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/espresso ${CMAKE_CURRENT_SOURCE_DIR}/utility)
target_compile_definitions(machxl_espresso PUBLIC IBMPC)
set_target_properties(machxl_espresso PROPERTIES C_STANDARD 99 C_STANDARD_REQUIRED ON)
