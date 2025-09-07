#pragma once
#include <Arduino.h>

// ========= Duty ctrl  params =========
static constexpr uint8_t PWM_RES_BITS = 10;
static constexpr uint32_t MAX_DUTY = (1u << PWM_RES_BITS) - 1;
static const uint32_t DEFAULT_MIN_DUTY = MAX_DUTY / 2u; // ~50%
static constexpr uint32_t FREQ_HZ = 20000;
static constexpr uint16_t BREAK_MS = 200;
static constexpr uint32_t BREAK_DUTY = MAX_DUTY;

// Boost for a bit at slow speed starts. Spring is stronger in neg (up) direction
static constexpr uint32_t START_BOOST_DUTY_POS = MAX_DUTY / 2u;
static constexpr uint32_t START_BOOST_DUTY_NEG = MAX_DUTY / 2u;
static constexpr uint32_t START_BOOST_MS = 40;

static constexpr uint16_t CTRL_UPDATE_HZ = 500; // Duty control task run freq

// ========= Current sense  params for current sense voltage-to-current readings =========
static constexpr uint16_t CS_UPDATE_HZ = 100;  // How often we refresh the averaged current reading. e.g. 100 Hz → every 10 ms
static constexpr uint16_t CS_READ_SAMPLES = 8; // How many raw ADC samples to average per update
constexpr float CS_VREF = 3.30f;               // should calibrate and tune together with attenuation
constexpr adc_attenuation_t CS_ADC_ATTENUATION = ADC_11db;
constexpr uint32_t CS_MV_PER_A = 1133; // mV per 1 A motor current (R=2.49kΩ on CS in DRV8874 pololu carrier)
// No divider resistor  on CS pin as current limited via VREF top <2A so CS < 3.3V
// For higher current limits add a divider and update CS_MV_PER_A

class DRV8874
{
public:
    // in1Pin/in2Pin: logic inputs to the driver
    // csPin: current sense input, pass 0 if not used. Note: works on ADC1 pins; ADC2 behavior depends on WiFi usage.
    // sleepPin: nSLEEP (active HIGH). If you hard-tied nSLEEP HIGH, pass 0.
    // chIn1/chIn2: LEDC PWM channels (distinct per channel and motor)
    // minDutyPos/minDutyNeg: minimum duty for positive/negative speeds to map speedPt to actual Duty.
    //    option for different values per direction for asymmetric duty ranges.
    DRV8874(uint8_t in1Pin, uint8_t in2Pin, uint8_t csPin, uint8_t sleepPin,
            uint8_t chIn1, uint8_t chIn2,
            uint32_t minDutyPos = DEFAULT_MIN_DUTY, uint32_t minDutyNeg = DEFAULT_MIN_DUTY)
        : in1(in1Pin), in2(in2Pin), csPin(csPin), nSLEEP(sleepPin),
          ch1(chIn1), ch2(chIn2),
          _minDutyPos(minDutyPos), _minDutyNeg(minDutyNeg) {}

    void begin();

    // Signed speed: % of _max_duty (-100..+100). START_BOOST_DUTY for START_BOOST_MS
    void run(int8_t speedPt);

    // High-impedance (coast): IN1=0, IN2=0
    void coast();

    // Active brake (low-side slow-decay): IN1=1, IN2=1 , BREAK_DUTY for BREAK_MS
    void brake();

    // Put driver into low-power sleep (nSLEEP=LOW).
    void sleep();

    // Wake driver (nSLEEP=HIGH).
    void wake();

    uint32_t getMaxDuty() const { return MAX_DUTY; }

    // Getter for current direction
    int8_t getDirection() const { return currentDir; }

    // Getter for current speed % (-100...+100)
    int8_t getSpeed() const { return currentSpeedPt; }

    // returns last measured current on CS pin in milli Amps. returns 0 if CS pin was set to -1
    uint32_t getCurrentmA() const { return lastCurrentmA; }

    uint32_t getDutyCmd() const { return dutyCmd; }

private:
    uint8_t in1, in2, csPin, nSLEEP;
    uint8_t ch1, ch2;
    int8_t currentDir;     // +1 forward, -1 reverse, 0 stopped
    int8_t currentSpeedPt; // 0 to 100 percent
    uint32_t _minDutyPos;
    uint32_t _minDutyNeg;

    // ======== CS updater  ========
    // Updater State
    volatile uint32_t lastCurrentmA = 0u; // updated by FreeRTOS task if CS pin is set
    // Updater task
    TaskHandle_t csUpdateTaskHandle = nullptr; // CS updater task handle
    static void csUpdateTaskEntry(void *arg);

    // ======== Current-based duty control  ========
    // ControlState
    uint32_t dutyCmd = 0;      // persisted duty command (0..MAX_DUTY)
    uint32_t boostUntilMs = 0; // set to millis() + START_BOOST_MS on start/reverse

    // Control task
    TaskHandle_t ctrlTaskHandle = nullptr;
    static void ctrlTaskEntry(void *arg);
};