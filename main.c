#include <msp430.h>
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#ifndef RINGBUFFER_H__
#define RINGBUFFER_H__

/* forward declare the type */
struct ringbuffer;

void rb_init(struct ringbuffer* rb);
int rb_put(struct ringbuffer* rb, char *value);
const char* rb_get(struct ringbuffer* rb);
int rb_empty(const struct ringbuffer* rb);
int rb_full(const struct ringbuffer* rb);


#define E_RBEMPTY   -1
#define E_RBFULL    -1

#define BUFFERSIZE  32
struct ringbuffer {
    size_t head;
    size_t tail;
    size_t count;
    char *container[BUFFERSIZE];
};

#endif  /* RINGBUFFER_H__ */


// helper functions
void UARTSendArray(unsigned char *TxArray, unsigned char ArrayLength);
void Init_UART(void);
void tempInit(void);
void process(void);
void OUTA_UART(unsigned char A);
char* format(int temp);

// variable declaration
static char input[17];
static char output[15];
struct ringbuffer data;
int if_UART = 0, if_TIMER = 0;
static int hour = 0, min = 0, sec = 0;


int main(void)
{
    // variable to hold the temperature
    int temp;

    WDTCTL = WDTPW + WDTHOLD;   // stop WDT

    // initialize UART
    Init_UART();
    // initialize Temperature Sensor
    tempInit();

    // initialize Timer_A
    TACCR0  =   4096;           // set count top -> 4096 -> 1 sec
    TACTL   =   TASSEL_1 |       // clock source selection - use ACLK
                    MC0 |        // up mode - count to TACCR0, then reset
                    ID_3;        // clock divider - divide ACLK by 8
    TACCTL0 = CCIE;              // enable timer interrupt

    // create ring buffer
    rb_init(&data);

    // enter LPM3, interrupts enabled
    while(1)
    {
        // enter low power mode and enable interrupts
        __bis_SR_register(LPM3_bits + GIE);


        if (if_UART == 1)                   // if a command has been entered
        {
          process();
          if_UART = 0;

          OUTA_UART('\r');
          OUTA_UART('\n');
        }
        else if (if_TIMER == 1)             // if it has been five minuntes
        {                                   // take a temp reading and save it
          if(rb_full(&data))
          {
            data.tail++;                    // moves the tail ahead a space to keep the tail and head from meeting
            data.tail %= BUFFERSIZE;
            data.count--;
          }

          // get temperature reading
          ADC10CTL0 |= ENC + ADC10SC;       // enable conversion and start conversion
          while(ADC10CTL1 & BUSY);
          temp = ADC10MEM;                  // get temperature reading
          ADC10CTL0 &= ~ENC;                // disable adc conv

          // save timestamp and temperature to string 'output'
          if (sec < 10 && min > 10 && hour > 10)
              sprintf(output, "%d%d0%d  %d", hour, min, sec, temp);
          else if (sec < 10 && min < 10 && hour > 10)
              sprintf(output, "%d0%d0%d  %d", hour, min, sec, temp);
          else if (sec > 10 && min < 10 && hour < 10)
              sprintf(output, "0%d0%d%d  %d", hour, min, sec, temp);
          else if (sec > 10 && min < 10 && hour > 10)
              sprintf(output, "%d0%d%d  %d", hour, min, sec, temp);
          else if (sec < 10 && min > 10 && hour < 10)
              sprintf(output, "0%d%d0%d  %d", hour, min, sec, temp);
          else if (sec < 10 && min < 10 && hour < 10)
              sprintf(output, "0%d0%d0%d  %d", hour, min, sec, temp);

          //save string to ring buffer
          rb_put(&data, output);

          if_TIMER = 0;
        }
    }
}

/* ISR for receiving commands */
#pragma vector=USCIAB0RX_VECTOR
__interrupt void USCI0RX_ISR(void)
{
    static int index = 0, count = 0;
    static char data;

    data = UCA0RXBUF;
    data = tolower(data);

    if (data != '\r' && count != 16)
    {
          input[index++] = data;
          count++;
    }
    else
    {
        // store carriage return
        input[index] = data;

        // echo string to terminal
        UARTSendArray(&input, index);

        // reset variables
        index = 0;
        count = 0;

        if_UART = 1;

        __bic_SR_register_on_exit(LPM3_bits);
    }

}

/* Timer A Interrupt - Real Time Clock */
#pragma vector=TIMER0_A0_VECTOR
__interrupt void timer0_isr(void)
{
    // increment the second variable
    sec += 1;

    // if else statements deal with incrementing
    // the min and hour variables
    if (sec == 60)
    {
        sec = 0;
        min += 1;
        if (min == 60)
        {
            min = 0;
            hour += 1;
            if (hour == 24 && sec == 1)
            {
                sec = 0;
                min = 0;
                hour = 0;
            }
        }
    }

    // if it has been 5 minutes
    // go to main and take a temp reading
    if (min % 5 == 0 && sec == 0)
    {
      if_TIMER = 1;
       __bic_SR_register_on_exit(LPM3_bits);
    }
}

void process(void)
{
    if (input[0] == 't')
    {
        // show current time
        char time_char[7];

        if (sec < 10 && min > 10 && hour > 10)
            sprintf(time_char, "%d%d0%d", hour, min, sec);
        else if (sec < 10 && min < 10 && hour > 10)
            sprintf(time_char, "%d0%d0%d", hour, min, sec);
        else if (sec > 10 && min < 10 && hour < 10)
            sprintf(time_char, "0%d0%d%d", hour, min, sec);
        else if (sec > 10 && min < 10 && hour > 10)
            sprintf(time_char, "%d0%d%d", hour, min, sec);
        else if (sec < 10 && min > 10 && hour < 10)
            sprintf(time_char, "0%d%d0%d", hour, min, sec);
        else if (sec < 10 && min < 10 && hour < 10)
            sprintf(time_char, "0%d0%d0%d", hour, min, sec);

        UARTSendArray(&time_char, 6);
    }
    else if (input[0] == 's')
    {
        //set current time
        hour = (input[1]-48)*10 + (input[2]-48);
        min = (input[3]-48)*10 + (input[4]-48);
        sec = (input[5]-48)*10 + (input[6]-48);
    }
    else if (input[0] == 'o')
    {
        // show the oldest temp reading and its time stamp
        if (data.count == 0)
            UARTSendArray("No temperatures recorded.", 25);
        else
        {
            // get the oldest reading
            // to_screen = rb_get(&data);
            // send to terminal
            UARTSendArray(rb_get(&data), 15);
        }
    }
    else if (input[0] == 'l')
    {
        if (data.count == 0)
             UARTSendArray("No temperatures recorded.", 25);
         while(!rb_empty(&data))
         {
             UARTSendArray(rb_get(&data), 15);
             OUTA_UART('\n');
         }
    }

    return;
}

void tempInit(void)
{
    ADC10CTL0 = SREF_1 + REFON + ADC10ON + ADC10SHT_3 ; //1.5V ref,Ref on,64 clocks for sample
    ADC10CTL1 = INCH_10+ ADC10DIV_3;                    //temp sensor is at 10 and clock/4
}

void Init_UART(void)
{
    /* Use Calibration values for 1MHz Clock DCO */
    DCOCTL = 0;
    BCSCTL1 = CALBC1_1MHZ;
    DCOCTL = CALDCO_1MHZ;

    /* Configure Pin Muxing P1.1 RXD and P1.2 TXD */
    P1SEL = BIT1 | BIT2 ;
    P1SEL2 = BIT1 | BIT2;

    /* Place UCA0 in Reset to be configured */
    UCA0CTL1 = UCSWRST;

    /* Configure */
    UCA0CTL1 |= UCSSEL_2;       // SMCLK
    UCA0BR0 = 104;              // 1MHz 9600
    UCA0BR1 = 0;                // 1MHz 9600
    UCA0MCTL = UCBRS0;          // modulation UCBRSx = 1

    /* Take UCA0 out of reset */
    UCA0CTL1 &= ~UCSWRST;

    /* Enable USCI_A0 RX interrupt */
    IE2 |= UCA0RXIE;
}

void UARTSendArray(unsigned char *TxArray, unsigned char ArrayLength){

    while(ArrayLength--)
    {                                   // loop until StringLength == 0 and post decrement
        while(!(IFG2 & UCA0TXIFG));     // wait for TX buffer to be ready for new data
        UCA0TXBUF = *TxArray;           // write the character at the location specified py the pointer
        TxArray++;                      // increment the TxString pointer to point to the next character
    }
}

void OUTA_UART(unsigned char A)
{

    do{
    }while ((IFG2&0x02)==0);

    UCA0TXBUF = A;
}

// Ring Buffer Functions
void rb_init(struct ringbuffer* rb) {
    rb->head = 0;
    rb->tail = BUFFERSIZE - 1;
    rb->count = 0;
}

int rb_put(struct ringbuffer* rb, char *value) {
    if(rb_full(rb)) {
        return E_RBFULL;
    }

    rb->count++;
    rb->container[rb->head++] = value;
    rb->head %= BUFFERSIZE;

    return 0;
}

const char* rb_get(struct ringbuffer* rb) {

    rb->count--;
    rb->tail++;
    rb->tail %= BUFFERSIZE;

    return rb->container[rb->tail];
}

int rb_empty(const struct ringbuffer* rb) {
    return (rb->count == 0);
}

int rb_full(const struct ringbuffer* rb) {
    return (rb->count == BUFFERSIZE);
}
