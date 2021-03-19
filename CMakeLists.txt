CMAKE_MINIMUM_REQUIRED(VERSION 3.0)

PROJECT(minimg C CXX)

ADD_DEFINITIONS(-std=c++11)
SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -W")
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -W")

#SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pie -fPIE")
#SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pie -fPIE")

FILE(GLOB_RECURSE samples ${CMAKE_CURRENT_SOURCE_DIR}/sample/sample*.cpp)
FILE(GLOB src ${CMAKE_CURRENT_SOURCE_DIR}/src/minimg.cpp)

INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/inc)
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/inc)
link_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/3rdparty//lib)

#SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g  -fsanitize=address -fno-omit-frame-pointer -fsanitize=leak")

FOREACH(sample ${samples})
	GET_FILENAME_COMPONENT(sample_name ${sample}  NAME_WE)
	ADD_EXECUTABLE(${sample_name} ${sample} ${src})
	TARGET_LINK_LIBRARIES(${sample_name} libjpeg.a libpng.a libz.a)
ENDFOREACH()
