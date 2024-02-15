cmake_minimum_required(VERSION 3.16.5)
project(OcleaLog)

SET(HEADER_FILES
    logger.h
)

install(FILES
    ${HEADER_FILES} DESTINATION "${CMAKE_INSTALL_PREFIX}/include/oclea/log/"
)
