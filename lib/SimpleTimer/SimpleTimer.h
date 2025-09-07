// SimpleTimer.h
#pragma once
#include <Arduino.h>
#include "esp_timer.h"

class SimpleTimer
{
public:
    using Callback = void (*)(void *user_ctx);

    explicit SimpleTimer(uint64_t duration_ms = 1000)
        : _durationMs(duration_ms)
    {
        esp_timer_create_args_t args = {};
        args.callback = &SimpleTimer::_timerThunk;
        args.arg = this;
        args.name = "SimpleTimer";
        esp_timer_create(&args, &_timer);
    }

    ~SimpleTimer()
    {
        if (_timer)
        {
            esp_timer_stop(_timer);
            esp_timer_delete(_timer);
            _timer = nullptr;
        }
    }

    // --- set/get duration setting ---
    void setDurationMs(uint32_t ms) { _durationMs = ms; }
    uint32_t getDurationMs() const { return _durationMs; }

    // --- start a timer with a callback ---
    // Returns true if (re)started successfully.
    bool start(Callback cb, void *user_ctx = nullptr)
    {
        Serial.println("SimpleTimer::start");
        if (!_timer)
        {
            // Lazy-create timer if constructor ran too early (e.g., global object)
            esp_timer_create_args_t args = {};
            args.callback = &SimpleTimer::_timerThunk;
            args.arg = this;
            args.name = "SimpleTimer";
            esp_err_t rc = esp_timer_create(&args, &_timer);
            if (rc != ESP_OK)
            {
                Serial.printf("SimpleTimer: esp_timer_create failed: %d\n", (int)rc);
                return false;
            }
        }

        _cb = cb;
        _ctx = user_ctx;
        esp_timer_stop(_timer); // safe even if not running
        _startUs = esp_timer_get_time();
        _running = (esp_timer_start_once(_timer, _durationMs * 1000ULL) == ESP_OK);
        if (!_running)
            Serial.println("SimpleTimer::start esp_timer_start_once failed");
        return _running;
    }

    // Optional helpers
    void stop()
    {
        if (_timer)
            esp_timer_stop(_timer);
        _running = false;
    }

    bool isRunning() const { return _running; }

    // Elapsed/remaining are best-effort (no locking; good enough for UI/logic)
    uint32_t elapsedMs() const
    {
        if (!_running)
            return 0;
        uint64_t now = esp_timer_get_time();
        return (now - _startUs) / 1000ULL;
    }

    int64_t remainingMs() const
    {
        if (!_running)
            return (int64_t)_durationMs;
        int64_t rem_us = (int64_t)_durationMs * 1000LL - (int64_t)(esp_timer_get_time() - _startUs);
        return rem_us > 0 ? rem_us / 1000LL : 0;
    }

private:
    static void _timerThunk(void *arg)
    {
        static_cast<SimpleTimer *>(arg)->_onTimeout();
    }

    void _onTimeout()
    {
        _running = false; // mark not running before invoking user cb
        if (_cb)
            _cb(_ctx); // keep callback very light; runs in timer task context
    }

    esp_timer_handle_t _timer = nullptr;
    uint32_t _durationMs = 0;
    uint64_t _startUs = 0;
    Callback _cb = nullptr;
    void *_ctx = nullptr;
    volatile bool _running = false;
};
