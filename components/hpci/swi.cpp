#include "swi.h"

namespace swi
{
    volatile unsigned long triggerTime = 0;
    volatile unsigned long lastTriggerTime = 0;

    volatile unsigned long triggerDeltaTime;

    volatile byte triggerStatus;
    volatile byte lastTriggerStatus;

    volatile boolean triggered = false;

    volatile wireDirection currentDirection = RECEIVING;

    volatile boolean frame_available = false;

    uint8_t read_frame[MAX_FRAME_SIZE];
    uint8_t frameCnt;

    volatile communicationState swi_state = IDLE;
    receiveState swi_receive_state = START_FRAME;
    static uint8_t error_count = 0;


    IRAM_ATTR void isrCallback(void)
    {
        lastTriggerTime = triggerTime;
        triggerTime = micros();
        triggerDeltaTime = delaisWithoutRollover(lastTriggerTime, triggerTime);
        lastTriggerStatus = triggerStatus;
        triggerStatus = digitalRead(PIN);
        triggered = true;
    }

    void setWireDirection(wireDirection direction)
    {
        if (direction == RECEIVING)
        {
            ESP_LOGI("SWI", "Setting wire direction to RECEIVING");
            pinMode(PIN, INPUT_PULLUP);
            attachInterrupt(digitalPinToInterrupt(PIN), isrCallback, CHANGE);
            currentDirection = RECEIVING;
        }
        else if (direction == SENDING)
        {
            ESP_LOGI("SWI", "Setting wire direction to SENDING");
            detachInterrupt(digitalPinToInterrupt(PIN));
            pinMode(PIN, OUTPUT);
            currentDirection = SENDING;
        }
        else
        {
            ESP_LOGE("SWI", "Invalid wire direction specified.");
        }
    }

    void swi_setup()
    {
        clear_reception_flags();
        error_count = 0;
        swi_state = IDLE;
        frame_available = false;
        setWireDirection(RECEIVING);
        ESP_LOGI("SWI", "SWI setup complete.");
    }

    void clear_reception_flags()
    {
        ESP_LOGI("SWI", "Clearing reception flags...");
        triggerTime = 0;
        lastTriggerTime = 0;
        triggerDeltaTime = 0;
        triggerStatus = LOW;
        lastTriggerStatus = LOW;
        triggered = false;
        frameCnt = 0;
        swi_receive_state = START_FRAME;
    }

    void swi_loop()
    {
        static unsigned long lastLogTime = 0;
        unsigned long currentTime = millis();

        // Log every second
        if (currentTime - lastLogTime >= 3000)
        {
            ESP_LOGI("SWI", "SWI loop running... (state is %d)", swi_state);
            ESP_LOGI("SWI", "Trigger time: %lu", triggerTime);
            ESP_LOGI("SWI", "Last Trigger time: %lu", lastTriggerTime);
            lastLogTime = currentTime;
        }

        if (error_count > MAX_ERROR_COUNT)
        {
            error_count = 0;
            ESP_LOGW("SWI", "Error count exceeded! Resetting connection.");
            swi_setup();
            return;
        }

        switch (swi_state)
        {
        case IDLE:
        case RECEIVING_DATA:
            if (currentDirection != RECEIVING)
            {
                setWireDirection(RECEIVING);
            }
            readFrame();
            break;

        case TRANSMITTING_DATA:
            // Handle transmission state
            break;

        default:
            ESP_LOGE("SWI", "Unknown state encountered!");
            swi_setup();
            break;
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

    inline uint8_t readBit()
    {
        if (triggerDeltaTime > (HIGH_1_TIME - DURATION_MARGIN) && triggerDeltaTime < (HIGH_1_TIME + DURATION_MARGIN))
        {
            return 1;
        }
        if (triggerDeltaTime > (HIGH_0_TIME - DURATION_MARGIN) && triggerDeltaTime < (HIGH_0_TIME + DURATION_MARGIN))
        {
            return 0;
        }

        error_count++;
        ESP_LOGW("SWI", "Incompatible duration! Count: %d", error_count);
        return 0xff; // Invalid bit
    }

    inline boolean silence()
    {
        cli();
        unsigned long delta = delaisWithoutRollover(triggerTime, micros());
        sei();
        return delta > MAX_TIME;
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
        static boolean startByte = true;
        static uint8_t newByte = 0;
        static uint8_t cptByte = 0;

        if (swi_receive_state == START_FRAME)
        {
            if (triggered && lastTriggerStatus == HIGH)
            {
                triggered = false;
                if (startFrame())
                {
                    frameCnt = 0;
                    ESP_LOGI("SWI", "Receiving frame...");
                    swi_receive_state = IN_FRAME;
                    swi_state = RECEIVING_DATA;
                    startByte = true;
                }
            }
        }
        else if (swi_receive_state == IN_FRAME)
        {
            if (silence())
            {
                swi_receive_state = END_FRAME;
            }
            else if (triggered && lastTriggerStatus == HIGH)
            {
                triggered = false;
                if (startByte)
                {
                    newByte = 0;
                    cptByte = 0;
                    startByte = false;
                }

                uint8_t bit = readBit();
                if (bit == 0xff) // Invalid bit
                {
                    return;
                }

                newByte |= bit << cptByte++;
                if (cptByte == 8)
                {
                    startByte = true;
                    read_frame[frameCnt++] = newByte;

                    if (frameCnt >= MAX_FRAME_SIZE)
                    {
                        ESP_LOGE("SWI", "Frame overflow detected. Resetting frame counter.");
                        error_count++;
                        clear_reception_flags();
                        return;
                    }
                }
            }
        }
        else if (swi_receive_state == END_FRAME)
        {
            frame_available = true;
            ESP_LOGI("SWI", "Frame received!");
            swi_state = IDLE;
            clear_reception_flags();
        }
    }
}
