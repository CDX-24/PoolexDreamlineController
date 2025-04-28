#include "swi.h"

namespace swi
{
    volatile unsigned long triggerTime = 0;
    volatile unsigned long lastTriggerTime = 0;

    volatile unsigned long triggerDeltaTime;

    volatile byte triggerStatus;
    volatile byte lastTriggerStatus;

    volatile boolean triggered = false;
    volatile wireDirection currentDirection = UNDEFINED;

    volatile boolean frame_available = false;

    uint8_t read_frame[MAX_FRAME_SIZE];
    uint8_t frameCnt;
    volatile communicationState swi_state = IDLE;
    receiveState swi_receive_state = START_FRAME;
    uint8_t current_frame[16];
    uint8_t frame_index = 0;
    uint8_t bit_index = 0;
    static uint8_t error_count = 0;

    void swi_setup()
    {
        clear_reception_flags();
        error_count = 0;
        swi_state = IDLE;
        frame_available = false;
        setWireDirection(RECEIVING);
    }

    void clear_reception_flags()
    {
        triggerTime = 0;
        lastTriggerTime = 0;
        triggerDeltaTime = 0;
        triggerStatus = LOW;
        lastTriggerStatus = LOW;
        triggered = false;
        frameCnt = 0;
        frame_index = 0;
        bit_index = 0;
        swi_receive_state = START_FRAME;
    }

    void swi_loop()
    {
        if (error_count > MAX_ERROR_COUNT)
        {
            error_count = 0;
            // Handle the error condition here
            ESP_LOGW("SWI", "Error count exceeded! Resetting connection.");
            swi_setup(); // Reset the state
        }
        if (swi_state == IDLE || swi_state == RECEIVING_DATA)
        {
            readFrame();
        }
        else if (swi_state == TRANSMITTING_DATA)
        {
            // Handle transmission state
        }
    }

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
            currentDirection = RECEIVING;
        }
        else
        {
            detachInterrupt(digitalPinToInterrupt(PIN));
            pinMode(PIN, OUTPUT);
            clear_reception_flags();
            currentDirection = SENDING;
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
        waitForDuration(ms * 1000);
    }

    /**
     * @brief
     *
     * @param ms
     */
    void sendLow(uint16_t ms)
    {
        digitalWrite(PIN, LOW);
        waitForDuration(ms * 1000);
    }

    void waitForDuration(uint32_t duration_us)
    {
        unsigned long start = micros();
        while (micros() - start < duration_us)
        {
            esphome::App.feed_wdt(); // Feed the watchdog to prevent resets
            yield();                 // Allow ESPHome to process other tasks
        }
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
            esphome::App.feed_wdt(); // Feed the watchdog
            sendHigh(500);
        }
    }

    /**
     * @brief
     *
     * @param frame
     * @param size
     */
    boolean sendFrame(uint8_t frame[], uint8_t size)
    {
        if (swi_state == IDLE)
        {
            uint8_t frame_send[16];
            setWireDirection(SENDING);
            swi_state = TRANSMITTING_DATA;
            for (uint8_t i = 0; i < size; i++)
            {
                frame_send[i] = reverseBits(frame[i]); // 1's complement before sending
            }
            for (uint8_t occurrence = 0; occurrence < SEND_MSG_OCCURENCE; occurrence++)
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
            ESP_LOGI("SWI", "Successfully sent frame !");
            setWireDirection(RECEIVING);
            swi_state = IDLE;
            return true;
        }
        else
        {
            return false;
        }
    }

    boolean startFrame(void)
    {
        return (triggerDeltaTime > (HIGH_START_FRAME - DURATION_MARGIN)) && (triggerDeltaTime < (HIGH_START_FRAME + DURATION_MARGIN));
    }

    inline uint8_t readBit(void)
    {
        if ((triggerDeltaTime > (HIGH_1_TIME - DURATION_MARGIN)) && (triggerDeltaTime < (HIGH_1_TIME + DURATION_MARGIN)))
        {
            return 1;
        }
        if ((triggerDeltaTime > (HIGH_0_TIME - DURATION_MARGIN)) && (triggerDeltaTime < (HIGH_0_TIME + DURATION_MARGIN)))
        {
            return 0;
        }

        // Increment the counter for incompatible durations
        error_count++;
        ESP_LOGW("SWI", "Incompatible duration! Count: %d", error_count);
        return 0xff; // Return invalid bit
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

    void readFrame()
    {

        // La transition de "IN" à "END" n'est pas déclenché par une interruption, car on ne peut pas attendre la fin d'un silence
        // qui est de 125 ms ou 1s
        // mais par l'absence de signal pendant plus de MAX_TIME
        // Les autres transitions sont déclenchées par les interuptions.
        static boolean startByte = true;
        static uint8_t newByte;
        static uint8_t cptByte;

        switch (swi_receive_state)
        {
        case START_FRAME:
            if (triggered && lastTriggerStatus == LOW)
            {
                triggered = false;
                if (startFrame())
                { // Si on reçoit un signal de démarrage de trame
                    frameCnt = 0;
                    swi_receive_state = IN_FRAME;
                    swi_state = RECEIVING_DATA;
                    startByte = true;
                }
            }
            break;

        case IN_FRAME:
            if (silence())
            { // détection de la fin d'une trame par l'arrivé d'un silence
                swi_receive_state = END_FRAME;
            }
            else if (triggered && lastTriggerStatus == LOW)
            {
                triggered = false;
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
                        ESP_LOGE("SWI", "Frame overflow detected. Resetting frame counter.");
                        error_count++;
                        clear_reception_flags();
                    }
                }
            }
            break;

        case END_FRAME:
            frame_available = true;
            swi_state = IDLE;
            clear_reception_flags();
            break;
        }
    }
}
