// Spock form code

#include <string>
#include <chrono>
#include "g72x.h"
#include "Spock.h"
#include <fstream>
#include <iostream>
#include "rs.h"

#include <Windows.h>
#pragma comment(lib, "winmm.lib")

// Declare FEC global variables
extern int recd[];
extern int index_of[];

using namespace std;
using namespace std::chrono;
using namespace System;
using namespace System::Threading;
using namespace System::IO;
using namespace System::Runtime::InteropServices;
using namespace System::Windows::Forms::DataVisualization::Charting;

// Raw data reception buffer parameters for the data received from the RSP1A receiver 

// Declare Rx buffer size
const int RxBufferSize = 2000000;

// Define i and q data buffers
short RxiData[RxBufferSize];
short RxqData[RxBufferSize];

// Define Rx time tag buffer
unsigned int RxTimeTag[RxBufferSize];

// Define Rx current start pointer
unsigned int RxDataStartingIndex = 0;

// Define Rx current end pointer
unsigned int RxDataEndingIndex = 0;

// Define a constant of short type size
const int ShortSize = sizeof(short);



// Time parameters

// Define initial software start time
high_resolution_clock::time_point StartTime = high_resolution_clock::now();

// Define a current time variable
high_resolution_clock::time_point CurrentTime;

// Define a time diff variable
duration<double, std::milli> CurrentTimeMinusStartTime;

// Define a variable holding the time tag of the first packet in every second
unsigned int CycleZeroTimeTag = 0;



// Recovered bits data array

// Declare Rx symbols buffer size
const int RxBitsArraySize = 100000;

// Declare Rx symbols length buffer
int RxBitsArrayLength[RxBitsArraySize];

// Declare Rx symbols time tag buffer
unsigned int RxBitsArrayLengthTimeTag[RxBitsArraySize];

// Declare and Rx symbols buffer current start pointer
unsigned int RxBitsStartingIndex = 0;

// Declare and Rx symbols buffer current end pointer
unsigned int RxBitsEndingIndex = 0;

// Declare and Rx symbols buffer previouis end pointer
unsigned int PreviousRxBitsEndingIndex = 0;



// Recovered packet content

// Define the data size at the output of the FEC algorithm
const int FECOutDataSize = PacketLength + 2;

// Define the data size at the intput of the FEC algorithm
const int FECInDataSize = PacketLength + 2 + nn - kk;

// Define a packet reception buffer
byte RxPacketData[PacketLength];



// Debug buffers

// Define a debug Rx symbols buffer size
const int DebugRxBitsArraySize = 4 * FECRawPacketLength * SymbolsPerByte;

// Define a debug Rx symbols buffer
int DebugRxBitsArrayLength[DebugRxBitsArraySize];

// Define a debug buffer for recd array
int Debugrecd[nn] = { 0 };

// Define a debug buffer for the input of the FEC algorithm
int DebugFECInData[nn] = { 0 };

// Define a debug buffer for the packet reception buffer
byte DebugRxPacketData[FECOutDataSize] = { 0 };



// Form cyclic data variables

// Define a good packets counter
unsigned int GoodPacketsCounter = 0;

// Define a lost packets counter
unsigned int LostPacketsCounter = 0;

// Define a packet search counter
unsigned int PacketSearchCounter = 0;

// Define a data rate variable
float DataRate = 0;

// Define a debug line buffer for debug text messages
char DebugLine[msgbufSize];

// Define a debug chksum fails counter
int chksumFails = 0;



// PCM buffer parameter

// Define 1 second worth of PCM data buffer size
const int PCMBuffer1SecSize = 2 * 8000; // 2 characters per sample, 8000 samples per second

// Define PCM data sub buffer size
const int PCMSubBufferSize = 2 * PCMBuffer1SecSize; // 2 second worth of data

// Define the number of sub buffers within the PCM data buffer
const int PCMBufferLength = 8;

// Define a PCM data buffer. The buffer comprise a number of sub buffers.
// A sub buffer us used to fill the PCM data and to play the PCM data.
// A numbr of sub buffers is required to ensure smooth storage and replay.
char PCMDataBuffer[PCMBufferLength][AudioChannels * PCMSubBufferSize];

// Define the number of the sub buffer that is being played
char PlayedPCMDataSubBuffer = -1;

// Define the number of the sub buffer that is being filled
char FilledPCMDataSubBuffer = 0;

// Define an array of sub buffers end pointers
unsigned int PCMDataSubBufferEndingIndex[PCMBufferLength] = { 0 };

// Define a variable to hold the index value of the last received packet
unsigned char LastPacket = 0;



// G.726 state
g726_state G726StatePointer;



// Wav recording parameters
// The recording is use for debugging

// Define the number of data "channels" that will be saved
const int NumberOfChannels = 5;

// Define an array to save the data
int SaveDataBuffer[5 * 2000000];

// Define a pointer to the daved debug data
int SaveDataIndex = 0;



// SDR devices names array
char DevicesNames[4][64];

// Define receiver callback functions
// The basic callback routines were taken from documentaion supplied by SDRplay

void StreamACallback(short *xi, short *xq, sdrplay_api_StreamCbParamsT *params, unsigned int numSamples, unsigned int reset, void *cbContext)
{
	if (reset)
	{
		sprintf_s(msgbuf, msgbufSize, "sdrplay_api_StreamACallback: numSamples=%d\n", numSamples);
		OutputDebugStringA(msgbuf);
	}

	// Process stream callback data here

	// Calculate number of places to the end of the buffer
	unsigned int DistanceToEnd = RxBufferSize - RxDataEndingIndex;

	// Calculate the data time tag - counting milliseconds since software start
	CurrentTime = high_resolution_clock::now();
	CurrentTimeMinusStartTime = CurrentTime - StartTime;
	unsigned int CurrentTimeTag = Convert::ToUInt32(CurrentTimeMinusStartTime.count());

	// If the buffer is not close to full, copy the data to the buffer and advance the buffer end index
	// Else copy the data till the end of the buffer and save the rest at the beginning of the buffer in a cyclic manner
	if (numSamples < DistanceToEnd)
	{
		memcpy(RxiData + RxDataEndingIndex, xi, numSamples * ShortSize);
		memcpy(RxqData + RxDataEndingIndex, xq, numSamples * ShortSize);
		fill_n(RxTimeTag + RxDataEndingIndex, numSamples, CurrentTimeTag);
		RxDataEndingIndex += numSamples;
	}
	else
	{
		memcpy(RxiData + RxDataEndingIndex, xi, DistanceToEnd * ShortSize);
		memcpy(RxqData + RxDataEndingIndex, xq, DistanceToEnd * ShortSize);
		fill_n(RxTimeTag + RxDataEndingIndex, numSamples, CurrentTimeTag);
		memcpy(RxiData, xi + DistanceToEnd, (numSamples - DistanceToEnd) * ShortSize);
		memcpy(RxqData, xq + DistanceToEnd, (numSamples - DistanceToEnd) * ShortSize);
		fill_n(RxTimeTag, (numSamples - DistanceToEnd), CurrentTimeTag);
		RxDataEndingIndex = numSamples - DistanceToEnd;
	}

	return;
}

void StreamBCallback(short *xi, short *xq, sdrplay_api_StreamCbParamsT *params, unsigned int numSamples, unsigned int reset, void *cbContext)
{
	if (reset)
	{
		sprintf_s(msgbuf, msgbufSize, "sdrplay_api_StreamBCallback: numSamples=%d\n", numSamples);
		OutputDebugStringA(msgbuf);
	}

	// Process stream callback data here - this callback will only be used in dual tuner mode

	return;
}

void EventCallback(sdrplay_api_EventT eventId, sdrplay_api_TunerSelectT tuner, sdrplay_api_EventParamsT *params, void *cbContext)
{
	switch (eventId)
	{
	case sdrplay_api_GainChange:
		sprintf_s(msgbuf, msgbufSize, "sdrplay_api_EventCb: %s, tuner=%s gRdB=%d lnaGRdB=%d systemGain=%.2f\n", "sdrplay_api_GainChange", (tuner == sdrplay_api_Tuner_A) ? "sdrplay_api_Tuner_A" : "sdrplay_api_Tuner_B", params->gainParams.gRdB, params->gainParams.lnaGRdB, params->gainParams.currGain);
		OutputDebugStringA(msgbuf);

		SystemGain = (float)params->gainParams.currGain;
		break;

	case sdrplay_api_PowerOverloadChange:
		sprintf_s(msgbuf, msgbufSize, "sdrplay_api_PowerOverloadChange: tuner=%s powerOverloadChangeType=%s\n", (tuner == sdrplay_api_Tuner_A) ? "sdrplay_api_Tuner_A" : "sdrplay_api_Tuner_B", (params->powerOverloadParams.powerOverloadChangeType == sdrplay_api_Overload_Detected) ? "sdrplay_api_Overload_Detected" : "sdrplay_api_Overload_Corrected");
		OutputDebugStringA(msgbuf);

		// Check overload state
		if (params->powerOverloadParams.powerOverloadChangeType == sdrplay_api_Overload_Detected)
			OverloadDetected = true;
		else if (params->powerOverloadParams.powerOverloadChangeType == sdrplay_api_Overload_Corrected)
			OverloadDetected = false;

		// Send update message to acknowledge power overload message received
		sdrplay_api_Update(chosenDevice->dev, tuner, sdrplay_api_Update_Ctrl_OverloadMsgAck, sdrplay_api_Update_Ext1_None);
		break;

	case sdrplay_api_RspDuoModeChange:
		sprintf_s(msgbuf, msgbufSize, "sdrplay_api_EventCb: %s, tuner=%s modeChangeType=%s\n", "sdrplay_api_RspDuoModeChange", (tuner == sdrplay_api_Tuner_A) ? "sdrplay_api_Tuner_A" : "sdrplay_api_Tuner_B", (params->rspDuoModeParams.modeChangeType == sdrplay_api_MasterInitialised) ? "sdrplay_api_MasterInitialised" :
			(params->rspDuoModeParams.modeChangeType == sdrplay_api_SlaveAttached) ? "sdrplay_api_SlaveAttached" :
			(params->rspDuoModeParams.modeChangeType == sdrplay_api_SlaveDetached) ? "sdrplay_api_SlaveDetached" :
			(params->rspDuoModeParams.modeChangeType == sdrplay_api_SlaveInitialised) ? "sdrplay_api_SlaveInitialised" :
			(params->rspDuoModeParams.modeChangeType == sdrplay_api_SlaveUninitialised) ? "sdrplay_api_SlaveUninitialised" :
			"unknown type");
		OutputDebugStringA(msgbuf);

		if (params->rspDuoModeParams.modeChangeType == sdrplay_api_MasterInitialised)
		{
			masterInitialised = 1;
		}
		if (params->rspDuoModeParams.modeChangeType == sdrplay_api_SlaveUninitialised)
		{
			slaveUninitialised = 1;
		}
		break;

	case sdrplay_api_DeviceRemoved:
		sprintf_s(msgbuf, msgbufSize, "sdrplay_api_EventCb: %s\n", "sdrplay_api_DeviceRemoved");
		OutputDebugStringA(msgbuf);
		break;

	default:
		sprintf_s(msgbuf, msgbufSize, "sdrplay_api_EventCb: %d, unknown event\n", eventId);
		OutputDebugStringA(msgbuf);
		break;
	}
}

#pragma once

namespace Spock {

	using namespace System;
	using namespace System::ComponentModel;
	using namespace System::Collections;
	using namespace System::Windows::Forms;
	using namespace System::Data;
	using namespace System::Drawing;

	/// <summary>
	/// Summary for MyForm
	/// </summary>
	public ref class MyForm : public System::Windows::Forms::Form
	{
	public:
		MyForm(void)
		{
			// Initialize form components
			InitializeComponent();

			// Initialize radio thread
			InitRadioThread();

			// Wait for radio thread to finish radio setup
			int ElapsedTime = 0;
			while ((SDRFinishedInit == false) & (ElapsedTime < RadioResponseTimeout))
			{
				ElapsedTime += 200;
				Sleep(200);
			}

			// Check if radio init failed. If so - exit the application.
			if (SDRFailedInit)
				Application::Exit();

		    // Flag that SDR related componenets are initialization
			SDRFinishedInit = false;

			// Initialize the GUI values
			InitGUI();

			// Initialize the chart properties
			InitChart();

			// Flag that SDR related componenets finished initialization
			SDRFinishedInit = true;

			// Generate the Reed-Solomon FEC Galois Field GF(2**mm)
			generate_gf();

			// Compute the generator polynomial for the Reed-Solomon FEC
			gen_poly();

			// Set all bytes in FEC recd buffer to 0
			for (int i = 0; i < nn; i++) recd[i] = 0;

			// Initialize the G.726 state
			g726_init_state(&G726StatePointer);

			// Initialize speakers thread
			InitSpeakersThread();

			// Initiate the inner timer routine to start the data processing
			InitInnerTimer(200);
		}

	protected:
		/// <summary>
		/// Clean up any resources being used.
		/// </summary>
		~MyForm()
		{
			if (components)
			{
				delete components;
			}
		}
	private: System::Windows::Forms::Label^  SDRs_list_label;
	private: System::Windows::Forms::ListBox^  SDRsList;
	private: System::Windows::Forms::TextBox^  RadioInit_textBox;
	private: System::Windows::Forms::Label^  CenterFreq_label;
	private: System::Windows::Forms::NumericUpDown^  CenterFreqnumeric;
	private: System::Windows::Forms::Label^  CenterFreqMhz_label;
	private: System::Windows::Forms::Label^  GRdB_label;
	private: System::Windows::Forms::Label^  GainRed_label;
	private: System::Windows::Forms::NumericUpDown^  GainRednumeric;
	private: System::Windows::Forms::Label^  LNAStatelabel;
	private: System::Windows::Forms::NumericUpDown^  LNAStatenumeric;
	private: System::Windows::Forms::DataVisualization::Charting::Chart^  XYchart;
	private: System::Windows::Forms::CheckBox^  SaveSample_checkBox;
	private: System::Windows::Forms::TextBox^  Overload_textBox;
	private: System::Windows::Forms::TextBox^  SysGain_textBox;
	private: System::Windows::Forms::Label^  SysGaindB_label;
	private: System::Windows::Forms::Label^  SysGain_label;
	private: System::Windows::Forms::TextBox^  SamplesPerIteration_textBox;
	private: System::Windows::Forms::Label^  SamplesPerIteration_label;
	private: System::Windows::Forms::Label^  GoodPackets_label;
	private: System::Windows::Forms::TextBox^  GoodPackets_textBox;
	private: System::Windows::Forms::Label^  FailedSearches_label;
	private: System::Windows::Forms::TextBox^  FailedSearches_textBox;
	private: System::Windows::Forms::TextBox^  Symbols_textBox;
	private: System::Windows::Forms::Label^  Symbols_label;
	private: System::Windows::Forms::CheckBox^  ClearNumbers_checkBox;
	private: System::Windows::Forms::CheckBox^  SaveWAV_checkBox;
	private: System::Windows::Forms::CheckBox^  PlayAudio_checkBox;
	private: System::Windows::Forms::Label^  SamplesVsTime_label;
	private: System::Windows::Forms::TextBox^  DataRate_textBox;
	private: System::Windows::Forms::Label^  kbps_label;
	private: System::Windows::Forms::Label^  DataRate_label;
	private: System::Windows::Forms::Label^  Debug_label;
	private: System::Windows::Forms::TextBox^  Debug_textBox;
	private: System::Windows::Forms::TrackBar^  Volume_trackBar;
	private: System::Windows::Forms::Label^  LostPackets_label;
	private: System::Windows::Forms::TextBox^  LostPackets_textBox;
	private: System::Windows::Forms::Label^  GoodPacketsRatio_label;
	private: System::Windows::Forms::TextBox^  GoodPacketsRatio_textBox;
private: System::Windows::Forms::Label^  GoodPacketPercent_label;

	private:
		/// <summary>
		/// Required designer variable.
		/// </summary>
		System::ComponentModel::Container ^components;

#pragma region Windows Form Designer generated code
		/// <summary>
		/// Required method for Designer support - do not modify
		/// the contents of this method with the code editor.
		/// </summary>
		void InitializeComponent(void)
		{
			System::Windows::Forms::DataVisualization::Charting::ChartArea^  chartArea1 = (gcnew System::Windows::Forms::DataVisualization::Charting::ChartArea());
			System::Windows::Forms::DataVisualization::Charting::Legend^  legend1 = (gcnew System::Windows::Forms::DataVisualization::Charting::Legend());
			System::Windows::Forms::DataVisualization::Charting::Series^  series1 = (gcnew System::Windows::Forms::DataVisualization::Charting::Series());
			this->SDRs_list_label = (gcnew System::Windows::Forms::Label());
			this->SDRsList = (gcnew System::Windows::Forms::ListBox());
			this->RadioInit_textBox = (gcnew System::Windows::Forms::TextBox());
			this->CenterFreq_label = (gcnew System::Windows::Forms::Label());
			this->CenterFreqnumeric = (gcnew System::Windows::Forms::NumericUpDown());
			this->CenterFreqMhz_label = (gcnew System::Windows::Forms::Label());
			this->GRdB_label = (gcnew System::Windows::Forms::Label());
			this->GainRed_label = (gcnew System::Windows::Forms::Label());
			this->GainRednumeric = (gcnew System::Windows::Forms::NumericUpDown());
			this->LNAStatelabel = (gcnew System::Windows::Forms::Label());
			this->LNAStatenumeric = (gcnew System::Windows::Forms::NumericUpDown());
			this->XYchart = (gcnew System::Windows::Forms::DataVisualization::Charting::Chart());
			this->SaveSample_checkBox = (gcnew System::Windows::Forms::CheckBox());
			this->Overload_textBox = (gcnew System::Windows::Forms::TextBox());
			this->SysGain_textBox = (gcnew System::Windows::Forms::TextBox());
			this->SysGaindB_label = (gcnew System::Windows::Forms::Label());
			this->SysGain_label = (gcnew System::Windows::Forms::Label());
			this->SamplesPerIteration_textBox = (gcnew System::Windows::Forms::TextBox());
			this->SamplesPerIteration_label = (gcnew System::Windows::Forms::Label());
			this->GoodPackets_label = (gcnew System::Windows::Forms::Label());
			this->GoodPackets_textBox = (gcnew System::Windows::Forms::TextBox());
			this->FailedSearches_label = (gcnew System::Windows::Forms::Label());
			this->FailedSearches_textBox = (gcnew System::Windows::Forms::TextBox());
			this->Symbols_textBox = (gcnew System::Windows::Forms::TextBox());
			this->Symbols_label = (gcnew System::Windows::Forms::Label());
			this->ClearNumbers_checkBox = (gcnew System::Windows::Forms::CheckBox());
			this->SaveWAV_checkBox = (gcnew System::Windows::Forms::CheckBox());
			this->PlayAudio_checkBox = (gcnew System::Windows::Forms::CheckBox());
			this->SamplesVsTime_label = (gcnew System::Windows::Forms::Label());
			this->DataRate_textBox = (gcnew System::Windows::Forms::TextBox());
			this->kbps_label = (gcnew System::Windows::Forms::Label());
			this->DataRate_label = (gcnew System::Windows::Forms::Label());
			this->Debug_label = (gcnew System::Windows::Forms::Label());
			this->Debug_textBox = (gcnew System::Windows::Forms::TextBox());
			this->Volume_trackBar = (gcnew System::Windows::Forms::TrackBar());
			this->LostPackets_label = (gcnew System::Windows::Forms::Label());
			this->LostPackets_textBox = (gcnew System::Windows::Forms::TextBox());
			this->GoodPacketsRatio_label = (gcnew System::Windows::Forms::Label());
			this->GoodPacketsRatio_textBox = (gcnew System::Windows::Forms::TextBox());
			this->GoodPacketPercent_label = (gcnew System::Windows::Forms::Label());
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^>(this->CenterFreqnumeric))->BeginInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^>(this->GainRednumeric))->BeginInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^>(this->LNAStatenumeric))->BeginInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^>(this->XYchart))->BeginInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^>(this->Volume_trackBar))->BeginInit();
			this->SuspendLayout();
			// 
			// SDRs_list_label
			// 
			this->SDRs_list_label->AutoSize = true;
			this->SDRs_list_label->Font = (gcnew System::Drawing::Font(L"Arial", 12, System::Drawing::FontStyle::Bold, System::Drawing::GraphicsUnit::Point,
				static_cast<System::Byte>(177)));
			this->SDRs_list_label->Location = System::Drawing::Point(12, 9);
			this->SDRs_list_label->Name = L"SDRs_list_label";
			this->SDRs_list_label->Size = System::Drawing::Size(96, 24);
			this->SDRs_list_label->TabIndex = 3;
			this->SDRs_list_label->Text = L"SDRs list";
			// 
			// SDRsList
			// 
			this->SDRsList->Font = (gcnew System::Drawing::Font(L"Arial", 12, System::Drawing::FontStyle::Regular, System::Drawing::GraphicsUnit::Point,
				static_cast<System::Byte>(177)));
			this->SDRsList->FormattingEnabled = true;
			this->SDRsList->ItemHeight = 23;
			this->SDRsList->Location = System::Drawing::Point(12, 44);
			this->SDRsList->Name = L"SDRsList";
			this->SDRsList->RightToLeft = System::Windows::Forms::RightToLeft::No;
			this->SDRsList->Size = System::Drawing::Size(353, 96);
			this->SDRsList->TabIndex = 38;
			// 
			// RadioInit_textBox
			// 
			this->RadioInit_textBox->Font = (gcnew System::Drawing::Font(L"Arial", 12, System::Drawing::FontStyle::Regular, System::Drawing::GraphicsUnit::Point,
				static_cast<System::Byte>(177)));
			this->RadioInit_textBox->Location = System::Drawing::Point(230, 160);
			this->RadioInit_textBox->Name = L"RadioInit_textBox";
			this->RadioInit_textBox->RightToLeft = System::Windows::Forms::RightToLeft::No;
			this->RadioInit_textBox->Size = System::Drawing::Size(135, 30);
			this->RadioInit_textBox->TabIndex = 39;
			this->RadioInit_textBox->TextAlign = System::Windows::Forms::HorizontalAlignment::Center;
			// 
			// CenterFreq_label
			// 
			this->CenterFreq_label->AutoSize = true;
			this->CenterFreq_label->Font = (gcnew System::Drawing::Font(L"Arial", 12, System::Drawing::FontStyle::Bold, System::Drawing::GraphicsUnit::Point,
				static_cast<System::Byte>(177)));
			this->CenterFreq_label->Location = System::Drawing::Point(12, 172);
			this->CenterFreq_label->Name = L"CenterFreq_label";
			this->CenterFreq_label->Size = System::Drawing::Size(174, 24);
			this->CenterFreq_label->TabIndex = 41;
			this->CenterFreq_label->Text = L"Center frequency";
			// 
			// CenterFreqnumeric
			// 
			this->CenterFreqnumeric->DecimalPlaces = 3;
			this->CenterFreqnumeric->Font = (gcnew System::Drawing::Font(L"Arial", 12, System::Drawing::FontStyle::Regular, System::Drawing::GraphicsUnit::Point,
				static_cast<System::Byte>(177)));
			this->CenterFreqnumeric->Increment = System::Decimal(gcnew cli::array< System::Int32 >(4) { 1, 0, 0, 65536 });
			this->CenterFreqnumeric->Location = System::Drawing::Point(16, 208);
			this->CenterFreqnumeric->Maximum = System::Decimal(gcnew cli::array< System::Int32 >(4) { 1999, 0, 0, 0 });
			this->CenterFreqnumeric->Minimum = System::Decimal(gcnew cli::array< System::Int32 >(4) { 100, 0, 0, 0 });
			this->CenterFreqnumeric->Name = L"CenterFreqnumeric";
			this->CenterFreqnumeric->Size = System::Drawing::Size(136, 30);
			this->CenterFreqnumeric->TabIndex = 40;
			this->CenterFreqnumeric->TextAlign = System::Windows::Forms::HorizontalAlignment::Center;
			this->CenterFreqnumeric->Value = System::Decimal(gcnew cli::array< System::Int32 >(4) { 100, 0, 0, 0 });
			this->CenterFreqnumeric->ValueChanged += gcnew System::EventHandler(this, &MyForm::CenterFreqnumeric_ValueChanged);
			// 
			// CenterFreqMhz_label
			// 
			this->CenterFreqMhz_label->AutoSize = true;
			this->CenterFreqMhz_label->Font = (gcnew System::Drawing::Font(L"Arial", 12, System::Drawing::FontStyle::Regular, System::Drawing::GraphicsUnit::Point,
				static_cast<System::Byte>(177)));
			this->CenterFreqMhz_label->Location = System::Drawing::Point(156, 211);
			this->CenterFreqMhz_label->Name = L"CenterFreqMhz_label";
			this->CenterFreqMhz_label->Size = System::Drawing::Size(46, 23);
			this->CenterFreqMhz_label->TabIndex = 42;
			this->CenterFreqMhz_label->Text = L"Mhz";
			// 
			// GRdB_label
			// 
			this->GRdB_label->AutoSize = true;
			this->GRdB_label->Font = (gcnew System::Drawing::Font(L"Arial", 12, System::Drawing::FontStyle::Regular, System::Drawing::GraphicsUnit::Point,
				static_cast<System::Byte>(177)));
			this->GRdB_label->Location = System::Drawing::Point(156, 295);
			this->GRdB_label->Name = L"GRdB_label";
			this->GRdB_label->Size = System::Drawing::Size(34, 23);
			this->GRdB_label->TabIndex = 45;
			this->GRdB_label->Text = L"dB";
			// 
			// GainRed_label
			// 
			this->GainRed_label->AutoSize = true;
			this->GainRed_label->Font = (gcnew System::Drawing::Font(L"Arial", 12, System::Drawing::FontStyle::Bold, System::Drawing::GraphicsUnit::Point,
				static_cast<System::Byte>(177)));
			this->GainRed_label->Location = System::Drawing::Point(12, 256);
			this->GainRed_label->Name = L"GainRed_label";
			this->GainRed_label->Size = System::Drawing::Size(150, 24);
			this->GainRed_label->TabIndex = 44;
			this->GainRed_label->Text = L"Gain reduction";
			// 
			// GainRednumeric
			// 
			this->GainRednumeric->Font = (gcnew System::Drawing::Font(L"Arial", 12, System::Drawing::FontStyle::Regular, System::Drawing::GraphicsUnit::Point,
				static_cast<System::Byte>(177)));
			this->GainRednumeric->Location = System::Drawing::Point(16, 292);
			this->GainRednumeric->Maximum = System::Decimal(gcnew cli::array< System::Int32 >(4) { 84, 0, 0, 0 });
			this->GainRednumeric->Name = L"GainRednumeric";
			this->GainRednumeric->Size = System::Drawing::Size(136, 30);
			this->GainRednumeric->TabIndex = 43;
			this->GainRednumeric->TextAlign = System::Windows::Forms::HorizontalAlignment::Center;
			this->GainRednumeric->Value = System::Decimal(gcnew cli::array< System::Int32 >(4) { 65, 0, 0, 0 });
			this->GainRednumeric->ValueChanged += gcnew System::EventHandler(this, &MyForm::GainRednumeric_ValueChanged);
			// 
			// LNAStatelabel
			// 
			this->LNAStatelabel->AutoSize = true;
			this->LNAStatelabel->Font = (gcnew System::Drawing::Font(L"Arial", 12, System::Drawing::FontStyle::Bold, System::Drawing::GraphicsUnit::Point,
				static_cast<System::Byte>(177)));
			this->LNAStatelabel->Location = System::Drawing::Point(12, 340);
			this->LNAStatelabel->Name = L"LNAStatelabel";
			this->LNAStatelabel->Size = System::Drawing::Size(101, 24);
			this->LNAStatelabel->TabIndex = 53;
			this->LNAStatelabel->Text = L"LNA state";
			// 
			// LNAStatenumeric
			// 
			this->LNAStatenumeric->Font = (gcnew System::Drawing::Font(L"Arial", 12, System::Drawing::FontStyle::Regular, System::Drawing::GraphicsUnit::Point,
				static_cast<System::Byte>(177)));
			this->LNAStatenumeric->Location = System::Drawing::Point(16, 376);
			this->LNAStatenumeric->Maximum = System::Decimal(gcnew cli::array< System::Int32 >(4) { 27, 0, 0, 0 });
			this->LNAStatenumeric->Name = L"LNAStatenumeric";
			this->LNAStatenumeric->Size = System::Drawing::Size(136, 30);
			this->LNAStatenumeric->TabIndex = 52;
			this->LNAStatenumeric->TextAlign = System::Windows::Forms::HorizontalAlignment::Center;
			this->LNAStatenumeric->Value = System::Decimal(gcnew cli::array< System::Int32 >(4) { 27, 0, 0, 0 });
			this->LNAStatenumeric->ValueChanged += gcnew System::EventHandler(this, &MyForm::LNAStatenumeric_ValueChanged);
			// 
			// XYchart
			// 
			chartArea1->Name = L"ChartArea1";
			this->XYchart->ChartAreas->Add(chartArea1);
			legend1->Enabled = false;
			legend1->Name = L"Legend1";
			this->XYchart->Legends->Add(legend1);
			this->XYchart->Location = System::Drawing::Point(391, 44);
			this->XYchart->Name = L"XYchart";
			series1->ChartArea = L"ChartArea1";
			series1->Legend = L"Legend1";
			series1->Name = L"Series1";
			this->XYchart->Series->Add(series1);
			this->XYchart->Size = System::Drawing::Size(646, 300);
			this->XYchart->TabIndex = 54;
			this->XYchart->Text = L"chart1";
			// 
			// SaveSample_checkBox
			// 
			this->SaveSample_checkBox->AutoSize = true;
			this->SaveSample_checkBox->Font = (gcnew System::Drawing::Font(L"Arial", 12, System::Drawing::FontStyle::Regular, System::Drawing::GraphicsUnit::Point,
				static_cast<System::Byte>(177)));
			this->SaveSample_checkBox->Location = System::Drawing::Point(655, 379);
			this->SaveSample_checkBox->Name = L"SaveSample_checkBox";
			this->SaveSample_checkBox->Size = System::Drawing::Size(145, 27);
			this->SaveSample_checkBox->TabIndex = 55;
			this->SaveSample_checkBox->Text = L"Save sample";
			this->SaveSample_checkBox->UseVisualStyleBackColor = true;
			this->SaveSample_checkBox->CheckedChanged += gcnew System::EventHandler(this, &MyForm::SaveSample_checkBox_CheckedChanged);
			// 
			// Overload_textBox
			// 
			this->Overload_textBox->Font = (gcnew System::Drawing::Font(L"Arial", 12, System::Drawing::FontStyle::Regular, System::Drawing::GraphicsUnit::Point,
				static_cast<System::Byte>(177)));
			this->Overload_textBox->Location = System::Drawing::Point(230, 207);
			this->Overload_textBox->Name = L"Overload_textBox";
			this->Overload_textBox->RightToLeft = System::Windows::Forms::RightToLeft::No;
			this->Overload_textBox->Size = System::Drawing::Size(135, 30);
			this->Overload_textBox->TabIndex = 56;
			this->Overload_textBox->TextAlign = System::Windows::Forms::HorizontalAlignment::Center;
			// 
			// SysGain_textBox
			// 
			this->SysGain_textBox->Font = (gcnew System::Drawing::Font(L"Arial", 12, System::Drawing::FontStyle::Regular, System::Drawing::GraphicsUnit::Point,
				static_cast<System::Byte>(177)));
			this->SysGain_textBox->Location = System::Drawing::Point(230, 375);
			this->SysGain_textBox->Name = L"SysGain_textBox";
			this->SysGain_textBox->RightToLeft = System::Windows::Forms::RightToLeft::No;
			this->SysGain_textBox->Size = System::Drawing::Size(135, 30);
			this->SysGain_textBox->TabIndex = 57;
			this->SysGain_textBox->TextAlign = System::Windows::Forms::HorizontalAlignment::Center;
			// 
			// SysGaindB_label
			// 
			this->SysGaindB_label->AutoSize = true;
			this->SysGaindB_label->Font = (gcnew System::Drawing::Font(L"Arial", 12, System::Drawing::FontStyle::Regular, System::Drawing::GraphicsUnit::Point,
				static_cast<System::Byte>(177)));
			this->SysGaindB_label->Location = System::Drawing::Point(375, 378);
			this->SysGaindB_label->Name = L"SysGaindB_label";
			this->SysGaindB_label->Size = System::Drawing::Size(34, 23);
			this->SysGaindB_label->TabIndex = 58;
			this->SysGaindB_label->Text = L"dB";
			// 
			// SysGain_label
			// 
			this->SysGain_label->AutoSize = true;
			this->SysGain_label->Font = (gcnew System::Drawing::Font(L"Arial", 12, System::Drawing::FontStyle::Bold, System::Drawing::GraphicsUnit::Point,
				static_cast<System::Byte>(177)));
			this->SysGain_label->Location = System::Drawing::Point(226, 340);
			this->SysGain_label->Name = L"SysGain_label";
			this->SysGain_label->Size = System::Drawing::Size(130, 24);
			this->SysGain_label->TabIndex = 59;
			this->SysGain_label->Text = L"System Gain";
			// 
			// SamplesPerIteration_textBox
			// 
			this->SamplesPerIteration_textBox->Font = (gcnew System::Drawing::Font(L"Arial", 12, System::Drawing::FontStyle::Regular, System::Drawing::GraphicsUnit::Point,
				static_cast<System::Byte>(177)));
			this->SamplesPerIteration_textBox->Location = System::Drawing::Point(1069, 44);
			this->SamplesPerIteration_textBox->Name = L"SamplesPerIteration_textBox";
			this->SamplesPerIteration_textBox->RightToLeft = System::Windows::Forms::RightToLeft::No;
			this->SamplesPerIteration_textBox->Size = System::Drawing::Size(135, 30);
			this->SamplesPerIteration_textBox->TabIndex = 60;
			this->SamplesPerIteration_textBox->TextAlign = System::Windows::Forms::HorizontalAlignment::Center;
			// 
			// SamplesPerIteration_label
			// 
			this->SamplesPerIteration_label->AutoSize = true;
			this->SamplesPerIteration_label->Font = (gcnew System::Drawing::Font(L"Arial", 12, System::Drawing::FontStyle::Bold, System::Drawing::GraphicsUnit::Point,
				static_cast<System::Byte>(177)));
			this->SamplesPerIteration_label->Location = System::Drawing::Point(1065, 9);
			this->SamplesPerIteration_label->Name = L"SamplesPerIteration_label";
			this->SamplesPerIteration_label->Size = System::Drawing::Size(211, 24);
			this->SamplesPerIteration_label->TabIndex = 62;
			this->SamplesPerIteration_label->Text = L"Samples per iteration";
			// 
			// GoodPackets_label
			// 
			this->GoodPackets_label->AutoSize = true;
			this->GoodPackets_label->Font = (gcnew System::Drawing::Font(L"Arial", 12, System::Drawing::FontStyle::Bold, System::Drawing::GraphicsUnit::Point,
				static_cast<System::Byte>(177)));
			this->GoodPackets_label->Location = System::Drawing::Point(1065, 94);
			this->GoodPackets_label->Name = L"GoodPackets_label";
			this->GoodPackets_label->Size = System::Drawing::Size(142, 24);
			this->GoodPackets_label->TabIndex = 64;
			this->GoodPackets_label->Text = L"Good packets";
			// 
			// GoodPackets_textBox
			// 
			this->GoodPackets_textBox->Font = (gcnew System::Drawing::Font(L"Arial", 12, System::Drawing::FontStyle::Regular, System::Drawing::GraphicsUnit::Point,
				static_cast<System::Byte>(177)));
			this->GoodPackets_textBox->Location = System::Drawing::Point(1069, 129);
			this->GoodPackets_textBox->Name = L"GoodPackets_textBox";
			this->GoodPackets_textBox->RightToLeft = System::Windows::Forms::RightToLeft::No;
			this->GoodPackets_textBox->Size = System::Drawing::Size(135, 30);
			this->GoodPackets_textBox->TabIndex = 63;
			this->GoodPackets_textBox->TextAlign = System::Windows::Forms::HorizontalAlignment::Center;
			// 
			// FailedSearches_label
			// 
			this->FailedSearches_label->AutoSize = true;
			this->FailedSearches_label->Font = (gcnew System::Drawing::Font(L"Arial", 12, System::Drawing::FontStyle::Bold, System::Drawing::GraphicsUnit::Point,
				static_cast<System::Byte>(177)));
			this->FailedSearches_label->Location = System::Drawing::Point(1253, 178);
			this->FailedSearches_label->Name = L"FailedSearches_label";
			this->FailedSearches_label->Size = System::Drawing::Size(158, 24);
			this->FailedSearches_label->TabIndex = 70;
			this->FailedSearches_label->Text = L"Failed searches";
			// 
			// FailedSearches_textBox
			// 
			this->FailedSearches_textBox->Font = (gcnew System::Drawing::Font(L"Arial", 12, System::Drawing::FontStyle::Regular, System::Drawing::GraphicsUnit::Point,
				static_cast<System::Byte>(177)));
			this->FailedSearches_textBox->Location = System::Drawing::Point(1257, 213);
			this->FailedSearches_textBox->Name = L"FailedSearches_textBox";
			this->FailedSearches_textBox->RightToLeft = System::Windows::Forms::RightToLeft::No;
			this->FailedSearches_textBox->Size = System::Drawing::Size(135, 30);
			this->FailedSearches_textBox->TabIndex = 69;
			this->FailedSearches_textBox->TextAlign = System::Windows::Forms::HorizontalAlignment::Center;
			// 
			// Symbols_textBox
			// 
			this->Symbols_textBox->Font = (gcnew System::Drawing::Font(L"Arial", 12, System::Drawing::FontStyle::Regular, System::Drawing::GraphicsUnit::Point,
				static_cast<System::Byte>(177)));
			this->Symbols_textBox->Location = System::Drawing::Point(1069, 297);
			this->Symbols_textBox->Name = L"Symbols_textBox";
			this->Symbols_textBox->RightToLeft = System::Windows::Forms::RightToLeft::No;
			this->Symbols_textBox->Size = System::Drawing::Size(135, 30);
			this->Symbols_textBox->TabIndex = 71;
			this->Symbols_textBox->TextAlign = System::Windows::Forms::HorizontalAlignment::Center;
			// 
			// Symbols_label
			// 
			this->Symbols_label->AutoSize = true;
			this->Symbols_label->Font = (gcnew System::Drawing::Font(L"Arial", 12, System::Drawing::FontStyle::Bold, System::Drawing::GraphicsUnit::Point,
				static_cast<System::Byte>(177)));
			this->Symbols_label->Location = System::Drawing::Point(1065, 262);
			this->Symbols_label->Name = L"Symbols_label";
			this->Symbols_label->Size = System::Drawing::Size(212, 24);
			this->Symbols_label->TabIndex = 76;
			this->Symbols_label->Text = L"Symbols per iteration";
			// 
			// ClearNumbers_checkBox
			// 
			this->ClearNumbers_checkBox->AutoSize = true;
			this->ClearNumbers_checkBox->Font = (gcnew System::Drawing::Font(L"Arial", 12, System::Drawing::FontStyle::Regular, System::Drawing::GraphicsUnit::Point,
				static_cast<System::Byte>(177)));
			this->ClearNumbers_checkBox->Location = System::Drawing::Point(835, 379);
			this->ClearNumbers_checkBox->Name = L"ClearNumbers_checkBox";
			this->ClearNumbers_checkBox->Size = System::Drawing::Size(160, 27);
			this->ClearNumbers_checkBox->TabIndex = 77;
			this->ClearNumbers_checkBox->Text = L"Clear numbers";
			this->ClearNumbers_checkBox->UseVisualStyleBackColor = true;
			this->ClearNumbers_checkBox->CheckedChanged += gcnew System::EventHandler(this, &MyForm::ClearNumbers_checkBox_CheckedChanged);
			// 
			// SaveWAV_checkBox
			// 
			this->SaveWAV_checkBox->AutoSize = true;
			this->SaveWAV_checkBox->Font = (gcnew System::Drawing::Font(L"Arial", 12, System::Drawing::FontStyle::Regular, System::Drawing::GraphicsUnit::Point,
				static_cast<System::Byte>(177)));
			this->SaveWAV_checkBox->Location = System::Drawing::Point(487, 379);
			this->SaveWAV_checkBox->Name = L"SaveWAV_checkBox";
			this->SaveWAV_checkBox->Size = System::Drawing::Size(125, 27);
			this->SaveWAV_checkBox->TabIndex = 78;
			this->SaveWAV_checkBox->Text = L"Save WAV";
			this->SaveWAV_checkBox->UseVisualStyleBackColor = true;
			this->SaveWAV_checkBox->CheckedChanged += gcnew System::EventHandler(this, &MyForm::SaveWAV_CheckedChanged);
			// 
			// PlayAudio_checkBox
			// 
			this->PlayAudio_checkBox->AutoSize = true;
			this->PlayAudio_checkBox->Font = (gcnew System::Drawing::Font(L"Arial", 12, System::Drawing::FontStyle::Regular, System::Drawing::GraphicsUnit::Point,
				static_cast<System::Byte>(177)));
			this->PlayAudio_checkBox->Location = System::Drawing::Point(229, 436);
			this->PlayAudio_checkBox->Name = L"PlayAudio_checkBox";
			this->PlayAudio_checkBox->Size = System::Drawing::Size(123, 27);
			this->PlayAudio_checkBox->TabIndex = 79;
			this->PlayAudio_checkBox->Text = L"Play audio";
			this->PlayAudio_checkBox->UseVisualStyleBackColor = true;
			// 
			// SamplesVsTime_label
			// 
			this->SamplesVsTime_label->AutoSize = true;
			this->SamplesVsTime_label->Font = (gcnew System::Drawing::Font(L"Arial", 12, System::Drawing::FontStyle::Bold, System::Drawing::GraphicsUnit::Point,
				static_cast<System::Byte>(177)));
			this->SamplesVsTime_label->Location = System::Drawing::Point(387, 9);
			this->SamplesVsTime_label->Name = L"SamplesVsTime_label";
			this->SamplesVsTime_label->Size = System::Drawing::Size(164, 24);
			this->SamplesVsTime_label->TabIndex = 80;
			this->SamplesVsTime_label->Text = L"Samples vs time";
			// 
			// DataRate_textBox
			// 
			this->DataRate_textBox->Font = (gcnew System::Drawing::Font(L"Arial", 12, System::Drawing::FontStyle::Regular, System::Drawing::GraphicsUnit::Point,
				static_cast<System::Byte>(177)));
			this->DataRate_textBox->Location = System::Drawing::Point(1069, 381);
			this->DataRate_textBox->Name = L"DataRate_textBox";
			this->DataRate_textBox->RightToLeft = System::Windows::Forms::RightToLeft::No;
			this->DataRate_textBox->Size = System::Drawing::Size(135, 30);
			this->DataRate_textBox->TabIndex = 83;
			this->DataRate_textBox->TextAlign = System::Windows::Forms::HorizontalAlignment::Center;
			// 
			// kbps_label
			// 
			this->kbps_label->AutoSize = true;
			this->kbps_label->Font = (gcnew System::Drawing::Font(L"Arial", 12, System::Drawing::FontStyle::Bold, System::Drawing::GraphicsUnit::Point,
				static_cast<System::Byte>(177)));
			this->kbps_label->Location = System::Drawing::Point(1220, 382);
			this->kbps_label->Name = L"kbps_label";
			this->kbps_label->Size = System::Drawing::Size(56, 24);
			this->kbps_label->TabIndex = 85;
			this->kbps_label->Text = L"kbps";
			// 
			// DataRate_label
			// 
			this->DataRate_label->AutoSize = true;
			this->DataRate_label->Font = (gcnew System::Drawing::Font(L"Arial", 12, System::Drawing::FontStyle::Bold, System::Drawing::GraphicsUnit::Point,
				static_cast<System::Byte>(177)));
			this->DataRate_label->Location = System::Drawing::Point(1065, 346);
			this->DataRate_label->Name = L"DataRate_label";
			this->DataRate_label->Size = System::Drawing::Size(225, 24);
			this->DataRate_label->TabIndex = 84;
			this->DataRate_label->Text = L"Data rate per 1 second";
			// 
			// Debug_label
			// 
			this->Debug_label->AutoSize = true;
			this->Debug_label->Font = (gcnew System::Drawing::Font(L"Arial", 12, System::Drawing::FontStyle::Bold, System::Drawing::GraphicsUnit::Point,
				static_cast<System::Byte>(177)));
			this->Debug_label->Location = System::Drawing::Point(12, 463);
			this->Debug_label->Name = L"Debug_label";
			this->Debug_label->Size = System::Drawing::Size(71, 24);
			this->Debug_label->TabIndex = 89;
			this->Debug_label->Text = L"Debug";
			// 
			// Debug_textBox
			// 
			this->Debug_textBox->Font = (gcnew System::Drawing::Font(L"Arial", 12, System::Drawing::FontStyle::Regular, System::Drawing::GraphicsUnit::Point,
				static_cast<System::Byte>(177)));
			this->Debug_textBox->Location = System::Drawing::Point(16, 498);
			this->Debug_textBox->Name = L"Debug_textBox";
			this->Debug_textBox->RightToLeft = System::Windows::Forms::RightToLeft::No;
			this->Debug_textBox->Size = System::Drawing::Size(1376, 30);
			this->Debug_textBox->TabIndex = 88;
			// 
			// Volume_trackBar
			// 
			this->Volume_trackBar->Location = System::Drawing::Point(379, 436);
			this->Volume_trackBar->Maximum = 65535;
			this->Volume_trackBar->Name = L"Volume_trackBar";
			this->Volume_trackBar->Size = System::Drawing::Size(233, 56);
			this->Volume_trackBar->TabIndex = 90;
			// 
			// LostPackets_label
			// 
			this->LostPackets_label->AutoSize = true;
			this->LostPackets_label->Font = (gcnew System::Drawing::Font(L"Arial", 12, System::Drawing::FontStyle::Bold, System::Drawing::GraphicsUnit::Point,
				static_cast<System::Byte>(177)));
			this->LostPackets_label->Location = System::Drawing::Point(1065, 178);
			this->LostPackets_label->Name = L"LostPackets_label";
			this->LostPackets_label->Size = System::Drawing::Size(132, 24);
			this->LostPackets_label->TabIndex = 94;
			this->LostPackets_label->Text = L"Lost packets";
			// 
			// LostPackets_textBox
			// 
			this->LostPackets_textBox->Font = (gcnew System::Drawing::Font(L"Arial", 12, System::Drawing::FontStyle::Regular, System::Drawing::GraphicsUnit::Point,
				static_cast<System::Byte>(177)));
			this->LostPackets_textBox->Location = System::Drawing::Point(1069, 213);
			this->LostPackets_textBox->Name = L"LostPackets_textBox";
			this->LostPackets_textBox->RightToLeft = System::Windows::Forms::RightToLeft::No;
			this->LostPackets_textBox->Size = System::Drawing::Size(135, 30);
			this->LostPackets_textBox->TabIndex = 93;
			this->LostPackets_textBox->TextAlign = System::Windows::Forms::HorizontalAlignment::Center;
			// 
			// GoodPacketsRatio_label
			// 
			this->GoodPacketsRatio_label->AutoSize = true;
			this->GoodPacketsRatio_label->Font = (gcnew System::Drawing::Font(L"Arial", 12, System::Drawing::FontStyle::Bold, System::Drawing::GraphicsUnit::Point,
				static_cast<System::Byte>(177)));
			this->GoodPacketsRatio_label->Location = System::Drawing::Point(1253, 94);
			this->GoodPacketsRatio_label->Name = L"GoodPacketsRatio_label";
			this->GoodPacketsRatio_label->Size = System::Drawing::Size(191, 24);
			this->GoodPacketsRatio_label->TabIndex = 92;
			this->GoodPacketsRatio_label->Text = L"Good packets ratio";
			// 
			// GoodPacketsRatio_textBox
			// 
			this->GoodPacketsRatio_textBox->Font = (gcnew System::Drawing::Font(L"Arial", 12, System::Drawing::FontStyle::Regular, System::Drawing::GraphicsUnit::Point,
				static_cast<System::Byte>(177)));
			this->GoodPacketsRatio_textBox->Location = System::Drawing::Point(1257, 129);
			this->GoodPacketsRatio_textBox->Name = L"GoodPacketsRatio_textBox";
			this->GoodPacketsRatio_textBox->RightToLeft = System::Windows::Forms::RightToLeft::No;
			this->GoodPacketsRatio_textBox->Size = System::Drawing::Size(135, 30);
			this->GoodPacketsRatio_textBox->TabIndex = 91;
			this->GoodPacketsRatio_textBox->TextAlign = System::Windows::Forms::HorizontalAlignment::Center;
			// 
			// GoodPacketPercent_label
			// 
			this->GoodPacketPercent_label->AutoSize = true;
			this->GoodPacketPercent_label->Font = (gcnew System::Drawing::Font(L"Arial", 12, System::Drawing::FontStyle::Bold, System::Drawing::GraphicsUnit::Point,
				static_cast<System::Byte>(177)));
			this->GoodPacketPercent_label->Location = System::Drawing::Point(1407, 132);
			this->GoodPacketPercent_label->Name = L"GoodPacketPercent_label";
			this->GoodPacketPercent_label->Size = System::Drawing::Size(27, 24);
			this->GoodPacketPercent_label->TabIndex = 95;
			this->GoodPacketPercent_label->Text = L"%";
			// 
			// MyForm
			// 
			this->AutoScaleDimensions = System::Drawing::SizeF(8, 16);
			this->AutoScaleMode = System::Windows::Forms::AutoScaleMode::Font;
			this->ClientSize = System::Drawing::Size(1462, 556);
			this->Controls->Add(this->GoodPacketPercent_label);
			this->Controls->Add(this->LostPackets_label);
			this->Controls->Add(this->LostPackets_textBox);
			this->Controls->Add(this->GoodPacketsRatio_label);
			this->Controls->Add(this->GoodPacketsRatio_textBox);
			this->Controls->Add(this->Volume_trackBar);
			this->Controls->Add(this->Debug_label);
			this->Controls->Add(this->Debug_textBox);
			this->Controls->Add(this->kbps_label);
			this->Controls->Add(this->DataRate_label);
			this->Controls->Add(this->DataRate_textBox);
			this->Controls->Add(this->SamplesVsTime_label);
			this->Controls->Add(this->PlayAudio_checkBox);
			this->Controls->Add(this->SaveWAV_checkBox);
			this->Controls->Add(this->ClearNumbers_checkBox);
			this->Controls->Add(this->Symbols_label);
			this->Controls->Add(this->Symbols_textBox);
			this->Controls->Add(this->FailedSearches_label);
			this->Controls->Add(this->FailedSearches_textBox);
			this->Controls->Add(this->GoodPackets_label);
			this->Controls->Add(this->GoodPackets_textBox);
			this->Controls->Add(this->SamplesPerIteration_label);
			this->Controls->Add(this->SamplesPerIteration_textBox);
			this->Controls->Add(this->SysGain_label);
			this->Controls->Add(this->SysGaindB_label);
			this->Controls->Add(this->SysGain_textBox);
			this->Controls->Add(this->Overload_textBox);
			this->Controls->Add(this->SaveSample_checkBox);
			this->Controls->Add(this->XYchart);
			this->Controls->Add(this->LNAStatelabel);
			this->Controls->Add(this->LNAStatenumeric);
			this->Controls->Add(this->GRdB_label);
			this->Controls->Add(this->GainRed_label);
			this->Controls->Add(this->GainRednumeric);
			this->Controls->Add(this->CenterFreqMhz_label);
			this->Controls->Add(this->CenterFreq_label);
			this->Controls->Add(this->CenterFreqnumeric);
			this->Controls->Add(this->RadioInit_textBox);
			this->Controls->Add(this->SDRsList);
			this->Controls->Add(this->SDRs_list_label);
			this->Name = L"MyForm";
			this->RightToLeft = System::Windows::Forms::RightToLeft::Yes;
			this->Text = L"Spock";
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^>(this->CenterFreqnumeric))->EndInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^>(this->GainRednumeric))->EndInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^>(this->LNAStatenumeric))->EndInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^>(this->XYchart))->EndInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^>(this->Volume_trackBar))->EndInit();
			this->ResumeLayout(false);
			this->PerformLayout();

		}
#pragma endregion

	// Declare variables

	// Define a variable that holds a pointer to the selected SDR in the radio list
	int SelectedSDR = 0;

	// Define a variable that indicates whether the SDR initialization had failed
	bool SDRFailedInit = false;

	// Define variables for a second low pass butterworth filter
	float SecondButterLPFyn = 0;   // y(n) = present cycle output value
	float SecondButterLPFyn_1 = 0; // y(n-1) = output value at previous cycle
	float SecondButterLPFyn_2 = 0; // y(n-2) = output value 2 cycles ago
	float SecondButterLPFxn = 0;   // y(n) = present cycle input value
	float SecondButterLPFxn_1 = 0; // x(n-1) = input value at previous cycle
	float SecondButterLPFxn_2 = 0; // x(n-2) = input value 2 cycles ago

	// Define received data moving average index
	int MovingAverageIndex = 0;

	// Define received data moving address current value variable
	float MovingAverage = 0;

	// Define a slow low pass filtered value of received data variable
	float DCSlowFilteredData = 0;

	//  Define a slow low pass filtered value of received data peaks variable
	float PeakSlowFilteredData = 0;

	//  Define a slow low pass filtered value of received data noise variable
	float FloorSlowFilteredData = 0;

	// Define a detection threshold for low state detection
	float LowBitDetectionValue = 0;

	// Defined a detection threshold for high state detection
	float HighBitDetectionValue = 0;

	// Define a variable holding the present symbol state during the symbol recovering process
	bool CurrentBitState = false;

	// Define a variable holding the present symbol length oduring the symbol recovering process
	int CurrentStateLength = 0;

	// Define a variable holding the recovered symbol state
	bool RecoveredBitState = false;

	// Define a variable holding the recovered symbol length
	int RecoveredStateLength = 0;

	// Define a variabl holding previous recovered symbol state
	int PrevRecoveredStateLength = 0;

	// Define a variable holding bit ratio moving average index
	int BitRatioMovingAverageIndex = 0;

	// Define a variable holding bit ratio present value
	int BitRatio = 0;

	// Define a flag to mark that the bit ratio averaging buffer had not yet filled
	bool BitRatioAverageFlag = true;

	// Define a variable for debugging to indicate that recording is required
	bool RecordFlag = false;

	private: System::Windows::Forms::Timer^  InnerTimer;

	private:
		// Initialize the form GUI
		void InitGUI(void)
		{
			// Fill the cards list
			for (unsigned int i = 0; i < NumberOfSDRDevices; i++)
			{
				String^ SysFullName = gcnew String(DevicesNames[i]);
				SDRsList->Items->Add(SysFullName);
			}
			sprintf_s(msgbuf, msgbufSize, "chosenDevice = %d\n", SelectedSDR);
			OutputDebugStringA(msgbuf);

			// Set form values
			SDRsList->SelectedIndex = SelectedSDR;
			CenterFreqnumeric->Value = Convert::ToDecimal(CenterFreqeuncyInit);
			LNAStatenumeric->Value = LNAstateInit;
			GainRednumeric->Value = GainReductionInit;
		}

		// Initialize the receiving radio
		void InitRadio(void)
		{
			RadioInit_textBox->Text == "";

			// Lock API while device selection is performed
			sdrplay_api_LockDeviceApi();

			// Fetch list of available devices
			if ((err = sdrplay_api_GetDevices(SDRDevices, &NumberOfSDRDevices, sizeof(SDRDevices) / sizeof(sdrplay_api_DeviceT))) != sdrplay_api_Success)
			{
				sprintf_s(msgbuf, msgbufSize, "sdrplay_api_GetDevices failed %s\n", sdrplay_api_GetErrorString(err));
				OutputDebugStringA(msgbuf);
				sdrplay_api_UnlockDeviceApi();
				SDRFailedInit = true;
			}
			else
			{
				sprintf_s(msgbuf, msgbufSize, "MaxDevs=%d NumDevs=%d\n", (int)(sizeof(SDRDevices) / sizeof(sdrplay_api_DeviceT)), (int)NumberOfSDRDevices);
				OutputDebugStringA(msgbuf);
				if (NumberOfSDRDevices > 0)
				{
					for (int i = 0; i < (int)NumberOfSDRDevices; i++)
					{
						if (SDRDevices[i].hwVer == SDRPLAY_RSPduo_ID)
						{
							sprintf_s(msgbuf, msgbufSize, "Dev%d: SerNo=%s hwVer=%d tuner=0x%.2x rspDuoMode=0x%.2x\n", i, SDRDevices[i].SerNo, SDRDevices[i].hwVer, SDRDevices[i].tuner, SDRDevices[i].rspDuoMode);
							OutputDebugStringA(msgbuf);
						}
						else
						{
							sprintf_s(msgbuf, msgbufSize, "Dev%d: SerNo=%s hwVer=%d tuner=0x%.2x\n", i, SDRDevices[i].SerNo, SDRDevices[i].hwVer, SDRDevices[i].tuner);
							OutputDebugStringA(msgbuf);
						}
					}

					// Get chosen SDR
					chosenDevice = &SDRDevices[SelectedSDR];

					// If chosen device is an RSPduo, assign additional fields
					if (chosenDevice->hwVer == SDRPLAY_RSPduo_ID)
					{
						if (chosenDevice->rspDuoMode & sdrplay_api_RspDuoMode_Master)  // If master device is available, select device as master
						{
							// Select tuner based on user input (or default to TunerA)
							chosenDevice->tuner = sdrplay_api_Tuner_A;

							// Set operating mode
							chosenDevice->rspDuoMode = sdrplay_api_RspDuoMode_Single_Tuner;
						}
					}

					// Select chosen device
					if ((err = sdrplay_api_SelectDevice(chosenDevice)) != sdrplay_api_Success)
					{
						sprintf_s(msgbuf, msgbufSize, "sdrplay_api_SelectDevice failed %s\n", sdrplay_api_GetErrorString(err));
						OutputDebugStringA(msgbuf);
						SDRFailedInit = true;
					}

					// Unlock API now that device is selected
					sdrplay_api_UnlockDeviceApi();

					// Retrieve device parameters so they can be changed if wanted
					if ((err = sdrplay_api_GetDeviceParams(chosenDevice->dev, &deviceParams)) != sdrplay_api_Success)
					{
						sprintf_s(msgbuf, msgbufSize, "sdrplay_api_GetDeviceParams failed %s\n", sdrplay_api_GetErrorString(err));
						OutputDebugStringA(msgbuf);
						SDRFailedInit = true;
					}

					// Check for NULL pointers before changing settings
					if (deviceParams == NULL)
					{
						sprintf_s(msgbuf, msgbufSize, "sdrplay_api_GetDeviceParams returned NULL deviceParams pointer\n");
						OutputDebugStringA(msgbuf);
						SDRFailedInit = true;
					}

					// Configure dev parameters
					if (deviceParams->devParams != NULL)
					{
						// Set sampling frequency
						deviceParams->devParams->fsFreq.fsHz = SamplingFrequency;
					}

					// Configure tuner parameters (depends on selected Tuner which set of parameters to use)
					chParams = (chosenDevice->tuner == sdrplay_api_Tuner_B) ? deviceParams->rxChannelB : deviceParams->rxChannelA;
					if (chParams != NULL)
					{
						// Set center frequency
						chParams->tunerParams.rfFreq.rfHz = 1000000.0 * CenterFreqeuncyInit;
						// Set bandwidth in MHz. For example: 0_200 = 0.200MHz = 200KHz
						//chParams->tunerParams.bwType = sdrplay_api_BW_1_536;
						chParams->tunerParams.bwType = sdrplay_api_BW_5_000;
						// Set IF type
						chParams->tunerParams.ifType = sdrplay_api_IF_Zero;
						// Set LNA state
						chParams->tunerParams.gain.LNAstate = LNAstateInit;
						// Set IF gain reduction in dB
						chParams->tunerParams.gain.gRdB = GainReductionInit;
						// Disable AGC
						chParams->ctrlParams.agc.enable = sdrplay_api_AGC_DISABLE;
						// Set decimation
						if (DecimationEnable)
						{
							chParams->ctrlParams.decimation.enable = 1;
							chParams->ctrlParams.decimation.decimationFactor = DecimationFactor;
							chParams->ctrlParams.decimation.wideBandSignal = 1;
						}
					}
					else
					{
						sprintf_s(msgbuf, msgbufSize, "sdrplay_api_GetDeviceParams returned NULL chParams pointer\n");
						OutputDebugStringA(msgbuf);
						SDRFailedInit = true;
					}

					// Assign callback functions to be passed to sdrplay_api_Init()
					cbFns.StreamACbFn = StreamACallback;
					cbFns.StreamBCbFn = StreamBCallback;
					cbFns.EventCbFn = EventCallback;

					// Now we're ready to start by calling the initialisation function
					// This will configure the device and start streaming
					if ((err = sdrplay_api_Init(chosenDevice->dev, &cbFns, NULL)) != sdrplay_api_Success)
					{
						sprintf_s(msgbuf, msgbufSize, "sdrplay_api_Init failed %s\n", sdrplay_api_GetErrorString(err));
						OutputDebugStringA(msgbuf);
						SDRFailedInit = true;
					}

					// Get system gain
					SystemGain = chParams->tunerParams.gain.gainVals.curr;

					// Fill the cards list
					for (unsigned int i = 0; i < NumberOfSDRDevices; i++)
					{
						string FullName;

						// Build SDR devices name table
						switch (SDRDevices[i].hwVer)
						{
						case SDRPLAY_RSP1_ID:
							FullName = "SDRPLAY RSP1 " + (string)SDRDevices[i].SerNo;
							break;
						case SDRPLAY_RSP2_ID:
							FullName = "SDRPLAY RSP2 " + (string)SDRDevices[i].SerNo;
							break;
						case SDRPLAY_RSPduo_ID:
							FullName = "SDRPLAY RSPduo " + (string)SDRDevices[i].SerNo;
							break;
						case SDRPLAY_RSPdx_ID:
							FullName = "SDRPLAY RSPdx " + (string)SDRDevices[i].SerNo;
							break;
						case SDRPLAY_RSP1A_ID:
//							FullName = "SDRPLAY RSP1A " + (string)SDRDevices[i].SerNo;
							FullName = "SDRPLAY RSP1A";
							break;
						default:
							FullName = "Unknown " + (string)SDRDevices[i].SerNo;
						}

						FullName.copy(DevicesNames[i], FullName.length());
					}

					// Mark that the initialization of SDR is done
					SDRFinishedInit = true;
				}
				else
				{
					sprintf_s(msgbuf, msgbufSize, "sdrplay devices were not detected\n");
					OutputDebugStringA(msgbuf);
					SDRFailedInit = true;
				}
			}
		}

		// Redio reception thread
		void RadioThread()
		{
			// Initialise the SDR
			InitRadio();

			// As long as the variable RadioOn = true, do the reception loop
			while (RadioOn)
			{
				// Update frequency value
				if (UpdateCenterFrequency)
				{
					// Set center frequency
					chParams->tunerParams.rfFreq.rfHz = 1000000.0 * Convert::ToDouble(CenterFreqnumeric->Value);

					// Update controls
					if ((err = sdrplay_api_Update(chosenDevice->dev, chosenDevice->tuner, sdrplay_api_Update_Tuner_Frf, sdrplay_api_Update_Ext1_None)) != sdrplay_api_Success)
					{
						sprintf_s(msgbuf, msgbufSize, "sdrplay_api_Update sdrplay_api_Update_Tuner_Frf failed %s\n", sdrplay_api_GetErrorString(err));
						OutputDebugStringA(msgbuf);

						UpdateFailed = true;
					}
					else
						UpdateFailed = false;

					// Lower update center frequency flag
					UpdateCenterFrequency = false;
				}

				// Update gain reduction and LNA state
				if (UpdateGainReductionAndLNAState)
				{
					// Set IF gain reduction in dB
					chParams->tunerParams.gain.gRdB = Convert::ToInt32(GainRednumeric->Value);

					// Set LNA state
					chParams->tunerParams.gain.LNAstate = Convert::ToInt32(LNAStatenumeric->Value);

					// Update controls
					if ((err = sdrplay_api_Update(chosenDevice->dev, chosenDevice->tuner, sdrplay_api_Update_Tuner_Gr, sdrplay_api_Update_Ext1_None)) != sdrplay_api_Success)
					{
						sprintf_s(msgbuf, msgbufSize, "sdrplay_api_Update sdrplay_api_Update_Tuner_Gr failed %s\n", sdrplay_api_GetErrorString(err));
						OutputDebugStringA(msgbuf);

						UpdateFailed = true;
					}
					else
						UpdateFailed = false;

					// Lower update gain reduction and LNA state flag
					UpdateGainReductionAndLNAState = false;
				}
			}

			// Finished with device so uninitialise it
			if ((err = sdrplay_api_Uninit(chosenDevice->dev)) != sdrplay_api_Success)
			{
				sprintf_s(msgbuf, msgbufSize, "sdrplay_api_Uninit failed %s\n", sdrplay_api_GetErrorString(err));
				OutputDebugStringA(msgbuf);
			}

			// Release device (make it available to other applications)
			sdrplay_api_ReleaseDevice(chosenDevice);

			// Mark end of radio thread
			SDRFinishedInit = true;
		}

		// Audio out thread
		void SpeakersThread()
		{
			HWAVEOUT        HWaveOut;
			WAVEHDR         WaveHeader;
			WAVEFORMATEX    WafeFromatX;
			HANDLE          AudioHandle;

			// Set the format of the waveform sound
			WafeFromatX.wFormatTag = WAVE_FORMAT_PCM;
			// Set the number of channels of the audio file
			WafeFromatX.nChannels = AudioChannels;
			// Set the sample frequency when playing and recording each channel
			WafeFromatX.nSamplesPerSec = (DWORD)CodecSamplingRate;
			// Set the average data transfer rate of the request, in bytes
			WafeFromatX.nAvgBytesPerSec = AudioChannels * 2 * (DWORD)CodecSamplingRate;
			// Set the block alignment in bytes
			WafeFromatX.nBlockAlign = AudioChannels * 2;
			// Set the number of bits per sample
			WafeFromatX.wBitsPerSample = 16;
			//The size of the extra information
			WafeFromatX.cbSize = 0;

			// Create event
			AudioHandle = CreateEvent(NULL, 0, 0, NULL);
			// Open a waveform audio output device for playback
			waveOutOpen(&HWaveOut, WAVE_MAPPER, &WafeFromatX, (DWORD_PTR)AudioHandle, 0L, CALLBACK_EVENT); 

			// Set flags and loops
			WaveHeader.dwFlags = 0L;
			WaveHeader.dwLoops = 1L;

			while (SpeakersOn)
			{
				if (PlayAudio_checkBox->Checked)
				{
					if (PlayedPCMDataSubBuffer < 0)
					{
						// Check if there's enough data in the buffer to start playing the content
						int TempSubBufferNumber = FilledPCMDataSubBuffer - (PCMBufferLength >> 1);
						if (TempSubBufferNumber < 0) TempSubBufferNumber += PCMBufferLength;
						if (PCMDataSubBufferEndingIndex[TempSubBufferNumber] > 0) PlayedPCMDataSubBuffer = TempSubBufferNumber;
					}
					else if (PlayedPCMDataSubBuffer == FilledPCMDataSubBuffer) PlayedPCMDataSubBuffer = -1;

					if (PlayedPCMDataSubBuffer >= 0)
					{
						if (PCMDataSubBufferEndingIndex[PlayedPCMDataSubBuffer] > 0)
						{
							// Get data pointers
							WaveHeader.dwBufferLength = PCMDataSubBufferEndingIndex[PlayedPCMDataSubBuffer];
							WaveHeader.lpData = PCMDataBuffer[PlayedPCMDataSubBuffer];

							// Prepare a waveform data block for playback
							waveOutPrepareHeader(HWaveOut, &WaveHeader, sizeof(WAVEHDR));
							// Set volume
							waveOutSetVolume(HWaveOut, unsigned long (((0xFFFF - Volume_trackBar->Value) << 16) + (0xFFFF - Volume_trackBar->Value)));
							// Play the data
							waveOutWrite(HWaveOut, &WaveHeader, sizeof(WAVEHDR));
							// Wait for playback to end
							WaitForSingleObject(AudioHandle, INFINITE);

							// Zero the PCM data buffer index
							PCMDataSubBufferEndingIndex[PlayedPCMDataSubBuffer] = 0;
						}

						// Advance PlayedPCMDataBuffer
						PlayedPCMDataSubBuffer++;
					}
				}
				// Mark unselected buffer
				PlayedPCMDataSubBuffer = -1;
			}

			// Close the waveform audio output device for playback
			waveOutClose(HWaveOut);

			// Mark end of speakers thread
			SpeakersFinished = true;
		}

	    // Initialize the radio thread
		void InitRadioThread(void)
		{
			RadioOn = true;

			System::Threading::Thread^ t1;
			System::Threading::ThreadStart^ ts = gcnew System::Threading::ThreadStart(this, &MyForm::RadioThread);
			t1 = gcnew System::Threading::Thread(ts);
			t1->Start();
		}

		// Initialize the audio thread
		void InitSpeakersThread(void)
		{
			SpeakersOn = true;

			System::Threading::Thread^ t2;
			System::Threading::ThreadStart^ tspk = gcnew System::Threading::ThreadStart(this, &MyForm::SpeakersThread);
			t2 = gcnew System::Threading::Thread(tspk);
			t2->Start();
		}

		// Initialize the XY chart (in the form)
		void InitChart(void)
		{
			// Default Color
			XYchart->Series[0]->Color = Color::DarkBlue;
			// Chart Type
			XYchart->Series[0]->ChartType = SeriesChartType::Line; // SeriesChartType::Column;
			// Set chart y min and max
			XYchart->ChartAreas[0]->RecalculateAxesScale();
			XYchart->ChartAreas[0]->AxisY->Minimum = -20000;
			XYchart->ChartAreas[0]->AxisY->Maximum = 20000;
		}

		// Initialize the cyclic form timer, to perform cyclic operations.
		void InitInnerTimer(int TickIntervals)
		{
			this->components = (gcnew System::ComponentModel::Container());
			this->InnerTimer = (gcnew System::Windows::Forms::Timer(this->components));
			this->InnerTimer->Enabled = true;
			this->InnerTimer->Interval = TickIntervals;
			this->InnerTimer->Tick += gcnew System::EventHandler(this, &MyForm::InnerTimer_Tick);
		}

	// Form cyclic operations functions
private: System::Void InnerTimer_Tick(System::Object^  sender, System::EventArgs^  e) {

	// Set setup fail flag
	if (UpdateFailed)
	{
		RadioInit_textBox->Text = "Setup Failed";
		RadioInit_textBox->BackColor = System::Drawing::Color::Red;
	}
	else
	{
		RadioInit_textBox->Text = "Setup OK";
		RadioInit_textBox->BackColor = System::Drawing::Color::LightGreen;
	}

	// Set overload flag
	if (OverloadDetected)
	{
		Overload_textBox->Text = "Overload";
		Overload_textBox->BackColor = System::Drawing::Color::Red;
	}
	else
	{
		Overload_textBox->Text = "";
		Overload_textBox->BackColor = System::Drawing::Color::White;
	}

	// Set system gain value
	SysGain_textBox->Text = SystemGain.ToString("N1");

	// Clear old values and adding new DataPoints
	if (RxDataEndingIndex > RxDataStartingIndex)
	{
		XYchart->Series[0]->Points->Clear();

		for (int j = 0; j < 1000; j++)
			XYchart->Series[0]->Points->Add(Decimal::ToDouble(RxiData[RxDataStartingIndex + j]));

		SamplesPerIteration_textBox->Text = Convert::ToString(RxDataEndingIndex - RxDataStartingIndex);
	}

	// Get present EngindIndex
	unsigned int CurrentRxDataEndingIndex = RxDataEndingIndex;
	PreviousRxBitsEndingIndex = RxBitsEndingIndex;

	unsigned int EndIndex;

	// Check if the RxData buffer wrapped. 
	// If not - a single calculation iteration will be perfromed.
	// If so - Perform an iteration from present position to the end of the buffer.
	//         A second iteration on the rest of the data shall be initiated.
	bool PerformIteration = true;
	bool InitAveragingFilter = true;
	if (CurrentRxDataEndingIndex >= RxDataStartingIndex)
		EndIndex = CurrentRxDataEndingIndex;
	else
		EndIndex = RxBufferSize;

	// Process the received data
	while (PerformIteration)
	{
		// Process received data
		for (unsigned int j = RxDataStartingIndex; j < EndIndex; j++)
		{
			// Calculate the absolute value of the I-Q stream. Define it as the input value.
			float xi = (float)RxiData[j];
			float xq = (float)RxqData[j];
			float InputValue = sqrt(xi*xi + xq*xq);

			// Apply a moving average in the input value
			float MovingAverageIncomingValue = InputValue / (float)MovingAverageLength;
			float MovingAverageLastValue;
			if (MovingAverageIndex == 0)
			{
				MovingAverageLastValue = MovingAverageBuffer[MovingAverageLength - 1];
			}
			else
			{
				MovingAverageLastValue = MovingAverageBuffer[MovingAverageIndex - 1];
			}

			// Apply 2nd order butterworth filter on the output of the moving average
			SecondButterLPFxn += MovingAverageIncomingValue - MovingAverageLastValue;
			if (SecondButterLPFxn < 0) SecondButterLPFxn = 0;
			MovingAverageBuffer[MovingAverageIndex] = MovingAverageIncomingValue;
			MovingAverageIndex++;
			
			if (MovingAverageIndex == MovingAverageLength)
			{
				MovingAverageIndex = 0;
			}

			SecondButterLPFyn_2 = SecondButterLPFyn_1;
			SecondButterLPFyn_1 = SecondButterLPFyn;
			SecondButterLPFyn = SecondButterLPFb0 * SecondButterLPFxn +
				SecondButterLPFb1 * SecondButterLPFxn_1 + SecondButterLPFb2 * SecondButterLPFxn_2 +
				SecondButterLPFa1 * SecondButterLPFyn_1 + SecondButterLPFa2 * SecondButterLPFyn_2;
			SecondButterLPFxn_2 = SecondButterLPFxn_1;
			SecondButterLPFxn_1 = SecondButterLPFxn;

			// Apply a very slow LPF on the filtered data, to extract the DC part of the signal
			DCSlowFilteredData += SlowLPFAlpha * (SecondButterLPFyn - DCSlowFilteredData);

			// Apply a very slow LPF on the filtered data above the DC part, to get the maximum part of the signal
			if (SecondButterLPFyn > DCSlowFilteredData)
			{
				PeakSlowFilteredData += SlowLPFAlpha * (SecondButterLPFyn - PeakSlowFilteredData);
				LowBitDetectionValue = (PeakSlowFilteredData - FloorSlowFilteredData ) * BitDetectionLowThreshold + FloorSlowFilteredData;
				HighBitDetectionValue = (PeakSlowFilteredData - FloorSlowFilteredData ) * BitDetectionHighThreshold + FloorSlowFilteredData;
			}
			else
			// Apply a very slow LPF on the filtered data below the DC part, to get the floor level of the signal
			{
				FloorSlowFilteredData += SlowLPFAlpha * (SecondButterLPFyn - FloorSlowFilteredData);
			}

			// Check if the current bit state had changed
			if (SecondButterLPFyn > HighBitDetectionValue)
			{
				if (CurrentBitState == false)
				{
					// Change the current bit state to true
					CurrentBitState = true;
					
					// Zero the current state length counter
					CurrentStateLength = 0;
				}
			}
			else if (SecondButterLPFyn < LowBitDetectionValue)
			{
				if (CurrentBitState)
				{
					// Change the current bit state to false
					CurrentBitState = false;

					// Zero the current state length counter
					CurrentStateLength = 0;
				}
			}

			// Advance the bit length counters
			CurrentStateLength += 1;
			RecoveredStateLength += 1;

			// Check if a new valid bit is detected if so - Set the value of the recovered bit length
			if (CurrentStateLength == MinimumBitLengthThreshold)
			{
				// Store present bit
				if (RecoveredBitState & (CurrentBitState == false))
				{
					PrevRecoveredStateLength = RecoveredStateLength;
					RxBitsArrayLength[RxBitsEndingIndex] = RecoveredStateLength;
					RxBitsArrayLengthTimeTag[RxBitsEndingIndex] = RxTimeTag[j];
					RxBitsEndingIndex++;

					// Check if RxBitsEndingIndex reached the end of the buffer. If so - zero it.
					if (RxBitsEndingIndex >= RxBitsArraySize) RxBitsEndingIndex = 0;

					// Zero recovered state length counter
					RecoveredStateLength = 0;
				}
				// If bit changed from 0 to 1 then zero recovered state length counter
				if ((RecoveredBitState == false) & CurrentBitState) RecoveredStateLength = 0;

				// Set recovered bit state
				RecoveredBitState = CurrentBitState;
			}

			// If SaveSample checkbox is checked then save the data memory
			if (RecordFlag)
			{
				SaveDataBuffer[SaveDataIndex] = (int) InputValue;
				SaveDataIndex++;
				SaveDataBuffer[SaveDataIndex] = (int) (20 * SecondButterLPFyn);
				SaveDataIndex++;
				if (CurrentBitState)
					SaveDataBuffer[SaveDataIndex] = (int)(10000);
				else
					SaveDataBuffer[SaveDataIndex] = (int)(0);
				SaveDataIndex++;
				if (RecoveredBitState)
					SaveDataBuffer[SaveDataIndex] = (int)(10000);
				else
					SaveDataBuffer[SaveDataIndex] = (int)(0);
				SaveDataIndex++;
				SaveDataBuffer[SaveDataIndex] = (int)(100 * RecoveredStateLength);
				SaveDataIndex++;
			}
		}

		// Check if a another iteration is required
		if (EndIndex == RxBufferSize)
		{
			RxDataStartingIndex = 0;
			EndIndex = CurrentRxDataEndingIndex;
		}
		else
			PerformIteration = false;
	}

	// Get size of data stored in RxBitsArrayLength.
	int AvailableSymbols;
	if (RxBitsEndingIndex >= RxBitsStartingIndex)
	{
		AvailableSymbols = RxBitsEndingIndex - RxBitsStartingIndex;
	}
	else
	{
		AvailableSymbols = RxBitsArraySize - RxBitsStartingIndex + RxBitsEndingIndex;
	}

	Symbols_textBox->Text = Convert::ToString(AvailableSymbols);

	// Check if there are enough symbols to process. If so - process the data
	if (AvailableSymbols > 2 * RawPacketLength * SymbolsPerByte)
	{
		unsigned int i = RxBitsStartingIndex;

		int SymbolsCount = 0;
		// Process the symbols into packets
		while(SymbolsCount < (AvailableSymbols - 2 * RawPacketLength * SymbolsPerByte))
		{
			// Declare inner index
			i = RxBitsStartingIndex;
			if (++RxBitsStartingIndex >= RxBitsArraySize) RxBitsStartingIndex = 0;
			SymbolsCount++;
			PacketSearchCounter++;

			// Get time tag of the potential packet
			unsigned int CurrentTimeTag = RxBitsArrayLengthTimeTag[i];

			// Search for message header's bytes 2, 3 and 4
			// The software assumes the bit length may vary, so it first looks for the ratio between the MSB and LSB parts of the header
			bool HeaderBytes234TestPassed = true;
			int HeaderBytes234Array[12];
			int SymbolsSum = 0;
			int MatchCount = 0;
			if (SymbolsPerByte == 4)
			{
				// 4 symbols per byte / 2 bits per symbol
				for (int HeaderIndex = 0; HeaderIndex < 6; HeaderIndex++)
				{
					// Read 2 bits pairs length
					int TempPairMSP = RxBitsArrayLength[i];
					if (++i >= RxBitsArraySize) i = 0;
					int TempPairLSP = RxBitsArrayLength[i];
					if (++i >= RxBitsArraySize) i = 0;

					// Read 2 bits pairs from the header expected array
					int TempTxHeaderMSP = TxHeader234PairsArray[HeaderIndex << 1];
					int TempTxHeaderLSP = TxHeader234PairsArray[(HeaderIndex << 1) + 1];

					HeaderBytes234Array[HeaderIndex << 1] = TempPairMSP;
					HeaderBytes234Array[(HeaderIndex << 1) + 1] = TempPairLSP;
					SymbolsSum = SymbolsSum + TempPairMSP + TempPairLSP;

					// Check pairs ratio
					int HeaderRatioResidue;
					if (TempTxHeaderMSP <= TempTxHeaderLSP)
						HeaderRatioResidue = abs((1000 * TempPairMSP) / TempPairLSP - (1000 * TempTxHeaderMSP) / TempTxHeaderLSP);
					else
						HeaderRatioResidue = abs((1000 * TempPairLSP) / TempPairMSP - (1000 * TempTxHeaderLSP) / TempTxHeaderMSP);

					if (HeaderRatioResidue < HeaderRatioResidueThreshold) MatchCount++;
					else HeaderBytes234TestPassed = false;
				}

				MatchCount = MatchCount >> 1;
			}
			else
			{
				// 2 symbols per byte / 4 bits per symbol
				for (int HeaderIndex = 0; HeaderIndex < 3; HeaderIndex++)
				{
					// Read 2 nibbles length
					int TempMSN = RxBitsArrayLength[i];
					if (++i >= RxBitsArraySize) i = 0;
					int TempLSN = RxBitsArrayLength[i];
					if (++i >= RxBitsArraySize) i = 0;

					// Read 2 nibbles from the header expected array
					int TempTxHeaderMSN = TxHeader234NibblesArray[HeaderIndex << 1];
					int TempTxHeaderLSN = TxHeader234NibblesArray[(HeaderIndex << 1) + 1];

					HeaderBytes234Array[HeaderIndex << 1] = TempMSN;
					HeaderBytes234Array[(HeaderIndex << 1) + 1] = TempLSN;
					SymbolsSum = SymbolsSum + TempMSN + TempLSN;

					// Check nibbles ratio
					int HeaderRatioResidue;
					if (TempTxHeaderMSN <= TempTxHeaderLSN)
						HeaderRatioResidue = abs((1000 * TempMSN) / TempLSN - (1000 * TempTxHeaderMSN) / TempTxHeaderLSN);
					else
						HeaderRatioResidue = abs((1000 * TempLSN) / TempMSN - (1000 * TempTxHeaderLSN) / TempTxHeaderMSN);

					if (HeaderRatioResidue < HeaderRatioResidueThreshold) MatchCount++;
					else HeaderBytes234TestPassed = false;
				}
			}

			int LocalBitRatio;
			// If the ratio test passed, check the bits pairs value
			if (MatchCount >= 2)
			{
				MatchCount = 0;

				if (SymbolsPerByte == 4)
				{
					// 4 symbols per byte / 2 bits per symbol
					if (HeaderBytes234TestPassed)
					{
						// If the ratios are correct, then the software calculate the expected bit length * 16 and check for the correct values
						LocalBitRatio = (SymbolsSum << 4) / Header234PairsLenghtSum;

						// If the bit ratio array is full (averaging is ready), use the average bit ratio instead of local bit ratio
						// Otherwise, use local bit ratio as bit ratio
						if (BitRatioAverageFlag) BitRatio = LocalBitRatio;
					}
					else LocalBitRatio = BitRatio;

					if ((HeaderBytes234TestPassed) | (BitRatioAverageFlag == false))
					{
						for (int HeaderIndex = 0; HeaderIndex < 12; HeaderIndex++)
						{
							// Check pairs values
							if (((HeaderBytes234Array[HeaderIndex] << 4) + (BitRatio >> 1)) / BitRatio == TxHeader234PairsArray[HeaderIndex]) MatchCount++;
						}
					}

					MatchCount = MatchCount >> 2;
				}
				else
				{
					// 2 symbols per byte / 4 bits per symbol
					if (HeaderBytes234TestPassed)
					{
						// If the ratios are correct, then the software calculate the expected bit length * 16 and check for the correct values
						LocalBitRatio = (SymbolsSum << 4) / Header234NibblesLenghtSum;

						// If the bit ratio array is full (averaging is ready), use the average bit ratio instead of local bit ratio
						// Otherwise, use local bit ratio as bit ratio
						if (BitRatioAverageFlag) BitRatio = LocalBitRatio;
					}
					else LocalBitRatio = BitRatio;

					if ((HeaderBytes234TestPassed) | (BitRatioAverageFlag == false))
					{
						for (int HeaderIndex = 0; HeaderIndex < 6; HeaderIndex++)
						{
							// Check pairs values
							if (((HeaderBytes234Array[HeaderIndex] << 4) + (BitRatio >> 1)) / BitRatio == TxHeader234NibblesArray[HeaderIndex]) MatchCount++;
						}
					}

					MatchCount = MatchCount >> 1;
				}

				if (MatchCount == 3) HeaderBytes234TestPassed = true;
				else if ((MatchCount == 2) & (BitRatioAverageFlag == false)) HeaderBytes234TestPassed = true;
					 else HeaderBytes234TestPassed = false;
			}

			// If the header bits pairs test passed process the packet bits
			if (HeaderBytes234TestPassed)
			{
				unsigned int RecoveredChkSum = 0;
				
				int RecoveredPacketLength;
				if (useFEC) RecoveredPacketLength = FECInDataSize;
				else RecoveredPacketLength = PacketLength;

				// Recover packet
				for (int j = 0; j < RecoveredPacketLength; j++)
				{
					int RxByte;
					if (SymbolsPerByte == 4)
					{
						// 4 symbols per byte / 2 bits per symbol
						RxByte = (((RxBitsArrayLength[i] << 4) + (BitRatio >> 1)) / BitRatio) - ZeroOffset;
						RxByte = RxByte < 0 ? 0 : RxByte;
						RxByte = RxByte > 3 ? 3 : RxByte;
						int TempMSB = RxByte << 6;
						if (++i >= RxBitsArraySize) i = 0;
						RxByte = (((RxBitsArrayLength[i] << 4) + (BitRatio >> 1)) / BitRatio) - ZeroOffset;
						RxByte = RxByte < 0 ? 0 : RxByte;
						RxByte = RxByte > 3 ? 3 : RxByte;
						TempMSB |= RxByte << 4;
						if (++i >= RxBitsArraySize) i = 0;
						RxByte = (((RxBitsArrayLength[i] << 4) + (BitRatio >> 1)) / BitRatio) - ZeroOffset;
						RxByte = RxByte < 0 ? 0 : RxByte;
						RxByte = RxByte > 3 ? 3 : RxByte;
						TempMSB |= RxByte << 2;
						if (++i >= RxBitsArraySize) i = 0;
						RxByte = (((RxBitsArrayLength[i] << 4) + (BitRatio >> 1)) / BitRatio) - ZeroOffset;
						RxByte = RxByte < 0 ? 0 : RxByte;
						RxByte = RxByte > 3 ? 3 : RxByte;
						RxByte |= TempMSB;
						if (++i >= RxBitsArraySize) i = 0;
					}
					else
					{
						// 2 symbols per byte / 4 bits per symbol
						RxByte = (((RxBitsArrayLength[i] << 4) + (BitRatio >> 1)) / BitRatio) - ZeroOffset;
						RxByte = RxByte < 0 ? 0 : RxByte;
						RxByte = RxByte > 15 ? 15 : RxByte;
						int TempMSB = RxByte << 4;
						if (++i >= RxBitsArraySize) i = 0;
						RxByte = (((RxBitsArrayLength[i] << 4) + (BitRatio >> 1)) / BitRatio) - ZeroOffset;
						RxByte = RxByte < 0 ? 0 : RxByte;
						RxByte = RxByte > 15 ? 15 : RxByte;
						RxByte |= TempMSB;
						if (++i >= RxBitsArraySize) i = 0;
					}

					// if FEC is used - store the data in the FEC data buffers
					// Otherwise - store the data in the reception buffer
					if (useFEC) recd[j] = (int)RxByte;
					else
					{
						RxPacketData[j] = (byte)RxByte;
						RecoveredChkSum += RxByte;
					}
				}
				
				// Debug Debug Debug
				if (useFEC) memcpy(Debugrecd, recd, FECInDataSize * sizeof(int));

				unsigned int ChkSum = 0;
				// Check if FEC is used. If so - perform error correction
				if (useFEC)
				{
					// Apply error correction, build reception array and calculate checksum
					for (int j = 0; j < nn; j++) recd[j] = index_of[recd[j]]; // Put recd[j] into index form

				    // Decode recv[]
					decode_rs(); // recd[] is returned in polynomial form

					const int recdDataOffset = nn - kk;
					for (int j = 0; j < PacketLength; j++)
					{
						byte RxByte = (byte) recd[j + recdDataOffset];
						RxPacketData[j] = RxByte;
						RecoveredChkSum += RxByte;
					}

					// Get ChkSum value
					ChkSum = (((byte)recd[PacketLength + recdDataOffset]) << 8) | ((byte)recd[PacketLength + recdDataOffset + 1]);
				}
				else
				{
					// Get ChkSum value
					for (int j = 0; j < (2 * SymbolsPerByte); j++)
					{
						int RxByte = ((((RxBitsArrayLength[i] << 4) + (BitRatio >> 1)) / BitRatio) - ZeroOffset);
						if (++i >= RxBitsArraySize) i = 0;
						if (SymbolsPerByte == 4)
						{
							// 4 symbols per byte / 2 bits per symbol
							ChkSum = (ChkSum << 2) | RxByte;
						}
						else
						{
							// 2 symbols per byte / 4 bits per symbol
							ChkSum = (ChkSum << 4) | RxByte;
						}
					}
				}

				// Check ChkSum
				if (RecoveredChkSum == ChkSum)
				{
					// Advance the good packet counter
					GoodPacketsCounter++;

					// Reduce the packet search counter by SymbolsPerByte minus 1 (since a good packet was detected)
					if (PacketSearchCounter > SymbolsPerByte) PacketSearchCounter -= SymbolsPerByte + 1;

					// Set RxBitsStartingIndex according to i
					RxBitsStartingIndex = i;
					if (RxBitsStartingIndex >= RxBitsArraySize) RxBitsStartingIndex = RxBitsStartingIndex - RxBitsArraySize;

					// Reduce symbols counter by 1 since it was advanced at the beginning of the routine + SymbolsPerByte to skip header's 1st byte symbols
					if (useFEC) SymbolsCount += FECRawPacketLength * SymbolsPerByte + 1 - SymbolsPerByte;
					else SymbolsCount += RawPacketLength * SymbolsPerByte + 1 - SymbolsPerByte;

					// Check if the packet is first in a cycle and the present buffer contains data.
					// If so advance to the next storage buffer and calculate data rate and set the time tag
					if (((RxPacketData[0] % PacketsPerSecond) == 0) & (PCMDataSubBufferEndingIndex[FilledPCMDataSubBuffer] > 0))
					{
						// Advance to the next storage buffer and zero it end index
						FilledPCMDataSubBuffer++;
						if (FilledPCMDataSubBuffer == PCMBufferLength) FilledPCMDataSubBuffer = 0;
						PCMDataSubBufferEndingIndex[FilledPCMDataSubBuffer] = 0;

						// Calculate data rate
						DataRate = (float)((PacketsPerSecond * (PacketLength - 1) - ReservedBytesPerSecond) * 8) / (float)(CurrentTimeTag - CycleZeroTimeTag);

						// Set the zero packet time tag
						CycleZeroTimeTag = CurrentTimeTag;
					}
					else
					{
						if (PCMDataSubBufferEndingIndex[FilledPCMDataSubBuffer] == 0)
							// Set the zero packet time tag
							CycleZeroTimeTag = CurrentTimeTag;
					}

					// Calculate bit ratio moving average
					int BitRatioMovingAverageIncomingValue = LocalBitRatio / BitRatioMovingAverageLength;
					int BitRatioMovingAverageLastValue;
					if (BitRatioMovingAverageIndex == 0)
					{
						BitRatioMovingAverageLastValue = BitRatioMovingAverageBuffer[BitRatioMovingAverageLength - 1];
					}
					else
					{
						BitRatioMovingAverageLastValue = BitRatioMovingAverageBuffer[BitRatioMovingAverageIndex - 1];
					}

					if (BitRatioAverageFlag == false) BitRatio += BitRatioMovingAverageIncomingValue - BitRatioMovingAverageLastValue;
					BitRatioMovingAverageBuffer[BitRatioMovingAverageIndex] = BitRatioMovingAverageIncomingValue;
					BitRatioMovingAverageIndex++;

					if (BitRatioMovingAverageIndex == BitRatioMovingAverageLength)
					{
						BitRatio = 0;
						for (int j = 0; j < BitRatioMovingAverageLength; j++) BitRatio += BitRatioMovingAverageBuffer[j];
						BitRatioMovingAverageIndex = 0;
						BitRatioAverageFlag = false;
					}

					// Check if the packet number advanced by one.
					// If not - fill the gap, up to an amount defined by PacketDiffThreshold
					int PacketDiff = RxPacketData[0] - LastPacket;
					if (PacketDiff < 0) PacketDiff += PacketPerUnsignedCharThreshold + 1;
					if (PacketDiff > 1)
					{
						// Add lost packets to lost packets counter
						LostPacketsCounter += PacketDiff;

						// Limit PacketDiff toPacketDiffThreshold
						if (PacketDiff > PacketDiffThreshold) PacketDiff = PacketDiffThreshold;

						// Calculate number of missing bytes
						unsigned int SamplesNumber = PacketDiff * (PacketLength - 1);
						// Check if the last packet is missing. If so - add reserved bytes number
						if (((LastPacket % PacketsPerSecond) < (PacketsPerSecond - 1)) &
							((RxPacketData[0] % PacketsPerSecond) >=0 ))
							SamplesNumber += ReservedBytesPerSecond;
						// Calculate number of missing samples
						SamplesNumber = ( SamplesNumber * 8 ) / CodecCompression;

						// Fill the missing samples with zeros and calculate the new end index
						fill_n(&PCMDataBuffer[FilledPCMDataSubBuffer][PCMDataSubBufferEndingIndex[FilledPCMDataSubBuffer]], AudioChannels * SamplesNumber, 0);
						PCMDataSubBufferEndingIndex[FilledPCMDataSubBuffer] += SamplesNumber;
					}

					// Turn the data to PCM
					int ShiftIndex = 0;
					unsigned int TempBitReg = (RxPacketData[1] << 8) | RxPacketData[2];
					int RxPacketDataIndex = 2;
					int SamplesNumber;
					// If the last packet in a packet cycle is turned to PCM, reduce the reserved bytes.
					if ((RxPacketData[0] % PacketsPerSecond) == (PacketsPerSecond - 1))
						SamplesNumber = (((PacketLength - 1 - ReservedBytesPerSecond) << 3) / CodecCompression);
					else
						SamplesNumber = (((PacketLength - 1) << 3) / CodecCompression);
					for (int j = 0; j < SamplesNumber; j++)
					{
						//Extract compressed data
						TempBitReg = (TempBitReg << CodecCompression) & ReceptionMask;
						int EncodedData = TempBitReg >> 16;
						ShiftIndex += CodecCompression;

						// Check if at least 8 bits were filled
						if (ShiftIndex >= 8)
						{
							// Set ShiftIndex
							ShiftIndex -= 8;
							// Add 8 bits of incoming data to the 8 LSB of TempBitReg
							TempBitReg |= (RxPacketData[++RxPacketDataIndex] << ShiftIndex);
						}

						// Decode the data
						short int DecodedData;
						switch (CodecCompression)
						{
						case 3:
							DecodedData = g726_24_decoder(EncodedData, AUDIO_ENCODING_LINEAR, &G726StatePointer);
							break;
						case 4:
							DecodedData = g726_32_decoder(EncodedData, AUDIO_ENCODING_LINEAR, &G726StatePointer);
							break;
						case 5:
							DecodedData = g726_40_decoder(EncodedData, AUDIO_ENCODING_LINEAR, &G726StatePointer);
							break;
						default: // Assuming 2 bits per channel was selected (16,000 bps)
							DecodedData = g726_16_decoder(EncodedData, AUDIO_ENCODING_LINEAR, &G726StatePointer);
						}

						// Store decoded data
						PCMDataBuffer[FilledPCMDataSubBuffer][PCMDataSubBufferEndingIndex[FilledPCMDataSubBuffer]] = DecodedData & 0xFF;
						PCMDataBuffer[FilledPCMDataSubBuffer][PCMDataSubBufferEndingIndex[FilledPCMDataSubBuffer] + 1] = (DecodedData >> 8) & 0xFF;
						PCMDataSubBufferEndingIndex[FilledPCMDataSubBuffer] += 2;
						if (AudioChannels == 2)
						{
							PCMDataBuffer[FilledPCMDataSubBuffer][PCMDataSubBufferEndingIndex[FilledPCMDataSubBuffer]] = DecodedData & 0xFF;
							PCMDataBuffer[FilledPCMDataSubBuffer][PCMDataSubBufferEndingIndex[FilledPCMDataSubBuffer] + 1] = (DecodedData >> 8) & 0xFF;
							PCMDataSubBufferEndingIndex[FilledPCMDataSubBuffer] += 2;
						}
					}

					// Set LastPacket value
					LastPacket = RxPacketData[0];
				}
				else
				{
					chksumFails++;

					//if (RecordFlag)
					//{
						memcpy(DebugRxPacketData, RxPacketData, FECOutDataSize);
						memcpy(DebugFECInData, Debugrecd, FECInDataSize * sizeof(int));
						if (i > DebugRxBitsArraySize) memcpy(DebugRxBitsArrayLength,
															 RxBitsArrayLength + i - DebugRxBitsArraySize * sizeof(int),
															 DebugRxBitsArraySize * sizeof(int));
						else memset(DebugRxBitsArrayLength, 0, DebugRxBitsArraySize * sizeof(int));
					//}
				}
			}
		}
	}

	// Check if a timeout period had passed and no new packets were received. If so - reset the BitRatio averaging mechanism.
	CurrentTime = high_resolution_clock::now();
	CurrentTimeMinusStartTime = CurrentTime - StartTime;
	unsigned int TimeDeltaToCycleZeroTimeTag = Convert::ToUInt32(CurrentTimeMinusStartTime.count()) - CycleZeroTimeTag;
	if ((BitRatioAverageFlag == false) & (TimeDeltaToCycleZeroTimeTag > BitRatioResetTime)) BitRatioAverageFlag = true;

	// Check if a buffer is partially filled and its filling time had timeout
	// If so - start a new buffer
	if ( (TimeDeltaToCycleZeroTimeTag > SilenseTimeout) & (PCMDataSubBufferEndingIndex[FilledPCMDataSubBuffer] > 0))
	{
		// Advance to the next storage buffer and zero its end index
		FilledPCMDataSubBuffer++;
		if (FilledPCMDataSubBuffer == PCMBufferLength) FilledPCMDataSubBuffer = 0;
		PCMDataSubBufferEndingIndex[FilledPCMDataSubBuffer] = 0;
	}

	// DebugLine
	//sprintf_s(DebugLine, msgbufSize, "Input buffer number = %d   Output buffer number = %d", FilledPCMDataSubBuffer, PlayedPCMDataSubBuffer);
	sprintf_s(DebugLine, msgbufSize, "BitRatio = %f     chksumFails = %d", (float)BitRatio/(float)16.0, chksumFails);

	// If SaveSample checkbox is checked then uncheck it to save the WAV file
	if (RecordFlag)
	{
		// Clear the record flag
		RecordFlag = false;

		// Clear SaveSample checkbox
		SaveSample_checkBox->Checked = false;
	}

	// Revise starting index
	RxDataStartingIndex = CurrentRxDataEndingIndex;

	// Update counters display
	GoodPackets_textBox->Text = Convert::ToString(GoodPacketsCounter);
	LostPackets_textBox->Text = Convert::ToString(LostPacketsCounter);
	if ((GoodPacketsCounter + LostPacketsCounter) > 0) GoodPacketsRatio_textBox->Text =
		   Convert::ToString((float)((1000 * GoodPacketsCounter) / (GoodPacketsCounter + LostPacketsCounter))/10.0);
	FailedSearches_textBox->Text = Convert::ToString(PacketSearchCounter);
	DataRate_textBox->Text = Convert::ToString(ceil(100.0 * DataRate)/100.0);
	Debug_textBox->Text = gcnew String(DebugLine);
}

private: System::Void CenterFreqnumeric_ValueChanged(System::Object^  sender, System::EventArgs^  e) {
	if (SDRFinishedInit) UpdateCenterFrequency = true;
}

private: System::Void GainRednumeric_ValueChanged(System::Object^  sender, System::EventArgs^  e) {
	if (SDRFinishedInit) UpdateGainReductionAndLNAState = true;
}

private: System::Void LNAStatenumeric_ValueChanged(System::Object^  sender, System::EventArgs^  e) {
	if (SDRFinishedInit) UpdateGainReductionAndLNAState = true;
}
private: System::Void SaveSample_checkBox_CheckedChanged(System::Object^  sender, System::EventArgs^  e) {
	if (SaveSample_checkBox->Checked)
	{
		//memset(DebugFECInData, 0, FECInDataSize * sizeof(int));
		//memset(DebugRxPacketData, 0 , FECOutDataSize);
		RecordFlag = true;
	}
	else
	{
		// Open WAV file
		ofstream WAVfile;
		WAVfile.open("sample.wav", ios::binary);

		// Write the file headers
		WAVfile << "RIFF----WAVEfmt ";     // (chunk size to be filled in later)
		// Write extension data bytes - no extension data
		for (int t = 0; t < 4; t++) WAVfile.put((16 >> (t * 8)) & 0xFF);
		// Write PCM integer samples bytes
		for (int t = 0; t < 2; t++) WAVfile.put((1 >> (t * 8)) & 0xFF);
		// Write number of channels bytes
		for (int t = 0; t < 2; t++) WAVfile.put((NumberOfChannels >> (t * 8)) & 0xFF);
		// Write sampling rate
		for (int t = 0; t < 4; t++) WAVfile.put(((int)SoftwareSamplingRate >> (t * 8)) & 0xFF);
		// Write audio data rate in Hz
		int AudioDataRate = ((int)SoftwareSamplingRate * 16 * NumberOfChannels) >> 3;
		for (int t = 0; t < 4; t++) WAVfile.put((AudioDataRate >> (t * 8)) & 0xFF);
		// Write data block size (size of two integer samples, one for each channel, in bytes)
		for (int t = 0; t < 2; t++) WAVfile.put(((2 * NumberOfChannels) >> (t * 8)) & 0xFF);
		// Write number of bits per sample
		for (int t = 0; t < 2; t++) WAVfile.put((16 >> (t * 8)) & 0xFF);

		// Write the data chunk header
		size_t data_chunk_pos = WAVfile.tellp();
		WAVfile << "data----";

		// Save the data
		for (int s = 0; s < SaveDataIndex; s++)
		{
			WAVfile.put(SaveDataBuffer[s] & 0xFF);
			WAVfile.put((SaveDataBuffer[s] >> 8) & 0xFF);
		}

		// Fix the data chunk header to contain the data size
		size_t file_length = WAVfile.tellp();
		WAVfile.seekp(data_chunk_pos + 4);
		for (int t = 0; t < 4; t++) WAVfile.put(((file_length - data_chunk_pos + 8) >> (t * 8)) & 0xFF);

		// Fix the file header to contain the proper RIFF chunk size, which is (file size - 8) bytes
		WAVfile.seekp(0 + 4);
		for (int t = 0; t < 4; t++) WAVfile.put(((file_length - 8) >> (t * 8)) & 0xFF);

		// Zero the data index
		SaveDataIndex = 0;

		// Start a CSV file
		ofstream csvf;
		csvf.open("sample.csv");

		// Write headline
		csvf << "RxBitsArrayLength\n";

		// Save the data
		unsigned int i = PreviousRxBitsEndingIndex;
		while (i < RxBitsEndingIndex)
		{
			csvf << std::to_string(RxBitsArrayLength[i]) + "\n";
			i++;
		}

		// Write headline
		csvf << "Bad chksum RxBitsArrayLength\n";

		// Save the data
		for (int j = 0; j < DebugRxBitsArraySize; j++) csvf << std::to_string(DebugRxBitsArrayLength[j]) + "\n";

		// Write headline
		csvf << "Bad chksum FECInData\n";

		// Save the data
		for (int j = 0; j < FECInDataSize; j++) csvf << std::to_string((byte)DebugFECInData[j]) + "\n";

		// Write headline
		csvf << "Bad chksum RxPacketData\n";

		// Save the data
		for (int j = 0; j < FECOutDataSize; j++) csvf << std::to_string(DebugRxPacketData[j]) + "\n";

		// Close CSV file
		csvf.close();
	}
}
private: System::Void ClearNumbers_checkBox_CheckedChanged(System::Object^  sender, System::EventArgs^  e) {
	if (ClearNumbers_checkBox->Checked)
	{
		GoodPacketsCounter = 0;
		LostPacketsCounter = 0;
		PacketSearchCounter = 0;
		ClearNumbers_checkBox->Checked = false;
	}
}
private: System::Void SaveWAV_CheckedChanged(System::Object^  sender, System::EventArgs^  e) {
	if (SaveWAV_checkBox->Checked)
		SaveWAV_checkBox->Checked = false;
	else
	{
		// Open WAV file
		ofstream WAVfile;
		WAVfile.open("Short_WAV.wav", ios::binary);

		// Write the file headers
		WAVfile << "RIFF----WAVEfmt ";     // (chunk size to be filled in later)
		// Write extension data bytes - no extension data
		for (int t = 0; t < 4; t++) WAVfile.put((16 >> (t * 8)) & 0xFF);
		// Write PCM integer samples bytes
		for (int t = 0; t < 2; t++) WAVfile.put((1 >> (t * 8)) & 0xFF);
		// Write number of channels bytes
		for (int t = 0; t < 2; t++) WAVfile.put((AudioChannels >> (t * 8)) & 0xFF);
		// Write sampling rate
		for (int t = 0; t < 4; t++) WAVfile.put(((int)8000 >> (t * 8)) & 0xFF);
		// Write audio data rate in Hz
		int AudioDataRate = ((int)8000 * 16 * AudioChannels) >> 3;
		for (int t = 0; t < 4; t++) WAVfile.put((AudioDataRate >> (t * 8)) & 0xFF);
		// Write data block size (size of two integer samples, one for each channel, in bytes)
		for (int t = 0; t < 2; t++) WAVfile.put(((2 * AudioChannels) >> (t * 8)) & 0xFF);
		// Write number of bits per sample
		for (int t = 0; t < 2; t++) WAVfile.put((16 >> (t * 8)) & 0xFF);

		// Write the data chunk header
		size_t data_chunk_pos = WAVfile.tellp();
		WAVfile << "data----";  // (chunk size to be filled in later)

		// save last three data buffers
		int TempSubBufferNumber = FilledPCMDataSubBuffer - 4;
		if (TempSubBufferNumber < 0) TempSubBufferNumber += PCMBufferLength;
		while (TempSubBufferNumber != FilledPCMDataSubBuffer)
		{
			for (unsigned int s = 0; s < PCMDataSubBufferEndingIndex[TempSubBufferNumber]; s++)
			{
				WAVfile.put(PCMDataBuffer[TempSubBufferNumber][s]);
			}

			TempSubBufferNumber++;
			if (TempSubBufferNumber == PCMBufferLength) TempSubBufferNumber = 0;
		}

		// Fix the data chunk header to contain the data size
		size_t file_length = WAVfile.tellp();
		WAVfile.seekp(data_chunk_pos + 4);
		for (int t = 0; t < 4; t++) WAVfile.put(((file_length - data_chunk_pos + 8) >> (t * 8)) & 0xFF);

		// Fix the file header to contain the proper RIFF chunk size, which is (file size - 8) bytes
		WAVfile.seekp(0 + 4);
		for (int t = 0; t < 4; t++) WAVfile.put(((file_length - 8) >> (t * 8)) & 0xFF);
	}
}
};
}
