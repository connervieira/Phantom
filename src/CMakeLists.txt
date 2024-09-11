project(src)

set(CMAKE_BUILD_TYPE RelWithDebugInfo)
#set(CMAKE_BUILD_TYPE Debug)

cmake_minimum_required (VERSION 2.6)

# Set the Phantom version in cmake, and also add it as a DEFINE for the code to access
SET(PHANTOM_MAJOR_VERSION "1")
SET(PHANTOM_MINOR_VERSION "3")
SET(PHANTOM_PATCH_VERSION "0")
SET(PHANTOM_VERSION ${PHANTOM_MAJOR_VERSION}.${PHANTOM_MINOR_VERSION}.${PHANTOM_PATCH_VERSION})

add_definitions( -DPHANTOM_MAJOR_VERSION=${PHANTOM_MAJOR_VERSION})
add_definitions( -DPHANTOM_MINOR_VERSION=${PHANTOM_MINOR_VERSION})
add_definitions( -DPHANTOM_PATCH_VERSION=${PHANTOM_PATCH_VERSION})

SET(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake_modules/")

add_definitions( -DINSTALL_PREFIX="${CMAKE_INSTALL_PREFIX}")

# TODO: switch to http://www.cmake.org/cmake/help/v2.8.5/cmake.html#module:GNUInstallDirs ?
IF (NOT CMAKE_INSTALL_SYSCONFDIR) 
  SET(CMAKE_INSTALL_SYSCONFDIR "/etc")
ENDIF()
IF (NOT CMAKE_INSTALL_VARDIR) 
  SET(CMAKE_INSTALL_VARDIR "${CMAKE_INSTALL_PREFIX}/var")
ENDIF()


IF ( NOT DEFINED WITH_STATEDETECTION )
  SET(WITH_STATEDETECTION OFF)
ENDIF()

if ( NOT DEFINED WITH_GPU_DETECTOR )
  SET(WITH_GPU_DETECTOR OFF)
ENDIF()

IF(Tesseract_FRAMEWORK_PATH AND Leptonica_FRAMEWORK_PATH) 
  MESSAGE(STATUS "Using Tesseract iOS framework: ${Tesseract_FRAMEWORK_PATH}")
  MESSAGE(STATUS "Using Leptonica iOS framework: ${Leptonica_FRAMEWORK_PATH}")
  # http://www.vtk.org/Wiki/CMake:HowToUseExistingOSXFrameworks
  SET(Tesseract_LIBRARIES "${Tesseract_FRAMEWORK_PATH};${Leptonica_FRAMEWORK_PATH}")
  SET(Tesseract_INCLUDE_DIRS "${Tesseract_FRAMEWORK_PATH}/Headers")
ELSE()
  FIND_PACKAGE( Tesseract REQUIRED )
ENDIF()

include_directories(${Tesseract_INCLUDE_DIRS})

IF(OpenCV_FRAMEWORK_PATH)
  MESSAGE(STATUS "Using OpenCV iOS framework: ${OpenCV_FRAMEWORK_PATH}")
  SET(OpenCV_INCLUDE_DIRS "${OpenCV_FRAMEWORK_PATH}/Headers")
  SET(OpenCV_LIBS ${OpenCV_FRAMEWORK_PATH})

  # OpenCV's released framework has this disabled, so we must too.
  # http://stackoverflow.com/a/32710441/868173
  SET(CMAKE_XCODE_ATTRIBUTE_ENABLE_BITCODE NO)
ELSE()
  # Discover OpenCV directory automatically
  find_path(OpenCV_DIR
    NAMES OpenCVConfig.cmake
    HINTS ${CMAKE_SOURCE_DIR}/../libraries/opencv/
    /storage/projects/alpr/libraries/opencv/
    )
  # Opencv Package
  FIND_PACKAGE( OpenCV REQUIRED )
ENDIF()

IF (${OpenCV_VERSION} VERSION_LESS 2.4.7)
	MESSAGE(FATAL_ERROR "OpenCV version is not compatible : ${OpenCV_VERSION}")
ENDIF()

include_directories(${OpenCV_INCLUDE_DIRS})

add_definitions( -DOPENCV_MAJOR_VERSION=${OpenCV_VERSION_MAJOR})


include_directories(./phantom)

IF (WIN32)
	add_definitions( -DWINDOWS)
	add_definitions( -DNOMINMAX)

	# Extra linker dependencies for Windows
	SET (Tesseract_LIBRARIES
		${Tesseract_LIBRARIES}
		ws2_32.lib
	)
ELSE()

	SET (Extra_LIBS
		pthread
	)

ENDIF()


set(CMAKE_CSS_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -Wall ")
if (NOT IOS)
  ADD_EXECUTABLE( alpr  main.cpp )
ENDIF()

if (WIN32) 
   SET(OPENALPR_LIB openalpr-static)
ELSE()
   SET(OPENALPR_LIB openalpr)
ENDIF()

IF (WITH_STATEDETECTION)
  SET(STATE_DETECTION_LIB statedetection)
  add_subdirectory(statedetection)
ELSE()
  SET(STATE_DETECTION_LIB "")
  add_definitions( -DSKIP_STATE_DETECTION=1 )
ENDIF()

if (NOT IOS)
  TARGET_LINK_LIBRARIES(alpr
    ${OPENALPR_LIB}
    ${STATE_DETECTION_LIB}
    support
    video
    ${OpenCV_LIBS}
    ${Tesseract_LIBRARIES}
    ${Extra_LIBS}
  )
ENDIF()


add_subdirectory(phantom)
add_subdirectory(video)

if (NOT IOS)
  install (TARGETS    alpr DESTINATION   ${CMAKE_INSTALL_PREFIX}/bin)
  install (FILES      ${CMAKE_SOURCE_DIR}/../doc/man/alpr.1 DESTINATION       ${CMAKE_INSTALL_PREFIX}/share/man/man1 COMPONENT doc)
ENDIF()
install (DIRECTORY  ${CMAKE_SOURCE_DIR}/../runtime_data DESTINATION         ${CMAKE_INSTALL_PREFIX}/share/phantom)

# set runtime_data to reflect the current CMAKE_INSTALL_PREFIX
CONFIGURE_FILE(${CMAKE_SOURCE_DIR}/../config/phantom.conf.user ${CMAKE_CURRENT_BINARY_DIR}/config/phantom.conf)
install (FILES      ${CMAKE_CURRENT_BINARY_DIR}/config/phantom.conf DESTINATION ${CMAKE_INSTALL_SYSCONFDIR}/phantom/      COMPONENT config)

CONFIGURE_FILE(${CMAKE_SOURCE_DIR}/../config/phantom.conf.defaults ${CMAKE_CURRENT_BINARY_DIR}/share/phantom/config/phantom.defaults.conf)
install (FILES      ${CMAKE_CURRENT_BINARY_DIR}/share/phantom/config/phantom.defaults.conf DESTINATION ${CMAKE_INSTALL_PREFIX}/share/phantom/config      COMPONENT config)


SET (CPACK_PACKAGE_VERSION ${PHANTOM_VERSION})
SET (CPACK_SET_DESTDIR "on")
SET (CPACK_GENERATOR "DEB;TGZ")
SET (CPACK_PACKAGE_NAME "phantomalpr")
SET (CPACK_INSTALL_CMAKE_PROJECTS "${CMAKE_SOURCE_DIR}/build;src;ALL;/")
SET (CPACK_CMAKE_GENERATOR "Unix Makefiles")

SET (CPACK_STRIP_FILES "1")
SET (CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
SET (CPACK_DEBIAN_PACKAGE_SECTION "video")
SET (CPACK_DEBIAN_ARCHITECTURE ${CMAKE_SYSTEM_PROCESSOR})
SET (CPACK_DEBIAN_PACKAGE_DEPENDS "libc6 (>= 2.1.3), libgcc1 (>= 4.1.1), libtesseract3 (>= 3.0.3), libopencv-core2.4 (>= 2.4.8), libopencv-objdetect2.4 (>= 2.4.8), libopencv-highgui2.4 (>= 2.4.8), libopencv-imgproc2.4 (>= 2.4.8), libopencv-flann2.4 (>= 2.4.8), libopencv-features2d2.4 (>= 2.4.8), libopencv-video2.4 (>= 2.4.8), libopencv-gpu2.4 (>=2.4.8), liblog4cplus-1.0-4, libcurl3")

SET (CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/../LICENSE")
SET (CPACK_PACKAGE_DESCRIPTION "Phantom - Open Source Automatic License Plate Recognition")
SET (CPACK_PACKAGE_DESCRIPTION_SUMMARY "Phantom is an open source license plate recognition system heavily based on OpenALPR. The library analyzes images and identifies license plates. The output is the text representation of any license plate characters found in the image.")
SET (CPACK_PACKAGE_CONTACT "Conner Vieira <admin@v0lttech.com>")
SET (CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}_${CPACK_PACKAGE_VERSION}_${CPACK_DEBIAN_ARCHITECTURE}")

SET (CPACK_COMPONENTS_ALL Libraries ApplicationData)
set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
set (CMAKE_CXX_STANDARD 11)
cmake_minimum_required(VERSION 3.1)
# Enable C++11
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)
INCLUDE(CPack)

# ----------------------------------------------------------------------------
# Uninstall target, for "make uninstall"
# http://www.cmake.org/Wiki/CMake_FAQ#Can_I_do_.22make_uninstall.22_with_CMake.3F
# ----------------------------------------------------------------------------
CONFIGURE_FILE(
  "${CMAKE_MODULE_PATH}/templates/cmake_uninstall.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
  @ONLY)

ADD_CUSTOM_TARGET(uninstall COMMAND ${CMAKE_COMMAND} -P "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake")

