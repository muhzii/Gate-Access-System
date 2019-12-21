#include "ps2_keyboard.h"

const char hex_map[16] = { '0', '1', '2', '3',
                        '4', '5', '6', '7',
                        '8', '9', 'A', 'B',
                        'C', 'D', 'E', 'F' };

char char_map[0xFF] = {};

void initCharMap(void) {
    // TODO: Complete the rest of mapping list
    char_map[0x1C] = 'A';
    char_map[0x1B] = 'S';
    char_map[0x23] = 'D';
    char_map[0x2B] = 'F';
    char_map[0x34] = 'G';
    char_map[0x33] = 'H';
    char_map[0x1A] = 'Z';
    char_map[0x22] = 'X';
    char_map[0x21] = 'C';
    char_map[0x2A] = 'V';
    char_map[0x32] = 'B';
    char_map[0x31] = 'N';
    char_map[0x66] = 0x66;
    char_map[0x5A] = '\n';
    char_map[0x29] = ' ';
}

char mapChar(char c) {
    return char_map[c];
}

char queue[BUFFER_SIZE];
char q_start = 0, q_end = 0, q_size = 0;

unsigned long curData = 0;
unsigned long curDataLen = 0;
int isUpEvent = 0;

void KEYBOARD_init(void) {
    SYSCTL_RCGCGPIO_R |= SYSCTL_RCGCGPIO_R5; // (a) activate clock for port F
    while (!(SYSCTL_PRGPIO_R&SYSCTL_PRGPIO_R5));

    GPIO_PORTF_LOCK_R = GPIO_LOCK_KEY;
    GPIO_PORTF_CR_R = 0x1F;
    GPIO_PORTF_DIR_R &= ~(CLK_PIN | DATA_PIN);
    GPIO_PORTF_AFSEL_R &= ~(CLK_PIN | DATA_PIN);  //     disable alt funct on Clk and Data
    GPIO_PORTF_DEN_R |= (CLK_PIN | DATA_PIN);     //     enable digital I/O on Clk and Data
    GPIO_PORTF_PCTL_R &= ~(CLK_PIN | CLK_PIN << 1 | CLK_PIN << 2 || CLK_PIN << 3); // configure Clk as GPIO
    GPIO_PORTF_PCTL_R &= ~(DATA_PIN | DATA_PIN << 1 | DATA_PIN << 2 || DATA_PIN << 3); // configure Data as GPIO
    GPIO_PORTF_AMSEL_R = 0;       //     disable analog functionality on PF
    GPIO_PORTF_PUR_R |= (CLK_PIN | DATA_PIN);     //     enable weak pull-up on Clk and Data

    GPIO_PORTF_IS_R &= ~CLK_PIN;     // Clk is edge-sensitive
    GPIO_PORTF_IBE_R &= ~CLK_PIN;    //     Clk is not both edges
    GPIO_PORTF_IEV_R &= ~CLK_PIN;    //     Clk falling edge event
    GPIO_PORTF_ICR_R = CLK_PIN;      // (e) clear flag of Clk interrupt
    GPIO_PORTF_IM_R |= CLK_PIN;      // (f) arm interrupt on Clk
    NVIC_PRI7_R = (NVIC_PRI7_R&0xFF00FFFF)|0x00A00000; // priority 5
    NVIC_EN0_R = 0x40000000;      // enable interrupt 30 in NVIC
}

void enqueue(char c) {
    if (q_size == BUFFER_SIZE)
        return;
    queue[q_end] = c;
    q_end = (q_end+1)%BUFFER_SIZE;
    ++q_size;
}

char dequeue() {
    if (q_size == 0)
        return 0;
    char value = queue[q_start];
    q_start = (q_start + 1)%BUFFER_SIZE;
    --q_size;
    return value;
}

int KEYBOARD_availableChars(void) {
    return q_size;
}

char KEYBOARD_readChar(void) {
    return dequeue();
}

int checkParity(unsigned long num) {
    unsigned int count = 0, i;

    for(i = 1; i < 10; ++i){
        if( num & (1U << i) ){count++;}
    }

    return (count%2);
}

void GPIOPortF_Handler(void) {
    GPIO_PORTF_ICR_R = CLK_PIN;      // acknowledge Clk

    unsigned char curBit = (DATA_DATA&DATA_PIN);
    if (curBit)
        curData += (1U << curDataLen);
    ++curDataLen;

    // Each data frame consists of 11 bits
    // First bit is start bit and always 0
    // The next 8 bits are data sent by keyboard
    // The 10th bit is parity bit (Odd Parity)
    // The last bit is stop bit and always 1
    // To see the shape of data frame, open the following url
    // http://www.burtonsys.com/ps2_chapweske_files/waveform1.jpg
    if (curDataLen == 11U) {
        // Check start, parity and stop bits
        if (!(curData & 0x1) && // Ensures first bit is 0
            (curData & 0x400) && // Ensures last (11th) bit is 1
            checkParity(curData)) { // Checks parity of data

            char actualData = (char)((curData & 0x1FE) >> 1);
            // When keyboard sends 0xF0 it means a specific key is released (user stopped clicking it)
            // The next data sent after 0xF0 is the key of key that released
            // For example when user clicks "A"
            // Keyboard sends 0x1C
            // And when user releases "A"
            // The keyboard sends 0xF0 then 0x1C
            if (actualData == 0xF0)
                isUpEvent = 1;
            else if (isUpEvent) {
                isUpEvent = 0;
            } else {
                char c = mapChar(actualData);
                if (c)
                    enqueue(c);
                else {
                    enqueue(hex_map[actualData/16]);
                    enqueue(hex_map[actualData%16]);
                }
            }
        }

        curData = 0;
        curDataLen = 0;
    }
}
