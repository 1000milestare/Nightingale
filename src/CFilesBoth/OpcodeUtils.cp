/* OpcodeUtils.c for Nightingale - revised for v.3.1 */

#include "Nightingale_Prefix.pch"
#include "Nightingale.appl.h"

/* Given a one-byte value, a starting address, and a length, fill memory with the value. */

void FillMem(Byte value, void *loc, DoubleWord len)
{
	Byte *ptr = (Byte *)loc;
	long slen = len;
	
	while (--slen >= 0L) *ptr++ = value;
}


/* ============================================================ from SIMexportN.c == */

/* A way of generating quantization:

The notes in one measure area are copied to a working area with note
ons and note offs recorded as separate events.  The quantization
unit is translated into a maximum error per event.  If the quantizing
is to happen on eighth notes, which are 240 units long, then the maximum
error per event is set to 120.

Starting with whole notes, this happens:  The times of all events in the
measure are compared with the nearest whole note events in the measure.
Any "beat" (including the notion of sub beats which occur in powers of two
to the beat), all of whose notes are within the minimum, has all its notes
quantized to that number. It is removed from further consideration.
The process repeats until all notes are quantized.

Note on's are given twice as much weight in deciding the model-i.e. their funnels
are half as wide at the top.  The other tuplets are given less weight than the
2-plets as #define'd below.

The Qnote is the intermediate notation that serves as a scratch pad
for the measure being translated into SIM.  It explains how the quantizer
has modeled the event:  as the Mth N-note sized beat in the measure (i.e.
the third quarter note) broken down into J-tuplets of which this event
is the I-th.  When it is time to issue SIM notes, Write SIDs Until gets
a region bounded by two of these Times, and a list of NoteOns telling
which notes if any are on.
*/


extern struct QNote QLast;

/*		EventTimEX  Off-Next-   LastNote Measure128  denompH              Velocity
										                   	  IthTuple    type										RV
														      					 num	          MIDInote 								Spare
														      					    denom         		Channel*/
struct QNote QFirst
		=          {0, 0, &QLast, &QFirst, 0, 0, 2, 1, 0, 0xff, 0xff, 0xff, 0x40, 0x40, 0xff};
struct QNote QLast
		= {0x3fffffff, 0, &QLast, &QFirst, 0, 0, 2, 1, 0, 0xff, 0xff, 0xff, 0x40, 0x40, 0xff};
struct QNote QFree
		=          {0, 0, &QLast, &QFirst, 0, 0, 2, 1, 0, 0xff, 0xff, 0xff, 0x40, 0x40, 0xff};



/*
proof of "(x-1)|x == 2x-1 iff x integral power of 2":

X is either 0, nnnnn1, or nnnn1000000 (m 0's)

if x == 0
	(0 - 1) | 0 + 1 == 0 which implies 0 is a power of 2...( -infinity is an integer??)
	
if x == nnnnnn1
  x-1 = nnnnnn0
  (x-1)|x = nnnnnn1 = x
  x = 2x - 1 iff x = 1
  
if x == nnnn1000000 (m 0's)
	x-1 = nnnn0111111 (m 1's)
	(x-1)|x = nnnn1111111 (m+1 1's)
	2x = nnnn10000000 (m+1 0's)
	2x-1 = nnnn01111111 (m+1 1's)
	and n n's + m+1 1's = n n's + 0 + m+1 1's iff n = 0
*/
  
#define integralPowerOf2(x) ((((x - 1) | x) + 1) == (x << 1))

void FillInWatershed128(
				DoubleWord start128,
				Word length128,
				Byte num,
				Byte denompH,
				Boolean toplevel,
				Byte meterType
				)
{
	Byte numies[3];
	Word lengths128[3];
	register Byte j;
	register Byte k;
	long lTemp;
	
	if ((length128 > 128) || (!integralPowerOf2(num))) {
		if ((meterType == Compound) && ((num & 1) != 0) && ((num % 3) == 0)) {
			k = 3;
			numies[0] = numies[1] = numies[2] = num / 3;
			lengths128[0] = lengths128[1] = lengths128[2] = length128 / 3;
			RCP->RelativeStrength = 2;
			}
		else {
			k = 2;
			numies[1] = num >> 1;
			lTemp = (long)length128*numies[1];
			lengths128[1] =  lTemp/num;
			numies[0] = num - numies[1];
			lengths128[0] = length128 - lengths128[1];
			}
		for (j = 0; j < k; j++) {
			FillInWatershed128 (start128, lengths128[j], numies[j], xTS.denompH, FALSE, meterType);
			start128 += lengths128[j];
			if (toplevel) {
				RCP->RelativeStrength = 3;
				}
			}
		}
	else {
		RCP->BeatsThisRunoff = num;
		RCP->Watershed128 = start128;
		for (k = 0; num > 1; k++, num >>= 1) ;
		RCP->RunoffdenompH = denompH - k;
		RCP->WatershedEX = start128 * (WholeNote / 128);
		RCP++;
		}
	}