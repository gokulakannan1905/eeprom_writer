/*
This program is to write data to an EEPROM using a shift register 74HC595.
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

#define HLT 0b1000000000000000
#define MI  0b0100000000000000
#define RI  0b0010000000000000
#define RO  0b0001000000000000
#define SR  0b0000100000000000
#define II  0b0000010000000000
#define AI  0b0000001000000000
#define AO  0b0000000100000000
#define EO  0b0000000010000000
#define SU  0b0000000001000000
#define BI  0b0000000000100000
#define OI  0b0000000000010000
#define CE  0b0000000000001000
#define CO  0b0000000000000100
#define J   0b0000000000000010
#define FI  0b0000000000000001


#define mask 0b0101111110100111



#define FLAGS_Z0C0 0
#define FLAGS_Z0C1 1
#define FLAGS_Z1C0 2
#define FLAGS_Z1C1 3

// pin 7 eeprom select
// pin 8 and pin 9
#define JC 0b0111 
#define JZ 0b1000 


uint16_t ucode_template[16][8] = {
    { MI|CO, RO|II|CE, SR,    0,        0,           0,           0,  0 },  // 0 NOP
    { MI|CO, RO|II|CE, MI|CO, RO|MI|CE, RO|AI,       SR,          0,  0 },  // 1 LDA
    { MI|CO, RO|II|CE, MI|CO, RO|MI|CE, RO|BI,       EO|FI|AI,    SR, 0 },  // 2 ADD
    { MI|CO, RO|II|CE, MI|CO, RO|MI|CE, RO|BI,       SU|EO|FI|AI, SR, 0 },  // 3 SUB
    { MI|CO, RO|II|CE, MI|CO, RO|MI|CE, AO|RI,       SR,          0,  0 },  // 4 STA
    { MI|CO, RO|II|CE, MI|CO, RO|AI|CE, SR,          0,           0,  0 },  // 5 LDI
    { MI|CO, RO|II|CE, MI|CO, RO|J,     SR,          0,           0,  0 },  // 6 JMP
    { MI|CO, RO|II|CE, CE,    SR,       0,           0,           0,  0 },  // 7 JC
    { MI|CO, RO|II|CE, CE,    SR,       0,           0,           0,  0 },  // 8 JZ
    { MI|CO, RO|II|CE, MI|CO, RO|BI|CE, EO|FI|AI,    SR,          0,  0 },  // 9 ADI
    { MI|CO, RO|II|CE, MI|CO, RO|BI|CE, SU|EO|FI|AI, SR,          0,  0 },  // 10 SUI
    { MI|CO, RO|II|CE, SR,    0,        0,           0,           0,  0 },  // 11 NOP
    { MI|CO, RO|II|CE, SR,    0,        0,           0,           0,  0 },  // 12 NOP
    { MI|CO, RO|II|CE, SR,    0,        0,           0,           0,  0 },  // 13 NOP
    { MI|CO, RO|II|CE, AO|OI, SR,       0,           0,           0,  0 },  // 14 OUT
    { MI|CO, RO|II|CE, HLT,   0,        0,           0,           0,  0 },  // 15 HLT
};

uint16_t ucode[4][16][8];
void init_ucode()
{
    // copy the template to the ucode array
    for (int i = 0; i < 16; i++)
    {
        for (int j = 0; j < 8; j++)
        {
            ucode[FLAGS_Z0C0][i][j] = ucode_template[i][j] ^ mask;
            ucode[FLAGS_Z0C1][i][j] = ucode_template[i][j] ^ mask;
            ucode[FLAGS_Z1C0][i][j] = ucode_template[i][j] ^ mask;
            ucode[FLAGS_Z1C1][i][j] = ucode_template[i][j] ^ mask;
        }
    }
    // ZF = 0, CF = 1
    ucode[FLAGS_Z0C1][JC][2] = (MI|CO) ^ mask;
    ucode[FLAGS_Z0C1][JC][3] = (RO|J)  ^ mask;
    ucode[FLAGS_Z0C1][JC][4] = SR      ^ mask;

    // ZF = 1, CF = 0
    ucode[FLAGS_Z1C0][JZ][2] = (MI|CO) ^ mask;
    ucode[FLAGS_Z1C0][JZ][3] = (RO|J)  ^ mask;
    ucode[FLAGS_Z1C0][JZ][4] = SR      ^ mask;

    // ZF = 1, CF = 1
    ucode[FLAGS_Z1C1][JZ][2] = (MI|CO) ^ mask;
    ucode[FLAGS_Z1C1][JZ][3] = (RO|J)  ^ mask;
    ucode[FLAGS_Z1C1][JZ][4] = SR      ^ mask;
    
    ucode[FLAGS_Z1C1][JC][2] = (MI|CO) ^ mask;
    ucode[FLAGS_Z1C1][JC][3] = (RO|J)  ^ mask;
    ucode[FLAGS_Z1C1][JC][4] = SR      ^ mask;
}

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
    // Set the address for the shift register
    nrf_gpio_pin_write(shift_latch, 0); // Set latch low
    for (int i = 0; i < 16; i++)
    {
        nrf_gpio_pin_write(shift_data, (address >> (15 - i)) & 1);
        nrf_gpio_pin_write(shift_clk, 1); // Set clock high
        nrf_delay_us(1);                  // Short delay for clock pulse
        nrf_gpio_pin_write(shift_clk, 0);
        nrf_delay_us(1); // Set clock low
    }
    nrf_gpio_pin_write(shift_latch, 1); // Set latch high
    nrf_delay_us(1);                    // Short delay to ensure latch is set
}

void writeEEPROM(uint16_t address, uint8_t data)
{
    // Set the address
    setAddress(address);

    nrf_gpio_pin_write(write_en, 0); // Set write enable low

    for (int i = 0; i < 8; i++)
    {
        // write LSB first
        nrf_gpio_pin_write(data_pins[i], (data >> i) & 1);
    }
    nrf_gpio_pin_write(write_en, 1); // Set latch low
    // delay for next write
    nrf_delay_ms(10);
}


void programEEPROM_7Segment()
{
    // Programming one's place of address with the corresponding digit
    // This loop writes the digits 0-9 to the EEPROM at addresses 0-255.
    // Each address will contain the corresponding digit value.
    // The digits are repeated every 10 addresses.
    // For example, address 0 will contain 0x7E (for digit 0),
    // address 1 will contain 0x30 (for digit 1), and so on.
    // After address 9, it will repeat with address 10 containing 0x7E again.

    for (int address = 0; address < 256; address++)
    {
        // Write the digit to the EEPROM
        writeEEPROM(address, digits[address % 10]);
    }

    // programming tens place of address with the corresponding digit
    // This loop writes the tens place of the digit to the EEPROM.

    for (int address = 0; address < 256; address++)
    {
        writeEEPROM(address + 256, digits[(address / 10) % 10]);
    }

    // programming hundreds place of address with the corresponding digit
    for (int address = 0; address < 256; address++)
    {
        writeEEPROM(address + 512, digits[(address / 100) % 10]);
    }

    // programming thousands place of address with the sign information
    for (int address = 0; address < 256; address++)
    {
        writeEEPROM(address + 768, 0x00); // Assuming 0x00 means no sign
    }

    // program 2's complement of the values from -128 to 127
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
}

void programEEPROM_ucode()
{
    for(int address = 0; address < 1024; address++){
        int flags =       (address & 0b1100000000) >> 8; // Extract the flags from the address
        int byte_sel =    (address & 0b0010000000) >> 7; // Extract the byte select from the address
        int instruction = (address & 0b0001111000) >> 3; // Extract the instruction from the address
        int step =        (address & 0b0000000111);      // Extract the step from the address

        if(byte_sel){
            writeEEPROM(address, ucode[flags][instruction][step]); // Write the LSB
        }else{
            writeEEPROM(address, ucode[flags][instruction][step] >> 8); // Write the MSB
        }
    }
}

/**
 * @brief Function for application main entry.
 */
int main(void)
{
    /* Initialize the ucode array with the microcode template. */
    init_ucode();
    /* Configure board. */
    bsp_board_init(BSP_INIT_LEDS | BSP_INIT_BUTTONS);

    // Turn on LED to indicate the application has started.
    // This is useful for debugging and confirming that the application is running.
    bsp_board_led_on(BSP_BOARD_LED_0); // Green LED
    nrf_delay_ms(500);

    /* Initialize GPIO pins for shift register control. */
    nrf_gpio_cfg_output(shift_data);
    nrf_gpio_cfg_output(shift_clk);
    nrf_gpio_cfg_output(shift_latch);

    /* Initialize GPIO pin for write enable control. */
    // This pin is used to enable writing to the EEPROM.
    nrf_gpio_cfg_output(write_en);
    nrf_gpio_pin_set(write_en); // Set write enable high

    /* Initialize GPIO pins for data output. */
    for (int i = 0; i < 8; i++)
    {
        nrf_gpio_cfg_output(data_pins[i]); // Configure each data pin as output
    }

    // program eeprom with ucode
    programEEPROM_ucode();    

    /* Main loop */
    while (true)
    {
        // pulse blue LED forever
        bsp_board_led_on(BSP_BOARD_LED_3);
        nrf_delay_ms(500);
        // read the button state
        if (bsp_board_button_state_get(BSP_BOARD_BUTTON_0))
        {
            // Button is pressed, turn off the green LED
            bsp_board_led_off(BSP_BOARD_LED_0);
            nrf_delay_ms(2000);

            // reset the board using nvic system reset
            NVIC_SystemReset();
        }
        bsp_board_led_off(BSP_BOARD_LED_3);
        nrf_delay_ms(500);
    }
}
