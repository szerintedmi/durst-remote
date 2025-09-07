#include "Buzzer.h"
#include "esp32_ledc_compat.h"

Buzzer::Buzzer(uint8_t buzzerPin, uint8_t ledcChannel, int8_t ledPin)
    : _pin(buzzerPin), _ch(ledcChannel), _ledPin(ledPin)
{
}

void Buzzer::begin(uint32_t basePwmFreq, uint8_t resolutionBits)
{
    // Set up LEDC channel and attach the pin
    // Handle both Arduino-ESP32 v3.x and older 2.x APIs
#if ARDUINO_ESP32_HAS_LEDC_ATTACH_CHANNEL
    // v3.x: attach with pin-based API
    ledcAttachChannel(_pin, basePwmFreq, resolutionBits, _ch);
#else
    // v2.x: setup channel then attach pin
    ledcSetup(_ch, basePwmFreq, resolutionBits);
    ledcAttachPin(_pin, _ch);
#endif

    // Ensure off initially
#if ARDUINO_ESP32_HAS_LEDC_ATTACH_CHANNEL
    ledcWriteTone(_pin, 0);
    ledcWrite(_pin, 0);
#else
    ledcWriteTone(_ch, 0);
    ledcWrite(_ch, 0);
#endif

    if (_ledPin >= 0)
    {
        pinMode(_ledPin, OUTPUT);
        digitalWrite(_ledPin, LOW);
    }

    // Create the one-shot timer once
    if (_timer == nullptr)
    {
        esp_timer_create_args_t args = {};
        args.callback = &Buzzer::timerStopCb;
        args.arg = this;
        args.name = "beepStop";
        esp_timer_create(&args, &_timer);
    }
}

void Buzzer::buzz(uint16_t tone_ms, uint16_t volume, uint32_t freq)
{
    // Start tone (duty acts as "volume"); stop any pending timer first.
    if (_timer)
    {
        esp_timer_stop(_timer);
    }
    // Set tone and volume
#if ARDUINO_ESP32_HAS_LEDC_ATTACH_CHANNEL
    ledcWriteTone(_pin, freq); // pin-based API
    ledcWrite(_pin, volume);
#else
    ledcWriteTone(_ch, freq); // channel-based API
    ledcWrite(_ch, volume);
#endif
    if (_ledPin >= 0)
        digitalWrite(_ledPin, HIGH);

    _active = true;

    // Arm one-shot to stop after tone_ms (convert ms -> us)
    if (_timer)
    {
        esp_timer_start_once(_timer, static_cast<uint64_t>(tone_ms) * 1000ULL);
    }
}

void Buzzer::stop()
{
    // Immediately silence and clear LED
    // Immediately silence and clear LED
#if ARDUINO_ESP32_HAS_LEDC_ATTACH_CHANNEL
    ledcWriteTone(_pin, 0);
    ledcWrite(_pin, 0);
#else
    ledcWriteTone(_ch, 0);
    ledcWrite(_ch, 0);
#endif
    if (_ledPin >= 0)
        digitalWrite(_ledPin, LOW);
    _active = false;

    if (_timer)
    {
        // If someone calls stop() manually, make sure any pending one-shot is disarmed.
        esp_timer_stop(_timer);
    }
}

void Buzzer::timerStopCb(void *arg)
{
    // Timer callbacks run in a system context—keep quick.
    static_cast<Buzzer *>(arg)->handleStop();
}

void Buzzer::handleStop()
{
    // Mirror your original stop_buzz_cb()
#if ARDUINO_ESP32_HAS_LEDC_ATTACH_CHANNEL
    ledcWriteTone(_pin, 0);
    ledcWrite(_pin, 0);
#else
    ledcWriteTone(_ch, 0);
    ledcWrite(_ch, 0);
#endif
    if (_ledPin >= 0)
        digitalWrite(_ledPin, LOW);
    _active = false;
}
