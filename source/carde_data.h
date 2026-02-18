#ifndef CARDE
#define CARDE

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CARD_LIST_SIZE 5

typedef struct {
    const uint32_t* data;
    const char* label;
    uint32_t crc32;
    int index;
    int size;
} CardEntry;

extern const CardEntry card_list[];

const uint32_t challenge_4bytes = 0x29F6F106;
#ifdef __cplusplus
}
#endif

#endif // CARDE