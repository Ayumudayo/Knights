-- 0002_indexes.sql — Indexes and extensions
-- 중요: CONCURRENTLY 구문이 있으므로 트랜잭션 블록 밖에서 실행해야 합니다.

-- 유사 검색용 확장
create extension if not exists pg_trgm;

-- memberships 조회 성능
create index concurrently if not exists idx_memberships_room on memberships(room_id, user_id);
create index concurrently if not exists idx_memberships_user on memberships(user_id, room_id);

-- messages 페이징/히스토리
create index concurrently if not exists idx_messages_room_id_id on messages(room_id, id);
create index concurrently if not exists idx_messages_user_id_created on messages(user_id, created_at);

-- rooms 이름 검색(대소문자 무시/유사 검색)
create index concurrently if not exists idx_rooms_name_ci on rooms (lower(name));
create index concurrently if not exists idx_rooms_name_trgm on rooms using gin (lower(name) gin_trgm_ops);

-- users 이름 검색(대소문자 무시/유사 검색)
create index concurrently if not exists idx_users_name_ci on users (lower(name));
create index concurrently if not exists idx_users_name_trgm on users using gin (lower(name) gin_trgm_ops);

-- sessions 만료/사용자 조회
create index concurrently if not exists idx_sessions_user on sessions(user_id, created_at);
create index concurrently if not exists idx_sessions_expires on sessions(expires_at);

