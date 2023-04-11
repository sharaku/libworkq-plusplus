cmake_minimum_required(VERSION 3.14)
project(test_workq++)
set(CMAKE_CXX_STANDARD 11)

include(FetchContent)
FetchContent_Declare(
  googletest
  # Specify the commit you depend on and update it regularly.
  URL https://github.com/google/googletest/archive/609281088cfefc76f9d0ce82e1ff6c30cc3591e5.zip
)

# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)


add_executable(test_workq++
	test_workque_fifo_internal___.cpp
	test_workque_timer_list_internal___.cpp
	test_workque_internal___.cpp

	test_event.cpp
	test_workque.cpp
	test_simple_workque.cpp
)

target_link_libraries(test_workq++ gtest_main)
