# Conan2 전면 전환 실행 계획 (ops/conan2-mig)

## 1) 목적

- `main` 기준 새 브랜치 `ops/conan2-mig`에서 Conan2 전면 전환을 시작한다.
- 구현에 들어가기 전에 영향받는 지점을 파일 단위로 확정하고, 단계별 실행 계획/검증/롤백 기준을 문서화한다.

## 2) 현재 상태와 전환 원칙

### 2.1 현재 상태 (요약)

- Windows 개발/CI 의존성 경로는 vcpkg 기준으로 강결합되어 있다.
  - Preset toolchain: `CMakePresets.json`
  - vcpkg toolchain wrapper: `cmake/knights_vcpkg_toolchain.cmake`
  - bootstrap script: `scripts/setup_vcpkg.ps1`
  - CI cache/prewarm: `.github/workflows/ci.yml`, `.github/workflows/vcpkg-prewarm.yml`, `.github/workflows/windows-sccache-poc.yml`
- Linux는 기본적으로 system deps + Docker build 흐름이며, Windows보다 vcpkg 결합도가 낮다.
- 컴파일 산출물 cross-run 공유는 현재 없음(의존성 캐시 중심).

### 2.2 전환 원칙

1. 의존성 해석은 Conan2 단일 경로로 수렴한다(최종적으로 vcpkg bootstrap/toolchain 제거).
2. 재현성은 lockfile + 고정 profile로 보장한다.
3. Windows 런타임 DLL 처리에서 `vcpkg_installed` 경로 가정을 제거한다.
4. 단계별로 "빌드 가능 상태"를 유지하며, 각 단계마다 명확한 통과 기준을 둔다.

## 3) 영향 지점 확정 (File-evidenced Impact Map)

### 3.1 Build System / Presets

| 파일 | 현재 결합 방식 | 전환 시 변경 내용 |
|---|---|---|
| `CMakePresets.json` | Windows preset이 `toolchainFile: cmake/knights_vcpkg_toolchain.cmake`를 사용, `VCPKG_TARGET_TRIPLET`, `VCPKG_MANIFEST_FEATURES` 설정 포함 | Conan profile + Conan generated toolchain 기반 preset으로 교체. vcpkg cache variables 제거 또는 Conan 변수로 치환 |
| `cmake/knights_vcpkg_toolchain.cmake` | vcpkg root 탐색 + `vcpkg.cmake` include 강제, 미탐색 시 FATAL | Conan2 전면 전환 완료 시 제거(또는 deprecated stub). Conan 경로는 `conan install`이 생성하는 toolchain으로 통일 |

### 3.2 CMake Targets / Runtime DLL Handling

| 파일 | 현재 결합 방식 | 전환 시 변경 내용 |
|---|---|---|
| `server/CMakeLists.txt` | `VCPKG_INSTALLED_DIR`, `VCPKG_TARGET_TRIPLET` 기반 DLL copy (`libprotobuf`, `libprotoc`, `abseil_dll`) | Conan-generated runtime path 또는 `TARGET_RUNTIME_DLLS` 중심으로 교체. hardcoded vcpkg 경로 제거 |
| `CMakeLists.txt` | `admin_app` post-build에서 `${CMAKE_BINARY_DIR}/vcpkg_installed/x64-windows/...` 디렉터리 전체 복사 | Conan 경로 기반 복사 또는 target metadata 기반 runtime DLL 수집으로 대체 |
| `tests/CMakeLists.txt` | `find_package(GTest CONFIG REQUIRED)` 등 패키지 탐색이 현재 vcpkg ecosystem과 호환 전제 | Conan CMakeDeps 생성 config를 기준으로 동작하도록 검증 및 필요 시 패키지 이름/target alias 조정 |
| `core/CMakeLists.txt` | `find_package(OpenSSL)`, `find_package(lz4)` 등 provider가 바뀌면 target 이름/해결 경로 영향 가능 | Conan package naming과 맞게 find/link path 검증, fallback PkgConfig 분기 재검토 |

### 3.3 Build Scripts

| 파일 | 현재 결합 방식 | 전환 시 변경 내용 |
|---|---|---|
| `scripts/setup_vcpkg.ps1` | clone/bootstrap/install 및 baseline fetch 재시도 로직 포함 | `scripts/setup_conan.ps1`(신규)로 대체. Conan client install/config/profile/remote/bootstrap 수행 |
| `scripts/build.ps1` | Windows에서 `vcpkg.json` 존재 시 `setup_vcpkg.ps1` 선행 실행 | Conan install + CMake configure/build 흐름으로 교체. `UseVcpkg` 옵션 제거/폐기 |

### 3.4 CI / Cache / Prewarm

| 파일 | 현재 결합 방식 | 전환 시 변경 내용 |
|---|---|---|
| `.github/workflows/ci.yml` | Windows job env에 `VCPKG_*`, cache key가 `vcpkg.json + setup_vcpkg + toolchain`, vcpkg restore/save telemetry 사용 | `conan-io/setup-conan@v1` + Conan cache 전략 + lockfile install 경로로 대체 |
| `.github/workflows/vcpkg-prewarm.yml` | vcpkg prewarm 전용 workflow | Conan 전환 시 워크플로우 자체를 제거(별도 prewarm 미운영), required CI 내 Conan cache restore/save로 일원화 |
| `.github/workflows/windows-sccache-poc.yml` | vcpkg cache + sccache 동시 실험 | Conan cache + sccache 조합으로 재작성 |

### 3.5 Docs

| 파일 | 현재 결합 방식 | 전환 시 변경 내용 |
|---|---|---|
| `README.md` | Dependency Manager를 vcpkg로 명시 | Conan2 기준 설치/빌드 안내로 갱신 |
| `core/README.md` | 코어 빌드 설명에 vcpkg 전제 | Conan2 기준 안내로 갱신 |
| `docs/db/write-behind.md` | `redis-plus-plus ... (vcpkg, x64-windows)` 표기 | provider 중립 표현 또는 Conan2 기준 정보로 수정 |
| `docs/ops/ci-build-cache-optimization-report.md` | vcpkg cache/prewarm 중심 기술 | Conan cache/lockfile 기준으로 재기술(별도 prewarm 미운영 원칙 반영) |

## 4) 단계별 실행 계획 (Full Migration)

### Phase 0 - Baseline Freeze (필수 선행)

목표:

- 전환 전 기준 성능/안정성/재현성 지표를 고정한다.

작업:

1. 기준 기록
   - `windows-fast-tests` 빌드 시간, 실패 유형, cache hit 관련 telemetry 수집.
2. 잠금
   - 본 브랜치에서 vcpkg 관련 파일 수정 전 baseline 수치 문서화.

통과 조건:

- 기준 리포트가 문서에 남고, 이후 단계 비교 기준이 확보됨.

### Phase 1 - Conan2 Scaffold 도입

목표:

- vcpkg를 아직 제거하지 않고 Conan2 실행 기반(conanfile/profile/lockfile)을 먼저 세운다.

작업:

1. `conanfile.py`(또는 `conanfile.txt`) 작성
   - `vcpkg.json` feature(`windows-dev`, `windows-client`)를 Conan profile/option 조합으로 매핑.
2. profile 설계
   - `profiles/windows-msvc-debug`, `profiles/windows-msvc-release`, `profiles/linux-gcc-debug`, `profiles/linux-gcc-release`.
   - MSVC 필수 항목(`compiler=msvc`, `compiler.version`, `compiler.runtime`, `compiler.cppstd`) 명시.
3. lockfile 정책
   - 초기 `conan.lock` 생성, 업데이트 규칙 문서화.

통과 조건:

- `conan install`이 Windows/Linux에서 성공하고 generators 생성됨.

### Phase 2 - CMake Provider 전환

목표:

- CMake가 vcpkg toolchain 대신 Conan generated toolchain/CMakeDeps를 사용하도록 전환.

작업:

1. `CMakePresets.json` 전환
   - Windows preset의 `toolchainFile`을 Conan 흐름에 맞게 교체.
2. `cmake/knights_vcpkg_toolchain.cmake` 제거 준비
   - 의존 경로를 Conan 쪽으로 이전 후 최종 제거.
3. `find_package`/target 확인
   - `libpqxx`, `redis++`, `GTest`, `lz4`, `OpenSSL`의 target 이름과 linkage 일관성 점검.

통과 조건:

- CMake configure/build가 Conan provider로 성공.
- vcpkg toolchain에 의존하지 않아도 동일 타깃 빌드 가능.

### Phase 3 - Runtime DLL Copy/Packaging 정리 (Windows 핵심)

목표:

- `vcpkg_installed` 경로 가정을 제거하고 Conan 기반 runtime DLL 처리로 통일.

작업:

1. `server/CMakeLists.txt` post-build copy 교체
   - `VCPKG_INSTALLED_DIR`/triplet 기반 경로 제거.
2. `CMakeLists.txt`의 `admin_app` copy_directory 제거/대체
   - `${CMAKE_BINARY_DIR}/vcpkg_installed/...` 복사 제거.
3. 실행 검증
   - `server_app`, `gateway_app`, `admin_app`, 테스트 실행 파일 런타임 로딩 검증.

통과 조건:

- Windows에서 주요 바이너리가 런타임 DLL 누락 없이 실행.

### Phase 4 - Script/Workflow 전면 전환

목표:

- CI와 로컬 스크립트 모두 Conan2 기준으로 일원화.

작업:

1. 스크립트 전환
   - `scripts/setup_vcpkg.ps1` -> `scripts/setup_conan.ps1`.
   - `scripts/build.ps1`에서 Conan install 흐름 반영.
2. CI 전환
   - `.github/workflows/ci.yml`에 `conan-io/setup-conan@v1` 도입.
   - lockfile 기반 install/build.
   - vcpkg cache telemetry/restore/save 단계 제거.
3. prewarm 제거
   - `vcpkg-prewarm.yml`를 retire하고, 별도 prewarm 없이 required CI cache 전략으로 운영.

통과 조건:

- PR/main/merge_group 모두 Conan 경로로 green.

### Phase 5 - 문서/운영 가이드 정리

목표:

- vcpkg 중심 문서를 Conan2 기준으로 정합화.

작업:

1. `README.md`, `core/README.md`, `docs/db/write-behind.md`, `docs/ops/ci-build-cache-optimization-report.md` 갱신.
2. 마이그레이션 운영 가이드(FAQ/트러블슈팅) 추가.

통과 조건:

- 문서에 vcpkg 전제 잔존 없음(의도된 compatibility note 제외).

## 5) 리스크 레지스터 (실행 시 필수 점검)

| 리스크 | 심각도 | 징후 | 대응 |
|---|---|---|---|
| MSVC profile 불완전 | Critical | `settings.os.subsystem`/toolset 오류 | 명시적 Windows profiles 유지, detect 결과 수동 보정 |
| shared/static mismatch | Critical | LNK 에러, unresolved symbols | 옵션 표준화(`*:shared` 정책), runtime 일관화 |
| transitive include/link 누락 | High | 헤더 미탐색/링크 실패 | CMakeDeps 설정 검증, package target mapping 표준화 |
| lockfile 오용 | Medium | 같은 커밋에서 재현성 붕괴 | lockfile 생성/업데이트 절차 고정, 수동편집 금지 |
| remote/auth 실패 | Medium | CI 401/403 | remotes/auth를 setup 단계에서 명시, secret 정책 정리 |
| line-ending/recipe revision 불일치 | High | 플랫폼별 revision mismatch | `.gitattributes` 정비 및 profile/revision 검증 |

## 6) 검증 매트릭스 (각 단계 공통)

### 6.1 로컬(Windows)

```powershell
cmake --preset windows
cmake --build --preset windows-debug --parallel
ctest --preset windows-test --output-on-failure
```

### 6.2 Linux (Docker)

```powershell
pwsh scripts/deploy_docker.ps1 -Action up -Detached -Build
python tests/python/verify_pong.py
python tests/python/test_load_balancing.py
pwsh scripts/deploy_docker.ps1 -Action down
```

### 6.3 CI

- PR checks: `core-api-consumer-windows`, `core-api-consumer-linux`, `windows-fast-tests`, `linux-docker-stack`.
- timeout 정책: Windows 주요 잡 60분 유지(기존 운영 제약 반영).

## 7) 롤백 계획

1. Phase 2~4 중 실패 시 즉시 provider fallback branch 유지.
2. Conan lane 성공 전에는 main path 보호(기존 vcpkg lane 삭제 금지).
3. 중대 링크/런타임 장애 발생 시:
   - 최근 phase 변경만 되돌려 빌드 복구,
   - 원인 분리 후 재시도.

## 8) 즉시 착수 체크리스트 (ops/conan2-mig)

- [x] `main` 기준 브랜치 생성: `ops/conan2-mig`
- [x] 영향 지점 인벤토리 확정
- [x] 상세 실행 계획 문서 작성
- [ ] Phase 0 baseline 데이터 확정
- [x] Phase 1 Conan scaffold 구현 시작 (`conanfile.py`, `conan/profiles/*`, `scripts/setup_conan.ps1`, `.github/workflows/conan2-poc.yml`)
- [x] 초기 lockfile 생성 (`conan.lock`, `conan-client.lock`)
- [x] Phase 2 선행 작업: Conan provider preset 추가 (`windows-conan`, `windows-client-conan`, `linux-conan*`) + `scripts/build.ps1 -UseConan` preset 연동
- [x] Phase 2 검증: Conan client-only 빌드 성공 (`pwsh scripts/build.ps1 -UseConan -ClientOnly -Target client_gui -MaxJobs 8`)
- [x] Phase 2 검증: Conan windows-dev Debug 빌드 + 테스트 성공 (`pwsh scripts/build.ps1 -UseConan -Config Debug -MaxJobs 8` + `ctest --preset windows-conan-test`)
- [ ] CI PoC 실검증: `.github/workflows/conan2-poc.yml` `workflow_dispatch` 실행 결과 반영

### 8.1 현재 kickoff 명령

로컬 Conan2 PoC(Windows):

```powershell
python -m pip install "conan>=2.0,<3"
conan lock create . --profile:host conan/profiles/host/windows-msvc-debug --profile:build conan/profiles/build/default --options:host "&:knights_feature=windows-dev" --lockfile-out conan.lock
conan lock create . --profile:host conan/profiles/host/windows-msvc-release --profile:build conan/profiles/build/default --options:host "&:knights_feature=windows-client" --lockfile-out conan-client.lock
pwsh scripts/build.ps1 -UseConan -Config Debug
pwsh scripts/build.ps1 -UseConan -ClientOnly -Target client_gui
ctest --test-dir build-windows-conan --build-config Debug --output-on-failure
```

GitHub Actions 수동 PoC:

- workflow: `.github/workflows/conan2-poc.yml`
- trigger: `workflow_dispatch`
- 기본 동작: Conan cache restore -> lockfile 확인(`conan.lock`) -> `scripts/build.ps1 -UseConan` -> (선택) `ctest`

### 8.2 최근 검증 결과 (2026-03-02)

성공:

- `pwsh scripts/build.ps1 -UseConan -ClientOnly -Target client_gui -MaxJobs 8`
- `pwsh scripts/build.ps1 -UseConan -Config Debug -Target server_app -MaxJobs 8`
- `pwsh scripts/build.ps1 -UseConan -Config Debug -MaxJobs 8`
- `ctest --preset windows-conan-test` -> **137/137 passed, 1 skipped(기존 storage_basic skip 시나리오)**

해결한 주요 이슈:

1. Conan toolchain 경로 mismatch
   - 증상: `Conan toolchain 파일을 찾지 못했습니다`.
   - 조치: Conan2 `cmake_layout` 기준 경로(`conan/build/.../generators/conan_toolchain.cmake`)로 preset/script 동기화.
2. Visual Studio multi-config + protobuf::protoc 구성 충돌
   - 증상: `IMPORTED_LOCATION not set ... protobuf::protoc (RelWithDebInfo/MinSizeRel)`.
   - 조치: Conan preset configure 시 `CMAKE_CONFIGURATION_TYPES=$Config` 강제(단일 구성).
3. 중복 preset(`conan-default`) 충돌
   - 증상: `Could not read presets ... Duplicate preset: "conan-default"`.
   - 조치: `conanfile.py`에서 `toolchain.user_presets_path = ""`로 CMakeUserPresets 생성 비활성화 + `scripts/setup_conan.ps1`에서 기존 conan-generated `CMakeUserPresets.json` 정리.
4. Boost::system target 부재
   - 증상: Conan Boost 사용 시 `Boost::system` link target 미존재.
   - 조치: root에서 `Boost::system` target 미존재 시 header-only 모드로 강제 fallback.
5. `TARGET_RUNTIME_DLLS` 빈 목록 post-build 실패
   - 증상: `cmake -E copy_if_different` 인자 부족으로 post-build 실패.
   - 조치: `cmake/copy_runtime_dlls_if_any.cmake` helper를 도입해 DLL 목록이 있을 때만 복사.
6. tools/admin_app JSON include 누락
   - 증상: `nlohmann/json.hpp` include 실패 (`wb_worker`, `wb_dlq_replayer`, `admin_app`).
   - 조치: 해당 타깃에 `nlohmann_json::nlohmann_json` 명시 링크.

잔여 리스크/후속:

- CI lane(`conan2-poc.yml`) 실런 결과가 아직 문서에 반영되지 않았다.
- 첫 실행 시 Conan source build(특히 Windows dev graph)로 시간이 길어질 수 있어 캐시 전략 검증이 필요하다.
- ConanCenter 패키지의 1.x deprecated 경고는 즉시 blocker는 아니지만 중장기적으로 버전/옵션 pinning 전략이 필요하다.

## 9) 외부 근거 (공식 중심)

- Conan2 CMake integration: https://docs.conan.io/2/integrations/cmake.html
- Lockfiles: https://docs.conan.io/2/tutorial/versioning/lockfiles.html
- Binary model: https://docs.conan.io/2/reference/binary_model.html
- Profiles: https://docs.conan.io/2/reference/config_files/profiles.html
- GitHub setup action: https://github.com/conan-io/setup-conan

## 10) Git submodule 대안 검토 (Conan2 대비)

### 10.1 현재 상태 (저장소 근거)

- 현재 실제 submodule은 `external/imgui` 1개다.
  - 근거: `.gitmodules`
  - 근거: `git submodule status --recursive` 결과
- CI는 일부 워크플로우에서 이미 submodule checkout을 사용한다.
  - 근거: `.github/workflows/vcpkg-prewarm.yml`, `.github/workflows/windows-sccache-poc.yml`의 `submodules: recursive`
- `external/vcpkg`는 submodule이 아니라 bootstrap script가 clone해서 쓰는 경로다.
  - 근거: `scripts/setup_vcpkg.ps1` (`external/vcpkg` clone/bootstrap)
  - 참고: `.gitignore`에 `external/vcpkg/` 무시 규칙 존재

### 10.2 submodule 적용 가능 후보 (실현성)

| 후보 | 현재 상태 | 실현성 | 판단 |
|---|---|---|---|
| `external/imgui` 유지 | 이미 submodule | High | 현행 유지가 합리적 |
| `external/vcpkg`를 submodule로 고정 | 현재는 runtime clone | Medium | vcpkg tool 버전 고정에는 유리하지만, "패키지 매니저 대체"는 아님 |
| vcpkg 의존 라이브러리(Boost/Protobuf/OpenSSL/libpqxx/redis++/GTest/lz4 등)를 개별 submodule화 | 현재 `vcpkg.json` + toolchain으로 관리 | Low | transitive graph/빌드 옵션/ABI 조합을 직접 관리해야 해서 운영 부담 큼 |

### 10.3 submodule 전환 시 CI/빌드 영향

- 장점
  - 특정 라이브러리 소스를 commit 단위로 고정 가능.
  - patch/fork를 직접 들고 가야 하는 라이브러리에는 유리.
- 단점
  - 현재 vcpkg 캐시/prewarm 구조(`ci.yml`, `vcpkg-prewarm.yml`)의 직접 효익이 크게 감소.
  - binary 패키지 재사용 대신 source build 비중이 늘어 Windows CI 시간이 길어질 가능성이 큼.
  - transitive dependency 해결, 옵션(shared/static), 플랫폼별 툴체인 조합을 수작업으로 관리해야 함.

### 10.4 Conan2 대비 비교 (핵심)

Git submodule은 다음 Conan2 기능을 실질적으로 대체하기 어렵다.

1. lockfile 기반 전이 그래프 재현성
2. `package_id` 기반 multi-config binary 모델
3. 자동 transitive dependency graph 해석/충돌 관리
4. profile 기반 host/build 분리와 설정 관리
5. `tool_requires` 기반 빌드 도구 버전 관리
6. remotes/auth 기반 패키지 배포/접근 정책
7. CI용 binary cache/remote 재사용 구조
8. GitHub Actions용 표준 setup 경로(`conan-io/setup-conan`)

### 10.5 결론 및 채택 전략

- 결론: submodule은 **선택적 보완 수단**으로는 유효하지만, 이 저장소의 의존성 lifecycle 전면 대체 수단으로는 부적합하다.
- 권고:
  1) `external/imgui` 같은 소스 중심 라이브러리는 submodule 유지
  2) 패키지/전이 의존성/CI 재현성 영역은 Conan2 중심으로 전환
  3) `external/vcpkg` submodule화는 "Conan2 전환 실패 시 fallback 안정화" 용도로만 검토

## 11) vcpkg 완전 대체 실행 계획 (컷오버 프로그램)

본 섹션은 "PoC/병행"이 아니라 **기본 경로를 Conan2로 뒤집고 vcpkg 경로를 제거**하기 위한 상세 실행안이다.

### 11.1 현재 기준선(2026-03-02)

- Conan lane 로컬 검증은 통과했다.
  - `pwsh scripts/build.ps1 -UseConan -Config Debug -MaxJobs 8`
  - `ctest --preset windows-conan-test`
- PR #7 기준 필수 CI 4개는 green이다.
  - `core-api-consumer-linux`, `core-api-consumer-windows`, `linux-docker-stack`, `windows-fast-tests`
- 하지만 required CI/job의 Windows 경로는 여전히 vcpkg env/cache/telemetry에 결합되어 있다.
  - 근거: `.github/workflows/ci.yml`
- 로컬 기본 preset/스크립트도 아직 vcpkg를 기본값으로 둔다.
  - 근거: `CMakePresets.json`의 `windows*` 기본 preset, `scripts/build.ps1`의 비-Conan 기본 경로

### 11.2 완전 대체 Definition of Done (DoD)

아래 8개를 모두 만족해야 "완전 대체"로 판정한다.

1. Windows required CI가 Conan-only 경로로 동작한다 (`ci.yml`에서 `VCPKG_*` 제거).
2. 로컬 기본 명령(`scripts/build.ps1 -Config Debug`, `ctest --preset windows-test`)이 Conan 경로를 사용한다.
3. vcpkg 전용 bootstrap/toolchain 파일이 main에서 제거된다.
   - `scripts/setup_vcpkg.ps1`, `cmake/knights_vcpkg_toolchain.cmake`
4. vcpkg prewarm 워크플로우가 제거되고, 별도 prewarm 없이 required CI의 Conan cache 운영으로 단순화된다.
   - `.github/workflows/vcpkg-prewarm.yml` 삭제
5. sccache PoC가 Conan 기준으로 동작한다.
   - `.github/workflows/windows-sccache-poc.yml`
6. 문서에서 vcpkg를 "현재 기본"으로 기술한 구문이 제거된다.
7. `grep` 기준으로 운영 경로(vcpkg 키/스크립트 참조) 잔존이 없다.
8. 컷오버 후 연속 3회 PR CI green + 1회 main push CI green.

### 11.3 실행 스트림과 단계

#### Stream A - CI 기본 경로 Conan 전환 (최우선)

목표:

- required CI의 Windows 잡을 Conan으로 전환하되, **job name은 유지**해 branch protection 변경을 최소화한다.

수정 파일:

- `.github/workflows/ci.yml`

작업:

1. Windows job env에서 `VCPKG_*` 제거, `CONAN_HOME` 추가.
2. cache 키를 Conan 입력으로 교체.
   - `conan.lock`, `conanfile.py`, `conan/profiles/**`, `scripts/setup_conan.ps1`, `scripts/build.ps1`
3. Python + Conan 설치 단계(`conan>=2,<3`)와 lockfile 존재 검증 추가.
4. 빌드 커맨드를 Conan 경로로 전환.
   - `pwsh scripts/build.ps1 -UseConan -Config Debug`
5. 테스트 커맨드를 Conan preset으로 전환.
   - `ctest --preset windows-conan-test --output-on-failure`

게이트:

- required CI 4개 green 3연속.
- Windows job wall-clock이 baseline 대비 +20% 이내(캐시 warm 이후 기준).

롤백:

- `ci.yml` 단일 revert로 즉시 복귀 가능.

#### Stream B - Cache/SCCache 단순화 (Prewarm 제거)

목표:

- 별도 prewarm 워크플로우를 운영하지 않고, required CI + SCCache PoC를 Conan 캐시 구조로 단순화한다.

수정 파일:

- 수정: `.github/workflows/windows-sccache-poc.yml`
- 삭제: `.github/workflows/vcpkg-prewarm.yml`

작업:

1. `vcpkg-prewarm.yml` 제거:
   - prewarm 목적을 required CI cache 단계로 흡수
2. `windows-sccache-poc.yml` 전환:
   - vcpkg env/cache 제거
   - Conan preset(권장: `windows-conan` 또는 `windows-ninja` Conan 변형) 사용
3. telemetry 항목을 Conan cache 기준으로 재정의하고 required CI step summary에 집중.

게이트:

- `vcpkg-prewarm.yml` 제거 후 required CI 안정성 유지.
- `windows-sccache-poc` 성공 + second build hit-rate 개선 확인.

#### Stream C - 로컬 기본 경로 전환

목표:

- 개발자 기본 명령이 Conan을 타도록 바꾼다.

수정 파일:

- `scripts/build.ps1`
- `CMakePresets.json`
- `scripts/configure_windows_ninja.ps1` (preset 참조 시)

작업:

1. `scripts/build.ps1`에서 Windows 기본 경로를 Conan으로 전환.
   - `-UseConan` 없이도 Conan lane 사용
2. `windows`, `windows-client`, `windows-ninja` preset을 Conan 기반으로 교체.
3. 필요 시 1 사이클 동안만 `*-vcpkg-legacy` hidden preset 제공(단, main 운영 가이드는 Conan 기준).

게이트:

- 기존 습관 명령이 그대로 동작하면서 Conan 경로로 빌드 성공:
  - `pwsh scripts/build.ps1 -Config Debug`
  - `ctest --preset windows-test`

#### Stream D - vcpkg 자산 제거

목표:

- 코드/CI/문서의 vcpkg 운영 의존을 제거한다.

수정/삭제 파일:

- 삭제: `scripts/setup_vcpkg.ps1`
- 삭제: `cmake/knights_vcpkg_toolchain.cmake`
- 삭제(권장): `vcpkg.json`
- 수정: `.gitignore` (`external/vcpkg/`, `vcpkg_installed/` 등 정리)

작업:

1. build/script/workflow에서 vcpkg 참조 제거 확인.
2. 제거 커밋을 별도 원자 커밋으로 분리.
3. `grep` 기반 잔존 참조 점검.

검증 커맨드:

```powershell
git grep -n "setup_vcpkg|knights_vcpkg_toolchain|VCPKG_|vcpkg.json" -- . ":(exclude)docs/**"
```

게이트:

- 위 커맨드 결과가 비어 있어야 함(운영 경로 기준).

#### Stream E - 문서/운영 가이드 최종 정리

목표:

- 사용자/운영자가 더 이상 vcpkg를 기본 경로로 오해하지 않게 문서를 정합화한다.

수정 파일:

- `README.md`
- `core/README.md`
- `docs/db/write-behind.md`
- `docs/ops/ci-build-cache-optimization-report.md`
- `docs/ops/conan2-full-migration-plan.md` (본 문서)

작업:

1. 의존성 매니저 기본값을 Conan2로 일원화.
2. 윈도우/CI 실행 예시 명령을 Conan 기준으로 교체.
3. vcpkg는 "retired path"로만 언급.

게이트:

- docs 내 `vcpkg`는 historical note를 제외하고 제거.

### 11.4 권장 커밋 순서(원자 단위)

1. `ci.yml` Conan 전환
2. `vcpkg-prewarm.yml` 제거
3. `windows-sccache-poc.yml` Conan 전환
4. `scripts/build.ps1` + `CMakePresets.json` 기본 경로 전환
5. vcpkg 파일 삭제(`setup_vcpkg`, `knights_vcpkg_toolchain`, `vcpkg.json`)
6. README/ops/docs 정리

### 11.5 일정(권장)

- Day 1: Stream A (CI required Windows Conan 전환)
- Day 2: Stream B (prewarm 제거 + sccache 전환)
- Day 3: Stream C (로컬 기본값 전환)
- Day 4: Stream D (vcpkg 자산 제거)
- Day 5: Stream E (문서 정리 + 최종 green 확인)

### 11.6 최종 검증 패키지

로컬:

```powershell
pwsh scripts/build.ps1 -Config Debug
ctest --preset windows-test --output-on-failure
pwsh scripts/deploy_docker.ps1 -Action up -Detached -Build
python tests/python/verify_pong.py
python tests/python/test_load_balancing.py
python tests/python/verify_whisper_cross_instance.py
python tests/python/verify_admin_api.py
python tests/python/verify_admin_auth.py
python tests/python/verify_admin_control_plane_e2e.py
python tests/python/verify_admin_read_only.py
pwsh scripts/deploy_docker.ps1 -Action down
```

CI:

- required checks 3회 연속 green
- main push 1회 green
- Conan cache hit telemetry 확보

### 11.7 승인 기준 (Go / No-Go)

Go:

- 11.2 DoD 8항목 전부 충족
- required CI 안정성 확보(연속 green)

No-Go:

- Conan 전환 후 required job flaky 증가
- Windows 런타임 DLL 누락/링크 오류 재발
- 캐시 전략 부재로 CI 시간이 운영 한계 초과
