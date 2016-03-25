project(compiler)

add_executable(compiler main.cpp)

install(TARGETS compiler RUNTIME DESTINATION bin)
