cmake_minimum_required( VERSION 2.8.8 )
project (rockit)

option(USE_ROCKIT_TGI  "enable rockit tgi" ON)
if (${USE_ROCKIT_TGI})
    message("Build rockit tgi")
    add_subdirectory(tgi)
endif()

option(USE_ROCKIT_MPI  "enable rockit mpi" ON)
if (${USE_ROCKIT_MPI})
    message("Build rockit mpi")
    add_subdirectory(mpi)
endif()
