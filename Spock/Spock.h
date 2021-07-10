/* Spock main header file
   It comprise main constants and variables */

#include "sdrplay_api.h"
#include "rs.h"

// Declare SDRplay RSP1A setup variables

int masterInitialised = 0;
int slaveUninitialised = 0;
sdrplay_api_DeviceT *chosenDevice = NULL;
sdrplay_api_DeviceT SDRDevices[4];
sdrplay_api_ErrT err;
sdrplay_api_DeviceParamsT *deviceParams = NULL;
sdrplay_api_CallbackFnsT cbFns;
sdrplay_api_RxChannelParamsT *chParams;
unsigned int NumberOfSDRDevices;



// Declare debugging message buffer
const int msgbufSize = 200;
char msgbuf[msgbufSize];



// Declare thread activity control variables and constants

// Define a radio on boolean, to signal that the radio is active
bool RadioOn = false;

// Declare a timeout for radio thread shutdown
const int RadioResponseTimeout = 20000; // [milliseconds]

// Define a speakers on boolean, to signal that the speakers are used to output the audio
bool SpeakersOn = false;

// Declare a timeout for speakers thread shutdown
const int SpeakersResponseTimeout = 20000; // [milliseconds]

// Define a SDR initialization finished boolean, to signal that the initialization process of the SDR is done
bool SDRFinishedInit = false;

// Define a speakers finished boolean, to signal that the speakers are not used anymore to output the audio
bool SpeakersFinished = false;



// Declare SDR setup booleans variables.

// Define a center frequency update boolean, to signal that the center frequency need to be updated
bool UpdateCenterFrequency = false;

// Define a gain reduction and LNA state update boolean, to signal that the gain reduction and LNA state need to be updated
bool UpdateGainReductionAndLNAState = false;

// Define an SDR update failed boolean, to signal that an SDR configuration update had failed
bool UpdateFailed = false;

// Define an SDR overload detected boolean, to signal that the SDR is in overload
bool OverloadDetected = false;



// Define a system gain vairable
float SystemGain = 0;



// Declare Radio related constants

// Declare the center frequency initial value
//const double CenterFreqeuncyInit = 739.0; // [MHz]
const double CenterFreqeuncyInit = 1565.0; // [MHz]

// Declare the LNA state initial value
const char LNAstateInit = 2;

// Declare the gain reduction initial value
const int GainReductionInit = 20;



// Delcare transmission constants and variables



// Default setup
// NOTE: When selecting a setup, one also need to select the setup at rs.h file
const double MinBitTime = 14; // microseconds
const int CodecCompression = 2; // Bits per sample: 2 to 5 bits
const int PacketsPerSecond = 32;
const int ReservedBytesPerSecond = 16;
const int PacketPerUnsignedCharThreshold = 32 * 7 - 1;

// Best bitrate setup
// NOTE: When selecting a setup, one also need to select the setup at rs.h file
//const double MinBitTime = 8; // microseconds
//const int CodecCompression = 2; // Bits per sample: 2 to 5 bits
//const int PacketsPerSecond = 32;
//const int ReservedBytesPerSecond = 16;
//const int PacketPerUnsignedCharThreshold = 32 * 7 - 1;

// Best audio quality setup
// This setup was tested on the laptop and on the desktop computer while the monitor was OFF (following power saving plan)
// NOTE: When selecting a setup, one also need to select the setup at rs.h file
//const double MinBitTime = 8; // microseconds
//const int CodecCompression = 3; // Bits per sample: 2 to 5 bits
//const int PacketsPerSecond = 48;
//const int ReservedBytesPerSecond = 24;
//const int PacketPerUnsignedCharThreshold = 48 * 5 - 1;



// Tx contants

/* MinBitTime is the minimal part of a symbol.
Transmitted symbol is defined by the length of a transmission.
Symbol length = ZeroOffset + MinBitTime * Data value.
Data value depends on the number of symbols per byte(of number of bits per symbol).
If 2 symbols per byte are selected, then each symbols represents 4 bits(Data value = 0 to 15).
If 4 symbols per byte are selected, then each symbols represents 2 bits(Data value = 0 to 3).*/

// Define BitTime constants. This defines minimal symbol length.
//const float MinBitTime = 22; // microseconds
//const float MinBitTime = 20; // microseconds
//const float MinBitTime = 14; // microseconds
//const float MinBitTime = 10; // microseconds
//const float MinBitTime = 8; // microseconds

// Define zero bit offest.
const int ZeroOffset = 1; // Zero nibble bit time offset

// Define packet length
const int PacketLength = 63 + 1; // In bytes, not including headers and footer
								 // A 63 bytes packet is good for complete voice messages
								 // with G.726 2,3,4 bits compression per symbol
								 // The plus 1 is for the packet counter

// Define number of symbols per data byte
//const int SymbolsPerByte = 4;
const int SymbolsPerByte = 2;

// Use forward error correction flag
bool useFEC = true;

// Define the lenght of a raw packet (not including FEC)
const int RawPacketLength = (PacketLength + 6); // In bytes

// Define the lenght of a raw packet, including FEC
const int FECRawPacketLength = RawPacketLength + nn - kk; // In bytes

// Define packet header bytes
const int TxHeader1 = 0x22;
const int TxHeader2 = 0x3C;
const int TxHeader3 = 0x3C;
const int TxHeader4 = 0x22;

// Defined header bytes parts in pairs (2 bits per symbol)
const int TxHeader1_MSP = ((TxHeader1 >> 6) & 0x03) + ZeroOffset;
const int TxHeader1_SP2 = ((TxHeader1 >> 4) & 0x03) + ZeroOffset;
const int TxHeader1_SP3 = ((TxHeader1 >> 2) & 0x03) + ZeroOffset;
const int TxHeader1_LSP =  (TxHeader1       & 0x03) + ZeroOffset;
const int TxHeader2_MSP = ((TxHeader2 >> 6) & 0x03) + ZeroOffset;
const int TxHeader2_SP2 = ((TxHeader2 >> 4) & 0x03) + ZeroOffset;
const int TxHeader2_SP3 = ((TxHeader2 >> 2) & 0x03) + ZeroOffset;
const int TxHeader2_LSP =  (TxHeader2       & 0x03) + ZeroOffset;
const int TxHeader3_MSP = ((TxHeader3 >> 6) & 0x03) + ZeroOffset;
const int TxHeader3_SP2 = ((TxHeader3 >> 4) & 0x03) + ZeroOffset;
const int TxHeader3_SP3 = ((TxHeader3 >> 2) & 0x03) + ZeroOffset;
const int TxHeader3_LSP =  (TxHeader3       & 0x03) + ZeroOffset;
const int TxHeader4_MSP = ((TxHeader4 >> 6) & 0x03) + ZeroOffset;
const int TxHeader4_SP2 = ((TxHeader4 >> 4) & 0x03) + ZeroOffset;
const int TxHeader4_SP3 = ((TxHeader4 >> 2) & 0x03) + ZeroOffset;
const int TxHeader4_LSP =  (TxHeader4       & 0x03) + ZeroOffset;

// Gather header pairs to an array and calculate its sum
const int TxHeader234PairsArray[] = { TxHeader2_MSP , TxHeader2_SP2 , TxHeader2_SP3 , TxHeader2_LSP ,
									  TxHeader3_MSP , TxHeader3_SP2 , TxHeader3_SP3 , TxHeader3_LSP ,
									  TxHeader4_MSP , TxHeader4_SP2 , TxHeader4_SP3 , TxHeader4_LSP };

const int Header234PairsLenghtSum = TxHeader2_MSP + TxHeader2_SP2 + TxHeader2_SP3 + TxHeader2_LSP + 
									TxHeader3_MSP + TxHeader3_SP2 + TxHeader3_SP3 + TxHeader3_LSP +
									TxHeader4_MSP + TxHeader4_SP2 + TxHeader4_SP3 + TxHeader4_LSP;

// Defined header bytes parts in nibbles (4 bits per symbol)
const int TxHeader1_MSN = ((TxHeader1 >> 4) & 0x0F) + ZeroOffset;
const int TxHeader1_LSN = (TxHeader1 & 0x0F) + ZeroOffset;
const int TxHeader2_MSN = ((TxHeader2 >> 4) & 0x0F) + ZeroOffset;
const int TxHeader2_LSN = (TxHeader2 & 0x0F) + ZeroOffset;
const int TxHeader3_MSN = ((TxHeader3 >> 4) & 0x0F) + ZeroOffset;
const int TxHeader3_LSN = (TxHeader3 & 0x0F) + ZeroOffset;
const int TxHeader4_MSN = ((TxHeader4 >> 4) & 0x0F) + ZeroOffset;
const int TxHeader4_LSN = (TxHeader4 & 0x0F) + ZeroOffset;

// Gather header nibbles to an array and calculate its sum
const int TxHeader234NibblesArray[] = { TxHeader2_MSN , TxHeader2_LSN , TxHeader3_MSN , TxHeader3_LSN , TxHeader4_MSN , TxHeader4_LSN };

const int Header234NibblesLenghtSum =   TxHeader2_MSN + TxHeader2_LSN + TxHeader3_MSN + TxHeader3_LSN + TxHeader4_MSN + TxHeader4_LSN;

// Define a ratio residue variable, to evaluate the match of a symbols stream to the header bytes
//const int HeaderRatioResidueThreshold = 120;
const int HeaderRatioResidueThreshold = 160;



// Define a sampling constant
//const float SamplingFrequency = 2000000.0; // Hz
const float SamplingFrequency = 5000000.0; // Hz



// Define SDR reception decimation use
//const bool DecimationEnable = true; // Note: When setting DecimationEnable = false then set DecimationFactor to 1 !
//const int DecimationFactor = 2;		// Note: When setting DecimationEnable = false then set DecimationFactor to 1 !
const bool DecimationEnable = false; // Note: When setting DecimationEnable = false then set DecimationFactor to 1 !
const int DecimationFactor = 1;		// Note: When setting DecimationEnable = false then set DecimationFactor to 1 !



// Calculate 2nd order butterworth low pass filter coefficiants
const float HighestFrequency = (float)1000000.0 / ((float)2.0 * (float) ZeroOffset * MinBitTime);

//const float SecondButterLPFCutoffFreq = (float)1.5 * HighestFrequency;
//const float SecondButterLPFCutoffFreq = (float)2.0 * HighestFrequency;
//const float SecondButterLPFCutoffFreq = (float)2.5 * HighestFrequency;
const float SecondButterLPFCutoffFreq = (float)3.0 * HighestFrequency;

const float SoftwareSamplingRate = SamplingFrequency / (float)DecimationFactor;

const float SecondButterLPFconst = (float)1.0 / (tan((float)3.14159265358979323846 * SecondButterLPFCutoffFreq / SoftwareSamplingRate));

const float SecondButterLPFb0 = (float)1.0 / ((float)1.0 + sqrt((float)2.0) * SecondButterLPFconst + SecondButterLPFconst * SecondButterLPFconst);

const float SecondButterLPFb1 = (float)2.0 * SecondButterLPFb0;

const float SecondButterLPFb2 = SecondButterLPFb0;

const float SecondButterLPFa1 = (float)2.0 * (SecondButterLPFconst * SecondButterLPFconst - (float)1.0) * SecondButterLPFb0;

const float SecondButterLPFa2 = -((float)1.0 - sqrt((float)2.0) * SecondButterLPFconst + SecondButterLPFconst * SecondButterLPFconst) * SecondButterLPFb0;



// Defined incoming receptions moving average variables

// Defined incoming receptions moving average constants
//const int MovingAverageLength = 5;
//const int MovingAverageLength = 21;
const int MovingAverageLength = 31;

// Define incoming receptions moving average buffer
float MovingAverageBuffer[MovingAverageLength] = { 0 };



// Defined a very slow low pass filter variables
//const float SlowLowPassFilterFrequency = 50; // [Hz]
const float SlowLowPassFilterFrequency = 10; // [Hz]
//const float SlowLowPassFilterFrequency = 1; // [Hz]

const float SlowLPFAlpha = SlowLowPassFilterFrequency / SoftwareSamplingRate;



// Define bit detection thresholds
const float BitDetectionLowThreshold = (float) 0.6; // [0.01%] of peak value

const float BitDetectionHighThreshold = (float) 0.4; // [0.01%] of peak value



// Defined bit ratio moving average variables

// Define bit ratio moving average constant
const int BitRatioMovingAverageLength = 11;

// Define bit ratio moving average buffer
int BitRatioMovingAverageBuffer[BitRatioMovingAverageLength] = { 0 };

// Define bit ratio moving average timeout - the moving average shall be reseted if there are
// no good receptions for a time period longer than the timeout
const int BitRatioResetTime = 3000; // [milliseconds]



// Define variable to manage the symbols length buffer

// Data bits length buffer
const int BitsRLEBufferSize = 10000;
int BitsRLEBuffer[BitsRLEBufferSize];

// Define a current buffer start pointer variable
int BitsRLEBufferStart = 0;

// Define a current buffer end pointer variable
int BitsRLEBufferEnd = 0;

// Define a minimum symbol length threshold, to filter noise
const int MinimumBitLengthThreshold = (int) ((float)0.75 * MinBitTime * SoftwareSamplingRate / 1000000); // [0.01%] of bit time value


// Audio processing variables

// Define audio sampling rate
const float CodecSamplingRate = 8000; // [Hz]

// Define G.726 number of bits per sample
//const int   CodecCompression = 2; // Bits per sample: 2 to 5 bits
//const int   CodecCompression = 3; // Bits per sample: 2 to 5 bits

// Define a reception mask for the audio bits extraction from the payload data bytes
const unsigned int ReceptionMask = (((1 << CodecCompression) - 1) << 16) | 0xFFFF;



// Audio packet per 1 second properties

// For CodecCompression = 2:
//const int PacketsPerSecond = 32;
//const int ReservedBytesPerSecond = 16;
//const int PacketPerUnsignedCharThreshold = 32 * 7 - 1;

// For CodecCompression = 3:
//const int PacketsPerSecond = 48;
//const int ReservedBytesPerSecond = 24;
//const int PacketPerUnsignedCharThreshold = 48 * 5 - 1;

// Define a packet diff threshold. Below the threshold, the PCM samples buffers shall bridge the gap with zeros.
const int PacketDiffThreshold = 14;

// Define a silence timeout, at which the software shall close the present buffer and wait for more data
const int SilenseTimeout = 1500; // 1.5 seconds in milliseconds

// Define the number of audio channels to be used for playback
const int AudioChannels = 2; // 1 = Mono, 2 = Stereo
