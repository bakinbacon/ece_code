cmake_minimum_required(VERSION 2.8)
project( Surf_Homography )
find_package( OpenCV )
include_directories( ${OpenCV_INCLUDE_DIRS} )
add_executable( Surf_Homography Surf_Homography.cpp )
target_link_libraries( Surf_Homography ${OpenCV_LIBS} )