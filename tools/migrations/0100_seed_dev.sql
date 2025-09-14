-- 0100_seed_dev.sql — Development seed (idempotent)
-- 목적: 로컬/스모크 테스트를 위해 최소 기본 데이터 생성
-- 운영 환경에서는 적용하지 않는 것을 권장(데이터 정책에 맞게 별도 seed 사용)

-- 1) 기본 공개 룸 'lobby' 보장
insert into rooms (id, name, is_public, is_active, created_at)
select gen_random_uuid(), 'lobby', true, true, now()
where not exists (
  select 1 from rooms where lower(name) = lower('lobby')
);

-- 2) 환영 메시지(중복 방지)
insert into messages (room_id, user_id, content, created_at)
select r.id, null, 'Welcome to lobby!', now()
from rooms r
where lower(r.name) = lower('lobby')
  and not exists (
    select 1 from messages m where m.room_id = r.id and m.content = 'Welcome to lobby!'
);

insert into messages (room_id, user_id, content, created_at)
select r.id, null, 'Type /rooms to list rooms, /who to list users.', now()
from rooms r
where lower(r.name) = lower('lobby')
  and not exists (
    select 1 from messages m where m.room_id = r.id and m.content like 'Type /rooms%'
);

