add_executable(stream-codec
	../../include/musicat/stream_codec.h
	../../src/musicat/stream_codec.cpp
	main.cpp)

target_link_libraries(stream-codec ogg)
target_include_directories(stream-codec PRIVATE
	../../include
	../../libs/libopusenc/include
	/usr/include/opus)

# vim: sw=8 noet
