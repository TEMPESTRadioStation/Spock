# Spock

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
