add_executable(ytdlp main.cpp)

target_link_libraries(ytdlp ${PYTHON_LIB} dl m)
target_include_directories(ytdlp PRIVATE ${PYTHON_INCLUDE})

target_sources(Shasha PRIVATE ytdlp.cpp)
target_link_libraries(Shasha ${PYTHON_LIB} dl m)

# vim: sw=8 noet
