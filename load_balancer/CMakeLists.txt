find_package(gRPC CONFIG REQUIRED)
add_executable(load_balancer_app
    src/load_balancer_app.cpp
    src/session_directory.cpp
    src/backend_registry.cpp
    src/backend_refresher.cpp
    src/grpc_service_impl.cpp
    src/main.cpp
)

target_include_directories(load_balancer_app PRIVATE
    ${CMAKE_SOURCE_DIR}/load_balancer/include
    ${CMAKE_BINARY_DIR}/proto
)

target_link_libraries(load_balancer_app PRIVATE
    server_core
    server_state
    gateway_lb_proto
    gRPC::grpc++
)
