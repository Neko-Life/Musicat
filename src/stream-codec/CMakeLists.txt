add_executable(stream-codec
	../../include/musicat/stream_codec.h
	../../include/musicat/decoder.h
	../../src/musicat/stream_codec.cpp
	../../src/musicat/decoder.cpp
	main.cpp)

target_link_libraries(stream-codec 
	avformat
	avfilter
	avcodec
	avutil
	swresample)

target_include_directories(stream-codec PRIVATE
	../../include)

# vim: sw=8 noet
