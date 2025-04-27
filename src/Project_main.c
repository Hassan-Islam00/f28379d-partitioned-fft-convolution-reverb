/*

Filename:            Project_main.c

Description:         This program implements a dedicated Reverb Audio Processor

Version:             1.0

Target:              TMS320F28379D

Author:              Hassan Islam and Federico Sarabia

Date:                04Dec2024

Note:                Both members worked on the code unless specified by comments
                     "Done by HS" For Sections done by Hassan Islam,
                     "Done by FS" For Sections done by Federico Sarabia
*/
// defines
#define CFFT_STAGES     10 // Specify the number of FFT stages
#define CFFT_SIZE       (1 << CFFT_STAGES) // FFT size of 1024

#define xdc__strict                  // suppress typedef warnings
#define BUFF_SIZE CFFT_SIZE * 2      // Buffer size is twice the CFFT_SIZE to account for imaginary values
#define PING_PONG_SIZE BUFF_SIZE * 2 // For ping pong operation buffer of 2x BUFF_SIZE is required

#define FIFO_DEPTH 2

//includes
#include <xdc/std.h>
#include <xdc/runtime/System.h>
#include <ti/sysbios/BIOS.h>
#include <ti/sysbios/knl/Swi.h>
#include <ti/sysbios/knl/Task.h>
#include "driverlib.h"
#include "device.h"
#include "fpu_cfft.h"
#include "math.h"
#include "examples_setup.h"
#include <Headers/F2837xD_device.h>
#include <string.h>

// SYSBIOS swi handles
extern const Swi_Handle swi0;
extern const Swi_Handle swi1;

extern const Task_Handle Tsk0;

// ADC data in/out Buffer
#pragma DATA_SECTION(CFFTin1Buff, "CFFTdata1")
#pragma DATA_SECTION(CFFTin1Buff2, "CFFTdata_1_2")

// both arrays are contiguous in memory
// a call to  CFFTin1Buff[CFFT_SIZE*2] = CFFTin1Buff2[0]
float CFFTin1Buff[CFFT_SIZE*2];
float CFFTin1Buff2[CFFT_SIZE*2];

// FFT Calculation Buffer
#pragma DATA_SECTION(CFFToutBuff, "CFFTdata2")

float CFFToutBuff[CFFT_SIZE*2];

// Twiddle Factors
#pragma DATA_SECTION(CFFTF32Coef, "CFFTdata3")

float CFFTF32Coef[CFFT_SIZE];

// Object of the structure CFFT_F32_STRUCT
CFFT_F32_STRUCT cfft;

// Handle to the CFFT_F32_STRUCT object
CFFT_F32_STRUCT_Handle hnd_cfft = &cfft;

// CFFTin1Buff and CFFTin1Buff2 indexes
Uint16 in_wr_index, in_rd_index, out_rd_index;

// Overlap-add buffer
float Saved_Vals[CFFT_SIZE * 3];

Uint16 Saved_Vals_wr_index, Saved_Vals_rd_index;

// SPI FIFO Receive data buffer
Uint16 rdata[FIFO_DEPTH];

//function prototypes:
extern void DeviceInit(void);
void initializeBuffers(void);

//declare global variables:
volatile Bool isrFlag = FALSE; //flag used by idle function
volatile UInt tickCount = 0; //counter incremented by timer interrupt

//new global variables
volatile Bool isrFlag2 = FALSE; //flag used by idle function 2, FS
volatile UInt time = 0; //Time in seconds, FS

#include <IR_FFT.h>


#pragma DATA_ALIGN(irFFTArray, 2);
// pointers to the reverb frequency responses
const float* irFFTArray[12] = {
    &IR_FFT_1[0],
    &IR_FFT_2[0],
    &IR_FFT_3[0],
    &IR_FFT_4[0],
    &IR_FFT_5[0],
    &IR_FFT_6[0],
    &IR_FFT_7[0],
    &IR_FFT_8[0],
    &IR_FFT_9[0],
    &IR_FFT_10[0],
    &IR_FFT_11[0],
    &IR_FFT_12[0],
};

#pragma DATA_ALIGN(sampleFFTArray, 2);
// pointers to the FFT results of the previous 12 ADC input block samples
float* sampleFFTArray[12] = {
    &Sample_FFT_1[0],
    &Sample_FFT_2[0],
    &Sample_FFT_3[0],
    &Sample_FFT_4[0],
    &Sample_FFT_5[0],
    &Sample_FFT_6[0],
    &Sample_FFT_7[0],
    &Sample_FFT_8[0],
    &Sample_FFT_9[0],
    &Sample_FFT_10[0],
    &Sample_FFT_11[0],
    &Sample_FFT_12[0],
};

int16 FFTArraySize = sizeof(sampleFFTArray)/sizeof(sampleFFTArray[0]);
int16 currentFFT = 0;
int16 prevFFT = 0;


/* ======== main ======== */
Int main()
{
    //initialization:
    DeviceInit(); //initialize processor

    initializeBuffers();

    hnd_cfft->Stages  = CFFT_STAGES;      // FFT stages
    hnd_cfft->FFTSize = CFFT_SIZE;        // FFT size
    hnd_cfft->CoefPtr = CFFTF32Coef;      // Twiddle factor table
    CFFT_f32_sincostable(hnd_cfft);       // Calculate twiddle factor

    //jump to RTOS (does not return):
    BIOS_start();
    return(0);
}

/* ======== hwi_adc_read ======== */
Void hwi_adc_read(Void) // Uses Hwi0 Handle
{

    CFFTin1Buff[in_wr_index] = (float)AdcaResultRegs.ADCRESULT0; // ADC result is stored in buffer
    CFFTin1Buff[in_wr_index + 1] = 0;                            // odd indexes represnt imaginary values and thus are zeroed to represent real signals
    AdcaRegs.ADCINTFLGCLR.bit.ADCINT1 = 1;   // clear interrupt flag

    in_wr_index += 2;  // double increment

    Swi_post(swi0);    // dac is posted when HWI interruption occurs to ensure continuous data output

    if( !(in_wr_index & (CFFT_SIZE - 1)) ){ // mod(pointer_offset, buffer_size)
        Swi_post(swi1);                       // ADC processing is posted after CFFT_SIZE values are sampled
        in_wr_index += CFFT_SIZE;             // increment by CFFT_SIZE to avoid writing buffer region where zero-padded values or current overlap add results are stored
    }

    in_wr_index = in_wr_index & (PING_PONG_SIZE - 1); // reset pointer if index exceeds buffer size

}


/* ======== swi_adc_proc ======== */
Void swi_adc_proc(Void)
{

        Uint16 i, j; // for loop index

        Uint16 ping_pong_offset = BUFF_SIZE - (in_wr_index & (BUFF_SIZE));    // determines which half of buffer to process
        in_rd_index = ping_pong_offset;     // initializing position of read handler

        memset(&CFFTin1Buff[ping_pong_offset + CFFT_SIZE], 0, CFFT_SIZE * sizeof(float));

        hnd_cfft->InPtr   = &CFFTin1Buff[in_rd_index];   // buffer for 0th/even stage of FFT
        hnd_cfft->OutPtr  = CFFToutBuff;                 // buffer for 1st/odd stage of FFT

        CFFT_f32(hnd_cfft);     // Calculate FFT

        i = 0; // index to iterate through impulse response FFTs

        prevFFT = currentFFT;

        // storing current FFT result in 12-level FFT circular buffer
        memcpy(sampleFFTArray[currentFFT], hnd_cfft->CurrentInPtr, (CFFT_SIZE * 2) * sizeof(float));



        do {    // Uniform Partition Frequency convolution algorithm
                // Unrolling Done by HS
                #pragma UNROLL(4) // Ensure the loop is unrolled for better performance
                #pragma MUST_ITERATE(,,4) // Enforce loop trip count to be a multiple of 4

                for (j = 0; j < BUFF_SIZE; j += 4) {
                    float X1 = sampleFFTArray[prevFFT][j];
                    float Y1 = sampleFFTArray[prevFFT][j + 1];
                    float H1 = irFFTArray[FFTArraySize - 1 - i][j];
                    float G1 = irFFTArray[FFTArraySize - 1 - i][j + 1];

                    float X2 = sampleFFTArray[prevFFT][j + 2];
                    float Y2 = sampleFFTArray[prevFFT][j + 3];
                    float H2 = irFFTArray[FFTArraySize - 1 - i][j + 2];
                    float G2 = irFFTArray[FFTArraySize - 1 - i][j + 3];

                    hnd_cfft->CurrentInPtr[j]     += X1 * H1 - Y1 * G1;
                    hnd_cfft->CurrentInPtr[j + 1] += X1 * G1 + Y1 * H1;

                    hnd_cfft->CurrentInPtr[j + 2] += X2 * H2 - Y2 * G2;
                    hnd_cfft->CurrentInPtr[j + 3] += X2 * G2 + Y2 * H2;
                }
                // End of Unrolling Done by HS
           i++;
           prevFFT--;
           if( prevFFT < 0 ) {
               prevFFT = FFTArraySize - 1; // circular buffer wrap around
           }

        } while(prevFFT != currentFFT); // processes until entire circular buffer has traversed


        currentFFT++;
        if(currentFFT > 11 ) {
            currentFFT = 0;  // circular buffer wrap around
        }

        hnd_cfft->InPtr  = hnd_cfft->CurrentInPtr;
        hnd_cfft->OutPtr = &CFFTin1Buff[in_rd_index];

        ICFFT_f32(hnd_cfft);  // Calculate the ICFFT

        // scale large values resulting from elementwise multiplication and summation of the uniform partition frequency convolution
        float scale = 1.0f / 50.0f;
        for (i = 0; i < CFFT_SIZE * 2; i++) {
            hnd_cfft->CurrentInPtr[i] *= scale ;
        }

        // circular buffer wrap around
        if(Saved_Vals_wr_index >= CFFT_SIZE*3){
            Saved_Vals_wr_index = 0;
        }

        // store most recent overlap values at Saved_Vals_wr_index
        memcpy(&Saved_Vals[Saved_Vals_wr_index], &hnd_cfft->CurrentInPtr[CFFT_SIZE], (CFFT_SIZE) * sizeof(float));


        Saved_Vals_wr_index += CFFT_SIZE; // increment by CFFT_SIZE to new write address of circular buffer

}


/* ======== swi_dac_out ======== */
Void swi_dac_out(Void)
{

    GpioDataRegs.GPATOGGLE.bit.GPIO26 = 1; //initialize output value to "1"

    if( !(out_rd_index & (CFFT_SIZE- 1)) ){ // mod(pointer_offset, buffer_size)
         out_rd_index += CFFT_SIZE;         // increment by CFFT_SIZE to avoid writing of zero-padded and overlap result sections of Buffer
    }

    out_rd_index = out_rd_index & (PING_PONG_SIZE - 1); // reset pointer if index exceeds buffer size

    // circular buffer wrap around
    if(Saved_Vals_rd_index >= CFFT_SIZE*3)
        Saved_Vals_rd_index = 0;
	
    DacaRegs.DACVALS.all = (CFFTin1Buff[out_rd_index]) + Saved_Vals[Saved_Vals_rd_index] + 2048;

    // increment pointer by 2 to avoid writing imaginary values
    Saved_Vals_rd_index += 2;
    out_rd_index += 2;

    GpioDataRegs.GPATOGGLE.bit.GPIO26 = 1; //initialize output value to "1"

}



/* ======== myIdleFxn ======== */
//Idle function that is called repeatedly from RTOS
Void LEDBlink(Void)
{

    DEVICE_DELAY_US(500000);
    GpioDataRegs.GPATOGGLE.bit.GPIO31 = 1;

}



void initializeBuffers(void){

     Uint16 i; // for loop index

     for(i = 0; i < BUFF_SIZE; i++){
        Saved_Vals[i] = 0;
        CFFToutBuff[i] = 0.0f;
     }

     for(i = 0; i < PING_PONG_SIZE; i=i+2){
         CFFTin1Buff[i] = 0.0f;
     }

     for(i = 0; i < FIFO_DEPTH ; i++){
           rdata[i] = 0;
     }

     // initializing indexes
     in_wr_index  = 0;
     in_rd_index  = 0;
     out_rd_index = 2;

     Saved_Vals_wr_index = 0;
     Saved_Vals_rd_index = 2;

}


