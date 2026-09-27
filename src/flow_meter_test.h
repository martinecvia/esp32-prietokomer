#pragma once

#include <Arduino.h>
#include <esp_timer.h>

namespace flow_meter_test
{
    class FlowMeterTest
    {
    public:
        explicit FlowMeterTest(void (*callback)()) : _callback(callback) {}
        void test(float lpm)
        {
            stop();
            if (!_handle)
            {
                esp_timer_create_args_t arg = {};
                arg.callback = &FlowMeterTest::handle;
                arg.arg = this;
                arg.name = "flow_meter_test";
                esp_timer_create(&arg, &_handle);
            }

            if (lpm > 0.0f)
            {
                esp_timer_start_periodic(_handle, (uint64_t)(60000000.0f / lpm));
            }
        }

        void stop(void)
        {
            if (_handle)
                esp_timer_stop(_handle);
        }

    private:
        void (*_callback)();
        esp_timer_handle_t _handle = nullptr;

        static void handle(void *arg) { static_cast<FlowMeterTest *>(arg)->_callback(); }
    };
}