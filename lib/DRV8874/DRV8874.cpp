#include "DRV8874.h"
#include "esp32_ledc_compat.h"

void DRV8874::csUpdateTaskEntry(void *arg)
{
    auto *self = static_cast<DRV8874 *>(arg);

    const TickType_t period = pdMS_TO_TICKS(1000 / CS_UPDATE_HZ);
    TickType_t lastWake = xTaskGetTickCount();

    for (;;)
    {
        uint32_t accRawmV = 0;
        for (uint16_t i = 0; i < CS_READ_SAMPLES; ++i) // Average CS_READ_SAMPLES into millivolts
            accRawmV += analogReadMilliVolts(self->csPin);
        const uint32_t avgRawmV = accRawmV / CS_READ_SAMPLES; // truncate fine as working with mV
        // Store milli amps (volatile, no heavy sync needed for single writer)
        self->lastCurrentmA = (avgRawmV * 1000u) / CS_MV_PER_A;
        vTaskDelayUntil(&lastWake, period);
    }
}

// Control task: updates duty based on current speedPt and direction values
//   considers boost duty settings when starting/reversing
void DRV8874::ctrlTaskEntry(void *arg)
{
    auto *self = static_cast<DRV8874 *>(arg);
    const TickType_t period = pdMS_TO_TICKS(1000 / CTRL_UPDATE_HZ);
    TickType_t lastWake = xTaskGetTickCount();

    for (;;)
    {
        const int8_t newSpeedPt = self->currentSpeedPt; // snapshot
        const int8_t newDir = (newSpeedPt == 0) ? 0 : ((newSpeedPt > 0) ? +1 : -1);
        const uint8_t magPct = (newDir >= 0) ? uint8_t(newSpeedPt) : uint8_t(-newSpeedPt);
        uint32_t minDuty = (newDir > 0) ? self->_minDutyPos : self->_minDutyNeg;
        const uint32_t newDutyCmd = (newSpeedPt == 0) ? 0 : map(magPct, 0, 100, minDuty, MAX_DUTY);

        // Detect transition: start from stop OR change of direction
        uint32_t boosDuty = (newDir > 0) ? START_BOOST_DUTY_POS : START_BOOST_DUTY_NEG;
        if (newDir != self->currentDir)
        {
            if (newDir == 0 || newDutyCmd >= boosDuty)
                self->boostUntilMs = 0;
            else // starting or reversing — enable boost window
                self->boostUntilMs = millis() + START_BOOST_MS;
        }

        // Determine duty to apply (boost only within window)
        uint32_t appliedDuty = newDutyCmd;
        if (self->boostUntilMs > 0 && newDir != 0)
        {
            if (millis() < self->boostUntilMs)
                appliedDuty = boosDuty;
            else // boost window over, working with already set newDutyCmd
                self->boostUntilMs = 0;
        }

        // Write new duty only if something changed (dir or applied duty)
        if (appliedDuty != self->dutyCmd || newDir != self->currentDir)
        {
            if (newDir > 0)
            {
#if ARDUINO_ESP32_HAS_LEDC_ATTACH_CHANNEL
                ledcWriteChannel(self->ch1, appliedDuty);
                ledcWriteChannel(self->ch2, 0);
#else
                ledcWrite(self->ch1, appliedDuty);
                ledcWrite(self->ch2, 0);
#endif
            }
            else if (newDir < 0)
            {
#if ARDUINO_ESP32_HAS_LEDC_ATTACH_CHANNEL
                ledcWriteChannel(self->ch1, 0);
                ledcWriteChannel(self->ch2, appliedDuty);
#else
                ledcWrite(self->ch1, 0);
                ledcWrite(self->ch2, appliedDuty);
#endif
            }
            else // newDir == 0 : actual break in break() or coast() functions , nothing needed here
                ;

            self->dutyCmd = appliedDuty;
            self->currentDir = newDir;
        }
        vTaskDelayUntil(&lastWake, period);
    }
}

void DRV8874::begin()
{
    if (nSLEEP > 0)
    {
        pinMode(nSLEEP, OUTPUT);
        digitalWrite(nSLEEP, LOW); // keep asleep while configuring
    }

    pinMode(in1, OUTPUT);
    pinMode(in2, OUTPUT);
    digitalWrite(in1, LOW);
    digitalWrite(in2, LOW);

    // LEDC: support both Arduino-ESP32 v3.x and older 2.x APIs
#if ARDUINO_ESP32_HAS_LEDC_ATTACH_CHANNEL
    ledcAttachChannel(in1, FREQ_HZ, PWM_RES_BITS, ch1);
    ledcAttachChannel(in2, FREQ_HZ, PWM_RES_BITS, ch2);
#else
    ledcSetup(ch1, FREQ_HZ, PWM_RES_BITS);
    ledcAttachPin(in1, ch1);
    ledcSetup(ch2, FREQ_HZ, PWM_RES_BITS);
    ledcAttachPin(in2, ch2);
#endif

    coast();

    constexpr BaseType_t CORE_APP = 1; // core-1
    constexpr UBaseType_t PRIO_CS = tskIDLE_PRIORITY + 1;
    constexpr UBaseType_t PRIO_CTRL = tskIDLE_PRIORITY + 2; // higher than CS

    if (csPin > 0)
    {
        pinMode(csPin, INPUT);
        // Channel will be initialized on first analogRead; attenuation is set
        // globally in setup() via analogSetAttenuation(CS_ADC_ATTENUATION).

        // CS updater task (runs at CS_UPDATE_HZ)
        xTaskCreatePinnedToCore(csUpdateTaskEntry, "drv8874_cs",
                                2048, this, PRIO_CS, &csUpdateTaskHandle, CORE_APP);
    }

    // Control task (runs at CTRL_UPDATE_HZ)
    xTaskCreatePinnedToCore(ctrlTaskEntry, "drv8874_ctrl",
                            3072, this, PRIO_CTRL, &ctrlTaskHandle, CORE_APP);

    if (nSLEEP > 0)
        wake();
}
void DRV8874::run(int8_t speedPt)
{
    if (speedPt > 100)
        speedPt = 100;
    else if (speedPt < -100)
        speedPt = -100;
    currentSpeedPt = speedPt;
}

void DRV8874::coast()
{
#if ARDUINO_ESP32_HAS_LEDC_ATTACH_CHANNEL
    ledcWriteChannel(ch1, 0);
    ledcWriteChannel(ch2, 0);
#else
    ledcWrite(ch1, 0);
    ledcWrite(ch2, 0);
#endif
    currentDir = 0;
    currentSpeedPt = 0;
    boostUntilMs = 0;
    dutyCmd = 0;
    Serial.println("coast");
}

void DRV8874::brake()
{

#if ARDUINO_ESP32_HAS_LEDC_ATTACH_CHANNEL
    ledcWriteChannel(ch1, BREAK_DUTY);
    ledcWriteChannel(ch2, BREAK_DUTY);
#else
    ledcWrite(ch1, BREAK_DUTY);
    ledcWrite(ch2, BREAK_DUTY);
#endif

    currentDir = 0;
    currentSpeedPt = 0;
    Serial.println("BREAK");

    delay(BREAK_MS);
    coast();
}

void DRV8874::sleep()
{
    if (nSLEEP > 0)
    {
        digitalWrite(nSLEEP, LOW);
        delay(2); // tSLEEP margin
    }
    else
    {
        // PWM mode: set all outputs to 0
        coast();
    }
}

void DRV8874::wake()
{
    if (nSLEEP > 0)
    {
        digitalWrite(nSLEEP, HIGH);
        delay(2); // tWAKE margin
    }
}
