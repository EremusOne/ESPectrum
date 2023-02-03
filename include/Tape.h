#ifndef Tape_h
#define Tape_h

#include <inttypes.h>
#include <string>

using namespace std;

// Tape status definitions
#define TAPE_STOPPED 0
#define TAPE_LOADING 1
#define TAPE_PAUSED 2

// Saving status
#define SAVE_STOPPED 0
#define TAPE_SAVING 1

// Tape phases
#define TAPE_PHASE_SYNC 1
#define TAPE_PHASE_SYNC1 2
#define TAPE_PHASE_SYNC2 3
#define TAPE_PHASE_DATA 4
#define TAPE_PHASE_PAUSE 5

// Tape sync phases lenght in microseconds
#define TAPE_SYNC_LEN 2168 // 620 microseconds for 2168 tStates (48K)
#define TAPE_SYNC1_LEN 667 // 190 microseconds for 667 tStates (48K)
#define TAPE_SYNC2_LEN 735 // 210 microseconds for 735 tStates (48K)

#define TAPE_HDR_LONG 8063   // Header sync lenght in pulses
#define TAPE_HDR_SHORT 3223  // Data sync lenght in pulses

#define TAPE_BIT0_PULSELEN 855 // tstates = 244 ms, lenght of pulse for bit 0
#define TAPE_BIT1_PULSELEN 1710 // tstates = 488 ms, lenght of pulse for bit 1

//#define TAPE_BLK_PAUSELEN 3500000UL // 1 second of pause between blocks
#define TAPE_BLK_PAUSELEN 1750000UL // 1/2 second of pause between blocks
//#define TAPE_BLK_PAUSELEN 875000UL // 1/4 second of pause between blocks

class Tape
{
public:

    // Tape
    static FILE *tape;
    static string tapeFileName;
    static uint8_t tapeStatus;
    static uint8_t SaveStatus;
    static uint8_t romLoading;
 
    static void Init();
    static void TAP_Play();
    static void TAP_Stop();    
    static uint8_t TAP_Read();

};

#endif
