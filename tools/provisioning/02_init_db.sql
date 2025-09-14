-- 02_init_db.sql
-- 목적: 새 DB에서 확장/권한/스키마 기본 설정을 수행한다.
-- 실행 컨텍스트: 대상 애플리케이션 DB (관리자/소유자)
-- 인자(psql -v): APP_DB, APP_USER

-- DB 레벨: PUBLIC에 대한 과도한 권한 제한(선택)
REVOKE ALL ON DATABASE :APP_DB FROM PUBLIC;
GRANT CONNECT ON DATABASE :APP_DB TO :APP_USER;

-- 확장: 애플리케이션이 기대하는 확장 설치
CREATE EXTENSION IF NOT EXISTS pgcrypto;
CREATE EXTENSION IF NOT EXISTS pg_trgm;

-- 스키마: public 사용 또는 별도 스키마 생성
-- 여기서는 public 스키마를 사용하되, 불필요한 CREATE 권한 제한
REVOKE CREATE ON SCHEMA public FROM PUBLIC;
GRANT USAGE ON SCHEMA public TO :APP_USER;

-- search_path(선택): 애플리케이션 유저의 기본 search_path 지정
ALTER ROLE :APP_USER IN DATABASE :APP_DB SET search_path = public;

-- 기본 오브젝트에 대한 권한 템플릿(소유자가 :APP_USER라면 불필요하지만 안전 차원에서 부여)
-- 테이블 접근 권한
GRANT SELECT, INSERT, UPDATE, DELETE ON ALL TABLES IN SCHEMA public TO :APP_USER;
-- 시퀀스 접근 권한
GRANT USAGE, SELECT, UPDATE ON ALL SEQUENCES IN SCHEMA public TO :APP_USER;

-- 향후 생성 오브젝트의 기본 권한(소유자 세션에서 실행되어야 적용)
ALTER DEFAULT PRIVILEGES IN SCHEMA public GRANT SELECT, INSERT, UPDATE, DELETE ON TABLES TO :APP_USER;
ALTER DEFAULT PRIVILEGES IN SCHEMA public GRANT USAGE, SELECT, UPDATE ON SEQUENCES TO :APP_USER;

