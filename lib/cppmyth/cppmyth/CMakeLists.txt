cmake_minimum_required (VERSION 2.6 FATAL_ERROR)
cmake_policy (VERSION 2.6)

project (cppmyth)
include (GNUInstallDirs)

# Options
OPTION (BUILD_SHARED_LIBS "Build shared libraries." OFF)
if (MSVC)
  # This option must match the settings used in your program, in particular if you
  # are linking statically
  OPTION( STATIC_CRT "Link the static CRT libraries" OFF )
endif (MSVC)

###############################################################################
# set lib version here
set (GENERIC_LIB_VERSION "1.0.0")
set (GENERIC_LIB_SOVERSION "1")

###############################################################################
# add definitions
if (MSVC)
  # Turn off Microsofts "security" warnings.
  add_definitions( "/W3 /D_USE_32BIT_TIME_T /D_CRT_SECURE_NO_WARNINGS /wd4005 /wd4996 /nologo" )
  if (STATIC_CRT)
    set(CMAKE_C_FLAGS_RELEASE "/MT")
    set(CMAKE_C_FLAGS_DEBUG "/MTd")
  endif (STATIC_CRT)
endif (MSVC)

if (CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_GNUCXX)
   set (CMAKE_C_FLAGS "-fPIC")
   set (CMAKE_CXX_FLAGS "-fPIC")
endif (CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_GNUCXX)

###############################################################################
# configure
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include (CheckFunctionExists)
include (CheckFunctionKeywords)

check_function_exists (timegm CHK_TIMEGM)
if (CHK_TIMEGM)
    set(HAVE_TIMEGM 1)
else (CHK_TIMEGM)
    set(HAVE_TIMEGM 0)
endif (CHK_TIMEGM)

check_function_exists (localtime_r CHK_LOCALTIME_R)
if (CHK_LOCALTIME_R)
    set(HAVE_LOCALTIME_R 1)
else (CHK_LOCALTIME_R)
    set(HAVE_LOCALTIME_R 0)
endif (CHK_LOCALTIME_R)

check_function_exists (gmtime_r CHK_GMTIME_R)
if (CHK_GMTIME_R)
    set(HAVE_GMTIME_R 1)
else (CHK_GMTIME_R)
    set(HAVE_GMTIME_R 0)
endif (CHK_GMTIME_R)

# Check what the inline keyword is.
check_function_keywords ("inline")
check_function_keywords ("__inline")
check_function_keywords ("__inline__")
if (HAVE_INLINE)
   set (CC_INLINE inline)
elseif (HAVE___INLINE)
   set (CC_INLINE __inline)
elseif (HAVE___INLINE__)
   set (CC_INLINE __inline__)
else (HAVE_INLINE)
   # no inline on this platform
   set (CC_INLINE)
endif (HAVE_INLINE)

# configure the public config file
configure_file ( ${CMAKE_CURRENT_SOURCE_DIR}/src/cppmyth_config.h.in
                 ${CMAKE_CURRENT_BINARY_DIR}/include/cppmyth_config.h)

# Copy the header files to the public include folder

file (COPY ${CMAKE_CURRENT_SOURCE_DIR}/src/atomic.h
           DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/include/)
file (COPY ${CMAKE_CURRENT_SOURCE_DIR}/src/mythsharedptr.h
           DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/include/)
file (COPY ${CMAKE_CURRENT_SOURCE_DIR}/src/mythtypes.h
           DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/include/)
file (COPY ${CMAKE_CURRENT_SOURCE_DIR}/src/mythcontrol.h
           DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/include/)
file (COPY ${CMAKE_CURRENT_SOURCE_DIR}/src/mytheventhandler.h
           DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/include/)
file (COPY ${CMAKE_CURRENT_SOURCE_DIR}/src/mythfileplayback.h
           DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/include/)
file (COPY ${CMAKE_CURRENT_SOURCE_DIR}/src/mythlivetvplayback.h
           DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/include/)
file (COPY ${CMAKE_CURRENT_SOURCE_DIR}/src/mythrecordingplayback.h
           DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/include/)
file (COPY ${CMAKE_CURRENT_SOURCE_DIR}/src/mythwsapi.h
           DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/include/)
file (COPY ${CMAKE_CURRENT_SOURCE_DIR}/src/mythdebug.h
           DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/include/)
file (COPY ${CMAKE_CURRENT_SOURCE_DIR}/src/mythstream.h
           DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/include/)
file (COPY ${CMAKE_CURRENT_SOURCE_DIR}/src/mythwsstream.h
           DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/include/)

file (COPY ${CMAKE_CURRENT_SOURCE_DIR}/src/proto/mythprotobase.h
           DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/include/proto/)
file (COPY ${CMAKE_CURRENT_SOURCE_DIR}/src/proto/mythprotomonitor.h
           DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/include/proto/)
file (COPY ${CMAKE_CURRENT_SOURCE_DIR}/src/proto/mythprotorecorder.h
           DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/include/proto/)
file (COPY ${CMAKE_CURRENT_SOURCE_DIR}/src/proto/mythprotoevent.h
           DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/include/proto/)
file (COPY ${CMAKE_CURRENT_SOURCE_DIR}/src/proto/mythprotoplayback.h
           DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/include/proto/)
file (COPY ${CMAKE_CURRENT_SOURCE_DIR}/src/proto/mythprototransfer.h
           DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/include/proto/)

include_directories (
  ${CMAKE_CURRENT_BINARY_DIR}/include/.
  ${CMAKE_BINARY_DIR}/jansson/include/.)

###############################################################################
# add sources
file (GLOB SRC_FILES
  src/private/mythdto/*.cpp
  src/private/*.c
  src/private/*.cpp
  src/proto/*.cpp
  src/*.cpp)

file (GLOB HDR_FILES
  src/proto/*.h src/*.h)

if (MSVC)
  file (GLOB PLATFORM_SRC_FILES
    src/private/platform/windows/*.cpp)
endif (MSVC)

set (CPPMYTH_SOURCES
  ${SRC_FILES} ${HDR_FILES} ${PLATFORM_SRC_FILES})

###############################################################################
# add targets
if (BUILD_SHARED_LIBS)
  add_library (cppmyth SHARED ${CPPMYTH_SOURCES})
  target_link_libraries (cppmyth pthread jansson)
  set_target_properties (cppmyth PROPERTIES
    VERSION "${GENERIC_LIB_VERSION}"
    SOVERSION "${GENERIC_LIB_SOVERSION}")
else (BUILD_SHARED_LIBS)
  add_library(cppmyth STATIC ${CPPMYTH_SOURCES})
endif (BUILD_SHARED_LIBS)

###############################################################################
# install targets
install (TARGETS cppmyth
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

install (FILES
  ${CMAKE_CURRENT_BINARY_DIR}/include/cppmyth_config.h
  ${CMAKE_CURRENT_BINARY_DIR}/include/atomic.h
  ${CMAKE_CURRENT_BINARY_DIR}/include/mythsharedptr.h
  ${CMAKE_CURRENT_BINARY_DIR}/include/mythtypes.h
  ${CMAKE_CURRENT_BINARY_DIR}/include/mythcontrol.h
  ${CMAKE_CURRENT_BINARY_DIR}/include/mytheventhandler.h
  ${CMAKE_CURRENT_BINARY_DIR}/include/mythfileplayback.h
  ${CMAKE_CURRENT_BINARY_DIR}/include/mythlivetvplayback.h
  ${CMAKE_CURRENT_BINARY_DIR}/include/mythrecordingplayback.h
  ${CMAKE_CURRENT_BINARY_DIR}/include/mythwsapi.h
  ${CMAKE_CURRENT_BINARY_DIR}/include/mythdebug.h
  ${CMAKE_CURRENT_BINARY_DIR}/include/mythstream.h
  ${CMAKE_CURRENT_BINARY_DIR}/include/mythwsstream.h
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/cppmyth/)
install (FILES
  ${CMAKE_CURRENT_BINARY_DIR}/include/proto/mythprotobase.h
  ${CMAKE_CURRENT_BINARY_DIR}/include/proto/mythprotomonitor.h
  ${CMAKE_CURRENT_BINARY_DIR}/include/proto/mythprotorecorder.h
  ${CMAKE_CURRENT_BINARY_DIR}/include/proto/mythprotoevent.h
  ${CMAKE_CURRENT_BINARY_DIR}/include/proto/mythprotoplayback.h
  ${CMAKE_CURRENT_BINARY_DIR}/include/proto/mythprototransfer.h
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/cppmyth/proto/)

foreach (p LIB INCLUDE)
  set (var CMAKE_INSTALL_${p}DIR)
  if (NOT IS_ABSOLUTE "${${var}}")
    set (${var} "${CMAKE_INSTALL_PREFIX}/${${var}}")
  endif ()
endforeach ()

configure_file (cppmyth.pc.in cppmyth.pc @ONLY)
install (FILES ${CMAKE_CURRENT_BINARY_DIR}/cppmyth.pc DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)
