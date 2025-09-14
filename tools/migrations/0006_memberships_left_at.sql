-- 0006_memberships_left_at.sql — add is_member/left_at to memberships

alter table memberships
  add column if not exists is_member boolean not null default true,
  add column if not exists left_at timestamptz;

