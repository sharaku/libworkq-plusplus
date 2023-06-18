set(${PROJECT_NAME}_EXAMPLE_WORKQ++ example-workq++)
add_executable(${${PROJECT_NAME}_EXAMPLE_WORKQ++} example-workq++.cpp)
target_include_directories(${${PROJECT_NAME}_EXAMPLE_WORKQ++}
	PRIVATE
		../include
)

set(${PROJECT_NAME}_EXAMPLE_COROUTINE example-coroutine)
add_executable(${${PROJECT_NAME}_EXAMPLE_COROUTINE} example-coroutine.cpp)
target_include_directories(${${PROJECT_NAME}_EXAMPLE_COROUTINE}
	PRIVATE
		../include
)

set(${PROJECT_NAME}_EXAMPLE_INTERVALTIMER example-intervaltimer)
add_executable(${${PROJECT_NAME}_EXAMPLE_INTERVALTIMER} example-intervaltimer.cpp)
target_include_directories(${${PROJECT_NAME}_EXAMPLE_INTERVALTIMER}
	PRIVATE
		../include
)
