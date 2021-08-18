# Configuration for Intel compiler
# release build

SET(CMAKE_C_COMPILER mpiicc)
SET(CMAKE_CXX_COMPILER mpiicpc)
SET(CMAKE_Fortran_COMPILER mpiifort)

# main config
set(FLOW_BUILD_TYPE release)
set(FLOW_CC_FLAGS "-O3 -DNDEBUG")
set(CMAKE_VERBOSE_MAKEFILE on)


# external libraries
set(PETSC_DIR              /usr/local/petsc_3.15.1)
set(BDDCML_ROOT            /usr/local/bddcml_2.6)
set(Armadillo_ROOT_HINT    /usr/local/armadillo_10.5.2)
set(YamlCpp_ROOT_HINT      /usr/local/yamlcpp_0.6.3)


# additional info
set(USE_PYTHON          "yes")
set(PLATFORM_NAME       "linux_x86_64")
