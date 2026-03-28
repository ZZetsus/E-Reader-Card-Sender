#include <gba.h>
#include <stdio.h>

//CARDS INCLUDE
#include "crc32.h"
#include "carde_data.h"

//REGIONS
#define TOTAL_REGIONS 2
IWRAM_DATA uint32_t _GAMEID_REGION;
IWRAM_DATA uint32_t _GAMEID_REGION_RECV;

int region_index = 0;
volatile bool region_selected = false;

const char* region_NAME[TOTAL_REGIONS] = {
    "USA",
    "JAP"
};


#define _REG_RCNT         *(volatile unsigned short int*)(REG_BASE + 0x134)
#define _REG_JOYCNT       *(volatile unsigned short int*)(REG_BASE + 0x140)

#define _REG_JOY_RECV_L   *(volatile unsigned short int*)(REG_BASE + 0x150)
#define _REG_JOY_RECV_H   *(volatile unsigned short int*)(REG_BASE + 0x152)

#define _REG_JOY_RECV     *(volatile unsigned int*)(REG_BASE + 0x150)

#define _REG_JOY_TRANS_L  *(volatile unsigned short int*)(REG_BASE + 0x154)
#define _REG_JOY_TRANS_H  *(volatile unsigned short int*)(REG_BASE + 0x156)

#define _REG_JOY_TRANS    *(volatile unsigned int*)(REG_BASE + 0x154)

#define _REG_JOYSTAT      *(volatile unsigned short int*)(REG_BASE + 0x158)

#define JOYCNT_RESET_FLAG   (1 << 0)
#define JOYCNT_RECEIVE_FLAG (1 << 1)
#define JOYCNT_SEND_FLAG    (1 << 2)

#define JOYSTAT_GPURPOSE5_FLAG (1 << 5)


//CARDS INFO
uint32_t MASK = 0xAA478422;

// Card controller
const uint32_t* card_data;
uint32_t byte_card_send = 0;
uint32_t card_4byte;
uint32_t real_4byte;
uint32_t basura;
bool reset_menu = false;
volatile bool card_sending = false;


// Card list and metadata
const char* label_card;
uint32_t crc32_card;
uint32_t card_selected_index;
int card_selected_size;
volatile bool card_selected = false;

//ENCRIPTING VAR
uint32_t GC_TICK;
uint32_t CRC32BYTE;
uint32_t new_mask;
uint32_t combined_mask;
uint32_t SEED_CANDIDATE;
uint32_t BYTE4_CHALLENGE;
uint32_t next_data_trans = 0;

//METADATA
uint32_t GAMEID;

//List
uint8_t items_index[CARD_LIST_SIZE];
const char* items_data_names[CARD_LIST_SIZE];
int index_list = 0;


//TEXT

void limpiarConsola() {

    printf("\x1b[2J\x1b[H");
}

void actualizarPantalla() {
    printf("\x1b[4;1H%-15s", items_data_names[index_list]);
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

        printf("\n\n Card selected: %s\n", items_data_names[index_list]);
        printf("\n Sending...");
        
    }

}

int isBPressed() {
    scanKeys();
    u16 keys = keysHeld();
    
    if (keys & KEY_B) {
        return 1;
    }
    return 0;
}

void challenge_4byte (uint32_t GAMEID_region) {
    SEED_CANDIDATE = (GAMEID >> 8) ^ crc32Table_4bytes[(GAMEID ^ 0x00000000) & 0xFF];
    BYTE4_CHALLENGE = ((SEED_CANDIDATE << 0x18) & 0xFFFFFFFF) | (((SEED_CANDIDATE & 0xFF00) << 8) & 0xFFFFFFFF) | ((SEED_CANDIDATE & 0xFFFFFFFF) >> 0x18) | ((SEED_CANDIDATE & 0xFF0000) >> 8);
}

IWRAM_CODE void prepare_word() {

    // printf("GC_TICK: %08X\n", (unsigned int)GC_TICK);

    if (byte_card_send < card_selected_size){
        
        card_4byte = card_data[byte_card_send];
        real_4byte = (card_4byte ^ combined_mask) ^ GC_TICK;
        next_data_trans = __builtin_bswap32(real_4byte);
        byte_card_send = byte_card_send + 1;

    } else if (byte_card_send >= card_selected_size && byte_card_send <= 1037){

        real_4byte = (0x00000000 ^ combined_mask) ^ GC_TICK;
        next_data_trans = __builtin_bswap32(real_4byte);
        byte_card_send = byte_card_send + 1;
        
    } else if (byte_card_send == 1038){

        // if CRC
        real_4byte = (crc32_card ^ combined_mask) ^ GC_TICK;
        next_data_trans = __builtin_bswap32(real_4byte);
        byte_card_send = byte_card_send + 1;

    } else if (byte_card_send == 1039){

        // if BYTE_CHALLENGE
        next_data_trans = __builtin_bswap32(BYTE4_CHALLENGE);
        byte_card_send = byte_card_send + 1;

    } else {
        byte_card_send = byte_card_send + 1;
    }


}

IWRAM_CODE void respond_quickly() {

    if (byte_card_send > 1040){
        _REG_JOYSTAT = 0x0000;

        //Reset flag 'send bit 2'
        _REG_JOYCNT |= JOYCNT_SEND_FLAG;
        
        card_sending = false;
        return;
    }

    _REG_JOY_TRANS = next_data_trans;

    //Reset flag 'send bit 2'
    _REG_JOYCNT |= JOYCNT_SEND_FLAG;

    prepare_word();
    
}


void chooseRegion() {
    scanKeys();
    u16 keys = keysDown();

    if (keys & (KEY_RIGHT | KEY_R)) {
        region_index = (region_index + 1) % TOTAL_REGIONS;
        printf("\x1b[4;1HRegion: %-15s", region_NAME[region_index]);
    }

    if (keys & (KEY_LEFT | KEY_L)) {
        region_index = (region_index - 1 + TOTAL_REGIONS) % TOTAL_REGIONS;
        printf("\x1b[4;1HRegion: %-15s", region_NAME[region_index]);
    }

    if (keys & KEY_A) {
        region_selected = true;
        limpiarConsola();

        if(region_index == 0){
            printf("\n Region selected: USA\n");
            _GAMEID_REGION = 0x45364347;
            _GAMEID_REGION_RECV = 0x65366367;

            GAMEID = 0x47433645;
            challenge_4byte(GAMEID);

        } else if (region_index == 1){
            printf("\n Region selected: JAP\n");

            _GAMEID_REGION = 0x4A364347;
            _GAMEID_REGION_RECV = 0x6A366367;

            GAMEID = 0x4743364A;
            challenge_4byte(GAMEID);

        }

        printf("\n Hold B to back.\n");
        printf("\n Wait for connection...\n");

    }
}

void FillListCard() {
    for (int i = 0; i < CARD_LIST_SIZE; i++) {
        items_data_names[i] = card_list[i].label;
    }
}

IWRAM_CODE void switch_to_joybus() {
    
    REG_IME = 0;

    _REG_JOYCNT = 0;
    _REG_JOYSTAT = 0;

    _REG_RCNT = 0xC000;
    _REG_RCNT &= 0xC000;
    

    basura = _REG_JOY_RECV;

    _REG_JOY_TRANS = 0x0;

    _REG_JOYCNT = 0x0047;

    REG_IF = IRQ_SERIAL;
    irqEnable(IRQ_SERIAL);

}

IWRAM_CODE void switch_to_gpio() {

    REG_IME = 0;

    irqDisable(IRQ_SERIAL);

    _REG_RCNT = 0x8000;
    _REG_RCNT &= 0x8000;

    REG_IF = IRQ_SERIAL;

}

IWRAM_CODE void resetComunication() {

    switch_to_gpio();

    for(int i=0; i<300; i++) __asm__("nop");

    switch_to_joybus();

    for(int i=0; i<3000000; i++) __asm__("nop");
    
}

IWRAM_CODE void _onSerial () {

    if (card_sending) {
        
        if (_REG_JOYCNT & JOYCNT_RECEIVE_FLAG){
            
            GC_TICK = __builtin_bswap32(_REG_JOY_RECV);
            BYTE4_CHALLENGE = BYTE4_CHALLENGE ^ GC_TICK;
            prepare_word();
            respond_quickly();

            //Reset flag 'receive bit 1'
            _REG_JOYCNT |= JOYCNT_RECEIVE_FLAG;

        } else if (_REG_JOYCNT & JOYCNT_SEND_FLAG){

            respond_quickly();

        } else {
            printf("\nError!\n");
            return;
        }

    } else if (_REG_JOYCNT & JOYCNT_RESET_FLAG) {

        _REG_JOY_TRANS = 0x45364347;
        _REG_JOYSTAT = 0x0000;

        //Reset flag 'reset bit 0'
        _REG_JOYCNT |= JOYCNT_RESET_FLAG;
    
    // Si está encendido 'send' joycnt
    } else if (_REG_JOYCNT & JOYCNT_SEND_FLAG) {

        //Reset flag 'send bit 2'
        _REG_JOYCNT |= JOYCNT_SEND_FLAG;
        // basura = _REG_JOY_RECV;
  
    } else if (_REG_JOYCNT & JOYCNT_RECEIVE_FLAG) {

       basura = _REG_JOY_RECV;

        if (basura == _GAMEID_REGION_RECV){
            //65366367

            //General Purpose Flag bit 4
            _REG_JOYSTAT |= (1 << 4);
            _REG_JOYCNT |= JOYCNT_RECEIVE_FLAG;

            limpiarConsola();
            printf("\n Hold B to back.\n");
            printf("\n Select E-Card (L/R): \n");
            actualizarPantalla();

            while (1) {
                manejarEntrada();

                if (card_selected){
                    _REG_JOYSTAT = 0x0020;
                    card_sending = true;
                    break;
                } 

                if (isBPressed()) {
                    region_selected = false;
                    break;
                }
            }

        } else {
            _REG_JOYSTAT = 0x0000;

            //Reset flag 'receive bit 1'
            _REG_JOYCNT |= (1 << 1);
        }
    }  
}

IWRAM_CODE void init(){
    
    irqInit();
    irqSet(IRQ_SERIAL, _onSerial);
    irqEnable(IRQ_VBLANK);
    consoleDemoInit();

}

void init_var(){

    CRC32BYTE = crc32Table_4bytes[(MASK ^ 0x00000000) & 0xFF];
    new_mask = MASK >> 8 ^ CRC32BYTE;
    combined_mask = (new_mask << 0x18 | (new_mask & 0xFF00) << 8 | CRC32BYTE >> 0x18 | (new_mask & 0xFF0000) >> 8);
    BYTE4_CHALLENGE = CHALLENGE_VALUE;
    byte_card_send = 0;
    index_list = 0;
    region_selected = false;
    card_selected = false;
    card_sending = false;
}

IWRAM_CODE int main() {

    init();
    while (1) {

        init_var();
        switch_to_gpio();
        FillListCard();
        limpiarConsola();
        printf("\x1b[2J");
        printf("\n Pokemon Colosseum\n E-Reader Card Sender\n");
        printf("\x1b[4;1HRegion: %-15s", region_NAME[region_index]);

        while (1){   
            
            chooseRegion();
    
            if (region_selected) {
                break;
            }
        }

        while (!card_selected) {

            if (region_selected){
                resetComunication();
            }
            
            if (isBPressed()) {
                reset_menu = true;
                break;
            }

        }

        if (reset_menu){
            reset_menu = false;
            printf("\n Resetting...\n");
            continue;
        }
        
        while (card_sending);

        printf("\n Card sent successfully! \n");
        
        //Esperar 3 segundos
        for(volatile int i = 0; i < 3000000; i++) {
            __asm("nop"); 
        }
        
        VBlankIntrWait();
    }

}