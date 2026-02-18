

#include <gba.h>
#include <stdio.h>
#include <cstdint>
#include <cstring>

// E-CARDS

#include "crc32.h"
#include "carde_data.h"

// #include "_link_common.hpp"

volatile bool resetFlag = false;
volatile bool card_selected  = false;
volatile bool scan_card = false;
volatile bool modo_tarjeta_activo = false;
volatile uint32_t GC_TICK;

// I/O Registers

constexpr u32 _REG_BASE = 0x04000000;

inline vu16& _REG_RCNT = *reinterpret_cast<vu16*>(_REG_BASE + 0x0134);
inline vu16& _REG_SIOCNT = *reinterpret_cast<vu16*>(_REG_BASE + 0x0128);
inline vu32& _REG_SIODATA32 = *reinterpret_cast<vu32*>(_REG_BASE + 0x0120);
inline vu16& _REG_SIODATA8 = *reinterpret_cast<vu16*>(_REG_BASE + 0x012A);
inline vu16& _REG_SIOMLT_SEND = *reinterpret_cast<vu16*>(_REG_BASE + 0x012A);
inline vu16* const _REG_SIOMULTI = reinterpret_cast<vu16*>(_REG_BASE + 0x0120);
inline vu16& _REG_JOYCNT = *reinterpret_cast<vu16*>(_REG_BASE + 0x0140);
inline vu16& _REG_JOY_RECV_L = *reinterpret_cast<vu16*>(_REG_BASE + 0x0150);
inline vu16& _REG_JOY_RECV_H = *reinterpret_cast<vu16*>(_REG_BASE + 0x0152);
inline vu16& _REG_JOY_TRANS_L = *reinterpret_cast<vu16*>(_REG_BASE + 0x0154);
inline vu16& _REG_JOY_TRANS_H = *reinterpret_cast<vu16*>(_REG_BASE + 0x0156);
inline vu16& _REG_JOYSTAT = *reinterpret_cast<vu16*>(_REG_BASE + 0x0158);
inline vu16& _REG_VCOUNT = *reinterpret_cast<vu16*>(_REG_BASE + 0x0006);
inline vu16& _REG_KEYS = *reinterpret_cast<vu16*>(_REG_BASE + 0x0130);
inline vu16& _REG_TM1CNT_L = *reinterpret_cast<vu16*>(_REG_BASE + 0x0104);
inline vu16& _REG_TM1CNT_H = *reinterpret_cast<vu16*>(_REG_BASE + 0x0106);
inline vu16& _REG_TM2CNT_L = *reinterpret_cast<vu16*>(_REG_BASE + 0x0108);
inline vu16& _REG_TM2CNT_H = *reinterpret_cast<vu16*>(_REG_BASE + 0x010A);
inline vu16& _REG_IME = *reinterpret_cast<vu16*>(_REG_BASE + 0x0208);

static constexpr int BIT_CMD_RESET = 0;
static constexpr int BIT_CMD_RECEIVE = 1;
static constexpr int BIT_CMD_SEND = 2;
static constexpr int BIT_CMD_GENERAL_PURPOSE_4 = 4;
static constexpr int BIT_CMD_GENERAL_PURPOSE_5 = 5;

static constexpr int BIT_STAT_SEND = 3;
static constexpr int BIT_STAT_RECEIVE = 1;

static constexpr int BIT_IRQ = 6;
static constexpr int BIT_JOYBUS_HIGH = 14;
static constexpr int BIT_GENERAL_PURPOSE_LOW = 1|   4;
static constexpr int BIT_GENERAL_PURPOSE_HIGH = 15;

uint32_t GAMEID     = 0x47433645;
uint32_t MASK       = 0xAA478422;

uint32_t SEED_CANDIDATE;

uint32_t card_layer1[1040];
uint16_t byte_card_send = 0;

uint32_t byteMask1;
uint32_t byteMask2;
uint32_t byteMask3;
uint32_t last_byte;
uint32_t mask_renewed;
uint32_t mask_renewed2;
uint32_t mask_renewed3;
uint32_t real_4byte;

uint8_t trans_h_h;
uint8_t trans_h_l;
uint8_t trans_l_h;
uint8_t trans_l_l;

uint8_t byte;
uint8_t real_4byte_layer2;

uint8_t index_array;

uint32_t _JOY_RECV_LOCAL_L;
uint32_t _JOY_RECV_LOCAL_H;

uint32_t CHALLENGE_BYTE = crc32Table_4bytes[(MASK ^ 0x00000000) & 0xFF];
uint32_t new_mask = MASK >> 8 ^ CHALLENGE_BYTE;

uint32_t next_data_L = 0;
uint32_t next_data_H = 0;

uint32_t basura;

//Params
uint32_t BYTE4_CHALLENGE = challenge_4bytes;

// Card controller
const uint32_t* card_data;
uint32_t card_4byte;

// Card list and metadata
const char* label_card;
uint32_t crc32_card;
uint32_t card_selected_index;
int card_selected_size;

//List
uint8_t items_index[CARD_LIST_SIZE];
const char* items_data_names[CARD_LIST_SIZE];
int index_list;

bool isBitCNTHigh(u8 bit) { return (_REG_JOYCNT >> bit) & 1; }
bool isBitSTATHigh(u8 bit) { return (_REG_JOYSTAT >> bit) & 1; }

void setBitHigh(u8 bit) { _REG_JOYCNT |= 1 << bit; }
void setBitLow(u8 bit) { _REG_JOYCNT &= ~(1 << bit); }

void setJOYCNT(u8 byte) { _REG_JOYCNT = byte; }

void setTransL(u16 word) { _REG_JOY_TRANS_L = word; }
void setTransH(u16 word) { _REG_JOY_TRANS_H = word; }

void setJOYSTAT(u16 word) { _REG_JOYSTAT = word; }

void setInterruptsOn() { setBitHigh(BIT_IRQ); }
void setInterruptsOff() { setBitLow(BIT_IRQ); }



void setJoybusMode(bool mode = true) {
    if (mode)
    _REG_RCNT = _REG_RCNT | (1 << BIT_JOYBUS_HIGH) |
                    (1 << BIT_GENERAL_PURPOSE_HIGH);
    else
    _REG_RCNT = 0xC000;
}

void setGeneralPurposeMode(bool mode = true) {
    if (mode)
    _REG_RCNT = (_REG_RCNT & ~(1 << BIT_GENERAL_PURPOSE_LOW)) |
                        (1 << BIT_GENERAL_PURPOSE_HIGH);
    else
    _REG_RCNT = 0x8000;
}

bool didReset(bool clear = true) {
    bool reset = resetFlag;
    if (clear)
      resetFlag = false;
    return reset;
}

IWRAM_CODE void prepare_word() {

    // printf("GC_TICK: %08X\n", (unsigned int)GC_TICK);

    if (byte_card_send < card_selected_size){
        
        card_4byte = card_data[byte_card_send];

        real_4byte = (card_4byte ^
                    (new_mask << 0x18 | 
                    (new_mask & 0xFF00) << 8 | CHALLENGE_BYTE >> 0x18 | (new_mask & 0xFF0000) >> 8)) ^ GC_TICK;

        next_data_L = ((((real_4byte >> 16) & 0xFFFF) >> 8) & 0x00FF) | ((((real_4byte >> 16) & 0xFFFF) << 8) & 0xFF00);
        next_data_H = (((real_4byte & 0xFFFF) >> 8) & 0x00FF) | (((real_4byte & 0xFFFF) << 8) & 0xFF00);
        
        byte_card_send = byte_card_send + 1;

        return;

    } 
    
    if (byte_card_send >= card_selected_size && byte_card_send <= 1037){

        // printf("2");

        card_4byte = 0x00000000;
        real_4byte = (card_4byte ^
                    (new_mask << 0x18 | 
                    (new_mask & 0xFF00) << 8 | CHALLENGE_BYTE >> 0x18 | (new_mask & 0xFF0000) >> 8)) ^ GC_TICK;
                    
        next_data_L = ((((real_4byte >> 16) & 0xFFFF) >> 8) & 0x00FF) | ((((real_4byte >> 16) & 0xFFFF) << 8) & 0xFF00);
        next_data_H = (((real_4byte & 0xFFFF) >> 8) & 0x00FF) | (((real_4byte & 0xFFFF) << 8) & 0xFF00);

        byte_card_send = byte_card_send + 1;
        
        return;
    }

    // if CRC
    if (byte_card_send == 1038){

        real_4byte = (crc32_card ^
                    (new_mask << 0x18 | 
                    (new_mask & 0xFF00) << 8 | CHALLENGE_BYTE >> 0x18 | (new_mask & 0xFF0000) >> 8)) ^ GC_TICK;

        next_data_L = ((((real_4byte >> 16) & 0xFFFF) >> 8) & 0x00FF) | ((((real_4byte >> 16) & 0xFFFF) << 8) & 0xFF00);
        next_data_H = (((real_4byte & 0xFFFF) >> 8) & 0x00FF) | (((real_4byte & 0xFFFF) << 8) & 0xFF00);

        byte_card_send = byte_card_send + 1;

        // printf("BYTE4_CHALLENGE: %08X\n", (unsigned int)BYTE4_CHALLENGE);
        return;
    }

    // if BYTE_CHALLENGE
    if (byte_card_send == 1039){

        next_data_L = ((((BYTE4_CHALLENGE >> 16) & 0xFFFF) >> 8) & 0x00FF) | ((((BYTE4_CHALLENGE >> 16) & 0xFFFF) << 8) & 0xFF00);
        next_data_H = (((BYTE4_CHALLENGE & 0xFFFF) >> 8) & 0x00FF) | (((BYTE4_CHALLENGE & 0xFFFF) << 8) & 0xFF00);

        byte_card_send = byte_card_send + 1;

        // printf("BYTE4_CHALLENGE: %08X\n", (unsigned int)BYTE4_CHALLENGE);
        return;
    }
    

    
    
}

IWRAM_CODE void respond_quickly() {

    _JOY_RECV_LOCAL_L = _REG_JOY_RECV_L;
    _JOY_RECV_LOCAL_H = _REG_JOY_RECV_H;

    _REG_JOY_TRANS_L = next_data_L;
    _REG_JOY_TRANS_H = next_data_H;
    

    if (byte_card_send > 1039) {


        printf("num: %d", byte_card_send);

        printf("card: %d", card_selected_size);

        modo_tarjeta_activo = false;
        _REG_JOYSTAT = 0x0000;
        return;
    }

    prepare_word();

}

void stop() {

    // setInterruptsOff();
    setGeneralPurposeMode(false);
 
}

void start() {
    
    setJoybusMode(false);
    // setInterruptsOn();
}


void interrupt_init(void) {
    irqInit();
}

void configurePacketTransfer(){

    setTransL(0x4347);
    setTransH(0x4536);
    setJOYSTAT(0x0000);
    setJOYCNT(0x0041);

}

void resetComunication() {

    stop();
    start();
    setJOYSTAT(0x0000);
    setTransL(0x0000);
    setTransH(0x0000);
    setJOYCNT(0x0047);

    for(int i=0; i<1000000; i++) __asm__("nop");
    
}

void limpiarConsola() {

    printf("\x1b[2J\x1b[H");
}

void actualizarPantalla() {
    printf("\x1b[2;1H%-15s", items_data_names[index_list]);
}

void manejarEntrada() {

    scanKeys();
    u16 keys = keysDown();

    // Botón R (Derecha) - Avanzar
    if (keys & KEY_R) {
        index_list++;
        if (index_list >= CARD_LIST_SIZE) {
            index_list = 0;
        }
        actualizarPantalla();
    }

    // Botón L (Izquierda) - Retroceder
    if (keys & KEY_L) {
        index_list--;
        if (index_list < 0) {
            index_list = CARD_LIST_SIZE - 1;
        }
        actualizarPantalla();
    }

    if (keys & KEY_A) {

        label_card = card_list[index_list].label;
        crc32_card = card_list[index_list].crc32;
        card_selected_size = card_list[index_list].size;
        card_data = card_list[index_list].data;

        card_selected_index = index_list;

        card_selected = true;

        printf("\n Card selected: %s\n", items_data_names[index_list]);
        
    }
}



void _onSerial() {

    if (modo_tarjeta_activo == true){
         
        if (isBitSTATHigh(BIT_CMD_GENERAL_PURPOSE_5) && isBitCNTHigh(BIT_CMD_SEND)) {

            respond_quickly();
            _REG_JOYCNT = 0x0044;

        } else if (isBitSTATHigh(BIT_CMD_GENERAL_PURPOSE_5) && _REG_JOYCNT == 0x42) {

            if (isBitSTATHigh(BIT_STAT_RECEIVE)){

                uint16_t low_fixed = ((_REG_JOY_RECV_L >> 8) & 0xFF) | ((_REG_JOY_RECV_L << 8) & 0xFF00);
                uint16_t high_fixed = ((_REG_JOY_RECV_H >> 8) & 0xFF) | ((_REG_JOY_RECV_H << 8) & 0xFF00);

                GC_TICK = (low_fixed << 16) | high_fixed;
                
                BYTE4_CHALLENGE = BYTE4_CHALLENGE ^ GC_TICK;


                // printf("A001JPN[1039]: %08X\n", (unsigned int)A001JPN[1039]);

                prepare_word();
                
            }

            respond_quickly();

            _REG_JOYCNT = 0x0042;

        }
        return;
    }
    
    
    if (isBitCNTHigh(BIT_CMD_RESET) && ((_REG_JOYSTAT & 0x0008) || (_REG_JOYSTAT == 0)) ) {

        _REG_JOY_TRANS_L = 0x4347;
        _REG_JOY_TRANS_H = 0x4536;
        _REG_JOYSTAT = 0x0000;
        _REG_JOYCNT = 0x0041;

        printf("\n entra a reset!\n"); 
        // resetFlag = true;

        // configurePacketTransfer();

    } else if (isBitCNTHigh(BIT_CMD_SEND) && (_REG_JOYSTAT == 0)) {
        _REG_JOYCNT = 0x0044;
        printf("\n entra a send!\n");

    } else if (isBitCNTHigh(BIT_CMD_RECEIVE) && (_REG_JOYSTAT & 0x0002)) {

        printf("\n entra2!\n");

        if (_REG_JOY_RECV_L == 0x6367 && _REG_JOY_RECV_H == 0x6536) {
        // if ((_REG_JOYSTAT == 0x20) && (_REG_JOYCNT == 0x42)) {
            
            _REG_JOYSTAT = 0x0010;
            _REG_JOYCNT = 0x0042;

            
            // scan_card = true;
            limpiarConsola();
            printf("\n Selecciona la e-card: \n");

            index_list = 0;
            actualizarPantalla();

            card_selected  = false;

            while (1) {

                manejarEntrada();

                if (isBitCNTHigh(BIT_CMD_RESET) && (isBitSTATHigh(BIT_CMD_GENERAL_PURPOSE_4))){
                    printf("\n reset scanCArd!\n");
                    scan_card = true;
                    break;
                }

                if (card_selected){

                    
                    // GC_TICK = 0;
                    // establecerSemillaFija();

                    // _REG_JOY_TRANS_L = 0xDA7C;
                    // _REG_JOY_TRANS_H = 0xEE22;

                    // SEED_CANDIDATE = A001JPN[1039];

                    modo_tarjeta_activo = true;
                    byte_card_send = 0;
                    uint32_t basura = (_REG_JOY_RECV_L << 16) | _REG_JOY_RECV_H;
                    basura = 0;
                    GC_TICK = basura + 0;

                    _REG_JOYSTAT = 0x0020;

                    // printf("\n byte _REG_JOY_RECV_L: %04X\n", _REG_JOY_RECV_L);

                    // printf("\n byte _REG_JOY_RECV_H: %04X\n", _REG_JOY_RECV_H);

                    // printf("\n byte JOYCNT: %04X\n", _REG_JOYCNT);
                    // printf("\n byte JOYSTAT: %04X\n", _REG_JOYSTAT);

                    break;
                }

            }
            
            // printf("\n return!\n");
            
           


            return;
        }

        _REG_JOYSTAT = 0x0000;
        _REG_JOYCNT = 0x0042;
        
        printf("\n byte _REG_JOY_RECV_L: %04X\n", _REG_JOY_RECV_L);
        printf("\n byte _REG_JOY_RECV_H: %04X\n", _REG_JOY_RECV_H);

    } else if (scan_card) {

        resetComunication();
        printf("\n entra a nada!\n");
        printf("\n byte scan_card: %d\n", scan_card);

    } else {
        printf("\nnada.");
    }

}

void FillListCard() {
    for (int i = 0; i < CARD_LIST_SIZE; i++) {
        items_data_names[i] = card_list[i].label;
    }
}


void init(){
    
    interrupt_init();
    irqEnable(IRQ_VBLANK);
    irqSet(IRQ_SERIAL, _onSerial);
    irqEnable(IRQ_SERIAL);
    consoleDemoInit();

}


int main() {
    
    
    init();
    FillListCard();

    printf("\x1b[2J");
    printf("\n Pokemon Colosseum JAP\n");
    printf("\n Esperando Conexion...\n");

    while (1)
    {   
        
        if (!modo_tarjeta_activo) 
        {
            resetComunication();
        }

        VBlankIntrWait();

    }

    // printf("mask_GAMEID: %08X\n", (unsigned int)mask_GAMEID);
    // printf("new_mask: %08X\n", (unsigned int)new_mask);
    
    // real_4byte = (A001JPN[byte_card_send] ^
    //             (new_mask << 0x18 | 
    //             (new_mask & 0xFF00) << 8 | mask_GAMEID >> 0x18 | (new_mask & 0xFF0000) >> 8));
    
    // printf("byte_layer1: %08X\n", (unsigned int)real_4byte);

    // next_data_L = ((((real_4byte >> 16) & 0xFFFF) >> 8) & 0x00FF) | ((((real_4byte >> 16) & 0xFFFF) << 8) & 0xFF00);
    // next_data_H = (((real_4byte & 0xFFFF) >> 8) & 0x00FF) | (((real_4byte & 0xFFFF) << 8) & 0xFF00);

    // printf("next_data_L: %08X\n", (unsigned int)next_data_L);
    // printf("next_data_H: %08X\n", (unsigned int)next_data_H);
    
    // printf("1039: %08X\n", (unsigned int)A001JPN[1039]);
    // printf("CHALLENGE: %08X\n", (unsigned int)SEED_CANDIDATE);

    VBlankIntrWait();
    
    

    return 0;
}