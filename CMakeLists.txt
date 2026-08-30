cmake_minimum_required(VERSION 3.7)
project(Filters)

include(filters.cmake)

add_definitions(-fpic)

SET(LIBQOI_SRC
        ${CMAKE_CURRENT_SOURCE_DIR}/dec_qoi.c
)

SET(LIBQOI_INC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}
)

add_filter(libqoi
        "${LIBQOI_SRC}"
        ""
        []
        ""
        "${LIBQOI_INC}"
        ""
        "1")
