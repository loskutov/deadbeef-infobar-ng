cmake_minimum_required (VERSION 2.6) 
 
set (PROJECT infobar) 
 
project (${PROJECT}) 

set(CMAKE_C_FLAGS "-std=c99 -D_GNU_SOURCE")

find_package(GTK2 REQUIRED)
find_package(LibXml2 REQUIRED)

include_directories(${GTK2_INCLUDE_DIRS} ${LIBXML2_INCLUDE_DIR})

set (HEADERS 
     support.h)
 
set (SOURCES 
     infobar.c
     support.c) 
     
add_library (${PROJECT} SHARED ${HEADERS} ${SOURCES})
target_link_libraries (${PROJECT} ${GTK2_LIBRARIES} ${LIBXML2_LIBRARIES})
set_target_properties(${PROJECT} PROPERTIES PREFIX "")
