cmake_minimum_required(VERSION 3.20)

# FTXUI 기반 최소 클라이언트(처음부터 다시 시작)
add_executable(dev_chat_cli src/main.cpp)
target_include_directories(dev_chat_cli PRIVATE ${CMAKE_CURRENT_LIST_DIR}/src)

# FTXUI 의존성 (vcpkg)
find_package(ftxui CONFIG REQUIRED)
target_link_libraries(dev_chat_cli PRIVATE ftxui::screen ftxui::dom ftxui::component)

if (MSVC)
    target_compile_definitions(dev_chat_cli PUBLIC _WIN32_WINNT=0x0602)
    # Visual Studio에서 인스톨/구성 매핑 경고 회피
    set(CMAKE_MAP_IMPORTED_CONFIG_RELWITHDEBINFO Release)
endif()
