# 식별 전략(Identity Strategy)

목표: 엔터티를 안정적으로 식별하고, 이름 충돌/변경에도 혼동 없이 동작하도록 규칙을 정의한다.

## 원칙
- 시스템 식별자는 UUID이다: `user_id`, `room_id`, `session_id`는 모두 UUID (tools/migrations/0001_init.sql:8; tools/migrations/0001_init.sql:18; tools/migrations/0001_init.sql:50)
- 이름(`user.name`, `room.name`)은 라벨(label)일 뿐 식별자가 아니다 (tools/migrations/0003_identity.sql:4; tools/migrations/0003_identity.sql:7)
- 프로토콜/키/저장소에서 이름 의존을 피하고, 항상 UUID로 참조한다 (server/src/chat/handlers_login.cpp:139; server/src/chat/chat_service_core.cpp:202)

## 룸(Room)
- `rooms.id`(UUID)로 식별, `rooms.name`은 중복 허용 (tools/migrations/0001_init.sql:18; tools/migrations/0003_identity.sql:11)
- 삭제/종료를 표현하기 위해 `is_active`, `closed_at` 필드를 사용(소프트 삭제 성격) (tools/migrations/0003_identity.sql:11)
- 검색/보기를 돕기 위한 `name` 인덱스 제공(대소문자 무시/유사 검색) (tools/migrations/0002_indexes.sql:16; tools/migrations/0002_indexes.sql:17)

## 사용자(User)
- `users.id`(UUID)로 식별 (tools/migrations/0001_init.sql:8)
- 사용자명은 중복을 허용한다. 시스템 정합성은 UUID에 전적으로 의존하며, 검색은 `lower(name)`/trigram 인덱스를 사용한다. (tools/migrations/0002_indexes.sql:20; tools/migrations/0002_indexes.sql:21)

## 세션(Session)
- `sessions.id`(UUID)로 식별, 토큰은 해시로 저장(`token_hash`) (tools/migrations/0001_init.sql:50; server/src/chat/handlers_login.cpp:117)
- `client_ip`(inet), `user_agent`는 보조 속성(보안/운영 분석용) (tools/migrations/0003_identity.sql:16; tools/migrations/0003_identity.sql:17)

## 프로토콜/키 규칙
- 네트워크 프로토콜 페이로드는 `room_id`, `user_id`, `session_id` 등 UUID를 포함
- Redis 키는 모두 UUID 기반: `room:{room_id}:*`, `msg:{message_id}`, `session:{token_hash}` 등 (server/src/chat/handlers_chat.cpp:209; server/src/chat/session_events.cpp:84)

## 마이그레이션/호환
- 기존에 이름 고유 제약을 사용 중이라면, 점진적으로 완화(제약 삭제 → 인덱스로 대체)
- API/클라이언트는 이름 대신 ID를 주고받도록 전환(호환 기간 동안 리졸버 제공 가능)
