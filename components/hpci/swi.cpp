#include "swi.h"

namespace swi
{
    volatile unsigned long triggerTime = 0;
    volatile unsigned long lastTriggerTime = 0;

    volatile unsigned long triggerDeltaTime;

    volatile byte triggerStatus;
    volatile byte lastTriggerStatus;

    volatile boolean triggered = false;

    uint8_t read_frame[MAX_FRAME_SIZE];
    uint8_t frameCnt;

    /**
     * @brief
     *
     * @param t1
     * @param t2
     * @return unsigned long
     */
    inline unsigned long delaisWithoutRollover(unsigned long t1, unsigned long t2)
    {
        // Les durées sont calculées en utilisant la fonction micros(), qui déborde.
        // t2 doit "normalement être plus grand que t1, mais lorsque micros déborde, c'est faux.
        return (t2 > t1) ? (t2 - t1) : ((unsigned long)(-1)) - (t1 - t2);
    }

    /**
     * @brief
     *
     * @param x
     * @return uint8_t
     */
    uint8_t reverseBits(unsigned char x)
    {
        x = ((x >> 1) & 0x55) | ((x << 1) & 0xaa);
        x = ((x >> 2) & 0x33) | ((x << 2) & 0xcc);
        x = ((x >> 4) & 0x0f) | ((x << 4) & 0xf0);
        return x;
    }

    /**
     * @brief
     *
     */
    IRAM_ATTR void isrCallback(void)
    {
        lastTriggerTime = triggerTime;
        triggerTime = micros();
        triggerDeltaTime = delaisWithoutRollover(lastTriggerTime, triggerTime);
        lastTriggerStatus = triggerStatus;
        triggerStatus = digitalRead(PIN);
        triggered = true;
    }
    /**
     * @brief Set the Wire Direction object
     *
     * @param direction
     */
    void setWireDirection(wireDirection direction)
    {
        if (direction == RECEIVING)
        {
            pinMode(PIN, INPUT_PULLUP);
            attachInterrupt(digitalPinToInterrupt(PIN), isrCallback, CHANGE);
        }
        else
        {
            detachInterrupt(digitalPinToInterrupt(PIN));
            pinMode(PIN, OUTPUT);
        }
    }

    /**
     * @brief
     *
     * @param ms
     */
    void sendHigh(uint16_t ms)
    {
        digitalWrite(PIN, HIGH);
        delayMicroseconds(ms * 1000);
    }

    /**
     * @brief
     *
     * @param ms
     */
    void sendLow(uint16_t ms)
    {
        digitalWrite(PIN, LOW);
        delayMicroseconds(ms * 1000);
    }

    /**
     * @brief
     *
     */
    void sendBinary0(void)
    {
        sendLow(1);
        sendHigh(1);
    }

    /**
     * @brief
     *
     */
    void sendBinary1(void)
    {
        sendLow(1);
        sendHigh(3);
    }

    /**
     * @brief
     *
     */
    void sendHeaderCmdFrame(void)
    {
        sendLow(9);
        sendHigh(5);
    }

    /**
     * @brief
     *
     */
    void sendSpaceCmdFrame(void)
    {
        sendLow(1);
        sendHigh(100);
    }

    /**
     * @brief
     *
     */
    void sendSpaceCmdFramesGroup(void)
    {
        sendLow(1);
        // to avoid software watchdog reset due to the long 2000ms delay, we cut the 2000ms in 4x500ms and feed the wdt each time.
        for (uint8_t i = 0; i < 4; i++)
        {

            sendHigh(500);
        }
    }

    /**
     * @brief
     *
     * @param frame
     * @param size
     */
    void sendFrame(uint8_t frame[], uint8_t size)
    {
        uint8_t frame_send[16];
        for (uint8_t i = 0; i < size; i++)
        {
            frame_send[i] = reverseBits(frame[i]); // 1's complement before sending
        }
        setWireDirection(SENDING);
        // repeat the frame 8 times
        for (uint8_t occurrence = 0; occurrence < 2; occurrence++)
        {

            sendHeaderCmdFrame();
            for (uint8_t frameIndex = 0; frameIndex < size; frameIndex++)
            {
                uint8_t value = frame_send[frameIndex];
                for (uint8_t bitIndex = 0; bitIndex < 8; bitIndex++)
                {
                    uint8_t bit = (value << bitIndex) & B10000000;
                    if (bit)
                    {
                        sendBinary1();
                    }
                    else
                    {
                        sendBinary0();
                    }
                }
            }

            sendSpaceCmdFramesGroup();
        }
        esphome::ESP_LOGI(__FILE__, "Successfully sent frame !");
        setWireDirection(RECEIVING);
    }

    boolean startFrame(void)
    {
        return (triggerDeltaTime > (HIGH_START_FRAME - DURATION_MARGIN)) && (triggerDeltaTime < (HIGH_START_FRAME + DURATION_MARGIN));
    }

    inline uint8_t readBit(void)
    {

        if ((triggerDeltaTime > (HIGH_1_TIME - DURATION_MARGIN)) && (triggerDeltaTime < (HIGH_1_TIME + DURATION_MARGIN)))
            return 1;
        if ((triggerDeltaTime > (HIGH_0_TIME - DURATION_MARGIN)) && (triggerDeltaTime < (HIGH_0_TIME + DURATION_MARGIN)))
            return 0;
        esphome::ESP_LOGW(__FILE__, "Incompatible duration !");
        return 0xff;
    }

    /**
     * @brief  Check if no signal detected
     * @note
     * @retval
     */
    inline boolean silence(void)
    {
        // Il faut bloquer les interruptions car le calcul ci-dessous peut être modifié par une interruption
        cli(); // stop interrupts
        unsigned long delta = delaisWithoutRollover(triggerTime, micros());
        sei(); // restart Interrupt
        return (delta > MAX_TIME);
    }

    // essaye de détecter le silence de 1s. A vérifier.
    boolean longSilence()
    {
        if (frameCnt < 9)
            return false;
        return read_frame[0] == 0xd1 && read_frame[1] == 0xb1;
    }

    // fonction principale. Lit une trame.
    // La lecture ne doit pas être bloquante
    // Renvoit faux tant que la lecture de la trame n'est pas finie.
    // Renvoit vrai "Dés que" un silence est détecté.

    boolean readFrame()
    {

        static uint8_t phase = START_FRAME;
        // indicateur de phase pour l'état de la lecture : START_TRAME, IN_TRAME, END_TRAME
        // La transition de "IN" à "END" n'est pas déclenché par une interruption, car on ne peut pas attendre la fin d'un silence
        // qui est de 125 ms ou 1s
        // mais par l'absence de signal pendant plus de MAX_TIME
        // Les autres transitions sont déclenchées par les interuptions.
        // Il faut "profiter" des périodes de silences pour faire des traitements long, comme envoyer les trames
        // voir ecrire une trame pour donner un ordre ?, pendant le silence de 1s ?
        // Le silence de 1s semble survenir toutes les 4 trames.
        // chaque fois que le "header" de la trame est 0xd1 0xb1 ?

        static boolean startByte = true;
        static uint8_t newByte;
        static uint8_t cptByte;

        if (phase == IN_FRAME)
        {
            if (silence())
            { // détection de la fin d'une trame par l'arrivé d'un silence
                phase = END_FRAME;
                return true; // Seul cas de return true.
            }
        }

        if (triggered)
        {
            // esphome::ESP_LOGD(__FILE__, "%d, %d", lastTriggerStatus, triggerDeltaTime);

            triggered = false;
            if (lastTriggerStatus == LOW)
            { // On ne décode que avec les états haut.
                return false;
            }

            // On démmare à un départ de trame
            if (phase == START_FRAME)
            {
                if (startFrame())
                { // Si on reçoit un signal de démmarage de trame
                    frameCnt = 0;
                    phase = IN_FRAME;
                    startByte = true;
                    // esphome::ESP_LOGD(__FILE__, "New Frame");

                    return false;
                }
            }

            if (phase == IN_FRAME)
            {
                if (startByte)
                {
                    newByte = 0;
                    cptByte = 0;
                    startByte = false;
                }

                newByte |= readBit() << cptByte++;
                // Little Endian. B10001111 vaut 0xF1 et pas 0x8F. Le premier bit transmit est le LSB
                // En "encodant" tout de suite, on évite l'appel à Bit_Reverse
                if (cptByte == 8)
                {
                    startByte = true;

                    // we have captured 8 bits, so this 1 uint8_t to add in our trame array
                    read_frame[frameCnt++] = newByte;
                    if (frameCnt >= MAX_FRAME_SIZE)
                    {
                        // esphome::ESP_LOGE(__FILE__, "Frame overflow");
                        frameCnt = 0;
                    }
                    return false;
                }
            } // if in trame

            if (phase == END_FRAME)
            { // Inutile pour l'instant ...
                phase = START_FRAME;

                // esphome::ESP_LOGD(__FILE__, "End of silence");
            }
        } // if triggered
        return false;
    }

}
