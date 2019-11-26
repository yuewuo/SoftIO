cmake_minimum_required(VERSION 3.7)
project(SoftF103-PC C CXX ASM)

set(CMAKE_BUILD_TYPE Debug)
# set(CMAKE_BUILD_TYPE Release)
set(CMAKE_CXX_STANDARD 11)

set(CMAKE_CXX_FLAGS_DEBUG "-g -Wall -Wextra -Wshadow -O2 -pg")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -funroll-loops -Wall -Wextra -Wshadow")

macro (Link _name)
	if (TARGET ${_name})
		if (APPLE) 
			target_link_libraries(${_name} "-framework CoreFoundation")
			target_link_libraries(${_name} "-framework IOKit")
			set_target_properties(${_name} PROPERTIES LINK_FLAGS "-Wl,-F/Library/Frameworks")
		elseif (WIN32)
			target_link_libraries(${_name} setupapi hid uuid)
		endif()
		target_link_libraries(${_name} pthread)
	endif()
endmacro()

include_directories(${PROJECT_SOURCE_DIR}/..)

file(GLOB serial_cc . ../serial/*.cc)
add_library(serial STATIC ${serial_cc})
Link(serial)
link_libraries(serial)

set(CMAKE_INSTALL_PREFIX ${PROJECT_BINARY_DIR})

file(GLOB SRC_FILES "*.cpp")
foreach (cpp ${SRC_FILES})
	string(REGEX REPLACE ".+/(.+)\\..*" "\\1" cppname ${cpp})
	add_executable(${cppname} ${cpp})
endforeach(cpp)
