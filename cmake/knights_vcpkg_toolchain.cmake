cmake_minimum_required(VERSION 3.21)

set(_KNIGHTS_TOOLCHAIN_DIR "${CMAKE_CURRENT_LIST_DIR}")
get_filename_component(_KNIGHTS_REPO_ROOT "${_KNIGHTS_TOOLCHAIN_DIR}/.." REALPATH)

set(_KNIGHTS_VCPKG_HINTS)
if(DEFINED KNIGHTS_VCPKG_ROOT)
    list(APPEND _KNIGHTS_VCPKG_HINTS "${KNIGHTS_VCPKG_ROOT}")
endif()
if(DEFINED ENV{KNIGHTS_VCPKG_ROOT})
    list(APPEND _KNIGHTS_VCPKG_HINTS "$ENV{KNIGHTS_VCPKG_ROOT}")
endif()
list(APPEND _KNIGHTS_VCPKG_HINTS "${_KNIGHTS_REPO_ROOT}/external/vcpkg")
if(DEFINED ENV{VCPKG_ROOT})
    list(APPEND _KNIGHTS_VCPKG_HINTS "$ENV{VCPKG_ROOT}")
endif()

if(WIN32)
    foreach(_vsRoot IN ITEMS
        "C:/Program Files/Microsoft Visual Studio/2022/Community/VC/vcpkg"
        "C:/Program Files/Microsoft Visual Studio/2022/Professional/VC/vcpkg"
        "C:/Program Files/Microsoft Visual Studio/2022/Enterprise/VC/vcpkg")
        list(APPEND _KNIGHTS_VCPKG_HINTS "${_vsRoot}")
    endforeach()
endif()

set(_KNIGHTS_VCPKG_ROOT "")
foreach(_candidate IN LISTS _KNIGHTS_VCPKG_HINTS)
    file(TO_CMAKE_PATH "${_candidate}" _candidate_path)
    if(EXISTS "${_candidate_path}/scripts/buildsystems/vcpkg.cmake")
        set(_KNIGHTS_VCPKG_ROOT "${_candidate_path}")
        break()
    endif()
endforeach()

if(NOT _KNIGHTS_VCPKG_ROOT)
    message(FATAL_ERROR "Knights: vcpkg toolchain을 찾지 못했습니다. scripts/setup_vcpkg.(ps1|sh)을 실행하거나 KNIGHTS_VCPKG_ROOT/VCPKG_ROOT 환경변수를 설정해 주세요.")
endif()

set(VCPKG_ROOT "${_KNIGHTS_VCPKG_ROOT}" CACHE PATH "Resolved vcpkg root for Knights" FORCE)
include("${_KNIGHTS_VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")
