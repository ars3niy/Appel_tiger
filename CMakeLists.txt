project(compiler)

find_package(FLEX)
find_package(BISON)

set(CMAKE_CXX_FLAGS -fpermissive)

BISON_TARGET(TigerParser Tiger.y ${CMAKE_CURRENT_BINARY_DIR}/parser.cpp)
FLEX_TARGET(TigerScanner Tiger.lex ${CMAKE_CURRENT_BINARY_DIR}/lexer.cpp)
ADD_FLEX_BISON_DEPENDENCY(TigerScanner TigerParser)
include_directories(${CMAKE_SOURCE_DIR})
include_directories(${CMAKE_CURRENT_BINARY_DIR})

add_executable(compiler ${FLEX_TigerScanner_OUTPUTS} ${BISON_TigerParser_OUTPUTS} errormsg.cpp main.cpp)
target_link_libraries(compiler "fl")

install(TARGETS compiler RUNTIME DESTINATION bin)
