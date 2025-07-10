/*
Program to write data to 28C64B EEPROM.
This code is designed to run on nRF52840 dongle.
*/

#include <stdbool.h>
#include <stdint.h>
#include "nrf_delay.h"
#include "boards.h"

#define abs(x) ((x) > 0 ? (x) : -(x))

const uint32_t shift_data = NRF_GPIO_PIN_MAP(0, 13);
const uint32_t shift_clk = NRF_GPIO_PIN_MAP(0, 17);
const uint32_t shift_latch = NRF_GPIO_PIN_MAP(0, 15);
const uint32_t write_en = NRF_GPIO_PIN_MAP(0, 31);

const uint32_t D0 = NRF_GPIO_PIN_MAP(0, 9);
const uint32_t D1 = NRF_GPIO_PIN_MAP(0, 10);
const uint32_t D2 = NRF_GPIO_PIN_MAP(1, 0);
const uint32_t D3 = NRF_GPIO_PIN_MAP(1, 10);
const uint32_t D4 = NRF_GPIO_PIN_MAP(1, 13);
const uint32_t D5 = NRF_GPIO_PIN_MAP(1, 15);
const uint32_t D6 = NRF_GPIO_PIN_MAP(0, 2);
const uint32_t D7 = NRF_GPIO_PIN_MAP(0, 29);

const uint32_t data_pins[] = {D0, D1, D2, D3, D4, D5, D6, D7};

const uint8_t digits[] = {
    // 7-segment display encoding for digits 0-9
    // common cathode configuration
    0x7E, // 0
    0x30, // 1
    0x6D, // 2
    0x79, // 3
    0x33, // 4
    0x5B, // 5
    0x5F, // 6
    0x70, // 7
    0x7F, // 8
    0x7B  // 9
};

void setAddress(uint16_t address)
{
    nrf_gpio_pin_write(shift_latch, 0); // Set latch low
    for (int i = 0; i < 16; i++)
    {
        nrf_gpio_pin_write(shift_data, (address >> (15 - i)) & 1);
        nrf_gpio_pin_write(shift_clk, 1);   // Set clock high
        nrf_delay_us(1);                    // Short delay for clock pulse
        nrf_gpio_pin_write(shift_clk, 0);
        nrf_delay_us(1);                    // Set clock low
    }
    nrf_gpio_pin_write(shift_latch, 1);     // Set latch high
    nrf_delay_us(1);                        // Short delay to ensure latch is set
}

void writeEEPROM(uint16_t address, uint8_t data)
{
    // Set the address
    setAddress(address);
    nrf_gpio_pin_write(write_en, 0);        // Set write enable low

    for (int i = 0; i < 8; i++)
    {
        // write LSB first
        nrf_gpio_pin_write(shift_data, (data >> i) & 1);
    }
    nrf_gpio_pin_write(write_en, 1);        // Set write enable high    
    nrf_delay_ms(1);                        // delay for next write
}

/**
 * @brief Function for application main entry.
 */
int main(void)
{
    /* Configure board. */
    bsp_board_init(BSP_INIT_LEDS | BSP_INIT_BUTTONS);

    // Turn on LED to indicate the application has started.
    bsp_board_led_on(BSP_BOARD_LED_0); // Green LED
    nrf_delay_ms(500);

    /* Initialize GPIO pins for shift register control. */
    nrf_gpio_cfg_output(shift_data);
    nrf_gpio_cfg_output(shift_clk);
    nrf_gpio_cfg_output(shift_latch);

    /* Initialize GPIO pin for write enable control. */
    nrf_gpio_cfg_output(write_en);
    nrf_gpio_pin_set(write_en); // Set write enable high

    /* Initialize GPIO pins for data output. */
    for (int i = 0; i < sizeof(data_pins) / sizeof(data_pins[0]); i++)
    {
        nrf_gpio_cfg_output(data_pins[i]); // Configure each data pin as output
    }

    // Programming one's place of value with the corresponding digit
    for (int address = 0; address < 256; address++)
    {
        writeEEPROM(address, digits[address % 10]);
    }

    // programming tens place of value with the corresponding digit
    for (int address = 0; address < 256; address++)
    {
        writeEEPROM(address + 256, digits[(address / 10) % 10]);
    }

    // programming hundreds place of value with the corresponding digit
    for (int address = 0; address < 256; address++)
    {
        writeEEPROM(address + 512, digits[(address / 100) % 10]);
    }

    // programming thousands place of value with the sign information
    for (int address = 0; address < 256; address++)
    {
        writeEEPROM(address + 768, 0x00); // Assuming 0x00 means no sign
    }

    // program 2's complement of the value from -128 to 127
    for (int val = -128; val < 128; val++)
    {
        writeEEPROM((uint8_t)val + 1024, digits[abs(val) % 10]);
    }

    for (int val = -128; val < 128; val++)
    {
        writeEEPROM((uint8_t)val + 1280, digits[abs(val / 10) % 10]);
    }

    for (int val = -128; val < 128; val++)
    {
        writeEEPROM((uint8_t)val + 1536, digits[abs(val / 100) % 10]);
    }

    for (int val = -128; val < 128; val++)
    {
        writeEEPROM((uint8_t)val + 1792, (val < 0) ? 0x01 : 0x00); // 0x01 for negative, 0x00 for positive
    }

    // EEPROM programming is complete.

    /* Main loop */
    while (true)
    {
        // pulse blue LED forever
        bsp_board_led_on(BSP_BOARD_LED_3);
        nrf_delay_ms(500);

        // read the button state
        if (bsp_board_button_state_get(BSP_BOARD_BUTTON_0))
        {
            // if Button is pressed, turn off the green LED
            bsp_board_led_off(BSP_BOARD_LED_0);
            nrf_delay_ms(2000);

            // reset the board using nvic system reset
            NVIC_SystemReset();
        }

        bsp_board_led_off(BSP_BOARD_LED_3);
        nrf_delay_ms(500);
    }
}