-- 0004_session_events.sql — Write-behind session events

create table if not exists session_events (
  id bigserial primary key,
  event_id text unique not null, -- Redis Streams XADD ID 또는 멱등 키
  type text not null,
  ts timestamptz not null,
  user_id uuid,
  session_id uuid,
  room_id uuid,
  payload jsonb
);

create index if not exists idx_session_events_user_ts on session_events(user_id, ts);
create index if not exists idx_session_events_session_ts on session_events(session_id, ts);
create index if not exists idx_session_events_type_ts on session_events(type, ts);

