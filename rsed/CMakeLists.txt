# (
cmake_minimum_required(VERSION 3.2)
project (rsed)
find_package(FLEX)
find_package(BISON)

FLEX_TARGET(RSED scanner.lxx ${CMAKE_CURRENT_BINARY_DIR}/scanner.cpp COMPILE_FLAGS "--c++ --yyclass=Scanner")
BISON_TARGET(RSED grammar.ypp  ${CMAKE_CURRENT_BINARY_DIR}/grammar.tab.cpp COMPILE_FLAGS "--debug -d")
SET( CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -I${CMAKE_CURRENT_SOURCE_DIR} -std=c++11" )
SET( CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libstdc++" )

set (headers
AST.h		Interpreter.h	Parser.h	StringRef.h
BuiltinCalls.h	LineBuffer.h	RegEx.h		Symbol.h
EvalState.h	Optimize.h	Scanner.h	rsed.h
ASTWalk.h
)

add_executable(rsed
AST.cpp			Optimize.cpp		Symbol.cpp
BuiltinCalls.cpp	Parser.cpp		main.cpp
Interpreter.cpp		RegEx.cpp		ScannerSupport.cpp
LineBuffer.cpp		StringRef.cpp
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

