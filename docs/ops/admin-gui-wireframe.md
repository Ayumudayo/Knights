# Admin GUI MVP IA / Wireframe

본 문서는 Phase 1(read-only) 기준 운영 GUI 정보구조(IA)와 화면 와이어프레임을 정의한다.

관련 문서:

- 아키텍처: `docs/ops/admin-console.md`
- API 계약: `docs/ops/admin-api-contract.md`

## 1. IA (Information Architecture)

MVP 네비게이션:

1. Dashboard
2. Instances
3. Session Lookup
4. Write-behind
5. Metrics Links

공통 UI 요소:

- 상단 환경 배지 (env/project)
- 마지막 갱신 시각
- 전역 검색(세션/인스턴스 id)
- 자동 새로고침 토글 (5s/10s/30s)

## 2. 화면별 와이어프레임

## 2.1 Dashboard

목적:

- 운영자가 첫 화면에서 전체 상태를 5초 내 파악

구성:

- Summary cards
  - gateway up/ready
  - server up/ready
  - wb_worker up/ready
  - haproxy up
- 경고 배너
  - not ready instance 존재 시 강조
- 최근 에러 카운터
  - `admin_http_errors_total`

API 매핑:

- `GET /api/v1/overview`

## 2.2 Instances

목적:

- backend registry 상태 점검

구성:

- 테이블 컬럼
  - instance_id
  - host:port
  - role
  - ready
  - active_sessions
  - last_heartbeat
- 정렬
  - active_sessions desc/asc
  - last_heartbeat 최신순
- 행 상세 패널
  - `/readyz` reason
  - metrics URL 링크

API 매핑:

- `GET /api/v1/instances`
- `GET /api/v1/instances/{instance_id}`

## 2.3 Session Lookup

목적:

- 특정 client_id가 어느 backend에 sticky 되었는지 확인

구성:

- 입력 폼
  - `client_id`
- 결과 카드
  - backend_instance_id
  - backend ready 상태
  - registry key/session key

API 매핑:

- `GET /api/v1/sessions/{client_id}`

## 2.4 Write-behind

목적:

- DB 적재 지연/실패 추세 파악

구성:

- KPI 카드
  - pending
  - flush ok/fail
  - dlq
  - ack fail
- 시계열 미니 차트 (Phase 1은 단순 값 + delta)

API 매핑:

- `GET /api/v1/worker/write-behind`

## 2.5 Metrics Links

목적:

- Grafana/Prometheus로 빠르게 점프

구성:

- 링크 목록
  - server dashboard
  - write-behind dashboard
  - infra dashboard
- 쿼리 템플릿 복사

API 매핑:

- `GET /api/v1/metrics/links`

## 3. 상태/에러 UX

1. 로딩 상태
   - skeleton + 마지막 정상 응답 시각 유지
2. 일시적 장애
   - toast + 자동 재시도 백오프
3. 권한 오류
   - 401/403 별도 안내
4. Not found
   - Session Lookup에서 빈 결과를 정상 UX로 표시

## 4. 반응형 기준

- Desktop first
- Tablet/모바일에서는 카드 우선 배치, 테이블은 축약 컬럼 표시

## 5. 접근성/운영성

- 색상 외 상태 표기(아이콘+텍스트)
- 키보드 포커스 이동 가능
- 타임스탬프는 UTC 기준 고정 표시

## 6. 구현 우선순위

Phase 1 순서:

1. Dashboard
2. Instances
3. Session Lookup
4. Write-behind
5. Metrics Links
