#include "server/core/util/paths.hpp"

#include <stdexcept>
#include <string>

#if defined(_WIN32)
#  include <windows.h>
#else
#  include <limits.h>
#  include <unistd.h>
#endif

namespace server::core::util::paths {

// 실행 중인 바이너리의 정규화된 경로를 반환한다. (테스트 실행 시에도 동일하게 동작)
std::filesystem::path executable_path() {
#if defined(_WIN32)
    std::wstring buffer(MAX_PATH, L'\0');
    DWORD length = 0;
    while (true) {
        length = ::GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (length == 0) {
            throw std::runtime_error("실행 파일 경로를 확인할 수 없습니다.");
        }
        if (length < buffer.size()) {
            buffer.resize(length);
            break;
        }
        buffer.resize(buffer.size() * 2);
    }
    std::filesystem::path exe_path(buffer);
#else
    std::string buffer(PATH_MAX, '\0');
    ssize_t length = ::readlink("/proc/self/exe", buffer.data(), buffer.size());
    if (length < 0) {
        throw std::runtime_error("실행 파일 경로를 확인할 수 없습니다.");
    }
    buffer.resize(static_cast<std::size_t>(length));
    std::filesystem::path exe_path(buffer);
#endif
    return std::filesystem::weakly_canonical(exe_path);
}

// 리소스 탐색을 위해 실행 파일 디렉터리를 별도로 노출한다.
std::filesystem::path executable_dir() {
    auto path = executable_path();
    auto dir = path.parent_path();
    if (dir.empty()) {
        throw std::runtime_error("실행 파일 디렉터리를 확인할 수 없습니다.");
    }
    return dir;
}

} // namespace server::core::util::paths

