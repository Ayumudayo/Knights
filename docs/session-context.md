# 세션 컨텍스트 스냅샷 (Assistant 대화 요약)

마지막 업데이트: 2025-09-07

## 현재 방향성/결정 사항
- 명명: 특정 프로젝트명 금지(예: Knights). 범용 타깃/네임스페이스 사용.
- 우선순위: 서버 코어(server_core)와 샘플 서버(server_app) + CLI(dev_chat_cli) MVP 구현 완료(로그인/룸/브로드캐스트).
- 아키텍처: 모놀리식이 아닌 MSA 로드맵. 현재는 단일 프로세스 내에서 Gateway+Chat 역할을 통합.
- 프로토콜: 외부(클라↔서버)는 바이너리 프레이밍(14B 고정 헤더 v1.1). 내부는 gRPC(+버스)로 설계 예정.
- CLI: 백그라운드 수신/주기적 핑, 슬래시 명령(/login, /join, /leave, /rooms, /who, /say) 동작.

## 문서/산출물 현황
- server-architecture: docs/server-architecture.md (MSA 다이어그램/구성)
- msa-architecture: docs/msa-architecture.md (서비스 경계/통신/보안/관측성)
- core-design: docs/core-design.md (서버 코어 상세)
- protocol: docs/protocol.md (외부/내부 프로토콜)
- configuration: docs/configuration.md (설정/운영)
- naming-conventions: docs/naming-conventions.md (범용 명명 규칙)
- cli-client-design: docs/cli-client-design.md (CLI UX/자동완성 설계)

## 다음 할 일(중요도 순)
1) 에러 코드 표준화 문서 반영 및 핸들러 전면 적용(일부 완료)
2) 타임아웃/heartbeat 정책 문서화 및 K회 미수신 종료 구현(현재 read 타임아웃 재무장 완료)
3) LEAVE 후 상태 동기화/목록(/rooms, /who) 고도화(권한/표시 정밀화)
4) gRPC IDL(proto/*) 초안 및 Gateway 라우팅 규칙 상세(Pre/Post-Auth)
5) 부하/회귀 테스트 초안 추가(server_tests)

## 열린 쟁점/결정 대기
- 메시지 버스 선택(NATS/Kafka/Redis Streams)
- 내부 보안 체계(mTLS 배포/CA 운영, RBAC 범위)
- CLI UI 라이브러리(현행 단순 STDIO → replxx 등 도입 여부)

## 재개 체크포인트(다음 세션 시작 시 확인)
- ERR 표준화 적용 범위 점검, 코드/문서 싱크 확인
- proto IDL 저장 위치/네임스페이스 결정(`proto/` 권장) 및 최소 메서드 정의
- Gateway 라우팅 규칙과 세션 수명주기(on_close 정리) 확인

---
이 문서는 대화 컨텍스트 복원을 위한 요약입니다. 업데이트 시 위 "마지막 업데이트"만 갱신하세요.
