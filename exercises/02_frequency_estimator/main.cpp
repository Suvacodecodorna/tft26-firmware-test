/* ***********************************************************************
**               Exercise 02 — Frequency Estimator                      **
**************************************************************************

 Approach:

- 1 Khz sampling;
- Apply moving average filter to ADC samples to reduce noise;
- Detect rising edge using hysteresis;
- Measure signal period using io.millis();
- Outliers rejection;


 **************************************************************************
 **************************************************************************/

#include <trac_fw_io.hpp>
#include <cstdint>


// ******************** Configuration constants *****************************
// **************************************************************************
constexpr uint32_t SAMPLE_PERIOD_MS = 1;

constexpr uint16_t ADC_MEAN = 2048;
constexpr uint16_t HYSTERESIS = 500;

// Expected frequency range (~6–9 Hz)
constexpr uint32_t MIN_PERIOD_MS = 110;
constexpr uint32_t MAX_PERIOD_MS = 170;

// Filter sizes
constexpr uint8_t SAMPLE_WINDOW = 50;
constexpr uint8_t PERIOD_WINDOW = 5;

// ******************** Moving average (ADC samples) ************************
// **************************************************************************
uint16_t sample_buffer[SAMPLE_WINDOW] = {0};
uint32_t sample_sum = 0;
uint8_t sample_index = 0;
uint8_t sample_count = 0;

uint16_t filter_sample(uint16_t new_sample)
{
    sample_sum -= sample_buffer[sample_index];
    sample_buffer[sample_index] = new_sample;
    sample_sum += new_sample;

    sample_index = (sample_index + 1) % SAMPLE_WINDOW;

    if (sample_count < SAMPLE_WINDOW)
        sample_count++;

    return sample_sum / sample_count;
}


// ********************** Moving average (period) ***************************
// **************************************************************************
uint32_t period_buffer[PERIOD_WINDOW] = {0};
uint32_t period_sum = 0;
uint8_t period_index = 0;
uint8_t period_count = 0;

uint32_t filter_period(uint32_t new_period)
{
    period_sum -= period_buffer[period_index];
    period_buffer[period_index] = new_period;
    period_sum += new_period;

    period_index = (period_index + 1) % PERIOD_WINDOW;

    if (period_count < PERIOD_WINDOW)
        period_count++;

    return period_sum / period_count;
}


// *********************** Main application *********************************
// **************************************************************************
int main()
{
    trac_fw_io_t io;

    uint32_t last_sample_time = 0;

    uint16_t raw_sample = 0;
    uint16_t filtered_sample = 0;

    bool current_state = false;
    bool previous_state = false;

    uint32_t last_cross_time = 0;

    while (true)
    {
        uint32_t now = io.millis();

        // Sampling at 1 Khz
        if (now - last_sample_time >= SAMPLE_PERIOD_MS)
        {
            last_sample_time = now;

            // Sample and filter signal
            raw_sample = io.analog_read(0);
            filtered_sample = filter_sample(raw_sample);


            // Hysteresis
            if (filtered_sample > (ADC_MEAN + HYSTERESIS))
            {
                current_state = true;
            }
            else if (filtered_sample < (ADC_MEAN - HYSTERESIS))
            {
                current_state = false;
            }

            // Rising edge detection
            if (current_state && !previous_state)
            {
                uint32_t period = now - last_cross_time;
                last_cross_time = now;

                // outlier rejection
                if (period > MIN_PERIOD_MS && period < MAX_PERIOD_MS)
                {
                    uint32_t filtered_period = filter_period(period);

                    // Convert to frequency (Hz)
                    float frequency = 1000.0f / filtered_period;

                    // Convert to centiHz
                    uint32_t freq_cHz = static_cast<uint32_t>(frequency * 100.0f);

                    io.write_reg(3, freq_cHz);
                }
            }

            previous_state = current_state;
        }
    }
}