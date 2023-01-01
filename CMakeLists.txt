cmake_minimum_required (VERSION 3.13)
if (MSVC)
set (CMAKE_SYSTEM_VERSION 10.0.22000.0)
endif (MSVC)
project (REDappWrapper VERSION ${PROMETHEUS_VERSION} DESCRIPTION "JNI Wrapper for REDapp")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_C_STANDARD 11)
set(CMAKE_VERBOSE_MAKEFILE ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}")

SET(LOWLEVEL_INCLUDE_DIR "error" CACHE STRING "The path to the includes from HSS LowLevel")
SET(THIRD_PARTY_INCLUDE_DIR "error" CACHE STRING "The path to third party include files")
SET(GRID_INCLUDE_DIR "error" CACHE STRING "The path to Grid module include files")
SET(BOOST_INCLUDE_DIR "error" CACHE STRING "The path to boost include files")

if (MSVC)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DNOMINMAX -DPROTOBUF_USE_DLLS -DBOOST_ALL_DYN_LINK -D_CRT_SECURE_NO_WARNINGS /Zc:__cplusplus")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MD -D_AFXDLL")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MDd -D_AFXDLL /W4")
else ()
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D_NO_MFC")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Wall")
endif (MSVC)

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -D_DEBUG -DDEBUG")

set(JAVA_INCLUDE_PATH "JAVA_INCLUDE_PATH-NOTFOUND")
set(JAVA_INCLUDE_PATH2 "JAVA_INCLUDE_PATH2-NOTFOUND")
find_package(JNI2)

if (NOT JNI_FOUND)
    message(FATAL_ERROR "JNI not found")
else()
    message(${JAVA_INCLUDE_PATH})
    message(${JAVA_INCLUDE_PATH2})
endif ()

add_library(REDappWrapper SHARED
    cpp/REDappWrapper.cpp
    include/jvm_wrapper.h
)

if (MSVC)
target_sources(REDappWrapper
    PRIVATE cpp/jvm_wrapper_win.cpp
)
else()
target_sources(REDappWrapper
    PRIVATE cpp/jvm_wrapper_unix.cpp
)
endif()

target_include_directories(REDappWrapper
    PUBLIC ${GRID_INCLUDE_DIR}
    PUBLIC ${JAVA_INCLUDE_PATH}
    PUBLIC ${JAVA_INCLUDE_PATH2}
    PUBLIC ${BOOST_INCLUDE_DIR}
    PUBLIC ${LOWLEVEL_INCLUDE_DIR}
    PUBLIC ${THIRD_PARTY_INCLUDE_DIR}
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
)

set_target_properties(REDappWrapper PROPERTIES VERSION ${CMAKE_PROJECT_VERSION})
set_target_properties(REDappWrapper PROPERTIES SOVERSION ${CMAKE_PROJECT_VERSION_MAJOR})
set_target_properties(REDappWrapper PROPERTIES DEFINE_SYMBOL "DLLEXPORT")

set_target_properties(REDappWrapper PROPERTIES
    PUBLIC_HEADER include/REDappWrapper.h
)

if (MSVC)
target_link_libraries(REDappWrapper PRIVATE delayimp ${JNI_LIBRARIES})
target_link_options(REDappWrapper PRIVATE "/DELAYLOAD:jvm.dll")
endif()
