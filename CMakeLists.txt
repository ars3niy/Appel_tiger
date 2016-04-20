project(compiler)
#add_definitions(-DDEBUG)
set(CMAKE_CXX_FLAGS "-std=c++11 -Wall")

find_package(FLEX)
find_package(BISON)

BISON_TARGET(TigerParser Tiger.y ${CMAKE_CURRENT_BINARY_DIR}/parser.cpp)
FLEX_TARGET(TigerScanner Tiger.lex ${CMAKE_CURRENT_BINARY_DIR}/lexer.cpp)
ADD_FLEX_BISON_DEPENDENCY(TigerScanner TigerParser)


include_directories(${CMAKE_SOURCE_DIR})
include_directories(${CMAKE_CURRENT_BINARY_DIR})

add_executable(compiler timer.cpp optimizer.cpp regallocator.cpp flowgraph.cpp assembler.cpp x86_64assembler.cpp syntaxtree.cpp debugprint.cpp ir_transformer.cpp types.cpp x86_64_frame.cpp x86_frame.cpp intermadiate.cpp frame.cpp idmap.cpp translator.cpp declarations.cpp layeredmap.cpp ${FLEX_TigerScanner_OUTPUTS} ${BISON_TigerParser_OUTPUTS} errormsg.cpp main.cpp)
add_library(tigerlibrary STATIC tigerlibrary_x86_64.c)

target_link_libraries(compiler "fl")

install(TARGETS compiler RUNTIME DESTINATION bin)
