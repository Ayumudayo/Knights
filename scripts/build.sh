#!/usr/bin/env bash
# 빌드/구성 스크립트 (Linux/WSL)
set -euo pipefail

GENERATOR="Unix Makefiles"
CONFIG="RelWithDebInfo"
BUILD_DIR="build-linux"
BOOST_ROOT_ENV="${BOOST_ROOT:-}"
CMD="all" # all|configure|build|clean|install|run-server|run-client
TARGET=""
PREFIX=""
PORT=5000
RELEASE_DIR=""
RELEASE_LIST="server_app,gateway_app,load_balancer_app,dev_chat_cli,wb_worker"
RELEASE_ARCHIVE=""

usage(){
  echo "사용: build.sh [옵션]"
  echo "  -g <generator>    (기본: $GENERATOR)"
  echo "  -c <config>       (Debug/Release/RelWithDebInfo) 기본: $CONFIG"
  echo "  -b <build-dir>    (기본: $BUILD_DIR)"
  echo "  -r <command>      all|configure|build|clean|install|run-server|run-client"
  echo "  -t <target>       빌드 타깃(기본: all/ALL_BUILD)"
  echo "  -p <prefix>       설치 경로(--install)"
  echo "  -P <port>         서버/클라이언트 포트 (기본: 5000)"
  echo "  -R <dir>          release 번들을 복사할 디렉터리(기본: 비활성)"
  echo "  -L <names>        release 대상 목록(콤마 구분, 예: server_app,...)"
  echo "  -z <archive>      release tar.gz 경로(선택)"
}

while getopts ":g:c:b:r:p:P:t:R:L:z:h" opt; do
  case $opt in
    g) GENERATOR="$OPTARG";;
    c) CONFIG="$OPTARG";;
    b) BUILD_DIR="$OPTARG";;
    r) CMD="$OPTARG";;
    p) PREFIX="$OPTARG";;
    P) PORT="$OPTARG";;
    t) TARGET="$OPTARG";;
    R) RELEASE_DIR="$OPTARG";;
    L) RELEASE_LIST="$OPTARG";;
    z) RELEASE_ARCHIVE="$OPTARG";;
    h) usage; exit 0;;
    :) echo "옵션 -$OPTARG 인자 필요"; usage; exit 2;;
    \?) echo "알 수 없는 옵션 -$OPTARG"; usage; exit 2;;
  esac
done

conf(){
  ARGS=( -S . -B "$BUILD_DIR" -G "$GENERATOR" -DCMAKE_BUILD_TYPE="$CONFIG" )
  if [[ -n "$BOOST_ROOT_ENV" ]]; then
    ARGS+=( "-DBOOST_ROOT=$BOOST_ROOT_ENV" )
  fi
  cmake "${ARGS[@]}"
}

build(){
  local t="$TARGET"
  if [[ -z "$t" ]]; then
    # VS/Ninja/Makefiles별 기본 타깃
    if [[ "$GENERATOR" == *"Visual Studio"* ]]; then t="ALL_BUILD"; else t="all"; fi
  fi
  cmake --build "$BUILD_DIR" --config "$CONFIG" --target "$t" -j
}

clean(){
  rm -rf "$BUILD_DIR"
}

install(){
  if [[ -z "$PREFIX" ]]; then echo "--install prefix를 -p로 지정하세요"; exit 2; fi
  cmake --install "$BUILD_DIR" --config "$CONFIG" --prefix "$PREFIX"
}

run_server(){
  exe="$BUILD_DIR/server/server_app"
  [[ -x "$exe" ]] || exe="$BUILD_DIR/server_app"
  [[ -x "$exe" ]] || { echo "server_app 실행 파일을 찾을 수 없습니다."; exit 1; }
  "$exe" "$PORT"
}

run_client(){
  exe="$BUILD_DIR/devclient/dev_chat_cli"
  [[ -x "$exe" ]] || exe="$BUILD_DIR/dev_chat_cli"
  [[ -x "$exe" ]] || { echo "dev_chat_cli 실행 파일을 찾을 수 없습니다."; exit 1; }
  "$exe" 127.0.0.1 "$PORT"
}

find_binary(){
  local name="$1"
  local ext=""
  [[ "$OS" == "Windows_NT" ]] && ext=".exe"
  local -a candidates=(
    "$BUILD_DIR/$name$ext"
    "$BUILD_DIR/$CONFIG/$name$ext"
  )
  for sub in server gateway load_balancer devclient tools wb; do
    candidates+=("$BUILD_DIR/$sub/$name$ext")
    candidates+=("$BUILD_DIR/$sub/$CONFIG/$name$ext")
  done
  for path in "${candidates[@]}"; do
    [[ -f "$path" ]] && { echo "$path"; return 0; }
  done
  local found
  found=$(find "$BUILD_DIR" -type f -name "$name$ext" -print -quit 2>/dev/null || true)
  [[ -n "$found" ]] && { echo "$found"; return 0; }
  return 1
}

release_bundle(){
  [[ -z "$RELEASE_DIR" ]] && return 0
  mkdir -p "$RELEASE_DIR"
  local folder="$RELEASE_DIR/release-$CONFIG"
  rm -rf "$folder"
  mkdir -p "$folder"
  IFS=',' read -ra names <<< "$RELEASE_LIST"
  for raw in "${names[@]}"; do
    local name
    name="$(echo "$raw" | xargs)"
    [[ -z "$name" ]] && continue
    local bin
    bin=$(find_binary "$name") || { echo "[warn] release 대상 바이너리를 찾지 못했습니다: $name" >&2; continue; }
    cp "$bin" "$folder/"
  done
  if [[ -n "$RELEASE_ARCHIVE" ]]; then
    tar -C "$folder" -czf "$RELEASE_ARCHIVE" .
    echo "[info] release archive: $RELEASE_ARCHIVE"
  fi
  echo "[info] release bundle: $folder"
}

case "$CMD" in
  all) conf; build ;;
  configure) conf ;;
  build) build ;;
  clean) clean ;;
  install) install ;;
  run-server) run_server ;;
  run-client) run_client ;;
  *) echo "알 수 없는 명령: $CMD"; usage; exit 2;;
esac

if [[ -n "$RELEASE_DIR" ]]; then
  if [[ "$CMD" == "all" || "$CMD" == "build" ]]; then
    release_bundle
  else
    echo "[warn] release 옵션은 all/build 명령에서만 사용할 수 있습니다." >&2
  fi
fi

echo "완료"
