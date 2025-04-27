/*

Filename:            Project_DevInit.c

Description:         This code is the device initiation code for Project_main.c

Version:             1.0

Target:              TMS320F28379D

Author:              Hassan Islam and Federico Sarabia

Date:                04Dec2024

Note:                Both members worked on the code unless specified by comments
                     "Done by HS" For Sections done by Hassan Islam,
                     "Done by FS" For Sections done by Federico Sarabia
*/

#include <Headers/F2837xD_device.h>
#include "driverlib.h"
#include "device.h"
#include "F28x_Project.h"

//void spi_fifo_init();


void DeviceInit(void)
{

    //InitSysCtrl();

    //InitSpiaGpio();

    //spi_fifo_init();   // Initialize the SPI only


    EALLOW; //allow access to protected registers
    CpuSysRegs.PCLKCR13.bit.ADC_A = 1; // Enable ADC-A clock

    // Configure ADC-A and DAC Done by FS
    AdcaRegs.ADCCTL2.bit.PRESCALE = 0x6; // Set prescaler for ADC clock
    AdcaRegs.ADCCTL1.bit.ADCPWDNZ = 1;   // Power up the ADC module

    CpuSysRegs.PCLKCR16.bit.DAC_A = 1; // Enable clock for DAC-A

    DacaRegs.DACCTL.bit.DACREFSEL = 1; // Internal or External Ref Voltage
    DacaRegs.DACCTL.bit.LOADMODE = 0;   // Immediate load mode (updates immediately on write)
    DacaRegs.DACOUTEN.bit.DACOUTEN = 1; // Enable DAC-A output

    // Wait for power-up stabilization
    DelayUs(1000);

    AdcaRegs.ADCSOC0CTL.bit.TRIGSEL = 2; //trigger source = CPU1 Timer 1
    AdcaRegs.ADCSOC0CTL.bit.CHSEL = 1; //set SOC0 to sample A0
    AdcaRegs.ADCSOC0CTL.bit.ACQPS = 20; //set SOC0 window to 21 SYSCLK cycles
    AdcaRegs.ADCINTSEL1N2.bit.INT1SEL = 0; //connect interrupt ADCINT1 to EOC0
    AdcaRegs.ADCINTSEL1N2.bit.INT1E = 1; //enable interrupt ADCINT1

    // End of ADC-A and DAC config Done by FS
    //configure D10 (blue LED)
    GpioCtrlRegs.GPAMUX2.bit.GPIO31 = 0; //set pin as gpio
    GpioCtrlRegs.GPADIR.bit.GPIO31 = 1; //set gpio as output
    GpioDataRegs.GPASET.bit.GPIO31 = 1; //initialize output value to "1"

    // test gpio
    GpioCtrlRegs.GPAMUX2.bit.GPIO26 = 0; //set pin as gpio
    GpioCtrlRegs.GPADIR.bit.GPIO26 = 1; //set gpio as output
    GpioDataRegs.GPASET.bit.GPIO26 = 1; //initialize output value to "1"

    EDIS; //disallow access to protected registers

}



void spi_fifo_init()
{
    //
    // Initialize SPI FIFO registers
    //
    SpiaRegs.SPIFFTX.all = 0xC022;    // Enable FIFOs, set TX FIFO level to 2
    SpiaRegs.SPIFFRX.all = 0x0022;    // Set RX FIFO level to 2
    SpiaRegs.SPIFFCT.all = 0x00;

    SpiaRegs.SPIFFTX.bit.TXFIFO=1;
    SpiaRegs.SPIFFRX.bit.RXFIFORESET=1;

    //
    // Initialize core SPI registers
    //
    InitSpi();
}
