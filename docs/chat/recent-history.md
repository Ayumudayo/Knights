# Recent Room History on Join (Redis-backed)

목표: 사용자가 방에 접속할 때 최근 채팅 20건을 빠르게 로드해 표시하고, 순서/중복 문제를 예방하기 위해 스냅샷 완료 전까지 입력을 비활성화한다.

## 데이터 원천 및 키
- SoR: Postgres(`messages` 테이블, `id`=bigserial, created_at/updated_at 기준)
- Redis 캐시:
  - `room:{room_id}:recent` — LIST/ZSET of `message_id` (최신 → 오래된 순) (server/src/chat/handlers_chat.cpp:209)
  - `msg:{message_id}` — JSON `{ id, room_id, user_id, content, created_at_ms, sender_sid }`
  - TTL: `CACHE_TTL_RECENT_MSGS` (기본 6h, 환경에 따라 1~24h 조정)

## 서버 알고리즘(Join 시)
1. **워터마크 계산**: `WM = MAX(messages.id)`를 캐시하거나 `room_last_id` 테이블에서 읽어온다. (server/src/chat/chat_service_core.cpp:262)
2. **최근 ID 조회**: `LRANGE room:{room_id}:recent -RECENT_HISTORY_LIMIT -1` 또는 `ZREVRANGE`. 결과를 오름차순 정렬해 안정된 순서를 만든다.
3. **본문 조회**: `MGET msg:{id}...` 혹은 pipeline `HGETALL`. 누락된 id는 Postgres에서 `SELECT ... WHERE room_id=? AND id IN (...)`로 로드한 뒤 Redis에 `SETEX msg:{id}`, `LPUSH/LTRIM`으로 보강한다.
4. **스냅샷 생성**: `MSG_STATE_SNAPSHOT` payload에 `[messages...]`, `room_id`, `wm`을 채워 전송한다. (server/src/chat/chat_service_core.cpp:213)
5. **브로드캐스트와의 연결**: 스냅샷 이후 도착하는 메시지 중 `id <= wm`은 중복이므로 서버·클라이언트에서 모두 드롭한다. `id > wm`만 신규로 표시한다.
6. **캐시 쓰기 정책**: 새 메시지는 Postgres에 기록 후 Redis에 `LPUSH room:{room_id}:recent id`와 `LTRIM ... 0 ROOM_RECENT_MAXLEN-1`. 본문은 `SETEX msg:{id}`로 write-through 한다.

## 클라이언트(UI) 지침
- 방 입장 요청 이후 스냅샷이 도착할 때까지 입력창/전송 버튼을 비활성화한다.
- `snapshot_complete` 신호를 받으면 UI를 활성화하고, 내부적으로 `max_id`를 저장해 `id <= max_id` 메시지를 무시한다.
- 스크롤을 최상단으로 고정하고, 사용자가 스크롤 업을 시도하면 서버에 추가 로드를 요청한다.

## 장애 및 폴백
- **Redis miss**: Postgres에서 `ORDER BY id DESC LIMIT RECENT_HISTORY_LIMIT`로 조회한 뒤 역정렬 해 전송하고, Redis 캐시를 재구축한다.
- **Redis 과부하**: 최근 N(예: 10)건으로 축소하고 `CACHE_TTL_RECENT_MSGS`를 줄여 메모리 사용량을 제한한다.
- **데이터 누락**: 브로드캐스트에서 gap이 감지되면 클라이언트가 `MSG_STATE_SNAPSHOT_REQ`를 재전송해 부분 스냅샷을 받도록 한다.

## 운영 값
- `RECENT_HISTORY_LIMIT` (기본 20)
- `CACHE_TTL_RECENT_MSGS`
- `ROOM_RECENT_MAXLEN` (기본 200)

## 보안/프라이버시
- 메시지 JSON에는 민감 정보(비밀번호, 토큰)를 포함하지 않는다.
- 필요 시 서버 측에서 마스킹/필터를 적용한 뒤 캐시에 적재한다.

## 장단점
- **장점**: 빠른 체감 로딩, RDB 부하 감소.
- **단점**: 캐시 일관성 관리 비용 증가, 복구 시 Postgres 쿼리 필요.
