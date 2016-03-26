project(compiler)

find_package(FLEX)
FLEX_TARGET(TigerScanner Tiger.lex ${CMAKE_CURRENT_BINARY_DIR}/lexer.cpp)
include_directories(${CMAKE_SOURCE_DIR})

add_executable(compiler ${FLEX_TigerScanner_OUTPUTS} errormsg.cpp main.cpp)
target_link_libraries(compiler "fl")

install(TARGETS compiler RUNTIME DESTINATION bin)
