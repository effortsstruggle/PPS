cmake_minimum_required(VERSION 3.0) #指定cmake最小版本

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

project(PSS_alpha) #设置项目名称

if(UNIX)
	#linux
	include(CheckCXXCompilerFlag)
	CHECK_CXX_COMPILER_FLAG("-std=c++11" COMPILER_SUPPORTS_CXX11)
	if(COMPILER_SUPPORTS_CXX11)
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
	endif()
endif()

set(ROOT_DIR ${PROJECT_SOURCE_DIR}) #设置工程根目录到ROOT_DIR中

if(UNIX)
	#linux
	if(CMAKE_BUILD_TYPE MATCHES "debug")
		set(CMAKE_C_FLAGS  "${CMAKE_C_FLAGS} -fPIC")
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O0 -D_DEBUG") #设置c编译选项
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -g -ggdb -fPIC")#设置c++编译选项
	else()
		set(CMAKE_BUILD_TYPE "Release")
		set(CMAKE_C_FLAGS  "${CMAKE_C_FLAGS} -fPIC")
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2 -DNDEBUG")
	endif()
	
	set(LDFLAGS "$ENV{LDFLAGS} -stdlib=libc++")
	#add_definitions(-Wno-unused-variable -Wno-unused-parameter -Wno-unused-function -Wno-strict-aliasing -Wno-pmf-conversions)
	
	set( BIN_OUTPUT_DIR ${ROOT_DIR}/Build/Linux ) #设置构建目录
else()
	#windows
	if(CMAKE_BUILD_TYPE MATCHES "debug")
		add_definitions(-D_DEBUG)
	else(CMAKE_BUILD_TYPE MATCHES "debug")
		add_definitions(-DNDEBUG)
	endif(CMAKE_BUILD_TYPE MATCHES "debug")
endif()

#add your own sub dirs
add_subdirectory(PSS_ASIO)
add_subdirectory(Module_Logic/Test_Logic)
add_subdirectory(PacketParse_Interface)
add_subdirectory(PSS_ASIO_CLIENT)
#TODO:add other dirs
