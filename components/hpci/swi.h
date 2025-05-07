#ifndef __SWI_H__
#define __SWI_H__
#include "esphome.h"

namespace swi
{
/**
 *  Defines
 */
#define LOW_TIME 1000
#define HIGH_1_TIME 1000
#define HIGH_0_TIME 3000

#define HIGH_START_FRAME 5000

#define DURATION_MARGIN 500 // Marge d'erreur pour les durées.

#define MAX_TIME 12000
#define MAX_FRAME_SIZE 20

#define SEND_MSG_OCCURENCE 3
    /**
     *  Enums
     */
    enum wireDirection
    {
        SENDING,
        RECEIVING
    };

    enum readingPhase
    {
        START_FRAME,
        IN_FRAME,
        END_FRAME
    };

    /**
     *  Variables
     */
    extern volatile unsigned long lastTriggerTime;

    extern volatile unsigned long triggerDeltaTime;

    extern volatile byte triggerStatus;
    extern volatile byte lastTriggerStatus;

    extern volatile boolean triggered;

    extern uint8_t read_frame[MAX_FRAME_SIZE];
    extern uint8_t frameCnt;
    extern uint8_t PIN;

    /**
     *  Functions
     */

    inline unsigned long delaisWithoutRollover(unsigned long t1, unsigned long t2);
    uint8_t reverseBits(unsigned char x);
    void isrCallback(void);
    void setWireDirection(wireDirection direction);

    /* ================================================== */
    void setup(uint8_t pin);
    void sendHigh(uint16_t ms);
    void sendLow(uint16_t ms);
    void sendBinary0(void);
    void sendBinary1(void);
    void sendHeaderCmdFrame(void);
    void sendSpaceCmdFrame(void);
    void sendSpaceCmdFramesGroup(void);
    void sendFrame(uint8_t frame[], uint8_t size);

    /* ================================================== */

    boolean startFrame(void);
    inline uint8_t readBit(void);
    inline boolean silence(void);
    boolean longSilence();
    boolean readFrame();

}
#endif // __SWI_H__