
#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#include <inc/hw_memmap.h>
#include <driverlib/gpio.h>
#include <driverlib/sysctl.h>
#include <driverlib/timer.h>
#include <driverlib/adc.h>
#pragma once
#define neither 0
#define acc 1
#define brk 2
#define digital_max 4095

volatile uint8_t disNum = 0;
volatile uint8_t disDiget = 0;
volatile bool disUpdate = false;

uint8_t controlConv(void) {
    double base = 2;
    double power = (double)disDiget;
    return (uint8_t)pow(base, power);
}

uint8_t displayConv(void) {
    uint8_t result = disNum;
    switch(disDiget) {
        case 1:
            disUpdate = true;
            return result % 10;
        case 2:
            result /= 10;
            return result % 10;
        case 3:
            result /= 100;
            return result % 10;
        default:
            return 0;
    }
}

void displayIntH(void) {
    if(disNum <= 99 && disDiget == 3) {
        disDiget %= 3;
        disDiget++;
    } else if(disNum <= 9 && disDiget == 2) {
        disDiget %= 3;
        disDiget += 2;
        disDiget %= 3;
    }

    uint8_t control = controlConv();
    GPIOPinWrite(GPIO_PORTE_BASE, GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3, control);

    uint8_t display = displayConv();
    GPIOPinWrite(GPIO_PORTD_BASE, GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3, display);

    disDiget %= 3;
    disDiget++;

    TimerIntClear(TIMER1_BASE, TIMER_TIMA_TIMEOUT);
}

#define mph 0
#define kmph 1
#define set 2

extern void displayIntH(void);

uint8_t currDisM = 0;
uint8_t brkMode = 0;
volatile uint32_t speed_off_adc = 0;
volatile uint32_t set_off_adc = 0;
bool speedADC = false;
bool setADC = false;
int32_t currSpeed = 0;
int32_t currSet = 0;
bool breaks = false;
bool acceleration = false;

void dbIntH(void) {
    TimerIntDisable(TIMER0_BASE, TIMER_TIMA_TIMEOUT);
    TimerDisable(TIMER0_BASE, TIMER_BOTH);
    TimerIntClear(TIMER0_BASE, TIMER_TIMA_TIMEOUT);

    if(GPIOPinRead(GPIO_PORTF_BASE, GPIO_PIN_4) == 0) {
        currDisM++;
        currDisM %= 3;
    }

    TimerDisable(TIMER2_BASE, TIMER_BOTH);
    TimerLoadSet(TIMER2_BASE, TIMER_A, 0x4C4B400);
    TimerIntEnable(TIMER2_BASE, TIMER_TIMA_TIMEOUT);
    TimerEnable(TIMER2_BASE, TIMER_BOTH);
    GPIOIntEnable(GPIO_PORTF_BASE, GPIO_PIN_4);
}

void t2IntH(void) {
    TimerIntDisable(TIMER2_BASE, TIMER_TIMA_TIMEOUT);
    TimerDisable(TIMER2_BASE, TIMER_BOTH);
    TimerIntClear(TIMER2_BASE, TIMER_TIMA_TIMEOUT);
    currDisM = 0;
}

void switchIH(void) {
    GPIOIntDisable(GPIO_PORTF_BASE, GPIO_PIN_4);
    GPIOIntClear(GPIO_PORTF_BASE, GPIO_PIN_4);
    TimerDisable(TIMER0_BASE, TIMER_BOTH);
    TimerLoadSet(TIMER0_BASE, TIMER_A, 0x27100);
    TimerIntEnable(TIMER0_BASE, TIMER_TIMA_TIMEOUT);
    TimerEnable(TIMER0_BASE, TIMER_BOTH);
}

void adc0IntH(void) {
    ADCIntDisable(ADC0_BASE, 3);
    ADCIntClear(ADC0_BASE, 3);
    ADCSequenceDataGet(ADC0_BASE, 3, &speed_off_adc);
    speedADC = true;
    ADCProcessorTrigger(ADC0_BASE, 3);
    ADCIntEnable(ADC0_BASE, 3);
}

void adc1IntH(void) {
    ADCIntDisable(ADC1_BASE, 3);
    ADCIntClear(ADC1_BASE, 3);
    ADCSequenceDataGet(ADC1_BASE, 3, &set_off_adc);
    setADC = true;
    ADCProcessorTrigger(ADC1_BASE, 3);
    ADCIntEnable(ADC1_BASE, 3);
}

void initializeADC(void) {
    SysCtlPeripheralEnable(SYSCTL_PERIPH_ADC0);
    while(!SysCtlPeripheralReady(SYSCTL_PERIPH_ADC0)) {
    }
	
    GPIOPinTypeADC(GPIO_PORTB_BASE, GPIO_PIN_5);
    ADCClockConfigSet(ADC0_BASE, ADC_CLOCK_SRC_PIOSC | ADC_CLOCK_RATE_FULL, 1);
    ADCHardwareOversampleConfigure(ADC0_BASE, 64);
    ADCSequenceConfigure(ADC0_BASE, 3, ADC_TRIGGER_PROCESSOR, 0);
    __asm("ADCCTL_0:      .field      0x40038038  ; pg 850 \n"
          "; SET DITHER BIT FOR 0 BASE                     \n"
          "             LDR         R10, ADCCTL_0          \n"
          "             LDR         R11, [R10]             \n"
          "             ORR         R11, #0x40     ; bit 6 \n"
          "             STR         R11, [R10]             \n"
          );

    ADCSequenceStepConfigure(ADC0_BASE, 3, 0, ADC_CTL_IE  | ADC_CTL_END | ADC_CTL_CH11);
    ADCSequenceEnable(ADC0_BASE, 3);
    ADCIntRegister(ADC0_BASE, 3, adc0IntH);
    ADCIntEnable(ADC0_BASE, 3);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_ADC1);
	
    while(!SysCtlPeripheralReady(SYSCTL_PERIPH_ADC1)) {
    }
	
    GPIOPinTypeADC(GPIO_PORTE_BASE, GPIO_PIN_5);
    ADCHardwareOversampleConfigure(ADC1_BASE, 64);
    __asm("ADCCTL_1:      .field      0x40039038  ; pg 850 \n"
          "; SET DITHER BIT FOR 1 BASE                     \n"
          "             LDR         R10, ADCCTL_1          \n"
          "             LDR         R11, [R10]             \n"
          "             ORR         R11, #0x40     ; bit 6 \n"
          "             STR         R11, [R10]             \n"
          );

    ADCSequenceStepConfigure(ADC1_BASE, 3, 0, ADC_CTL_IE | ADC_CTL_END | ADC_CTL_CH8);
    ADCSequenceEnable(ADC1_BASE, 3);
    ADCIntRegister(ADC1_BASE, 3, adc1IntH);
    ADCIntEnable(ADC1_BASE, 3);
}

void initializeGPIO(void) {
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOD);
    while(!SysCtlPeripheralReady(SYSCTL_PERIPH_GPIOD)) {
    }
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOE);
    while(!SysCtlPeripheralReady(SYSCTL_PERIPH_GPIOE)) {
    }
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOB);
    while(!SysCtlPeripheralReady(SYSCTL_PERIPH_GPIOB)) {
    }
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOF);
    while(!SysCtlPeripheralReady(SYSCTL_PERIPH_GPIOF)) {
    }

    GPIOPinTypeGPIOOutput(GPIO_PORTD_BASE, GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3);
    GPIOPinTypeGPIOOutput(GPIO_PORTE_BASE, GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3);
    GPIOPinTypeGPIOOutput(GPIO_PORTB_BASE, GPIO_PIN_0);
    GPIOPinTypeGPIOInput(GPIO_PORTF_BASE, GPIO_PIN_4);
    GPIOPinTypeGPIOOutput(GPIO_PORTF_BASE, GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3);
    GPIOPadConfigSet(GPIO_PORTD_BASE, GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3, GPIO_STRENGTH_2MA, GPIO_PIN_TYPE_STD);
    GPIOPadConfigSet(GPIO_PORTE_BASE, GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3, GPIO_STRENGTH_2MA, GPIO_PIN_TYPE_STD);
    GPIOPadConfigSet(GPIO_PORTB_BASE, GPIO_PIN_0, GPIO_STRENGTH_4MA, GPIO_PIN_TYPE_STD);
    GPIOPadConfigSet(GPIO_PORTF_BASE, GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3, GPIO_STRENGTH_2MA, GPIO_PIN_TYPE_STD);
    GPIOPadConfigSet(GPIO_PORTF_BASE, GPIO_PIN_4, GPIO_STRENGTH_2MA, GPIO_PIN_TYPE_STD_WPU);
    SysCtlClockSet(SYSCTL_USE_OSC | SYSCTL_OSC_INT);

    GPIOIntTypeSet(GPIO_PORTF_BASE, GPIO_PIN_4, GPIO_FALLING_EDGE);
    GPIOIntEnable(GPIO_PORTF_BASE, GPIO_INT_PIN_4);
    GPIOIntRegister(GPIO_PORTF_BASE, switchIH);
}

void initializeT0(void) {
    SysCtlPeripheralEnable(SYSCTL_PERIPH_TIMER0);
    while(!SysCtlPeripheralReady(SYSCTL_PERIPH_TIMER0)) {
    }

    TimerClockSourceSet(TIMER0_BASE, TIMER_CLOCK_PIOSC);
    TimerDisable(TIMER0_BASE, TIMER_BOTH);
    TimerConfigure(TIMER0_BASE, TIMER_CFG_PERIODIC);
    TimerLoadSet(TIMER0_BASE, TIMER_A, 0x27100);
    TimerIntEnable(TIMER0_BASE, TIMER_TIMA_TIMEOUT);

    TimerIntRegister(TIMER0_BASE, TIMER_A, dbIntH);
}

void initializeT1(void) {
    SysCtlPeripheralEnable(SYSCTL_PERIPH_TIMER1);
    while(!SysCtlPeripheralReady(SYSCTL_PERIPH_TIMER1)) {
    }

    TimerClockSourceSet(TIMER1_BASE, TIMER_CLOCK_PIOSC);
    TimerDisable(TIMER1_BASE, TIMER_BOTH);
    TimerConfigure(TIMER1_BASE, TIMER_CFG_PERIODIC);
    TimerLoadSet(TIMER1_BASE, TIMER_A, 0x15B38);
    TimerIntEnable(TIMER1_BASE, TIMER_TIMA_TIMEOUT);
    TimerIntRegister(TIMER1_BASE, TIMER_A, displayIntH);
}

int initializeT2(void) {
    SysCtlPeripheralEnable(SYSCTL_PERIPH_TIMER2);
    while(!SysCtlPeripheralReady(SYSCTL_PERIPH_TIMER2)) {
    }

    TimerClockSourceSet(TIMER2_BASE, TIMER_CLOCK_PIOSC);
    TimerDisable(TIMER2_BASE, TIMER_BOTH);
    TimerConfigure(TIMER2_BASE, TIMER_CFG_PERIODIC);
    TimerLoadSet(TIMER2_BASE, TIMER_A, 0x4C4B400);
    TimerIntEnable(TIMER2_BASE, TIMER_TIMA_TIMEOUT);
    TimerIntRegister(TIMER2_BASE, TIMER_A, t2IntH);
    return 0;
}

uint8_t currState(void) {
    int difference = currSpeed - currSet;
    switch(brkMode) {
        case neither:
            if(!breaks && difference > 3) {
                return 2;
            }

            if(!acceleration && difference < -3) {
                return 1;
            }
            break;

        case acc:
            if(difference >= 0) {
                return 0;
            }
            break;

        case brk:
            if(difference <= 0) {
                return 0;
            }
            break;

        default:
            return brkMode;
    }
    return brkMode;
}

int main(void) {
	
	//call intilization functions and enable timers
    initializeGPIO();
    initializeT0();
    initializeT1();
    initializeT2();
    initializeADC();
    TimerEnable(TIMER1_BASE, TIMER_BOTH);
    ADCProcessorTrigger(ADC0_BASE, 3);
    ADCProcessorTrigger(ADC1_BASE, 3);

    while(1) {
        if(speedADC && disUpdate) {
            currSpeed =  100 * speed_off_adc / digital_max;
            speedADC = false;
            disUpdate = false;
        }

        if(setADC && disUpdate) {
            currSet = 60 * set_off_adc / digital_max + 20;
            speedADC = false;
            disUpdate = false;
        }

        if (currDisM == mph) {
            GPIOPinWrite(GPIO_PORTB_BASE, GPIO_PIN_0, 0x00);
            disNum = currSpeed;
        }
        else if (currDisM == kmph) {
            GPIOPinWrite(GPIO_PORTB_BASE, GPIO_PIN_0, 0x00);
            disNum = currSpeed * 1.6093;
        }
        else if (currDisM == set) {
            GPIOPinWrite(GPIO_PORTB_BASE, GPIO_PIN_0, 0x01);
            disNum = currSet;
        }
        else {
            return -1;
        }

        brkMode = currState();

        if (brkMode == neither) {
            GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3, 0x04);
        }
        else if (brkMode == brk) {
            GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3, 0x02);
        }
        else if (brkMode == acc) {
            GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3, 0x08);
        }
        else {
            return -1;
        }
    }
}
