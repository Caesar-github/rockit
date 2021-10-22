cmake_minimum_required( VERSION 2.8.8 )
add_definitions(-fno-rtti)

add_compile_options(-std=c++11)
add_definitions(-std=c++11 -Wno-attributes -Wno-deprecated-declarations -DANDROID_STL=c++_shared)

set(RK_GTEST_SYS_SRC
    sys_bind_test.cpp
)

set(RK_SYS_MAIN_SRC
    rk_sys_gtest.cpp
    ${RK_GTEST_SYS_SRC}
)

#--------------------------
# rk_mpi_vpss_test
#--------------------------
add_executable(rk_sys_gtest ${RK_SYS_MAIN_SRC} ${RK_MPI_TEST_COMMON_SRC})
target_link_libraries(rk_sys_gtest ${RK_DEP_COMMON_LIBS} ${LIB_STATIC_GTEST})
install(TARGETS rk_sys_gtest RUNTIME DESTINATION "bin/rockit")

