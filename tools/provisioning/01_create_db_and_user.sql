-- 01_create_db_and_user.sql
-- 목적: 새 데이터베이스와 로그인 가능한 애플리케이션 유저를 생성한다.
-- 실행 컨텍스트: postgres DB (슈퍼유저)
-- 인자(psql -v): APP_DB, APP_USER, APP_PASS

-- 안전한 식별자/리터럴 처리를 위해 PL/pgSQL + format 사용
DO
$$
DECLARE
  v_db   text := :'APP_DB';
  v_user text := :'APP_USER';
  v_pass text := :'APP_PASS';
BEGIN
  -- Role 생성(없을 때만)
  IF NOT EXISTS (SELECT 1 FROM pg_roles WHERE rolname = v_user) THEN
    EXECUTE format('CREATE ROLE %I LOGIN PASSWORD %L NOSUPERUSER NOCREATEDB NOCREATEROLE NOINHERIT', v_user, v_pass);
  END IF;

  -- Database 생성(없을 때만), 소유자는 앱 유저
  IF NOT EXISTS (SELECT 1 FROM pg_database WHERE datname = v_db) THEN
    EXECUTE format('CREATE DATABASE %I OWNER %I ENCODING %L LC_COLLATE %L LC_CTYPE %L TEMPLATE %I',
                   v_db, v_user, 'UTF8', 'C', 'C', 'template0');
  END IF;
END
$$ LANGUAGE plpgsql;

-- DB 접속 권한(명시, 소유자라면 암묵적으로 보유)
GRANT CONNECT ON DATABASE :APP_DB TO :APP_USER;

