# (
cmake_minimum_required(VERSION 3.2)
project (rsed)
find_package(FLEX)
find_package(BISON)

FLEX_TARGET(RSED scanner.lxx ${CMAKE_CURRENT_BINARY_DIR}/scanner.cpp COMPILE_FLAGS "--c++ --yyclass=Scanner")
BISON_TARGET(RSED grammar.ypp  ${CMAKE_CURRENT_BINARY_DIR}/grammar.tab.cpp COMPILE_FLAGS "--debug -d")
SET( CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -I${CMAKE_CURRENT_SOURCE_DIR} -std=c++11" )

set (headers 
AST.h		LineBuffer.h	RegEx.h		StringRef.h	rsed.h
Interpreter.h	Parser.h	Scanner.h	Symbol.h	BuiltinCalls.h
)

add_executable(rsed
AST.cpp			Parser.cpp		grammar.tab.cpp
BuiltinCalls.cpp	RegEx.cpp		main.cpp
Interpreter.cpp		StringRef.cpp		scanner.cpp
LineBuffer.cpp		Symbol.cpp 
${FLEX_RSED_OUTPUTS} ${BISON_RSED_OUTPUTS} 
${headers}
		     ) 
# )

################################################################################
# GFlags
################################################################################
set(gflags_BUILD_STATIC_LIBS ON)
add_subdirectory(gflags)
target_link_libraries(rsed gflags-static)
include_directories("${gflags_BINARY_DIR}/include")

