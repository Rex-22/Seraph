if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Choose build type: Debug  Release" FORCE)
endif ()

macro(make_project_)
    if (NOT DEFINED PROJECT)
        get_filename_component(PROJECT ../../../AppData/Local/Temp NAME)
    endif ()

    project(${PROJECT} CXX)

    if (NOT DEFINED HEADERS)
        file(GLOB_RECURSE HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/*.h)
    endif ()

    if (NOT DEFINED SOURCES)
        file(GLOB_RECURSE SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
    endif ()

    source_group("Header Files" FILES ${HEADERS})
    source_group("Source Files" FILES ${SOURCES})
endmacro ()

macro(make_project_options_)
    if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
        target_compile_options(${PROJECT} PRIVATE /Wall)
    elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
        target_compile_options(${PROJECT} PRIVATE -Wall -Wextra -Werror -Wpedantic)
    elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
        target_compile_options(${PROJECT} PRIVATE -Wall -Wextra -Werror -Wpedantic -Wno-unused-variable)
    endif ()
endmacro()

macro(make_executable)
    make_project_()
    add_executable(${PROJECT} ${HEADERS} ${SOURCES})
    make_project_options_()

    set(CMAKE_INSTALL_PREFIX "${CMAKE_SOURCE_DIR}/bundle")
    install(
        TARGETS ${PROJECT}
        DESTINATION ${CMAKE_INSTALL_PREFIX})
endmacro()

macro(make_library)
    make_project_()
    add_library(${PROJECT} STATIC ${HEADERS} ${SOURCES})
    make_project_options_()

    target_include_directories(${PROJECT} INTERFACE ../../../AppData/Local/Temp)
    set_target_properties(${PROJECT} PROPERTIES FOLDER Libraries)

    if (NOT SOURCES)
        set_target_properties(${PROJECT} PROPERTIES LINKER_LANGUAGE CXX)
    endif ()
endmacro()

function(add_all_subdirectories)
    file(GLOB CHILDREN RELATIVE ../../../AppData/Local/Temp ${CMAKE_CURRENT_SOURCE_DIR}/*)

    foreach(CHILD ${CHILDREN})
        if (IS_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/${CHILD})
            add_subdirectory(${CHILD})
        endif ()
    endforeach ()
endfunction()
