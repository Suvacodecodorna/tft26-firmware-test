/* ***********************************************************************
**               Exercise 03 — I2C Sensors (Bit-bang)                   **
**************************************************************************

 Approach:

- Implement full I²C protocol in software using GPIO bit-bang
- SDA/SCL with internal pull-ups
- Delay for I2C with only "nop"
- Bus scan performed at startup to discover unknown devices
- Periodic non-blocking loop (~1s) reads sensors and updates display

 **************************************************************************
 **************************************************************************/

#include <trac_fw_io.hpp>
#include <cstdint>
#include <cstdio>
#include <cstring>


// Tractian hardware object ******************************
trac_fw_io_t io;

// *****************   I2C hardware configuration ****************************
// ***************************************************************************
#define SDA_PIN 9
#define SCL_PIN 8

#define TMP64_ADDR 0x48

// I2C pins control

void sda_high ( void ) 
{
    io.digital_write(SDA_PIN, 1);
}

void sda_low ( void ) 
{
    io.digital_write(SDA_PIN, 0);
}

void scl_high ( void ) 
{
    io.digital_write(SCL_PIN, 1);

}
void scl_low ( void ) 
{
    io.digital_write(SCL_PIN, 0);
}

bool read_sda ( void )
{
    return io.digital_read(SDA_PIN);
}

// Short deterministic delay for I²C bit timing ************
void i2c_delay ( void)
{
    for (int i = 0; i < 50; i++);
}

// ***************  I2C low level protocol routines  *************************
// ***************************************************************************

// START condition
void i2c_start ( void )
{
    sda_high();
    scl_high();
    i2c_delay();
    sda_low();
    i2c_delay();
    scl_low();
}

// STOP condition
void i2c_stop ( void )
{
    sda_low();
    scl_high();
    i2c_delay();
    sda_high();
    i2c_delay();
}

// Write byte and read ACK
bool i2c_write_byte ( uint8_t byte )
{
    for (int i = 0; i < 8; i++)
    {
        if (byte & 0x80) sda_high();
        else sda_low();

        i2c_delay();
        scl_high();
        i2c_delay();
        scl_low();

        byte <<= 1;
    }

    // ACK
    sda_high();  // release before clock

    i2c_delay();
    scl_high();
    i2c_delay();

    bool ack = (read_sda() == 0);

    i2c_delay();
    scl_low();

    return ack;
}

// Read byte and send ACK/NACK
uint8_t i2c_read_byte ( bool ack )
{
    uint8_t byte = 0;

    sda_high();

    for (int i = 0; i < 8; i++)
    {
        scl_high();
        i2c_delay();

        byte = (byte << 1) | (read_sda() ? 1 : 0);

        scl_low();
    }

    // Send ACK/NACK
    if (ack) sda_low();
    else sda_high();

    scl_high();
    i2c_delay();
    scl_low();
    sda_high();

    return byte;
}

// ******************** I2C high level protocol routines *********************
// ***************************************************************************

// Read registers
bool i2c_read_reg ( uint8_t addr, uint8_t reg, uint8_t *data, uint8_t len )
{
    i2c_start();

    if (!i2c_write_byte((addr << 1) | 0))
    {
        i2c_stop();
        return false;
    }

    if (!i2c_write_byte(reg))
    {
        i2c_stop();
        return false;
    }

    // Repeated start
    i2c_start();

    if (!i2c_write_byte((addr << 1) | 1))
    {
        i2c_stop();
        return false;
    }

    for (uint8_t i = 0; i < len; i++)
    {
        data[i] = i2c_read_byte(i < (len - 1));
    }

    i2c_stop();
    return true;
}

// ************************** I2C scan devices *******************************
// ***************************************************************************

uint8_t humidity_addr = 0;

void scan_devices ( void )
{
    printf("Scanning I2C devices...\n\r");

    for (uint8_t addr = 0x08; addr <= 0x77; addr++)
    {
        i2c_start();

        bool ack = i2c_write_byte(addr << 1);

        i2c_stop();

        if (ack)
        {
            printf("Found device at 0x%02X\n", addr);

            uint8_t who;

            if (i2c_read_reg(addr, 0x0F, &who, 1))
            {
                printf("Device 0x%02X WHO_AM_I: 0x%02X\n", addr, who);

                if (addr != TMP64_ADDR)
                {
                    humidity_addr = addr;
                }
            }
        }
    }
}

// ****************** Sensor identification **********************************
// ***************************************************************************

#define TMP64_WHOAMI 0x0F
#define TMP64_TEMP 0x00

#define HMD10_WHOAMI 0x0F
#define HMD10_HUM 0x00

void detect_sensors()
{
    uint8_t val;

    // TMP64 WHO_AM_I
    if (i2c_read_reg(TMP64_ADDR, TMP64_WHOAMI, &val, 1))
    {
        printf("TMP64 WHO_AM_I: 0x%02X\n", val);
    }
    else
    {
        printf("TMP64 not responding!\n");
    }

    // HMD10 WHO_AM_I
    if (humidity_addr)
    {
        if (i2c_read_reg(humidity_addr, HMD10_WHOAMI, &val, 1))
        {
            printf("HMD10 WHO_AM_I: 0x%02X\n", val);
        }
        else
        {
            printf("HMD10 read failed!\n");
        }
    }
}

// To read 32 bits from I2C
int32_t read_i32(uint8_t addr, uint8_t reg)
{
    uint8_t data[4];

    if (!i2c_read_reg(addr, reg, data, 4))return 0;

    return (int32_t(data[0]) << 24) |
           (int32_t(data[1]) << 16) |
           (int32_t(data[2]) << 8) |
           (int32_t(data[3]));
}

float get_temperature()
{
    return read_i32(TMP64_ADDR, 0x00) / 1000.0f;
}

float get_humidity()
{
    if (!humidity_addr)
        return 0;
    return read_i32(humidity_addr, 0x00) / 1000.0f;
}

// ************************ DISPLAY ******************************************
// ***************************************************************************

void show_temperature(float temp)
{
    char buf[9] = {};
    std::snprintf(buf, sizeof(buf), "%8.3f", temp);

    uint32_t r6, r7;
    std::memcpy(&r6, buf + 0, 4);
    std::memcpy(&r7, buf + 4, 4);

    io.write_reg(6, r6);
    io.write_reg(7, r7);
}

void show_humidity(float hum)
{
    char buf[9] = {};
    std::snprintf(buf, sizeof(buf), "%7.3f%%", hum);

    uint32_t r4, r5;
    std::memcpy(&r4, buf + 0, 4);
    std::memcpy(&r5, buf + 4, 4);

    io.write_reg(4, r4);
    io.write_reg(5, r5);
}

// ************************** Main & loop ************************************
// ***************************************************************************

int main()
{
    printf("Starting I2C bit-bang application...\n");

    // setting pull-ups
    io.set_pullup(SCL_PIN, true); // SCL
    io.set_pullup(SDA_PIN, true); // SDA

    // Release lines
    sda_high();
    scl_high();

    // Search for I2C devices on bus
    scan_devices();

    // Testing the WHO_AM_I
    detect_sensors();

    uint32_t last_update = 0;

    while (true)
    {
        uint32_t now = io.millis();

        // LCD 1 seg refresh
        if (now - last_update >= 1000)
        {
            last_update = now;

            float temp = get_temperature();
            float hum = get_humidity();

            printf("Temp: %.3f C | Hum: %.3f %%\n", temp, hum);

            show_temperature(temp);
            show_humidity(hum);
        }
    }

    return 0;
}