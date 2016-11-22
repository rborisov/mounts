cmake_minimum_required(VERSION 2.6)

project (moumon)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${PROJECT_SOURCE_DIR}/cmake/")
set(CPACK_PACKAGE_VERSION_MAJOR "1")
set(CPACK_PACKAGE_VERSION_MINOR "2")
set(CPACK_PACKAGE_VERSION_PATCH "2")

find_package(Threads REQUIRED)
find_package(PkgConfig REQUIRED)
find_package(LibMPDClient REQUIRED)
find_package(Sqlite3 REQUIRED)

include_directories(${PROJECT_BINARY_DIR} ${PROJECT_SOURCE_DIR}
    ${LIBMPDCLIENT_INCLUDE_DIR} ${PROJECT_SOURCE_DIR}/external/id3v2lib/include
    )

    link_directories(${LIBMPDCLIENT_LIBRARY_DIRS} 
        )

include(CheckCSourceCompiles)

set(CMAKE_C_FLAGS "-std=gnu99 -Wall -g -rdynamic") 
set(CMAKE_C_FLAGS_DEBUG "-ggdb -pedantic")

get_target_property(MKDATA_EXE mkdata LOCATION)

set(SOURCES
    mpd-utils.c
    mount-mon.c
    space-mon.c
    db_utils.c
    sql.c
    memory_utils.c
    id3v1.c
)

add_executable(moumon ${SOURCES})
target_link_libraries(moumon ${CMAKE_THREAD_LIBS_INIT}
    ${LIBMPDCLIENT_LIBRARIES}
    ${SQLITE3_LIBRARIES}
    -L${PROJECT_SOURCE_DIR}/external/id3v2lib/lib-${CMAKE_SYSTEM_PROCESSOR} -lid3v2)

install(TARGETS moumon DESTINATION bin)
