#pragma once
#include <stdint.h>
#include <string.h>

#define PROTO_MAGIC 0xA5 // to avoid collisions with other projects
#define PROTO_VERSION 1  // protocol version (1)

// Helper: Copy a C-string into a fixed buffer and guarantee NUL
template <size_t N>
void copy_cstr(char (&dst)[N], const char *src)
{
    const size_t W = N ? N - 1 : 0;
    size_t len = src ? strnlen(src, W) : 0;
    memcpy(dst, src ? src : "", len);
    dst[len] = '\0'; // ensure termination
}

template <size_t N, size_t M>
constexpr void copy_literal(char (&dst)[N], const char (&lit)[M])
{
    static_assert(N >= M, "destination too small for literal (incl. NUL)");
    memcpy(dst, lit, M); // copies the NUL too
}

enum : uint8_t
{
    CMD_DISPLAY_TEXT = 0x01, // payload uses `text[8]`
};

struct __attribute__((packed)) MsgV1
{
    uint8_t magic = PROTO_MAGIC; // 0xA5 marker
    uint8_t version = 1;         // protocol version (1)
    uint8_t cmd;                 // see enum above
    uint8_t flags;               // reserved/bitfield (0 for now)
    uint32_t seq;                // rolling sequence number
    uint8_t brightness;          // 0..7 (0=off)
    char segText[11];            // up to 8 chars plus max 2 for decimal points + NUL
    char lcdLine1[17];           // up to 16 chars + NUL
    char lcdLine2[17];           // up to 16 chars + NUL
};
static_assert(sizeof(MsgV1) == 54, "MsgV1 must be 54 bytes");

