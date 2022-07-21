cmake_minimum_required(VERSION 3.15)
project(JeffMcClintock)

add_subdirectory(ADSR)
add_subdirectory(Crusher)
add_subdirectory(DelayAnalog)
add_subdirectory(DrawingTest)
add_subdirectory(OscillatorHD)
add_subdirectory(SignalLogger)
add_subdirectory(PeakFollower)
add_subdirectory(Dynamics)
add_subdirectory(Loudness)
add_subdirectory(WaveformScroller)

if(WIN32)
# don't build on macOS yet
	add_subdirectory(Wrapper_VST2)
#	add_subdirectory(Wrapper_VST3)
endif(WIN32)
