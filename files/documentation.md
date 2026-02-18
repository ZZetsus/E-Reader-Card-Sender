# E-Reader Card Sender
E-Reader Card Sender is a project designed to transmit E-Card data (specifically Japanese E-Cards+) through the Game Boy Advance link port.

The primary goal is to obtain the three Shadow Pokémon that are otherwise region-locked or require expensive, hard-to-find physical cards. While printing Dot Codes on photo paper is an alternative, this tool provides a digital solution for those without access to high-quality printing materials.

Protocol and Handshake
The communication begins with an initial handshake where the GBA sends 4 bytes to identify the region of the Pokémon Colosseum Z80 program (E-Reader+).

Region ID: In the North American version, the GBA responds with `0x47434536` = `GCE6`, which is the GameCube ID for the US release.

GameCube Response: The GameCube apparently responds with the region ID in lowercase: `0x67633665` = `gc6e`.

Following this, the GameCube sends multiple "polls" while waiting for the GBA to signal that the card data has been read and stored in memory. The process involves the GameCube sending a 4-byte random seed `key_random_tick = os::OSGetTick();`. The GBA receives these 4 bytes and proceeds to encode the decompressed card data (starting at offset 0x51) using a two-layer encryption process.

Endianness Note: All bytes sent by the GameCube are Big-Endian, while the GBA uses Little-Endian.

Big-Endian: `0x12345678`

Little-Endian: `0x78563412`

# Encoding Layers

## Credits & Tools
* **Decompression:** Card decompression was performed using the `nedcmake` / `nedcenc` tools created by [caitsith2](https://www.caitsith2.com/ereader/devtools.htm/).
* **Encoding:** The Layer 1 and Layer 2 obfuscation logic was reverse-engineered from the Pokémon Colosseum.

### Layer 0: Bin Data
This layer consists of the pure, decompressed card data without any modifications.

### Layer 1: CRC32 Obfuscation
This layer uses a CRC32 table and a specific formula to encode each 4-byte chunk:

```
C++
card_4byte = card_data[byte_card_send];

real_4byte = (card_4byte ^ 
             (new_mask << 0x18 | 
             (new_mask & 0xFF00) << 8 | 
             CHALLENGE_BYTE >> 0x18 | 
             (new_mask & 0xFF0000) >> 8));
card_data[]: Array containing the raw data.
```

`card_4byte`: The current 4-byte chunk being encoded.

`new_mask`: A dynamic mask. The original Z80 program selects a random number between 0-127 to increase entropy. To simplify the implementation, this project uses a fixed seed where new_mask = MASK >> 8 ^ CHALLENGE_BYTE.

`MASK`: A constant used by Pokémon Colosseum: 0xAA478422.

`CHALLENGE_BYTE`: Calculated as `crc32Table_4bytes[(MASK ^ 0x00000000) & 0xFF]`, where `0x00000000` represents the seed (0-127).

This results in an encoding chain where each chunk depends on the previous state, repeating for all 1038 iterations.

After the data transfer, a Checksum (CRC32) is sent to verify integrity. These 4 bytes are also encoded via Layer 1. The checksum is pre-calculated by passing the raw card bytes through a custom CRC32 function initialized with the Game ID in uppercase `GAMEID = 0x47433645`. This is handled by a Python script for convenience.

Finally, a Challenge Byte is sent, calculated by swapping the endianness of a `SEED_CANDIDATE` derived from the `GAMEID` and a fixed seed `0x00000000`.

### Layer 2: XOR Masking
The final step involves a XOR operation between the Layer 1 encoded 4-byte chunks and the `GC_TICK` (the random number provided by the GameCube):

```
C++
Byte4_Layer2 = Byte4_Layer1 ^ GC_TICK;
```
Communication Flow Summary
Card Data (Layer 0-2): 1,038 iterations (Indices 0 to 1037).

Checksum: 1 iteration (Indices 1037 to 1038).

Challenge 4-Bytes: 1 iteration (Indices 1038 to 1039).

