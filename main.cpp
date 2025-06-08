// Contributors:
// Aditya Bhatia - htxbhatia@ucdavis.edu
// Hanna Shui - hshui@ucdavis.edu

//*****************************************************************************
//
// Copyright (C) 2014 Texas Instruments Incorporated - http://www.ti.com/ 
// 
// 
//  Redistribution and use in source and binary forms, with or without 
//  modification, are permitted provided that the following conditions 
//  are met:
//
//    Redistributions of source code must retain the above copyright 
//    notice, this list of conditions and the following disclaimer.
//
//    Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the 
//    documentation and/or other materials provided with the   
//    distribution. 
//
//    Neither the name of Texas Instruments Incorporated nor the names of
//    its contributors may be used to endorse or promote products derived
//    from this software without specific prior written permission.
//
//  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 
//  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT 
//  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
//  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT 
//  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
//  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT 
//  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
//  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
//  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
//  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE 
//  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
//
//*****************************************************************************


//*****************************************************************************
//
// Application Name     -   SSL Demo
// Application Overview -   This is a sample application demonstrating the
//                          use of secure sockets on a CC3200 device.The
//                          application connects to an AP and
//                          tries to establish a secure connection to the
//                          Google server.
// Application Details  -
// docs\examples\CC32xx_SSL_Demo_Application.pdf
// or
// http://processors.wiki.ti.com/index.php/CC32xx_SSL_Demo_Application
//
//*****************************************************************************


//*****************************************************************************
//
//! \addtogroup ssl
//! @{
//
//*****************************************************************************

// Contributors:
// Aditya Bhatia - htxbhatia@ucdavis.edu
// Hanna Shui - hshui@ucdavis.edu

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

// Simplelink includes
#include "simplelink.h"

// Driverlib includes
#include "hw_types.h"
#include "interrupt.h"
#include "hw_ints.h"
#include "hw_apps_rcm.h"
#include "hw_common_reg.h"
#include "prcm.h"
#include "rom.h"
#include "rom_map.h"
#include "hw_memmap.h"
#include "hw_nvic.h"
#include "timer.h"
#include "gpio.h"
#include "utils.h"
#include "uart.h"
#include "systick.h"
#include "spi.h"

//Common interface includes
#include "pinmux.h"
#include "gpio_if.h"
#include "common.h"
#include "uart_if.h"
#include "timer_if.h"

// Custom includes
#include "utils/network_utils.h"
#include "pitches.h"

// Includes Adafruit OLED and Fingerprint
#include "Adafruit_GFX.h"
#include "Adafruit_SSD1351.h"
#include "glcdfont.h"
#include "oled_test.h"
#include "Adafruit_Fingerprint.h"


//NEED TO UPDATE THIS FOR IT TO WORK!
#define DATE                4    /* Current Date */
#define MONTH               6     /* Month 1-12 */
#define YEAR                2025  /* Current year */
#define HOUR                15    /* Time - hours */
#define MINUTE              36    /* Time - minutes */
#define SECOND              0     /* Time - seconds */


#define APPLICATION_NAME      "SSL"
#define APPLICATION_VERSION   "SQ25"
#define SERVER_NAME           "a2oipe2vzqi2uq-ats.iot.us-east-1.amazonaws.com" // CHANGE ME
#define GOOGLE_DST_PORT       8443


#define POSTHEADER "POST /things/Aditya_CC3200_Board/shadow HTTP/1.1\r\n"             // CHANGE ME
#define GETHEADER "GET /things/Aditya_CC3200_Board/shadow HTTP/1.1\r\n"
#define HOSTHEADER "Host: a2oipe2vzqi2uq-ats.iot.us-east-1.amazonaws.com\r\n"  // CHANGE ME
#define CHEADER "Connection: Keep-Alive\r\n"
#define CTHEADER "Content-Type: application/json; charset=utf-8\r\n"
#define CLHEADER1 "Content-Length: "
#define CLHEADER2 "\r\n\r\n"

#define DATA1 "{" \
            "\"state\": {\r\n"                                              \
                "\"desired\" : {\r\n"                                       \
                    "\"Message\" : {\r\n"                                   \
                        "\"default\" : \""                                  \
                        "default "                                          \
                        "message from CC3200 via AWS IoT!"                  \
                        "\",\r\n"                                           \
                        "\"email\" : \""                                    \


#define DATA2           "\"\r\n"                                            \
                    "}"                                                     \
                "}"                                                         \
            "}"                                                             \
        "}\r\n\r\n"


//****************************************************************************
//                      LOCAL FUNCTION PROTOTYPES
//****************************************************************************
static int set_time();
static void BoardInit(void);
static int http_post(int, int);
static int http_get(int);


//*****************************************************************************
//
// Application Master/Slave mode selector macro
//
// MASTER_MODE = 1 : Application in master mode
// MASTER_MODE = 0 : Application in slave mode
//
//*****************************************************************************
#define MASTER_MODE      1

#define SPI_IF_BIT_RATE  100000
#define TR_BUFF_SIZE     100

#define MASTER_MSG       "This is CC3200 SPI Master Application\n\r"
#define SLAVE_MSG        "This is CC3200 SPI Slave Application\n\r"

// some helpful macros for sysTick
#define SYSCLKFREQ 80000000ULL

#define TICKS_TO_US(ticks) \
        ((((ticks) / SYSCLKFREQ) * 1000000ULL) + \
        ((((ticks) % SYSCLKFREQ) * 1000000ULL) / SYSCLKFREQ))\

#define US_TO_TICKS(us) ((SYSCLKFREQ / 1000000ULL) * (us))
// sysTick reload value set to 40ms period
// (PERIOD_SEC) * (SYSCLKFREQ) = PERIOD_TICKS
#define SYSTICK_RELOAD_VAL 3200000UL


#if defined(ccs)
extern void (* const g_pfnVectors[])(void);
#endif
#if defined(ewarm)
extern uVectorEntry __vector_table;
#endif


volatile bool systick_expired = 0;

static inline void SysTickReset(void) {
    HWREG(NVIC_ST_CURRENT) = 1;
    systick_expired = 0;
}

// pin 03 info for reading IR receiver
#define IR_GPIO_PORT GPIOA1_BASE
#define IR_GPIO_PIN 0x10

// volatile variables for interrupts
volatile uint64_t ulsystick_delta_us = 0;
volatile uint32_t bitsequence = 0;
volatile bool start = 0;
volatile bool readyToPrint = 0;
volatile int bitCount = 0;
// state variable for alarm
// 0 -> Off, 1 -> On, 2 -> Alert, 3 -> Reset password
volatile int alarmState = 0;
// var if in input mode
// 0 -> no input, 1 -> enable or password
volatile int inputMode = 0;
// 0 -> no fingerprint, 1 -> enable or fingerprint
volatile int fpMode = 0;
// var to print new screen in main while loop
volatile bool print = false;
// var to check if motion detected
volatile bool motion = false;


/**
* Interrupt handler for GPIOA1 port
*
* Only used here for decoding IR transmissions
*/
static void GPIOA1IntHandler(void) {
    static bool prev_state = 1;
    // get and clear status
    unsigned long ulStatus;
    ulStatus = MAP_GPIOIntStatus(IR_GPIO_PORT, true);
    MAP_GPIOIntClear(IR_GPIO_PORT, ulStatus);

    // check in interrupt occurred on pin 03
    if (ulStatus & IR_GPIO_PIN) {
        if (prev_state) {
        // previous state was high -> falling edge

            if (!systick_expired) {
                // if sysTick expired, the pulse was longer than 40ms
                // don't measure it in that case

                // calculate the pulse width
                ulsystick_delta_us = TICKS_TO_US(SYSTICK_RELOAD_VAL - MAP_SysTickValueGet());
                // if pulse between 4400 and 4500 us -> start bit -> start flag true, reset bit sequence
                if (ulsystick_delta_us > 4400 && ulsystick_delta_us < 4500) { start = 1; bitsequence = 0;}
                // if pulse b/w 500 and 600 us -> 0 bit
                if (start && ulsystick_delta_us > 500 && ulsystick_delta_us < 600) {
                    // simply increment bit count
                    bitCount++;
                }
                // if pulse b/w 1500 and 1800 us -> 1 bit
                else if (start && ulsystick_delta_us > 1500 && ulsystick_delta_us < 1800) {
                    // set bit at position 32 - bitCount to 1
                    bitsequence |= (0x80000000 >> bitCount);
                    bitCount++;
                }
                // if very long pulse -> sequence over
                else if (start && ulsystick_delta_us > 39000 && ulsystick_delta_us < 40000) {
                    // start is false, bitsequence is ready to read, reset bit count
                    start = 0; readyToPrint = 1; bitCount = 0;
                }
                // reset time delta
                ulsystick_delta_us = 0;
            }
        } else {
            // previous state was low -> rising edge

            // begin measuring a new pulse width, reset the delta and sysTick
            ulsystick_delta_us = 0;
            SysTickReset();
        }
        // store prev state, high or low
        prev_state = MAP_GPIOPinRead(IR_GPIO_PORT, IR_GPIO_PIN) ? 1 : 0;
    }
    return;
}

/**
* SysTick Interrupt Handler
*
* Keep track of whether the sysTick counter expired
*/
static void SysTickHandler(void) {
    systick_expired = 1;
}

// decoding information
uint32_t codes[] = {0x20df08f7, 0x20df8877, 0x20df48b7, 0x20dfc837, 0x20df28d7, 0x20dfa857, 0x20df6897, 0x20dfe817, 0x20df18e7, 0x20df9867, 0x20df906f, 0x20df58a7};
char buttons[] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'm', 'l'};
// password for disarming alarm
// 7 characters long due to weird behavior on character 0
char password[8] = "0123456";

// set max length of message to 6
#define MAX_MSG_LEN 6

// resets bottom half of screen with password input prompt
void resetBottom() {
    // Clear bottom half
    fillRect(0, 65, 128, 63, BLACK);
    char text[9] = "Password";
    int i;
    for (i = 0; i < 8; i++) {
        drawChar(i * 6, 66, text[i], WHITE, BLACK, 1);
    }
    // print '>' (62 in ascii)
    drawChar(48, 66, 62, WHITE, BLACK, 1);
    // print '_' (95 in ascii) at current character position
    drawChar(54, 66, 95, WHITE, BLACK, 1);
}


// initialises SPI for OLED
void SPIInit(){
    // Reset SPI
    MAP_SPIReset(GSPI_BASE);

    //Enables the transmit and/or receive FIFOs.
    //Base address is GSPI_BASE, SPI_TX_FIFO || SPI_RX_FIFO are the FIFOs to be enabled
    MAP_SPIFIFOEnable(GSPI_BASE, SPI_TX_FIFO || SPI_RX_FIFO);

    // Configure SPI interface
    MAP_SPIConfigSetExpClk(GSPI_BASE,MAP_PRCMPeripheralClockGet(PRCM_GSPI),
                     SPI_IF_BIT_RATE,SPI_MODE_MASTER,SPI_SUB_MODE_0,
                     (SPI_SW_CTRL_CS |
                     SPI_4PIN_MODE |
                     SPI_TURBO_OFF |
                     SPI_CS_ACTIVELOW |
                     SPI_WL_8));

    // Enable SPI for communication
    MAP_SPIEnable(GSPI_BASE);
}

// initialisations for input messages
char inputBuffer[MAX_MSG_LEN] = {0};
int inputIndex = 0;
// starting pos for bottom half
int x_pos = 54; int y_pos = 66;
// return value for http post
long lRetVal = -1;
// password attempts
int passwordAttempts = 3;
// incorrect password flag
bool incorrect = false;
// success password flag
bool success = false;
// fingerprint status
// -1 -> error, 0 -> nothing, 1 -> success, 2 -> no match
int fpSuccess = 0;
// confirm password flag to allow for second password input
bool confirm = false;
// id stage flag (choosing between resetting password or adding new fingerprint)
bool idStage = true;
// password attempt
char attempt[8] = "0000000";
// fingerprint sensor communicating over UART1
Adafruit_Fingerprint finger = Adafruit_Fingerprint(UARTA1_BASE);
// id for fingerprint
uint8_t id;

// draws char being typed in bottom half of screen
void displayChar(char c) {
    if (y_pos >= 115) return; // don't draw if end of screen
    drawChar(x_pos, y_pos, c, WHITE, BLACK, 1);
    x_pos += 6; // 6 pixels per character (5 pixels wide + 1 spacing)

    if (x_pos >= 120) {         // wrap to next line
        x_pos = 0;
        y_pos += 8;             // 8 pixel height per line
        if (y_pos >= 115) return; // stop drawing if end of screen
    }
    // print '_' (95 in ascii) at current character position
    drawChar(x_pos, y_pos, 95, WHITE, BLACK, 1);
}

// draws char being typed in bottom half of screen
void addCharToInput(char c) {
    // set curr index to char and following index to '\0'
    if (inputIndex < MAX_MSG_LEN) {
        inputBuffer[inputIndex++] = c;
        inputBuffer[inputIndex] = '\0';
        displayChar(c);
    }
}

// deletes last char
void deleteLastChar(bool changeIndex) {
    if (inputIndex > 0) {
        // only decrement index when deleting, else it simply replaces the character at the current spot
        if (changeIndex) { inputBuffer[--inputIndex] = '\0'; }
        // draw a black '_' (95 in ascii) to clear '_'
        drawChar(x_pos, y_pos, 95, BLACK, BLACK, 1);
        // check if we're at the start of a row with no written characters
        if (y_pos >= 74 && x_pos < 6) {
            // go back to the last character of the previous row
            y_pos -= 8; x_pos = 120;
        } else {
            x_pos -= 6;
        }
        // draw a black ' ' (32 in ascii) to clear a character
        drawChar(x_pos, y_pos, 32, BLACK, BLACK, 1);
        // redraw '_'
        drawChar(x_pos, y_pos, 95, WHITE, BLACK, 1);
    }
}


// print password incorrect message
void incorrectMessage(int attemptsLeft) {
    char text[11] = "Incorrect!";
    int i;
    for (i = 0; i < 10; i++) {
        drawChar(i * 6, 80, text[i], RED, BLACK, 1);
    }
    // if not on last attempt or on first attempt, print number of attempts left
    if (attemptsLeft != 3) {
        drawChar(0, 90, '0' + attemptsLeft, RED, BLACK, 1);
        char text2[15] = " Attempts Left";
        int j;
        for (j = 0; j < 14; j++) {
            drawChar(6 + j * 6, 90, text2[j], RED, BLACK, 1);
        }
    }
}


// print password/fingerprint success message
void successMessage() {
    char text[9] = "Success!";
    int i;
    for (i = 0; i < 8; i++) {
        drawChar(i * 6, 80, text[i], GREEN, BLACK, 1);
    }
}

// custom string comparison function
// returns 0 if strings are the same, 1 if not
// string 2 needs a blank character at the start due to weird behavior on character 0
int strcomp(const char* input, const char* string2) {
    int len = strlen(input);
    if (len != 6) { return 1; }
    int i;
    for (i = 0; i < 6; i++) {
        if (input[i] != string2[i + 1]) { return 1; }
    }
    return 0;
}


// prints 6 digits only message
void sixDigits() {
    char text[14] = "6 Digits Only";
    int i;
    for (i = 0; i < 13; i++) {
        drawChar(i * 6, 90, text[i], RED, BLACK, 1);
    }
}


// prints 1 digit only message
void oneDigit() {
    char text[19] = "1 Digit (1-9) Only";
    int i;
    for (i = 0; i < 18; i++) {
        drawChar(i * 6, 90, text[i], RED, BLACK, 1);
    }
}


// prints password/fingerprint mismatch message
void mismatchMessage() {
    char text[10] = "Mismatch!";
    int i;
    for (i = 0; i < 9; i++) {
        drawChar(i * 6, 90, text[i], RED, BLACK, 1);
    }

    char text2[13] = "SW3 to Reset";
    int j;
    for (j = 0; j < 12; j++) {
        drawChar(j * 6, 100, text2[j], RED, BLACK, 1);
    }

    inputIndex = 0;
    inputBuffer[0] = '\0';
    x_pos = 54; y_pos = 66;
}

// prints fingerprint error message
void fingerprintError() {
    char text[18] = "Fingerprint Error";
    int i;
    for (i = 0; i < 17; i++) {
        drawChar(i * 6, 90, text[i], RED, BLACK, 1);
    }
}


// processes button presses
void processButton(char btn) {
    // if message has reached max length, forcefully trigger a submit
    if (inputIndex == MAX_MSG_LEN) { btn = 'm'; }
    switch (btn) {
        // if button is 'Mute', handle alarm state changes or character input
        case 'm':
            // if alarm is off, handle input mode changes
            if (alarmState == 0) {
                // if not in input mode, set to input mode and print
                if (inputMode == 0) { inputMode = 1; print = true; return;}
                // if in input mode, set to alarm on and print
                else if (inputMode == 1) { inputMode = 0; alarmState = 1; print = true; return;}
            }
            // if alarm is on, handle input mode changes
            else if (alarmState == 1) {
                // if not in input mode, set to input mode and print
                if (inputMode == 0) { inputMode = 1; print = true; return;}
                // if in input mode, check if password is correct or fingerprint is successful
                else if (inputMode == 1) {
                    if ((strcomp(inputBuffer, password) == 0) || (fpSuccess == 1)) {
                        // if password is correct or fingerprint is successful, set alarm to off, input mode to off, print success message
                        // reset password attempts, set success flag, reset fingerprint success flag
                        alarmState = 0; inputMode = 0; print = true; incorrect = false; passwordAttempts = 3; success = true;
                        fpSuccess = 0;
                    }
                    // if fingerprint is not in error state, handle password/fingerprint attempts
                    else if (fpSuccess != -1) {
                        // if password/fingerprint is incorrect, set incorrect flag, decrement password attempts, print false, set fp mode to 1
                        incorrect = true; passwordAttempts--; print = false; fpMode = 1;
                        // if no attempts left, set alarm to triggered, reset incorrect flag, reset password attempts, print true, set fp mode to 0
                        if (passwordAttempts == 0) {
                            alarmState = 2; incorrect = false; passwordAttempts = 3; print = true; fpMode = 0;
                        }
                    }
                    // if fingerprint is in error state, print error message and set fp mode to 1
                    else {
                        fingerprintError(); fpMode = 1;
                    }
                }
            }
            // if alarm is triggered, handle password/fingerprint attempts
            else if (alarmState == 2) {
                // if password/fingerprint is correct, set alarm to off, input mode to off, print success message
                // reset password attempts, set success flag, reset fingerprint success flag
                if ((strcomp(inputBuffer, password) == 0) || (fpSuccess == 1)) {
                    alarmState = 0; inputMode = 0; print = true; incorrect = false; passwordAttempts = 3; success = true;
                    fpSuccess = 0;
                }
                // if fingerprint is not in error state, handle password/fingerprint attempts
                else if (fpSuccess != -1) {
                    // if password/fingerprint is incorrect, set incorrect flag, decrement password attempts, print false, set fp mode to 1
                    incorrect = true; print = false; fpMode = 1;
                    // if no attempts left, set alarm to triggered, reset incorrect flag, reset password attempts, print true, set fp mode to 0
                    if (passwordAttempts == 0) {
                        alarmState = 2; incorrect = false; passwordAttempts = 3; print = true; fpMode = 0;
                    }
                }
                // if fingerprint is in error state, print error message and set fp mode to 1
                else {
                    fingerprintError(); fpMode = 1;
                }
            }
            // if alarm is in reset password/new fingerprint state, handle id stage or password input
            else if (alarmState == 3) {
                // if id stage, handle id input
                if (idStage) {
                    // if not a single digit, print error message
                    if (inputIndex != 1) {
                        oneDigit(); return;
                    }
                    // if a single digit, set id to the digit and set fp mode to 0 if 0, 2 if 1-9
                    else {
                        id = inputBuffer[0] - '0';
                        if (id == 0) { fpMode = 0; } // reset password
                        else { fpMode = 2; } // new fingerprint
                        idStage = false; print = true;
                    }
                }
                // if not id stage, handle new password input
                else if (inputIndex != MAX_MSG_LEN) {
                    // if not 6 digits, print error message
                    sixDigits(); return;
                }
                // if 6 digits, handle password input
                else {
                    // if not confirming password, set attempt to input buffer and print
                    if (!confirm) {
                        int i;
                        for (i = 0; i < 6; i++) {
                            attempt[i + 1] = inputBuffer[i];
                        }
                        print = true; confirm = true;
                    }
                    // if confirming password, check if password is correct
                    else {
                        // if password is correct, set success flag, set input mode to off, set alarm to off, print success message, reset confirm flag
                        // set password to input buffer, send http post with state 3
                        if (strcomp(inputBuffer, attempt) == 0) {
                            success = true; inputMode = 0; alarmState = 0; print = true; confirm = false;
                            int i;
                            for (i = 0; i < 6; i++) {
                                password[i + 1] = inputBuffer[i];
                            }
                            http_post(lRetVal, 3);
                        }
                        // if password is incorrect, print mismatch message, set input mode to off, set alarm to off, reset confirm flag
                        // this forces user to press SW3 to reset back to this state
                        else {
                            mismatchMessage(); inputMode = 0; alarmState = 0; confirm = false;
                            return;
                        }
                    }
                }
            }
            // reset input index, input buffer
            inputIndex = 0;
            inputBuffer[0] = '\0';
            // if incorrect, print incorrect message
            if (incorrect) {
                incorrectMessage(passwordAttempts);
            }
            // if not success or confirming password, reset bottom half of screen
            if (!success && !confirm) { resetBottom(); }
            // if success, print success message
            if (success) {
                success = false;
                successMessage();
            }
            // reset x and y positions for next input
            x_pos = 54; y_pos = 66;
            break;
        // if 'last' delete character
        case 'l':
            // if in input mode, delete last character
            if (inputMode == 1) { deleteLastChar(true); }
            break;
        // if any number, add the number to input buffer
        default:
            // if in input mode, add character to input buffer
            if (inputMode == 1) { addCharToInput(btn); }
            break;
    }
}


// sets the OLED to the alarm off state
// sets alarm state to off, input mode to off, turns off red LED
// fills screen black, prints "Alarm Off" in green, prints "To Enable, Press m" in white
void printAlarmOffScreen() {
    alarmState = 0; inputMode = 0;
    GPIO_IF_LedOff(MCU_RED_LED_GPIO);

    fillScreen(BLACK);
    char text[10] = "Alarm Off";
    int i;
    for (i = 0; i < 9; i++) {
        drawChar(10 + i * 12, 20, text[i], GREEN, BLACK, 2);
    }
    char enable[19] = "To Enable, Press m";
    int j;
    for (j = 0; j < 18; j++) {
        drawChar(j * 6, 45, enable[j], WHITE, BLACK, 1);
    }
}


// prints alarm enable options when in off state
// clears bottom half of screen, prints "Enable?" in white, prints "Yes(m)    No(SW3)" in white
void printEnableOptions() {
    // Clear bottom half
    fillRect(0, 65, 128, 63, BLACK);
    char text[8] = "Enable?";
    int i;
    for (i = 0; i < 7; i++) {
        drawChar(10 + i * 8, 65, text[i], WHITE, BLACK, 1);
    }
    char options[19] = "Yes(m)    No(SW3)";
    int j;
    for (j = 0; j < 17; j++) {
        drawChar(10 + j * 6, 75, options[j], WHITE, BLACK, 1);
    }
}


// sets the OLED to the alarm on state
// sets alarm state to on, input mode to off, turns on red LED
// fills screen black, prints "Alarm On" in red, prints "To Disable, Press m" in white
void printAlarmOnScreen() {
    alarmState = 1; inputMode = 0;
    GPIO_IF_LedOn(MCU_RED_LED_GPIO);

    fillScreen(BLACK);
    char text[9] = "Alarm On";
    int i;
    for (i = 0; i < 8; i++) {
        drawChar(16 + i * 12, 20, text[i], RED, BLACK, 2);
    }
    char enable[20] = "To Disable, Press m";
    int j;
    for (j = 0; j < 19; j++) {
        drawChar(j * 6, 45, enable[j], WHITE, BLACK, 1);
    }
}


// sets the OLED to the alarm triggered state
// sets alarm state to triggered, input mode to 1
// fills screen black, prints "Triggered!" in red
void printAlarmTriggeredScreen() {
    alarmState = 2; inputMode = 1;
    fillScreen(BLACK);
    char text[11] = "Triggered!";
    int i;
    for (i = 0; i < 10; i++) {
        drawChar(4 + i * 12, 20, text[i], RED, BLACK, 2);
    }
}


// prints id stage
// prints "0:PSWD 1-9:NFP" in white, prints ">" in white, prints "_" in white
void drawIdStage() {
    char text[15] = "0:PSWD 1-9:NFP";
    int i;
    for (i = 0; i < 14; i++) {
        drawChar(i * 6, 66, text[i], WHITE, BLACK, 1);
    }
    // print '>' (62 in ascii)
    drawChar(84, 66, 62, WHITE, BLACK, 1);
    // print '_' (95 in ascii) at current character position
    drawChar(90, 66, 95, WHITE, BLACK, 1);
    x_pos = 90;
}


// prints confirm stage
// prints "Confirm New" in white
void drawConfirm() {
    char text[12] = "Confirm New";
    int i;
    for (i = 0; i < 11; i++) {
        drawChar(i * 6, 80, text[i], WHITE, BLACK, 1);
    }
}


// prints new password stage
// prints "New Password" in white
void drawNewPass() {
    char text[13] = "New Password";
    int i;
    for (i = 0; i < 12; i++) {
        drawChar(i * 6, 80, text[i], WHITE, BLACK, 1);
    }
}


// params for buzzer
#define TIMER_INTERVAL_RELOAD   40035 /* =(255*157) */
#define DUTYCYCLE_GRANULARITY   157


//*****************************************************************************
//                 GLOBAL VARIABLES -- Start
//*****************************************************************************

#if defined(ccs) || defined(gcc)
extern void (* const g_pfnVectors[])(void);
#endif
#if defined(ewarm)
extern uVectorEntry __vector_table;
#endif

//*****************************************************************************
//                 GLOBAL VARIABLES -- End: df
//*****************************************************************************


//****************************************************************************
//
//! Update the duty cycle of the PWM timer
//!
//! \param ulBase is the base address of the timer to be configured
//! \param ulTimer is the timer to be setup (TIMER_A or  TIMER_B)
//! \param ucLevel translates to duty cycle settings (0:255)
//!
//! This function
//!    1. The specified timer is setup to operate as PWM
//!
//! \return None.
//
//****************************************************************************
void UpdateDutyCycle(unsigned long ulBase, unsigned long ulTimer,
                     unsigned char ucLevel)
{
    //
    // Match value is updated to reflect the new duty cycle settings
    //
    MAP_TimerMatchSet(ulBase,ulTimer,(ucLevel*DUTYCYCLE_GRANULARITY));
}
//****************************************************************************
//
//! Setup the timer in PWM mode
//!
//! \param ulBase is the base address of the timer to be configured
//! \param ulTimer is the timer to be setup (TIMER_A or  TIMER_B)
//! \param ulConfig is the timer configuration setting
//! \param ucInvert is to select the inversion of the output
//!
//! This function
//!    1. The specified timer is setup to operate as PWM
//!
//! \return None.
//
//****************************************************************************
void SetupTimerPWMMode(unsigned long ulBase, unsigned long ulTimer,
                       unsigned long ulConfig, unsigned char ucInvert)
{
    //
    // Set GPT - Configured Timer in PWM mode.
    //
    MAP_TimerConfigure(ulBase,ulConfig);
    MAP_TimerPrescaleSet(ulBase,ulTimer,0);

    //
    // Inverting the timer output if required
    //
    MAP_TimerControlLevel(ulBase,ulTimer,ucInvert);

    //
    // Load value set to ~0.5 ms time period
    //
    MAP_TimerLoadSet(ulBase,ulTimer,TIMER_INTERVAL_RELOAD);

    //
    // Match value set so as to output level 0
    //
    MAP_TimerMatchSet(ulBase,ulTimer,TIMER_INTERVAL_RELOAD);

}

//****************************************************************************
//
//! Sets up the identified timers as PWM to drive the peripherals
//!
//! \param none
//!
//! This function sets up the following
//!    1. TIMERA0 as Buzzer
//!
//! \return None.
//
//****************************************************************************
void InitPWMModules()
{
    //
    // Initialization of timers to generate PWM output
    //
    MAP_PRCMPeripheralClkEnable(PRCM_TIMERA0, PRCM_RUN_MODE_CLK);

    //
    // TIMERA0 as Buzzer
    //
    SetupTimerPWMMode(TIMERA0_BASE, TIMER_A, (TIMER_CFG_SPLIT_PAIR | TIMER_CFG_A_PWM), 1);
    MAP_TimerEnable(TIMERA0_BASE,TIMER_A);
}

//****************************************************************************
//
//! Disables the timer PWMs
//!
//! \param none
//!
//! This function disables the timers used
//!
//! \return None.
//
//****************************************************************************
void DeInitPWMModules()
{
    //
    // Disable the peripherals
    //
    MAP_TimerDisable(TIMERA0_BASE, TIMER_A);
    MAP_PRCMPeripheralClkDisable(PRCM_TIMERA0, PRCM_RUN_MODE_CLK);
}


//*****************************************************************************
//
//! Board Initialization & Configuration
//!
//! \param  None
//!
//! \return None
//
//*****************************************************************************
static void BoardInit(void) {
/* In case of TI-RTOS vector table is initialize by OS itself */
#ifndef USE_TIRTOS
  //
  // Set vector table base
  //
#if defined(ccs)
    MAP_IntVTableBaseSet((unsigned long)&g_pfnVectors[0]);
#endif
#if defined(ewarm)
    MAP_IntVTableBaseSet((unsigned long)&__vector_table);
#endif
#endif
    //
    // Enable Processor
    //
    MAP_IntMasterEnable();
    MAP_IntEnable(FAULT_SYSTICK);

    PRCMCC3200MCUInit();
}




//*****************************************************************************
//
//! This function updates the date and time of CC3200.
//!
//! \param None
//!
//! \return
//!     0 for success, negative otherwise
//!
//*****************************************************************************

static int set_time() {
    long retVal;

    g_time.tm_day = DATE;
    g_time.tm_mon = MONTH;
    g_time.tm_year = YEAR;
    g_time.tm_sec = SECOND;
    g_time.tm_hour = HOUR;
    g_time.tm_min = MINUTE;

    retVal = sl_DevSet(SL_DEVICE_GENERAL_CONFIGURATION,
                          SL_DEVICE_GENERAL_CONFIGURATION_DATE_TIME,
                          sizeof(SlDateTime),(unsigned char *)(&g_time));

    ASSERT_ON_ERROR(retVal);
    return SUCCESS;
}

// initialises fingerprint sensor
// prints "Found fingerprint sensor!" if successful
// prints "Did not find fingerprint sensor :(" if unsuccessful
// sets fp mode to mode
void beginFingerprint(int mode) {
    finger.begin();
    if (finger.verifyPassword()) {
        Report("Found fingerprint sensor!\n\r");
        finger.LEDcontrol(true);
        fpMode = mode;
    }
    else {
        Report("Did not find fingerprint sensor :(\n\r");
        while (1);
    }
}


// enrolls fingerprint
// prints "Waiting for valid finger to enroll as #id"
// prints "Image taken" if successful
// prints "." if no finger detected
// prints "Imaging error" if imaging error
uint8_t getFingerprintEnroll() {
  int p = -1;
  Report("Waiting for valid finger to enroll as #%d\n\r", id);
  while (p != FINGERPRINT_OK) {
    p = finger.getImage();
    switch (p) {
      case FINGERPRINT_OK:
        Report("Image taken\n\r");
        break;
      case FINGERPRINT_NOFINGER:
        Report(".\n\r");
        break;
      case FINGERPRINT_PACKETRECIEVEERR:
        Report("Communication error\n\r");
        break;
      case FINGERPRINT_IMAGEFAIL:
        Report("Imaging error\n\r");
        break;
      default:
        Report("Unknown error\n\r");
        break;
    }
  }
  // convert image to template
  // wait for image to be converted
  // print "Image converted" if successful
  // print "." if no finger detected
  // print "Imaging error" if imaging error
  // print "Unknown error" if unknown error
  p = -1;
  while (p != FINGERPRINT_OK) {
      p = finger.image2Tz(1);
      switch (p) {
          case FINGERPRINT_OK:
            Report("Image converted\n\r");
            break;
          case FINGERPRINT_IMAGEMESS:
            fingerprintError();
            return p;
          case FINGERPRINT_PACKETRECIEVEERR:
            Report("Communication error\n\r");
            break;
          case FINGERPRINT_FEATUREFAIL:
          case FINGERPRINT_INVALIDIMAGE:
            fingerprintError();
            return p;
          default:
            fingerprintError();
            return p;
     }
  }
  // wait for finger to be removed
  Report("Remove finger\n\r");
  resetBottom();
  drawConfirm();
  MAP_UtilsDelay(10000000);
  p = 0;
  while (p != FINGERPRINT_NOFINGER) {
    p = finger.getImage();
  }
  // print "ID #id"
  Report("ID %d\n\r", id);
  p = -1;
  Report("Place same finger again\n\r");
  // wait for finger to be placed again
  while (p != FINGERPRINT_OK) {
    p = finger.getImage();
    switch (p) {
      case FINGERPRINT_OK:
        Report("Image taken\n\r");
        break;
      case FINGERPRINT_NOFINGER:
        Report(".\n\r");
        break;
      case FINGERPRINT_PACKETRECIEVEERR:
        Report("Communication error\n\r");
        break;
      case FINGERPRINT_IMAGEFAIL:
        Report("Imaging error\n\r");
        break;
      default:
        Report("Unknown error\n\r");
        break;
    }
  }
  // convert image to template
  p = -1;
  while (p != FINGERPRINT_OK) {
      p = finger.image2Tz(2);
      switch (p) {
          case FINGERPRINT_OK:
            Report("Image converted\n\r");
            break;
          case FINGERPRINT_IMAGEMESS:
            fingerprintError();
            return p;
          case FINGERPRINT_PACKETRECIEVEERR:
            Report("Communication error\n\r");
            break;
          case FINGERPRINT_FEATUREFAIL:
          case FINGERPRINT_INVALIDIMAGE:
            fingerprintError();
            return p;
          default:
            fingerprintError();
            return p;
    }
  }
  // create model for fingerprint
  // print "Creating model for #id"
  // print "Prints matched!" if successful
  // print "Communication error" if communication error
  // print "Enroll mismatch" if enroll mismatch
  // print "Unknown error" if unknown error
  Report("Creating model for #%d\n\r", id);
  p = finger.createModel();
  if (p == FINGERPRINT_OK) {
    Report("Prints matched!\n\r");
  } else if (p == FINGERPRINT_PACKETRECIEVEERR) {
    fingerprintError();
    return p;
  } else if (p == FINGERPRINT_ENROLLMISMATCH) {
    mismatchMessage();
    return p;
  } else {
    fingerprintError();
    return p;
  }
  // store model for fingerprint
  // print "ID #id"
  Report("ID %d\n\r", id);
  p = finger.storeModel(id);
  if (p == FINGERPRINT_OK) {
    successMessage();
  } else if (p == FINGERPRINT_PACKETRECIEVEERR) {
    fingerprintError();
    return p;
  } else if (p == FINGERPRINT_BADLOCATION) {
    fingerprintError();
    return p;
  } else if (p == FINGERPRINT_FLASHERR) {
    fingerprintError();
    return p;
  } else {
    fingerprintError();
    return p;
  }

  return true;
}

// gets fingerprint id
// prints "Image taken" if successful
// prints "No finger detected" if no finger detected
// prints "Communication error" if communication error
// prints "Imaging error" if imaging error
// prints "Unknown error" if unknown error
uint8_t getFingerprintID() {
  int p = -1;
  while (p != FINGERPRINT_OK) {
      p = finger.getImage();
        switch (p) {
          case FINGERPRINT_OK:
            Report("Image taken\n\r");
            break;
          case FINGERPRINT_NOFINGER:
            Report("No finger detected\n\r");
            return p;
          case FINGERPRINT_PACKETRECIEVEERR:
            Report("Communication error\n\r");
            break;
          case FINGERPRINT_IMAGEFAIL:
            Report("Imaging error\n\r");
            return p;
          default:
            Report("Unknown error\n\r");
            return p;
        }
  }

  // convert image to template
  // OK success!
  p = -1;
  while (p != FINGERPRINT_OK) {
      p = finger.image2Tz();
        switch (p) {
          case FINGERPRINT_OK:
            Report("Image converted\n\r");
            break;
          case FINGERPRINT_IMAGEMESS:
            Report("Image too messy\n\r");
            return p;
          case FINGERPRINT_PACKETRECIEVEERR:
            Report("Communication error\n\r");
            break;
          case FINGERPRINT_FEATUREFAIL:
          case FINGERPRINT_INVALIDIMAGE:
            Report("Could not find fingerprint features\n\r");
            return p;
          default:
            Report("Unknown error\n\r");
            return p;
        }
  }

  // search for fingerprint
  // OK converted!
  p = -1;
  while (p != FINGERPRINT_OK) {
      p = finger.fingerSearch();
        if (p == FINGERPRINT_OK) {
          Report("Found a print match!\n\r");
        } else if (p == FINGERPRINT_PACKETRECIEVEERR) {
          Report("Communication error\n\r");
          break;
        } else if (p == FINGERPRINT_NOTFOUND) {
          Report("Did not find a match\n\r");
          return p;
        } else {
          Report("Unknown error\n\r");
          return p;
        }
  }


  // found a match!
  Report("Found ID #%d with confidence of %d\n\r", finger.fingerID, finger.confidence);

  return FINGERPRINT_OK;
}

// motion grace period
unsigned long motionGrace = 1000000;
// time of last movement
unsigned long movementTime = 0;

// params for PIR sensor
// params for PIR sensor
#define PIR_GPIO_PORT GPIOA0_BASE
#define PIR_GPIO_PIN 0x80

// PIR interrupt handler
// if alarm is on, input mode is off, and motion is false, set motion to true, set input mode to 1, set fp mode to 1, set print to true, set movement time to current time
void PIR_InterruptHandler() {
    unsigned long ulStatus;

    // gets interrupt status and clears it
    ulStatus = MAP_GPIOIntStatus(PIR_GPIO_PORT, true);
    MAP_GPIOIntClear(PIR_GPIO_PORT, ulStatus);

    if (ulStatus & PIR_GPIO_PIN) {
        if (alarmState == 1 && inputMode == 0 && !motion) {
            motion = true; inputMode = 1; fpMode = 1; print = true;
            movementTime = MAP_PRCMSlowClkCtrGet();
        }
    }
}

// initialises PIR sensor
void InitPIRSensor() {
    // enables interrupt on rising edge
    MAP_GPIOIntTypeSet(PIR_GPIO_PORT, PIR_GPIO_PIN, GPIO_RISING_EDGE);

    // register and enable interrupt handler
    MAP_GPIOIntRegister(PIR_GPIO_PORT, PIR_InterruptHandler);
    MAP_GPIOIntClear(PIR_GPIO_PORT, PIR_GPIO_PIN);
    MAP_GPIOIntEnable(PIR_GPIO_PORT, PIR_GPIO_PIN);
}

//*****************************************************************************
//
//! Main 
//!
//! \param  none
//!
//! \return None
//!
//*****************************************************************************
void main() {
    //
    // Initialize board configuration
    //
    BoardInit();

    PinMuxConfig();

    // SysTick init
    systick_expired = 1;
    MAP_SysTickPeriodSet(SYSTICK_RELOAD_VAL);
    MAP_SysTickIntRegister(SysTickHandler);
    MAP_SysTickIntEnable();
    MAP_SysTickEnable();

    // GPIO init
    MAP_GPIOIntRegister(IR_GPIO_PORT, GPIOA1IntHandler);
    MAP_GPIOIntTypeSet(IR_GPIO_PORT, IR_GPIO_PIN, GPIO_BOTH_EDGES); // read ir_output
    uint64_t ulStatus = MAP_GPIOIntStatus(IR_GPIO_PORT, false);
    MAP_GPIOIntClear(IR_GPIO_PORT, ulStatus);
    MAP_GPIOIntEnable(IR_GPIO_PORT, IR_GPIO_PIN);

    // PWM Buzzer init
    InitPWMModules();

    // PIR sensor init
    InitPIRSensor();

    // SPI init
    SPIInit();
    // OLED init
    Adafruit_Init();
    InitTerm();
    ClearTerm();
    UART_PRINT("My terminal works!\n\r");

    // initialize global default app configuration
    g_app_config.host = (signed char*)SERVER_NAME;
    g_app_config.port = GOOGLE_DST_PORT;

    //Connect the CC3200 to the local access point
    lRetVal = connectToAccessPoint();
    //Set time so that encryption can be used

    lRetVal = set_time();
    if(lRetVal < 0) {
        UART_PRINT("Unable to set time in the device");
        LOOP_FOREVER();
    }
    //Connect to the website with TLS encryption
    lRetVal = tls_connect();
    if(lRetVal < 0) {
        ERR_PRINT(lRetVal);
    }

    // clear OLED to black
    fillScreen(BLACK);

    // show alarm off screen
    printAlarmOffScreen();

    // http get request for current shadow state
    http_get(lRetVal);
    // last get request time
    unsigned long lastGet = 0;
    // time between get requests
    unsigned long timeBetween = 1000000;

    // main program loop
    while(1) {
        // current time
        unsigned long now = MAP_PRCMSlowClkCtrGet();
        // if time since last get request is greater than time between get requests, make get request
        if ((now - lastGet) > timeBetween) {
            // http get request for current shadow state
            http_get(lRetVal);
            lastGet = now;
        }

        // this isnt actually doing anything
        if (ulsystick_delta_us) {
            // a pulse width was measured by the gpio handler

            // print the measured pulse width
            Report("measured pulse width: %llu us\n\r", ulsystick_delta_us);

        }

        // store button pressed. initially ' '
        char buttonPressed = ' ';
        // if bitsequence calculation has ended and its ready
        if (!start && readyToPrint) {
            // loop through all codes to find which button was pressed
            int i;
            for (i = 0; i < 12; i++) {
                // index of code matches index of corresponding button
                if (codes[i] == bitsequence) {
                    buttonPressed = buttons[i];
                }
            }
            // display pressed button to terminal
            Report("Button Pressed: %c\n\r", buttonPressed);
            // reset ready to print so that the loop is printing blank characters when no button is pressed
            readyToPrint = 0;
        }
        // if button pressed isnt blank then process the command
        // this check is for the ~2% case where the button inputs arent registered properly
        if (buttonPressed != ' ') {
            processButton(buttonPressed);
            // reset button pressed to blank so that it doesnt contsantly process the same button every loop
            buttonPressed = ' ';
        }
        // if alarm is on, input mode is 1, and motion is true, check if motion grace period has passed
        if (alarmState == 1 && inputMode == 1 && motion) {
            // if motion grace period has passed, trigger alarm
            if ((now - movementTime) > motionGrace) {
                Report("Timeout! Triggering Alarm");
                alarmState = 2; inputMode = 1; fpMode = 1; print = true; motion = false;
                continue;
            }
        }
        // if print is true, alarm state is 0, and input mode is 0, turn off alarm
        if (print && alarmState == 0 && inputMode == 0) {
            print = false;
            // 2 buzzes to indicate alarm is off
            UpdateDutyCycle(TIMERA0_BASE, TIMER_A, 150);
            MAP_UtilsDelay(500000);
            UpdateDutyCycle(TIMERA0_BASE, TIMER_A, 0);
            MAP_UtilsDelay(500000);
            UpdateDutyCycle(TIMERA0_BASE, TIMER_A, 150);
            MAP_UtilsDelay(500000);
            UpdateDutyCycle(TIMERA0_BASE, TIMER_A, 0);
            // reset motion
            motion = false;
            // print alarm off screen
            printAlarmOffScreen();
            // send http post request for alarm off state
            http_post(lRetVal, 0);
        }
        // if print is true, alarm state is 0, and input mode is 1, print enable options
        if (print && alarmState == 0 && inputMode == 1) {
            print = false;
            printEnableOptions();
        }
        // if print is true, alarm state is 1, and input mode is 0, turn on alarm
        if (print && alarmState == 1 && inputMode == 0) {
            print = false;
            // long buzz to indicate alarm is on
            UpdateDutyCycle(TIMERA0_BASE, TIMER_A, 150);
            MAP_UtilsDelay(3000000);
            UpdateDutyCycle(TIMERA0_BASE, TIMER_A, 0);
            // reset motion
            motion = false;
            // print alarm on screen
            printAlarmOnScreen();
            // send http post request for alarm on state
            http_post(lRetVal, 1);
        }
        // if print is true, alarm state is 1, and input mode is 1, begin asking for fingerprint or password
        if (print && alarmState == 1 && inputMode == 1) {
            print = false;
            resetBottom();
            beginFingerprint(1);
        }
        // if fp mode is 1, and input mode is 1, get fingerprint id
        if (fpMode == 1 && inputMode == 1) {
            int fpStatus = getFingerprintID();
            // if fingerprint is found, set fp mode to 0, set fp success to 1, force process mute button
            if (fpStatus == FINGERPRINT_OK) { Report("Found Finger!\n\r"); fpMode = 0; fpSuccess = 1; processButton('m');}
            // if no finger detected, do nothing
            else if (fpStatus == FINGERPRINT_NOFINGER) { }
            // if fingerprint is not found, set fp mode to 0, set fp success to 2, force process mute button
            else if (fpStatus == FINGERPRINT_NOTFOUND) { fpMode = 0; fpSuccess = 2; processButton('m');}
            // if unknown error, set fp success to -1, set fp mode to 0, force process mute button
            else { fpSuccess = -1; fpMode = 0; processButton('m'); }
        }
        // if fp mode is 2, and input mode is 1, enroll fingerprint
        if (fpMode == 2 && inputMode == 1) {
            // begin fingerprint enrollment
            beginFingerprint(2);
            // print "Enrolling ID #id"
            Report("Enrolling ID #%d\n\r", id);
            // wait for fingerprint enrollment
            while (! getFingerprintEnroll() );
            // print success message
            successMessage();
            // reset fp mode, input mode, alarm state, print, and id stage
            fpMode = 0; inputMode = 0; alarmState = 0; print = true; idStage = true;
        }
        // if print is true, alarm state is 2, print alarm triggered screen, reset bottom half of screen, send http post request for alarm triggered state, begin fingerprint enrollment
        if (print && alarmState == 2) {
            print = false; motion = false;
            // print alarm triggered screen
            printAlarmTriggeredScreen();
            // reset bottom half of screen and allow for fingerprint and password input
            resetBottom();
            // send http post request for alarm triggered state
            http_post(lRetVal, 2);
            beginFingerprint(1);
        }
        // if alarm state is 2, constantly pulse LED and buzzer
        if (alarmState == 2) {
            MAP_UtilsDelay(1000000);
            GPIO_IF_LedOn(MCU_RED_LED_GPIO);
            UpdateDutyCycle(TIMERA0_BASE, TIMER_A, 150);
            MAP_UtilsDelay(1000000);
            GPIO_IF_LedOff(MCU_RED_LED_GPIO);
            UpdateDutyCycle(TIMERA0_BASE, TIMER_A, 0);
        }
        // clear any input section if SW3 is pressed
        if (GPIOPinRead(GPIOA1_BASE, 0x20) && inputMode == 1 && alarmState != 2) {
            if (alarmState == 3) { alarmState = 0; idStage = true; }
            inputMode = 0; print = false; fpMode = 0;
            // Clear bottom half
            fillRect(0, 65, 128, 63, BLACK);
        }
        // password reset if on state 0, not inputting, and SW3 is pressed
        if (alarmState == 0 && inputMode == 0 && GPIOPinRead(GPIOA1_BASE, 0x20)) {
            alarmState = 3; print = true; inputMode = 1;
        }
        // if print is true, alarm state is 3, and input mode is 1, print id stage, confirm stage, or new password stage
        if (print && alarmState == 3 && inputMode == 1) {
            // if id stage, print id stage
            if (idStage) { drawIdStage(); }
            // if confirm stage, print confirm stage
            else if (confirm) {
                // if not new fingerprint, reset bottom half of screen
                if (fpMode != 2) { resetBottom(); }
                // print confirm stage
                drawConfirm();
            }
            // if new password stage, print new password stage
            else {
                drawNewPass();
            }
            // reset print
            print = false;
        }
    }
}
//*****************************************************************************
//
// Close the Doxygen group.
//! @}
//
//*****************************************************************************

static int http_post(int iTLSSockID, int state){
    char acSendBuff[512];
    char acRecvbuff[1460];
    char cCLLength[200];
    char* pcBufHeaders;
    int lRetVal = 0;


    pcBufHeaders = acSendBuff;
    strcpy(pcBufHeaders, POSTHEADER);
    pcBufHeaders += strlen(POSTHEADER);
    strcpy(pcBufHeaders, HOSTHEADER);
    pcBufHeaders += strlen(HOSTHEADER);
    strcpy(pcBufHeaders, CHEADER);
    pcBufHeaders += strlen(CHEADER);
    strcpy(pcBufHeaders, "\r\n\r\n");

    int dataLength = strlen(DATA1) + strlen(DATA2);
    if (state == 0) { dataLength += 9; }
    else if (state == 1) { dataLength += 8; }
    else if (state == 2) { dataLength += 15; }
    else if (state == 3) { dataLength += 14; }

    strcpy(pcBufHeaders, CTHEADER);
    pcBufHeaders += strlen(CTHEADER);
    strcpy(pcBufHeaders, CLHEADER1);

    pcBufHeaders += strlen(CLHEADER1);
    sprintf(cCLLength, "%d", dataLength);

    strcpy(pcBufHeaders, cCLLength);
    pcBufHeaders += strlen(cCLLength);
    strcpy(pcBufHeaders, CLHEADER2);
    pcBufHeaders += strlen(CLHEADER2);

    strcpy(pcBufHeaders, DATA1);
    pcBufHeaders += strlen(DATA1);

    // if state is 0, send "Alarm Off"
    if (state == 0) {
        char textBuffer[10] = "Alarm Off";
        strcpy(pcBufHeaders, textBuffer);
        pcBufHeaders += 9;
    }
    // if state is 1, send "Alarm On"
    else if (state == 1) {
        char textBuffer[9] = "Alarm On";
        strcpy(pcBufHeaders, textBuffer);
        pcBufHeaders += 8;
    }
    // if state is 2, send "Intruder Alert!"
    else if (state == 2) {
        char textBuffer[16] = "Intruder Alert!";
        strcpy(pcBufHeaders, textBuffer);
        pcBufHeaders += 15;
    }
    // if state is 3, send "Password Reset"
    else if (state == 3) {
        char textBuffer[15] = "Password Reset";
        strcpy(pcBufHeaders, textBuffer);
        pcBufHeaders += 14;
    }

    strcpy(pcBufHeaders, DATA2);
    pcBufHeaders += strlen(DATA2);

    int testDataLength = strlen(pcBufHeaders);

    UART_PRINT(acSendBuff);


    //
    // Send the packet to the server */
    //
    lRetVal = sl_Send(iTLSSockID, acSendBuff, strlen(acSendBuff), 0);
    if(lRetVal < 0) {
        UART_PRINT("POST failed. Error Number: %i\n\r",lRetVal);
        sl_Close(iTLSSockID);
        GPIO_IF_LedOn(MCU_RED_LED_GPIO);
        return lRetVal;
    }
    lRetVal = sl_Recv(iTLSSockID, &acRecvbuff[0], sizeof(acRecvbuff), 0);
    if(lRetVal < 0) {
        UART_PRINT("Received failed. Error Number: %i\n\r",lRetVal);
        //sl_Close(iSSLSockID);
        GPIO_IF_LedOn(MCU_RED_LED_GPIO);
           return lRetVal;
    }
    else {
        acRecvbuff[lRetVal+1] = '\0';
        UART_PRINT(acRecvbuff);
        UART_PRINT("\n\r\n\r");
    }

    return 0;
}


static int http_get(int iTLSSockID){
    char acSendBuff[512];
    char acRecvbuff[1460];
    char* pcBufHeaders;
    int lRetVal = 0;

    pcBufHeaders = acSendBuff;
    strcpy(pcBufHeaders, GETHEADER);
    pcBufHeaders += strlen(GETHEADER);
    strcpy(pcBufHeaders, HOSTHEADER);
    pcBufHeaders += strlen(HOSTHEADER);
    strcpy(pcBufHeaders, CHEADER);
    pcBufHeaders += strlen(CHEADER);
    strcpy(pcBufHeaders, "\r\n\r\n");

    int testDataLength = strlen(pcBufHeaders);

    UART_PRINT(acSendBuff);


    //
    // Send the packet to the server */
    //
    lRetVal = sl_Send(iTLSSockID, acSendBuff, strlen(acSendBuff), 0);
    if(lRetVal < 0) {
        UART_PRINT("POST failed. Error Number: %i\n\r",lRetVal);
        sl_Close(iTLSSockID);
        GPIO_IF_LedOn(MCU_RED_LED_GPIO);
        return lRetVal;
    }
    lRetVal = sl_Recv(iTLSSockID, &acRecvbuff[0], sizeof(acRecvbuff), 0);

    if(lRetVal < 0) {
        UART_PRINT("Received failed. Error Number: %i\n\r",lRetVal);
        //sl_Close(iSSLSockID);
        GPIO_IF_LedOn(MCU_RED_LED_GPIO);
           return lRetVal;
    }
    else {
        // print received data
        acRecvbuff[lRetVal+1] = '\0';
        UART_PRINT(acRecvbuff);
        UART_PRINT("\n\r\n\r");
        // check if alarm is on or off
        int i;
        for (i = 0; i < lRetVal - 8; i++) {
            // if alarm from server is on but local alarm is off, set alarm state to 1, input mode to 0, print to true
            if (acRecvbuff[i] == 'A' && acRecvbuff[i+1] == 'l' && acRecvbuff[i+2] == 'a' && acRecvbuff[i+3] == 'r' && acRecvbuff[i+4] == 'm' && acRecvbuff[i+5] == ' ') {
                if (acRecvbuff[i+6] == 'O' && acRecvbuff[i+7] == 'n' && alarmState == 0) {
                    alarmState = 1, inputMode = 0, print = true; break;
                }
                // if alarm from server is off but local alarm is on, set alarm state to 0, input mode to 0, print to true
                else if (acRecvbuff[i+6] == 'O' && acRecvbuff[i+7] == 'f' && alarmState == 1) {
                    alarmState = 0, inputMode = 0, print = true; break;
                }
            }
        }
    }

    return 0;
}
