#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "server/core/protocol/opcodes.hpp"
#include "wire.pb.h"

// tools/gen_wire_codec.py 출력 기반의 헬퍼
namespace server { namespace wire { namespace codec {

// MsgId<T> 기본 템플릿
template<typename T>
constexpr std::uint16_t MsgId() { return 0; }

// MsgId<T> 특수화: Protobuf 타입 -> msg_id
template<> inline constexpr std::uint16_t MsgId<server::wire::v1::LoginRes>() { return 0x0011; }
template<> inline constexpr std::uint16_t MsgId<server::wire::v1::ChatBroadcast>() { return 0x0101; }
template<> inline constexpr std::uint16_t MsgId<server::wire::v1::WhisperResult>() { return 0x0105; }
template<> inline constexpr std::uint16_t MsgId<server::wire::v1::WhisperNotice>() { return 0x0106; }
template<> inline constexpr std::uint16_t MsgId<server::wire::v1::StateSnapshot>() { return 0x0200; }
template<> inline constexpr std::uint16_t MsgId<server::wire::v1::RoomUsers>() { return 0x0201; }

// 인코딩 헬퍼: Protobuf 메시지를 payload 바이트로 직렬화한다.
inline std::vector<std::uint8_t> Encode(const server::wire::v1::LoginRes& m) { std::string bytes; m.SerializeToString(&bytes); return {bytes.begin(), bytes.end()}; }
inline std::vector<std::uint8_t> Encode(const server::wire::v1::ChatBroadcast& m) { std::string bytes; m.SerializeToString(&bytes); return {bytes.begin(), bytes.end()}; }
inline std::vector<std::uint8_t> Encode(const server::wire::v1::WhisperResult& m) { std::string bytes; m.SerializeToString(&bytes); return {bytes.begin(), bytes.end()}; }
inline std::vector<std::uint8_t> Encode(const server::wire::v1::WhisperNotice& m) { std::string bytes; m.SerializeToString(&bytes); return {bytes.begin(), bytes.end()}; }
inline std::vector<std::uint8_t> Encode(const server::wire::v1::StateSnapshot& m) { std::string bytes; m.SerializeToString(&bytes); return {bytes.begin(), bytes.end()}; }
inline std::vector<std::uint8_t> Encode(const server::wire::v1::RoomUsers& m) { std::string bytes; m.SerializeToString(&bytes); return {bytes.begin(), bytes.end()}; }

// 디코딩 헬퍼: payload 바이트를 Protobuf 메시지로 역직렬화한다.
inline bool Decode(const void* data, std::size_t size, server::wire::v1::LoginRes& out) { return out.ParseFromArray(data, static_cast<int>(size)); }
inline bool Decode(const void* data, std::size_t size, server::wire::v1::ChatBroadcast& out) { return out.ParseFromArray(data, static_cast<int>(size)); }
inline bool Decode(const void* data, std::size_t size, server::wire::v1::WhisperResult& out) { return out.ParseFromArray(data, static_cast<int>(size)); }
inline bool Decode(const void* data, std::size_t size, server::wire::v1::WhisperNotice& out) { return out.ParseFromArray(data, static_cast<int>(size)); }
inline bool Decode(const void* data, std::size_t size, server::wire::v1::StateSnapshot& out) { return out.ParseFromArray(data, static_cast<int>(size)); }
inline bool Decode(const void* data, std::size_t size, server::wire::v1::RoomUsers& out) { return out.ParseFromArray(data, static_cast<int>(size)); }

// 유틸리티: msg_id 와 대응하는 타입명을 반환한다.
inline const char* TypeName(std::uint16_t id) {
    switch (id) {
        case 0x0011: return "server::wire::v1::LoginRes";
        case 0x0101: return "server::wire::v1::ChatBroadcast";
        case 0x0105: return "server::wire::v1::WhisperResult";
        case 0x0106: return "server::wire::v1::WhisperNotice";
        case 0x0200: return "server::wire::v1::StateSnapshot";
        case 0x0201: return "server::wire::v1::RoomUsers";
        default: return "(unknown)";
    }
}

}}} // namespace server::wire::codec
