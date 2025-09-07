add_executable(server_app
    src/main.cpp
    src/chat/chat_service.cpp
)
target_include_directories(server_app PRIVATE ${CMAKE_CURRENT_LIST_DIR}/include)
target_link_libraries(server_app PRIVATE server_core wire_proto)
# protobuf 생성 헤더 경로 추가
target_include_directories(server_app PRIVATE ${CMAKE_BINARY_DIR}/proto)

# vcpkg DLL 자동 복사
if(WIN32)
    add_custom_command(TARGET server_app POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:protobuf::libprotobuf>
        $<TARGET_FILE:protobuf::libprotoc>
        $<TARGET_FILE_DIR:server_app>)
endif()
