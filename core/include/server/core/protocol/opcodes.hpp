#pragma once

#include <cstdint>

namespace server::core::protocol {

// tools/gen_opcodes.py 로 자동 생성된 상수 정의
static constexpr std::uint16_t MSG_HELLO             = 0x0001; // 접속/핸드셰이크
static constexpr std::uint16_t MSG_PING              = 0x0002; // 하트비트 ping
static constexpr std::uint16_t MSG_PONG              = 0x0003; // 하트비트 pong
static constexpr std::uint16_t MSG_ERR               = 0x0004; // 오류 응답
static constexpr std::uint16_t MSG_LOGIN_REQ         = 0x0010; // 로그인 요청
static constexpr std::uint16_t MSG_LOGIN_RES         = 0x0011; // 로그인 응답
static constexpr std::uint16_t MSG_CHAT_SEND         = 0x0100; // 채팅 전송
static constexpr std::uint16_t MSG_CHAT_BROADCAST    = 0x0101; // 채팅 브로드캐스트
static constexpr std::uint16_t MSG_JOIN_ROOM         = 0x0102; // 방 참가
static constexpr std::uint16_t MSG_LEAVE_ROOM        = 0x0103; // 방 이탈
static constexpr std::uint16_t MSG_WHISPER_REQ       = 0x0104; // 귓속말 요청
static constexpr std::uint16_t MSG_WHISPER_RES       = 0x0105; // 귓속말 응답
static constexpr std::uint16_t MSG_WHISPER_BROADCAST = 0x0106; // 귓속말 중계
static constexpr std::uint16_t MSG_STATE_SNAPSHOT    = 0x0200; // 전체 상태 스냅샷
static constexpr std::uint16_t MSG_ROOM_USERS        = 0x0201; // 특정 방 사용자 목록

} // namespace server::core::protocol
