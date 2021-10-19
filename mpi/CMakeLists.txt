cmake_minimum_required( VERSION 2.8.8 )
project (rockit)

include_directories(sdk/include)
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
set(ROCKIT_DEP_COMMON_LIBS
    ${CMAKE_CURRENT_LIST_DIR}/../lib/lib64/librockit.so
)
set(ROCKIT_FILE_LIBS
    ${ROCKIT_DEP_COMMON_LIBS}
    ${CMAKE_CURRENT_LIST_DIR}/sdk/lib64/libgraphic_lsf.so
)
set(ROCKIT_DUMPSYS_FILE ${CMAKE_CURRENT_LIST_DIR}/example/bin/bin64/dumpsys)
elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
set(ROCKIT_DEP_COMMON_LIBS
    ${CMAKE_CURRENT_LIST_DIR}/../lib/lib32/librockit.so
)
set(ROCKIT_FILE_LIBS
    ${ROCKIT_DEP_COMMON_LIBS}
)
set(ROCKIT_DUMPSYS_FILE ${CMAKE_CURRENT_LIST_DIR}/example/bin/bin32/dumpsys)
endif()

add_subdirectory(sdk)
add_subdirectory(example)
