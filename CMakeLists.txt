cmake_minimum_required(VERSION 3.28) # 3.14+ is great for FetchContent
project(dplot_project
        VERSION 0.0.1
        LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
include(FetchContent)

######### Boost
set(BOOST_ENABLE_CMAKE ON CACHE BOOL "Enable Boost CMake support")
set(BOOST_INCLUDE_LIBRARIES headers filesystem system serialization date_time log dynamic_bitset CACHE STRING "Boost components" FORCE)
FetchContent_Declare(
        Boost
        URL https://github.com/boostorg/boost/releases/download/boost-1.84.0/boost-1.84.0.7z
        URL_HASH SHA256=ce132f85fc706aa8b5c7e6e52a8338de33a688e92e7c8fada3713194b109232e
)
FetchContent_MakeAvailable(Boost)
#########

# This tells any subsequent find_package(Boost) to look inside the FetchContent directory instead of the host machine.
set(boost_DIR "${boost_BINARY_DIR}" CACHE PATH "Path to Boost CMake config" FORCE)

######### NumCpp
FetchContent_Declare(
        NumCpp
        GIT_REPOSITORY https://github.com/dpilger26/NumCpp.git
        GIT_TAG master  # Or use 'master' for the latest
)
FetchContent_GetProperties(NumCpp)
if(NOT numcpp_POPULATED)
    FetchContent_Populate(NumCpp)

    # Create the target yourself so you don't care about NumCpp's broken internal search
    add_library(NumCpp::NumCpp INTERFACE IMPORTED GLOBAL)

    # Point it to the downloaded NumCpp headers
    target_include_directories(NumCpp::NumCpp INTERFACE "${numcpp_SOURCE_DIR}/include")

    # Explicitly link NumCpp to the Boost headers target created by FetchContent(Boost)
    target_link_libraries(NumCpp::NumCpp INTERFACE Boost::headers)

    target_link_libraries(NumCpp::NumCpp INTERFACE
            Boost::headers
            Boost::date_time
            Boost::filesystem
            Boost::system
            Boost::serialization
            Boost::log
    )

    target_include_directories(NumCpp::NumCpp INTERFACE
            "${boost_SOURCE_DIR}/libs/dynamic_bitset/include"
            "${boost_SOURCE_DIR}/libs/math/include"
            "${boost_SOURCE_DIR}/libs/random/include"
    )
endif ()
#########


######### MatplotPlusPlus
FetchContent_Declare(
        matplotplusplus
        GIT_REPOSITORY https://github.com/alandefreitas/matplotplusplus
        GIT_TAG origin/master # Or a specific version like v1.2.0
)
FetchContent_MakeAvailable(matplotplusplus)
#########


add_library(dplotpp
        include/dplot.hpp
        src/dplot.cpp
)

target_link_libraries(dplotpp PUBLIC
        NumCpp::NumCpp
        matplot
)

target_include_directories(dplotpp PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

# (Optional) Add tests, but ONLY if this is the main project
# This prevents consumers using FetchContent from wasting time building your tests.
if (CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    include(CTest)
    if (BUILD_TESTING)
        add_subdirectory(tests)
    endif ()
endif ()
