#include "server/core/config/dotenv.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <string>

namespace server::core::config {

namespace {

inline bool is_space(unsigned char ch) {
    return std::isspace(ch) != 0;
}

inline void trim(std::string& s) {
    auto begin = std::find_if_not(s.begin(), s.end(), is_space);
    auto end = std::find_if_not(s.rbegin(), s.rend(), is_space).base();
    if (begin == s.end()) {
        s.clear();
        return;
    }
    s.assign(begin, end);
}

static inline std::string unquote(const std::string& v) {
    if (v.size() >= 2) {
        char q = v.front();
        if ((q == '"' || q == '\'') && v.back() == q) {
            return v.substr(1, v.size() - 2);
        }
    }
    return v;
}
static void set_env(const std::string& k, const std::string& v, bool override_existing) {
#if defined(_WIN32)
    if (!override_existing) {
        if (std::getenv(k.c_str())) return;
    }
    _putenv_s(k.c_str(), v.c_str());
#else
    if (!override_existing && std::getenv(k.c_str())) return;
    setenv(k.c_str(), v.c_str(), override_existing ? 1 : 0);
#endif
}
}

bool load_dotenv(const std::string& path, bool override_existing) {
    std::ifstream in(path);
    if (!in.is_open()) return false;
    std::string line;
    // 설정 파일(.env)을 한 줄씩 읽어 환경 변수에 반영한다.
    while (std::getline(in, line)) {
        // 공백과 주석 줄을 제거한다.
        trim(line);
        if (line.empty() || line[0] == '#') continue;
        // 'export ' 접두어를 허용한다.
        const std::string export_kw = "export ";
        if (line.rfind(export_kw, 0) == 0) line.erase(0, export_kw.size());
        auto pos = line.find('=');
        if (pos == std::string::npos) continue;
        std::string key = line.substr(0, pos);
        std::string val = line.substr(pos + 1);
        trim(key); trim(val);
        val = unquote(val);
        if (!key.empty()) set_env(key, val, override_existing);
    }
    return true;
}

} // namespace server::core::config
