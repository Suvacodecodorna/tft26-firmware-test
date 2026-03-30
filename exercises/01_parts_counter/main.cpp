/* ***********************************************************************
**                     Exercise 01 — Parts Counter                      **
**************************************************************************

 Approach:

- Detect edge changes;
- FSM to diferenciates idle or part detected;
- A time-based debounce filters spurious transitions
- A state machine ensures each part is counted exactly once
- A part is detected on a stable rising edge
- The system waits for the signal to return to idle before re-arming

 **************************************************************************
 **************************************************************************/

#include <trac_fw_io.hpp>
#include <cstdint>

trac_fw_io_t io;

uint32_t count = 0;
bool show_dsp = false;

#define DEBOUNCE 20

enum State {
    IDLE,
    DETECTED
};

State state = IDLE;

bool stable_state = false;
bool last_read = false;
uint32_t last_change_time = 0;

void sensor(void)
{
    uint32_t now = io.millis();
    bool reading = io.digital_read(0);

    // last edge change detect
    if (reading != last_read)
    {
        last_change_time = now;
    }

    // Debounce
    if ((now - last_change_time) > DEBOUNCE)
    {
        if (stable_state != reading)
        {
            stable_state = reading;

            switch (state)
            {
                case IDLE:
                    if (stable_state == true) // part detected
                    {
                        count++;
                        show_dsp = true;
                        state = DETECTED;
                    }
                    break;

                case DETECTED:
                    if (stable_state == false) // back to idle
                    {
                        state = IDLE;
                    }
                    break;
            }
        }
    }

    last_read = reading;
}

void display(void)
{
    if (show_dsp)
    {
        show_dsp = false;

        char buf[9] = {};
        std::snprintf(buf, sizeof(buf), "%8u", count); // right-aligned, 8 chars
        uint32_t r6, r7;
        std::memcpy(&r6, buf + 0, 4);
        std::memcpy(&r7, buf + 4, 4);
        io.write_reg(6, r6);
        io.write_reg(7, r7);
    }
}

int main()
{
    while (true)
    {
        // detect each part passing the sensor and increment count
        sensor();

        // update the display with the current count
        display();
    }
}
