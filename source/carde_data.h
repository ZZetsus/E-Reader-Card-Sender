#ifndef CARDE
#define CARDE

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CARD_LIST_SIZE 24
#define CHALLENGE_VALUE_USA 0x29F6F106
#define CHALLENGE_VALUE_JPN 0xB8EB4E96

typedef struct {
    const uint32_t* data;
    const char* label;
    uint32_t crc32_usa;
    uint32_t crc32_jpn;
    int index;
    int size;
} CardEntry;

extern const CardEntry card_list[];
#ifdef __cplusplus
}
#endif

#endif // CARDE