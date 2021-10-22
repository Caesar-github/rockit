cmake_minimum_required( VERSION 2.8.8 )

set(PLAYER_TEST_SRC
    case_player_test.cpp
    case_player_video.cpp	
)
add_executable(case_player_test ${PLAYER_TEST_SRC})
target_link_libraries(case_player_test ${ROCKIT_DEP_COMMON_LIBS})
install(TARGETS case_player_test RUNTIME DESTINATION "bin/")