/* Spock

Spock is a demonstartion software. It demonstrates how audio may be received from a host computer running a software
call Scotty. Scotty transmit audio from a host computer by controlling GPU memory data transfers. Each memory transfer
generates electromagnetic waves. By controlling the memory data tranfers, the software transmitts the data from the computer.

Spock performs the following tasks:
1. It receives data at a specific frequency, using an SDRplay RSP1A receiver.
2. It process the OOK data into a bit stream.
3. It calcualtes the length of each symbol.
4. It calculates what data out of the transmitted symbol.
5. When enough symbols are gathered, it searches for the transmitted header bytes.
6. When header bytes are located, the software process the incoming packet.
   6.1. It fixes errors using the Reed-Solomon parity bytes.
   6.2. It calculates the checksum and check its validty.
   6.3. If the checksum exam passes - the packet is good to process.
7. It passes the data from the incoming payload to a G.726 decoder.
8. It gather the PCM data in buffers, and play it at the user's will.

Spock was released by Paz Hameiri on 2021.

It includes a slightly modified version of Simon Rockliff' Reed-Solomon encoding-decoding code.
It also includes a G.726 code, provided by Sun Microsystems, Inc.
The basic SDRplay RSP1A routines were taken from documentaion supplied by SDRplay.

Permission to use, copy, modify, and/or distribute the software that were written by Paz Hameiri for any
purpose with or without fee is hereby granted.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT,
INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN
AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE
OF THIS SOFTWARE.

*/

#include "SpockGUI.h"

using namespace System;
using namespace System::Windows::Forms;

[STAThread]
void Main() {

	// Declare variables
	float SDRVersion = 0.0;

	// Open SDRPLAY API
	if ((err = sdrplay_api_Open()) != sdrplay_api_Success)
	{
		sprintf_s(msgbuf, msgbufSize, "sdrplay_api_Open failed %s\n", sdrplay_api_GetErrorString(err));
		OutputDebugStringA(msgbuf);
	}
	else
	{
		// Enable debug logging output
		if ((err = sdrplay_api_DebugEnable(NULL, (sdrplay_api_DbgLvl_t) 1)) != sdrplay_api_Success)
		{
			// Report the error
			sprintf_s(msgbuf, msgbufSize, "sdrplay_api_DebugEnable failed %s\n", sdrplay_api_GetErrorString(err));
			OutputDebugStringA(msgbuf);
		}

		// Check API versions match
		if ((err = sdrplay_api_ApiVersion(&SDRVersion)) != sdrplay_api_Success)
		{
			// Report the error
			sprintf_s(msgbuf, msgbufSize, "sdrplay_api_ApiVersion failed %s\n", sdrplay_api_GetErrorString(err));
			OutputDebugStringA(msgbuf);
		}
		if (SDRVersion != SDRPLAY_API_VERSION)
		{
			// Report the error
			sprintf_s(msgbuf, msgbufSize, "API version don't match (local=%.2f dll=%.2f)\n", SDRPLAY_API_VERSION, SDRVersion);
			OutputDebugStringA(msgbuf);
		}

		// Launch the form
		Application::EnableVisualStyles();
		Application::SetCompatibleTextRenderingDefault(false);
		Spock::MyForm form;
		Application::Run(%form);

		// End radio thread
		SDRFinishedInit = false;
		RadioOn = false;
		SpeakersOn = false;

		// Wait for radio and speakers threads to end
		int ElapsedTime = 0;
		while ((SDRFinishedInit == false) & (ElapsedTime < RadioResponseTimeout))
		{
			ElapsedTime += 200;
			Sleep(200);
		}

		ElapsedTime = 0;
		while ((SpeakersFinished == false) & (ElapsedTime < SpeakersResponseTimeout))
		{
			ElapsedTime += 200;
			Sleep(200);
		}
	}

	// Unlock API
	sdrplay_api_UnlockDeviceApi();

	// Close SDRPLAY API
	sdrplay_api_Close();
}