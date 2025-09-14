-- 0005_messages_room_name.sql — Denormalize room_name into messages

ALTER TABLE messages
  ADD COLUMN IF NOT EXISTS room_name text;

