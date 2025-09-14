-- 0003_identity.sql — Identity and constraints adjustments

-- users.name 고유 제약 제거(환경에 따라 제약명이 다를 수 있음)
alter table if exists users drop constraint if exists users_name_key;

-- rooms.name 고유 제약 제거(표준명 가정: rooms_name_key)
alter table if exists rooms drop constraint if exists rooms_name_key;

-- rooms 컬럼 추가(소프트 삭제/상태)
alter table if exists rooms
  add column if not exists is_active boolean not null default true,
  add column if not exists closed_at timestamptz;

-- sessions 컬럼 추가(운영 분석용)
alter table if exists sessions
  add column if not exists client_ip inet,
  add column if not exists user_agent text;

-- messages.user_id는 사용자 삭제 시 기록 보존을 위해 NULL 허용
alter table if exists messages alter column user_id drop not null;

