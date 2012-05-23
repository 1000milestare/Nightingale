/* MIDIUtils.c for Nightingale - rev. for v.2000 */

/*											NOTICE
 *
 * THIS FILE IS PART OF THE NIGHTINGALE™ PROGRAM AND IS CONFIDENTIAL PROP-
 * ERTY OF ADVANCED MUSIC NOTATION SYSTEMS, INC.  IT IS CONSIDERED A TRADE
 * SECRET AND IS NOT TO BE DIVULGED OR USED BY PARTIES WHO HAVE NOT RECEIVED
 * WRITTEN AUTHORIZATION FROM THE OWNER.
 * Copyright © 1997-99 by Advanced Music Notation Systems, Inc. All Rights Reserved.
 *
 */

#include "Nightingale_Prefix.pch"
#include "Nightingale.appl.h"

//#if TARGET_API_MAC_CARBON_MACHO
#include <CoreMIDI/MIDIServices.h>		/* for MIDIPacket */
//#else
//#include <midi.h>						/* for MIDIPacket */
//#endif

#include "MidiMap.h"
#include "CarbonStubs.h"

/* With THINK C 7 on my PowerBook 540 and my PowerBook G3/400, Nightingale links okay,
but trying to run results in a mysterious data segment overflow apparently resulting
from the fact that Symantec's ANSI—small library takes 600+ bytes more on those
machines, even though the file containing the library appears to be identical! Comment
the following kludge in to avoid it. */
#if 0
static MIDIEvent	eventList[10];
#else
static MIDIEvent	eventList[MAXEVENTLIST];
#endif
static CMMIDIEvent	cmEventList[MAXEVENTLIST];

static INT16		lastEvent;

static Boolean	InsertEvent(INT16 note, SignedByte channel, long endTime, short ioRefNum);

void		KillEventList(void);
static void		WaitForQueue(void);
void		SendAllNotesOff(void);

/* -------------------------------------------------------------- UseMIDIChannel -- */
/* Return the MIDI channel number to use for the given part. */

INT16 UseMIDIChannel(Document *doc, INT16	partn)
{
	INT16				useChan;
	LINK				partL;
	PPARTINFO		pPart;
	
	if (doc->polyTimbral) {
		partL = FindPartInfo(doc, partn);
		pPart = GetPPARTINFO(partL);
		useChan = pPart->channel;
		if (useChan<1) useChan = 1;
		if (useChan>MAXCHANNEL) useChan = MAXCHANNEL;
		return useChan;
	}
	else
		return doc->channel;
}


/* ---------------------------------------------------------------- NoteNum2Note -- */
/* Return the note (not rest) in the given sync and voice with the given MIDI note
number, if there is one; else return NILINK. If the sync and voice have more than
one note with the same note number, finds the first one. Intended for finding ties'
notes. */

LINK NoteNum2Note(LINK syncL, INT16 voice, INT16 noteNum)
{
	LINK aNoteL;
	
	aNoteL = FirstSubLINK(syncL);
	for ( ; aNoteL; aNoteL = NextNOTEL(aNoteL))
		if (NoteVOICE(aNoteL)==voice && !NoteREST(aNoteL) && NoteNUM(aNoteL)==noteNum) {
			return aNoteL;
		}
	return NILINK;
}


/* --------------------------------------------------------------------- TiedDur -- */
/* Return total performance duration of <aNoteL> and all following notes tied to
<aNoteL>. If <selectedOnly>, only includes tied notes until the first unselected
one. We use the note's <playDur> only for the last note of the series, its NOTATED
duration for all the others. */

long TiedDur(Document */*doc*/, LINK syncL, LINK aNoteL, Boolean selectedOnly)
{
	LINK			syncPrevL, aNotePrevL, continL, continNoteL;
	INT16			voice;
	long			dur, prevDur;
	
	voice = NoteVOICE(aNoteL);
	
	dur = 0L;
	syncPrevL = syncL;
	aNotePrevL = aNoteL;
	while (NoteTIEDR(aNotePrevL)) {
		continL = LVSearch(RightLINK(syncPrevL),			/* Tied note should always exist */
						SYNCtype, voice, GO_RIGHT, FALSE);
		continNoteL = NoteNum2Note(continL, voice, NoteNUM(aNotePrevL));
		if (continNoteL==NILINK) break;						/* Should never happen */
		if (selectedOnly && !NoteSEL(continNoteL)) break;

		/* We know this note will be played, so add in the previous note's notated dur. */
		prevDur = SyncAbsTime(continL)-SyncAbsTime(syncPrevL);
		dur += prevDur;

		syncPrevL = continL;
		aNotePrevL = continNoteL;
	}
	dur += NotePLAYDUR(aNotePrevL);
	return dur;
}


/* -------------------------------------------------------------- UseMIDINoteNum -- */
/* Return the MIDI note number that should be used to play the given note,
considering transposition. If the "note" is a rest or the continuation of a
tied note, or has velocity 0, it should not be played: indicate by returning
-1. */

INT16 UseMIDINoteNum(Document *doc, LINK aNoteL, INT16 transpose)
{
	INT16 midiNote;
	PANOTE aNote;
	LINK partL;
	PPARTINFO pPart;
	Byte patchNum;

	aNote = GetPANOTE(aNoteL);
	if (aNote->rest || aNote->tiedL					/* Don't play rests and continuations */
	||  aNote->onVelocity==0)							/* ...and notes w/velocity 0 */
		midiNote = -1;
	else {
		midiNote = aNote->noteNum;
		if (doc->transposed)								/* Handle "transposed score" */
			midiNote += transpose;
		if (midiNote<1) midiNote = 1;					/* Clip to legal range */
		if (midiNote>MAX_NOTENUM) midiNote = MAX_NOTENUM;
	}
#ifdef TARGET_API_MAC_CARBON_MIDI
	partL = Staff2PartLINK(doc, NoteSTAFF(aNoteL));
	pPart = GetPPARTINFO(partL);
	patchNum = pPart->patchNum;
	if (IsPatchMapped(doc, patchNum)) {
		midiNote = GetMappedNoteNum(doc, midiNote);
	}	
#endif
	return midiNote;
}


/* ------------------------------------------------------------- GetModNREffects -- */
/* Given a note, if it has any modifiers, get information about how its
modifiers should affect playback, and return TRUE. If it has no modifiers,
just return FALSE. NB: The time factor is unimplemented. */

Boolean GetModNREffects(LINK aNoteL, short *pVelOffset, short *pDurFactor,
			short *pTimeFactor)
{
	LINK		aModNRL;
	short		velOffset, durFactor, timeFactor;

	if (!config.useModNREffects)
		return FALSE;

	aModNRL = NoteFIRSTMOD(aNoteL);
	if (!aModNRL) return FALSE;

	velOffset = 0;
	durFactor = timeFactor = 100;

	for ( ; aModNRL; aModNRL = NextMODNRL(aModNRL)) {
		Byte code = ModNRMODCODE(aModNRL);
		if (code>31) continue;					/* Silent failure: arrays sized for 32 items. */
		velOffset += modNRVelOffsets[code];
// FIXME: What if more than one modifier?
		durFactor = modNRDurFactors[code];
//		timeFactor = modNRTimeFactors[code];
	}

	*pVelOffset = velOffset;
	*pDurFactor = durFactor;
	*pTimeFactor = timeFactor;

	return TRUE;
}


/* ------------------------------------------------------- Tempo/time conversion -- */

/* Given a TEMPO object, return its "timeScale", i.e., tempo in PDUR ticks per minute. */

long Tempo2TimeScale(LINK tempoL)
{
	PTEMPO pTempo; long tempo, beatDur, timeScale;
	
	pTempo = GetPTEMPO(tempoL);
	tempo = pTempo->tempo;
	beatDur = Code2LDur(pTempo->subType, (pTempo->dotted? 1 : 0));
	timeScale = tempo*beatDur;
	return timeScale;
}


/* Return the tempo in effect at the given LINK, if any. The tempo is computed from the
last preceding tempo mark; if there is no preceding tempo mark, it's computed from the
default tempo. In any case, the tempo is expressed as a "timeScale", i.e., in PDUR ticks
per minute. */

long GetTempo(
			Document */*doc*/,		/* unused */
			LINK startL)
{
	LINK tempoL; long timeScale;

	timeScale = (long)config.defaultTempo*DFLT_BEATDUR;	/* Default in case no tempo marks found */

	if (startL) {
		tempoL = LSSearch(startL, TEMPOtype, ANYONE, GO_LEFT, FALSE);
		if (tempoL) timeScale = Tempo2TimeScale(tempoL);
	}

	return timeScale;
}


/* Convert PDUR ticks to millisec., with tempo varying as described by tConvertTab[].
If we can't convert it, return -1L. */

long PDur2RealTime(
				long t,						/* time in PDUR ticks */
				TCONVERT	tConvertTab[],
				INT16 tabSize)
{
	INT16 i; long msAtPrevTempo, msSincePrevTempo;
	
	/*
	 * If the table is empty, just return zero. Otherwise, find the 1st entry in the
	 * table for a PDUR time after <t>; if there's no such entry, assume the last entry 
	 * in the table applies.
	 */	
	if (tabSize<=0) return 0L;
	
#ifdef NOTYET
	if (tConvertTab[tabSize-1].pDurTime<=t)
		i = tabSize;
	else
		for (i = 1; i<tabSize; i++)
			if (tConvertTab[i].pDurTime>t) break;
#else
	for (i = 1; i<tabSize; i++)
		if (tConvertTab[i].pDurTime>t) break;
	if (tConvertTab[i].pDurTime<=t) i = tabSize;	/* ??IS i GUARANTEED TO BE MEANINGFUL? */
#endif
	msAtPrevTempo = tConvertTab[i-1].realTime;
	
	msSincePrevTempo = PDUR2MS(t-tConvertTab[i-1].pDurTime, tConvertTab[i-1].microbeats);
	if (msSincePrevTempo<0) return -1L;
	
	return msAtPrevTempo+msSincePrevTempo;
}


/* Build a table of tempi in effect in the given range, sorted by increasing time.
Return value is the size of the table, or -1 if the table is too large. Intended to
get information for changing tempo during playback. */

INT16 MakeTConvertTable(
				Document *doc,
				LINK fromL, LINK toL,			/* range to be played */
				TCONVERT	tConvertTab[],
				INT16 maxTabSize
				)
{
	INT16		tempoCount;
	LINK		pL, measL, syncL, syncMeasL;
	long		microbeats,							/* microsec. units per PDUR tick */
				timeScale,							/* PDUR ticks per minute */
				pDurTime;							/* in PDUR ticks */

	tempoCount = 0;

	for (pL = fromL; pL!=toL; pL = RightLINK(pL)) {
		switch (ObjLType(pL)) {
			case MEASUREtype:
				measL = pL;
				break;
			case SYNCtype:
			  	/* If no tempo found yet, initial tempo is the last previous one. */
			  	
			  	if (tempoCount==0) {
					syncL = LSSearch(fromL, SYNCtype, ANYONE, GO_RIGHT, FALSE);
					timeScale = GetTempo(doc, syncL);				/* OK even if syncL is NILINK */
					microbeats = TSCALE2MICROBEATS(timeScale);
					tConvertTab[0].microbeats = microbeats;
					tConvertTab[0].pDurTime = 0L;
					tConvertTab[0].realTime = 0L;
					tempoCount = 1;
				}
				break;
			case TEMPOtype:
				if (tempoCount>=maxTabSize) return -1;

				timeScale = Tempo2TimeScale(pL);
				microbeats = TSCALE2MICROBEATS(timeScale);
				tConvertTab[tempoCount].microbeats = microbeats;
				syncL = SSearch(pL, SYNCtype, GO_RIGHT);
				syncMeasL = SSearch(syncL, MEASUREtype, GO_LEFT);
				pDurTime = MeasureTIME(syncMeasL)+SyncTIME(syncL);
				tConvertTab[tempoCount].pDurTime = pDurTime;
				
				/* It's OK to call PDur2RealTime here: the part of the table it needs
					already exists. */
					 
				tConvertTab[tempoCount].realTime = PDur2RealTime(pDurTime,
																				 tConvertTab, tempoCount);
				tempoCount++;
				break;
			default:
				;
		}
	}

#ifdef TDEBUG
{	INT16 i;
	for (i = 0; i<tempoCount; i++)
		DebugPrintf("tConvertTab[%d].microbeats=%ld pDurTime=%ld realTime=%ld\n",
			i, tConvertTab[i].microbeats, tConvertTab[i].pDurTime, tConvertTab[i].realTime);
}
#endif
	return tempoCount;
}


/* ------------------------------------------- MIDI Manager/Driver glue functions -- */
/* The following functions support Apple MIDI Manager, our built-in MIDI driver
for Macintosh (currently MIDI Pascal), and OMS. */

#include "MIDIPASCAL3.h"

void StartMIDITime()
{
	switch (useWhichMIDI) {
		case MIDIDR_FMS:
			FMSStartTime();
			break;
		case MIDIDR_OMS:
			OMSStartTime();
			break;
		case MIDIDR_CM:
			CMStartTime();
			break;
		case MIDIDR_MM:
			MIDISetCurTime(timeMMRefNum, 0L);	
			MIDIStartTime(timeMMRefNum);
			break;
		default:	/* MIDIDR_BI */
			MidiStartTime();
			break;
	};
}

#define TURN_PAGES_WAIT TRUE	/* After "turning" page, TRUE=resume in tempo, FALSE="catch up" */

long GetMIDITime(long pageTurnTOffset)
{
	long time;

	switch (useWhichMIDI) {
		case MIDIDR_FMS:
			time = FMSGetCurTime();
			break;
		case MIDIDR_OMS:
			time = OMSGetCurTime();
			break;
		case MIDIDR_CM:
			time = CMGetCurTime();
			break;
		case MIDIDR_MM:
			time = MIDIGetCurTime(timeMMRefNum);
			break;
		default:	/* MIDIDR_BI */
			MidiGetTime(&time);
			break;
	};
	return time-(TURN_PAGES_WAIT? pageTurnTOffset : 0);
}


void StopMIDITime()
{
	switch (useWhichMIDI) {
		case MIDIDR_FMS:
			FMSStopTime();
			break;
		case MIDIDR_OMS:
			OMSStopTime();
			break;
		case MIDIDR_CM:
			CMStopTime();
			break;
		case MIDIDR_MM:
			MIDIStopTime(timeMMRefNum);
			break;
		default:		/* MIDIDR_BI. NB: does not reset the driver's tickcount! */
			MidiStopTime();										/* we're done with the millisecond timer... */
			MayResetBIMIDI(FALSE);								/* ...and the SCC we're using as a MIDI port */
			break;
	};
}

void StopMIDI()
{
	if (useWhichMIDI==MIDIDR_CM)
		CMKillEventList();
	else
		KillEventList();												/* stop all notes being played */

	if (useWhichMIDI!=MIDIDR_MM) WaitForQueue();				/* or "useWhichMIDI==MIDIDR_BI"? */
	
	StopMIDITime();
}


/* Initialize the built-in MIDI driver's timer to tick every millisecond. Does not
reset the driver's tickcount! */

void InitBIMIDITimer()
{
	MidiTime(ONEMILLISEC);
}


/* ------------------------------------------------- MayInitBIMIDI,MayResetBIMIDI -- */
/* Be sure the the built-in MIDI driver is initialized. With MIDI Pascal, if it isn't
already initialized, this allocates buffers, and initializes the SCC chip for MIDI use.
Unfortunately, doing the latter modifies the interrupt vector, and that means we have
to be sure to call the reset routine (via MayResetBIMIDI) before quitting to avoid a
disaster after we've quit.

NB: these functions depend on <initedBIMIDI> to keep track of the state; except for
one-time initialization, it should not be set anywhere else! */

#define DEBUG_THRU

void MayInitBIMIDI(INT16 inBufSize, INT16 outBufSize)
{
	if (useWhichMIDI==MIDIDR_BI)
		if (!initedBIMIDI) {
#if (defined(DEBUG_THRU) && !defined(PUBLIC_VERSION))
			if (CapsLockKeyDown() && ShiftKeyDown())
				DebugPrintf("MayInitBIMIDI: about to InitMidi\n");
#endif
			InitMidi(inBufSize, outBufSize);
			MidiPort(portSettingBIMIDI);
			MidiPort(interfaceSpeedBIMIDI);
			initedBIMIDI = TRUE;
		}
}

/* Be sure the the built-in MIDI driver is reset. Exception: if MIDI Thru is on,
unless the caller specifies otherwise, just reset the driver's tickcount. This is
because--to keep MIDI Thru working--we shouldn't reset the driver till just before
we exit. */
 
void MayResetBIMIDI(
	Boolean evenIfMIDIThru)							/* Reset regardless of MIDI Thru? */
{
	if (useWhichMIDI==MIDIDR_BI)
		if (initedBIMIDI) {
#define MIDI_THRU
#ifdef MIDI_THRU
#if (defined(DEBUG_THRU) && !defined(PUBLIC_VERSION))
			if (CapsLockKeyDown() && ShiftKeyDown())
				DebugPrintf("MayResetBIMIDI: about to 'reset', evenIfMIDIThru=%d\n",
									evenIfMIDIThru);
#endif
			if (config.midiThru!=0 && !evenIfMIDIThru)
				MidiSetTime(0L);
			else {
				QuitMidi();
				initedBIMIDI = FALSE;
			}
#else
			QuitMidi();
			initedBIMIDI = FALSE;
#endif
		}
}

/* ----------------------------------------------------------------- WaitForQueue -- */
/*	Allow time for low-level queue to empty. For use with built-in MIDI driver. */

static void WaitForQueue()
{
	if (useWhichMIDI==MIDIDR_BI) {
		SleepTicks(5L);						/* May not be necessary but just in case */
	}
}


/* --------------------------------------------------------- Event list functions -- */

/* Initialize the Event list to empty. */

void InitEventList()
{
	lastEvent = 0;
}

/*	Insert the specified note into the event list. If we succeed, return TRUE; if
we fail (because the list is full), give an error message and return FALSE. */

static Boolean InsertEvent(INT16 note, SignedByte channel, long endTime, short ioRefNum)
{
	INT16			i;
	MIDIEvent	*pEvent;
	char			fmtStr[256];

	/* Find first free slot in list, which may be at lastEvent (end of list) */
	
	for (i=0, pEvent=eventList; i<lastEvent; i++,pEvent++)
		if (pEvent->note == 0) break;
	
	/* Insert note into free slot, or append to end of list if there's room */
	
	if (i<lastEvent || lastEvent++<MAXEVENTLIST) {
		pEvent->note = note;
		pEvent->channel = channel;
		pEvent->endTime = endTime;
		pEvent->omsIORefNum = ioRefNum;
		return TRUE;
	}
	else {
		lastEvent--;
		GetIndCString(fmtStr, MIDIPLAYERRS_STRS, 13);		/* "can play only %d notes at once" */
		sprintf(strBuf, fmtStr, MAXEVENTLIST);
		CParamText(strBuf, "", "", "");
		StopInform(GENERIC_ALRT);
		return FALSE;
	}
}

/*	Insert the specified note into the event list. If we succeed, return TRUE; if
we fail (because the list is full), give an error message and return FALSE. */

static Boolean CMInsertEvent(INT16 note, SignedByte channel, long endTime, long ioRefNum)
{
	INT16			i;
	CMMIDIEvent	*pEvent;
	char			fmtStr[256];

	/* Find first free slot in list, which may be at lastEvent (end of list) */
	
	for (i=0, pEvent=cmEventList; i<lastEvent; i++,pEvent++)
		if (pEvent->note == 0) break;
	
	/* Insert note into free slot, or append to end of list if there's room */
	
	if (i<lastEvent || lastEvent++<MAXEVENTLIST) {
		pEvent->note = note;
		pEvent->channel = channel;
		pEvent->endTime = endTime;
		pEvent->cmIORefNum = ioRefNum;
		return TRUE;
	}
	else {
		lastEvent--;
		GetIndCString(fmtStr, MIDIPLAYERRS_STRS, 13);		/* "can play only %d notes at once" */
		sprintf(strBuf, fmtStr, MAXEVENTLIST);
		CParamText(strBuf, "", "", "");
		StopInform(GENERIC_ALRT);
		return FALSE;
	}
}

/*	Checks eventList[] to see if any notes are ready to be turned off; if so,
frees their slots in the eventList and (if we're not using MIDI Manager) turns
them off. Returns TRUE if the list is empty. */

Boolean CheckEventList(long pageTurnTOffset)
{
	Boolean		empty;
	MIDIEvent	*pEvent;
	INT16			i;
	long			t;
	
	t = GetMIDITime(pageTurnTOffset);
	empty = TRUE;
	for (i=0, pEvent = eventList; i<lastEvent; i++, pEvent++)
		if (pEvent->note) {
			empty = FALSE;
			if (pEvent->endTime<=t) {							/* note is done, t = now */
				EndNoteNow(pEvent->note, pEvent->channel, pEvent->omsIORefNum);
				pEvent->note = 0;													/* slot available now */
			}
		}

	return empty;
}

/*	Checks eventList[] to see if any notes are ready to be turned off; if so,
frees their slots in the eventList and (if we're not using MIDI Manager) turns
them off. Returns TRUE if the list is empty. */

Boolean CMCheckEventList(long pageTurnTOffset)
{
	Boolean		empty;
	CMMIDIEvent	*pEvent;
	INT16			i;
	long			t;
	
	t = GetMIDITime(pageTurnTOffset);
	empty = TRUE;
	for (i=0, pEvent = cmEventList; i<lastEvent; i++, pEvent++)
		if (pEvent->note) {
			empty = FALSE;
			if (pEvent->endTime<=t) {							/* note is done, t = now */
				CMEndNoteNow(pEvent->cmIORefNum, pEvent->note, pEvent->channel);
				pEvent->note = 0;													/* slot available now */
			}
		}

	return empty;
}

/*	Turn off all notes in eventList[] and re-initialize it. */

void CMKillEventList()
{
	CMMIDIEvent	*pEvent;
	INT16			i;
	
	for (i = 0, pEvent = cmEventList; i<lastEvent; i++, pEvent++)
		if (pEvent->note) {
			CMEndNoteNow(pEvent->cmIORefNum, pEvent->note, pEvent->channel);
		}

	InitEventList();
}



/*	Turn off all notes in eventList[] and re-initialize it. */

void KillEventList()
{
	MIDIEvent	*pEvent;
	INT16			i;
	
	for (i = 0, pEvent = eventList; i<lastEvent; i++, pEvent++)
		if (pEvent->note) {
			EndNoteNow(pEvent->note, pEvent->channel, pEvent->omsIORefNum);
		}

	InitEventList();
}

/* ------------------------------------------------------------- GetPartPlayInfo -- */

void GetPartPlayInfo(Document *doc, short partTransp[], Byte partChannel[],
							Byte channelPatch[], SignedByte partVelo[])
{
	INT16 i; LINK partL; PARTINFO aPart;

	for (i = 1; i<=MAXCHANNEL; i++)
		channelPatch[i] = MAXPATCHNUM+1;						/* Initialize to illegal value */
	
	partL = FirstSubLINK(doc->headL);
	for (i = 0; i<=LinkNENTRIES(doc->headL)-1; i++, partL = NextPARTINFOL(partL)) {
		aPart = GetPARTINFO(partL);
		partVelo[i] = aPart.partVelocity;
		partChannel[i] = UseMIDIChannel(doc, i);
		channelPatch[partChannel[i]] = aPart.patchNum;
		partTransp[i] = aPart.transpose;
	}
}

/* ------------------------------------------------------------- GetNotePlayInfo -- */
/* Given a note and tables of part transposition, channel, and offset velocity, return
the note's MIDI note number, including transposition; channel number; and velocity,
limited to legal range. */

void GetNotePlayInfo(Document *doc, LINK aNoteL, short partTransp[],
						Byte partChannel[], SignedByte partVelo[],
						INT16 *pUseNoteNum, INT16 *pUseChan, INT16 *pUseVelo)
{
	INT16 partn;
	PANOTE aNote;

	partn = Staff2Part(doc,NoteSTAFF(aNoteL));
	*pUseNoteNum = UseMIDINoteNum(doc, aNoteL, partTransp[partn]);
	*pUseChan = partChannel[partn];
	aNote = GetPANOTE(aNoteL);
	*pUseVelo = doc->velocity+aNote->onVelocity;
	if (doc->polyTimbral) *pUseVelo += partVelo[partn];
	
	if (*pUseVelo<1) *pUseVelo = 1;
	if (*pUseVelo>MAX_VELOCITY) *pUseVelo = MAX_VELOCITY;
}

/* --------------------------------------------------------------- SetMIDIProgram -- */

#define PATCHNUM_BASE 1			/* Some synths start numbering at 1, some at 0 */

/* Set the given MIDI channel to play the given "program" (patch number). */

void SetMIDIProgram(INT16 channel, INT16 patchNum)
{
	MMMIDIPacket mPacket;
	INT16 rc;

	if (useWhichMIDI==MIDIDR_MM) {
		mPacket.flags = MM_STD_FLAGS;	
		mPacket.len = MM_HDR_SIZE+2;
				
		mPacket.tStamp = MM_NOW;
		mPacket.data[0] = MPGMCHANGE+channel-1;
		mPacket.data[1] = patchNum-PATCHNUM_BASE;
		MIDIWritePacket(outputMMRefNum, &mPacket);		
	}
	else {
		MidiNow(MPGMCHANGE+channel-1, patchNum-PATCHNUM_BASE, 255, (int *)&rc);	/* Should never fail */
	}
}


/* ---------------------------------------- StartNoteNow,EndNoteNow,EndNoteLater -- */
/* Functions to start and end notes; handle OMS, FreeMIDI, MIDI Manager, and built-in
MIDI (MIDI Pascal or MacTutor driver). Caveat: EndNoteLater does not communicate with
the MIDI system, it simply adds the note to our event-list routines' queue. */

OSStatus StartNoteNow(INT16 noteNum, SignedByte channel, SignedByte velocity, short ioRefNum)
{
#if DEBUG_KEEPTIMES
	if (nkt<MAXKEEPTIMES) kStartTime[nkt++] = TickCount();
#endif
	OSStatus err = noErr;
	
	if (noteNum>=0) {
		switch (useWhichMIDI) {
			case MIDIDR_FMS:
				FMSStartNoteNow(noteNum, channel, velocity, (fmsUniqueID)ioRefNum);
				break;
			case MIDIDR_OMS:
				OMSStartNoteNow(noteNum, channel, velocity, ioRefNum);
				break;
			case MIDIDR_CM:
				err = CMStartNoteNow(0, noteNum, channel, velocity);
				break;
			case MIDIDR_MM:
				MMStartNoteNow(noteNum, channel, velocity);
				break;
			case MIDIDR_BI:
				MIDITriple(MNOTEON+channel-1, noteNum, velocity);
				break;
			default:
				break;
		}
	}
	
	return err;
}

OSStatus EndNoteNow(INT16 noteNum, SignedByte channel, short ioRefNum)
{
	OSStatus err = noErr;
	
	switch (useWhichMIDI) {
		case MIDIDR_FMS:
			FMSEndNoteNow(noteNum, channel, (fmsUniqueID)ioRefNum);
			break;
		case MIDIDR_OMS:
			OMSEndNoteNow(noteNum, channel, ioRefNum);
			break;
		case MIDIDR_CM:
			err = CMEndNoteNow(0, noteNum, channel);
			break;
		case MIDIDR_MM:
			MMEndNoteAtTime(noteNum, channel, 0L);
			break;
		case MIDIDR_BI:
			MIDITriple(MNOTEON+channel-1, noteNum, 0);
			break;
		default:
			break;
	}
	
	return err;
}

Boolean EndNoteLater(
				INT16 noteNum,
				SignedByte channel,			/* 1 to MAXCHANNEL */
				long endTime,
				short ioRefNum)
{
	return InsertEvent(noteNum, channel, endTime, ioRefNum);
}

Boolean CMEndNoteLater(
				INT16 noteNum,
				SignedByte channel,			/* 1 to MAXCHANNEL */
				long endTime,
				long ioRefNum)
{
	return CMInsertEvent(noteNum, channel, endTime, ioRefNum);
}




/* -------------------------------------------- MMStartNoteNow, MMEndNoteAtTime -- */
/* For MIDI Manager: start the note now, end the note at the given time. */
 
void MMStartNoteNow(
				INT16			noteNum,
				SignedByte	channel,			/* 1 to MAXCHANNEL */
				SignedByte	velocity
				)
{
	MMMIDIPacket mPacket;

	mPacket.flags = MM_STD_FLAGS;	
	mPacket.len = MM_NOTE_SIZE;
			
	mPacket.tStamp = MM_NOW;
	mPacket.data[0] = MNOTEON+channel-1;
	mPacket.data[1] = noteNum;
	mPacket.data[2] = velocity;
	MIDIWritePacket(outputMMRefNum, &mPacket);
}

void MMEndNoteAtTime(
				INT16			noteNum,
				SignedByte	channel,			/* 1 to MAXCHANNEL */
				long			endTime
				)
{
	MMMIDIPacket mPacket;
	
	mPacket.flags = MM_STD_FLAGS;	
	mPacket.len = MM_NOTE_SIZE;
			
	mPacket.tStamp = endTime;
	mPacket.data[0] = MNOTEON+channel-1;
	mPacket.data[1] = noteNum;
	mPacket.data[2] = 0;										/* 0 velocity = Note Off */
	
	MIDIWritePacket(outputMMRefNum, &mPacket);
}


/* ------------------------------------------------------------------ MIDITriple -- */
/*	For built-in MIDI (MIDI Pascal or MacTutor): Transmit 3 bytes over MIDI, except
if 2nd is negative, do nothing. This is intended for note-related commands, where
the 2nd byte is the note number; Nightingale uses negative values for rests. */

void MIDITriple(INT16 MIDI1, INT16 MIDI2, INT16 MIDI3)
{
	INT16 rc;
	
	if (useWhichMIDI!=MIDIDR_BI) { MayErrMsg("MIDITriple: Not using Built In MIDI"); return; }
	
	if (MIDI2>=0) {
		MidiNow(MIDI1, MIDI2, MIDI3, (int *)&rc);					/* Should never fail */
	}
}


/* --------------------------------------------------------------- MIDIConnected -- */
/*	Return TRUE if a MIDI device is connected. */

Boolean MIDIConnected()
{
	Boolean	result;
	
#ifdef NOTYET
	Document *doc = (Document *)TopDocument;
	long		startTime;
	result = TRUE;										/* Assume a connection */
	MayInitBIMIDI(BIMIDI_SMALLBUFSIZE);
	MIDIFBNoteOn(doc, 0, doc->channel);			/* Send note commands */
	MIDIFBNoteOff(doc, 0, doc->channel);
	MIDIFBNoteOn(doc, 0, doc->channel);
	MIDIFBNoteOff(doc, 0, doc->channel);
	startTime = TickCount();
	SleepTicks(1L);
	if (!QueueEmpty())								/* Are the commands gone from queue? */
		result = FALSE;								/* No, assumption is false */
	if (TickCount()-startTime > 5)
		result = FALSE;
	MayResetBIMIDI(FALSE);
#else
	result = FALSE;									/* ??ABOVE CODE ALWAYS SAYS "TRUE" */
#endif
	return result;
}


/* ====================================================== MIDI Feedback Functions == */

/* -------------------------------------------------------------------- MIDIFBOn -- */
/*	If feedback is enabled, turn on MIDI stuff. Exception: if the port is busy,
do nothing. */

void MIDIFBOn(Document *doc)
{
	if (doc->feedback) {
		switch (useWhichMIDI) {
			case MIDIDR_FMS:
				FMSFBOn(doc);
				break;
			case MIDIDR_OMS:
				OMSFBOn(doc);
				break;
			case MIDIDR_CM:
				CMFBOn(doc);
				break;
			case MIDIDR_MM:
				MIDISetCurTime(timeMMRefNum, 0L);	
				MIDIStartTime(timeMMRefNum);
				break;
			case MIDIDR_BI:
//				if (portSettingBIMIDI==MODEM_PORT && !PORT_IS_FREE(MLM_PortAUse)) break;
//				if (portSettingBIMIDI==PRINTER_PORT && !PORT_IS_FREE(MLM_PortBUse)) break;
				MayInitBIMIDI(BIMIDI_SMALLBUFSIZE, BIMIDI_SMALLBUFSIZE);
				break;
			default:
				break;
		}
	}
}


/* ------------------------------------------------------------------- MIDIFBOff -- */
/*	If feedback is enabled, turn off MIDI stuff. Exception: if the port is busy,
do nothing. */

void MIDIFBOff(Document *doc)
{
	if (doc->feedback) {
		switch (useWhichMIDI) {
			case MIDIDR_FMS:
				FMSFBOff(doc);
				break;
			case MIDIDR_OMS:
				OMSFBOff(doc);
				break;
			case MIDIDR_CM:
				CMFBOff(doc);
				break;
			case MIDIDR_MM:
				MIDIStopTime(timeMMRefNum);
				break;
			case MIDIDR_BI:
//				if (portSettingBIMIDI==MODEM_PORT && !PORT_IS_FREE(MLM_PortAUse)) break;
//				if (portSettingBIMIDI==PRINTER_PORT && !PORT_IS_FREE(MLM_PortBUse)) break;
				WaitForQueue();
				MayResetBIMIDI(FALSE);
				break;
			default:
				break;
		}
	}
}


/* ---------------------------------------------------------------- MIDIFBNoteOn -- */
/*	Start MIDI "feedback" note by sending a MIDI NoteOn command for the specified
note and channel. Exception: if the port is busy, do nothing. */

void MIDIFBNoteOn(
				Document *doc,
				INT16	noteNum, INT16	channel,
				short	useIORefNum)			/* Ignored unless we're using OMS or FreeMIDI */
{
	if (doc->feedback) {
		switch (useWhichMIDI) {
			case MIDIDR_FMS:
				FMSFBNoteOn(doc, noteNum, channel, (fmsUniqueID)useIORefNum);
				break;
			case MIDIDR_OMS:
				OMSFBNoteOn(doc, noteNum, channel, useIORefNum);
				break;
			case MIDIDR_CM:
				CMFBNoteOn(doc, noteNum, channel, useIORefNum);
				break;
			case MIDIDR_MM:
				MMStartNoteNow(noteNum, channel, config.feedbackNoteOnVel); /* PRE_OMS was StartNoteNow */
				break;
			case MIDIDR_BI:
//				if (portSettingBIMIDI==MODEM_PORT && !PORT_IS_FREE(MLM_PortAUse)) return;
//				if (portSettingBIMIDI==PRINTER_PORT && !PORT_IS_FREE(MLM_PortBUse)) return;
				MIDITriple(MNOTEON+channel-1, noteNum, config.feedbackNoteOnVel);
				break;
			default:
				break;
		}
		
		/*
		 * Delay a bit before returning. NB: this causes problems with our little-used
		 * "chromatic" note input mode by slowing down AltInsTrackPitch. See comments
		 * in TrackVMove.c.
		 */
		SleepTicks(2L);
	}
}


/* --------------------------------------------------------------- MIDIFBNoteOff -- */
/*	End MIDI "feedback" note by sending a MIDI NoteOn command for the specified
note and channel with velocity 0 (which acts as NoteOff).  Exception: if the port
is busy, do nothing. */

void MIDIFBNoteOff(
				Document *doc,
				INT16	noteNum, INT16	channel,
				short useIORefNum)			/* Ignored unless we're using OMS or FreeMIDI */
{
	if (doc->feedback) {
		switch (useWhichMIDI) {
			case MIDIDR_FMS:
				FMSFBNoteOff(doc, noteNum, channel, (fmsUniqueID)useIORefNum);
				break;
			case MIDIDR_OMS:
				OMSFBNoteOff(doc, noteNum, channel, useIORefNum);
				break;
			case MIDIDR_CM:
				CMFBNoteOff(doc, noteNum, channel, useIORefNum);
				break;
			case MIDIDR_MM:
				MMEndNoteAtTime(noteNum, channel, 0L);
				break;
			case MIDIDR_BI:
//				if (portSettingBIMIDI==MODEM_PORT && !PORT_IS_FREE(MLM_PortAUse)) return;
//				if (portSettingBIMIDI==PRINTER_PORT && !PORT_IS_FREE(MLM_PortBUse)) return;
				MIDITriple(MNOTEON+channel-1, noteNum, 0);
				break;
			default:
				break;
		}
		
		/*
		 * Delay a bit before returning. NB: this causes problems with our little-used
		 * "chromatic" note input mode by slowing down AltInsTrackPitch. See comments
		 * in TrackVMove.c.
		 */
		SleepTicks(2L);
	}
}


#ifdef NOTYET

/* ------------------------------------------------------------- SendAllNotesOff -- */

#define MCHMODE_ALLNOTESOFF 123

/* Send the MIDI All Notes Off command. Unfortunately, many synthesizers, esp. older
ones--e.g., CZ-1, ESQ-1--don't recognize this command, so (as of v.3.1) this is
unused. Also, with MIDI Manager, the manual v.2 addendum points out that, if you know
which notes are playing, the best way to do this is using the new invisible input and
output ports. */

void SendAllNotesOff()
{
	INT16 channel; MIDIPacket mPacket;

	for (channel = 1; channel<=MAXCHANNEL; channel++)
		if (useWhichMIDI==MIDIDR_MM) {
			mPacket.flags = MM_STD_FLAGS;	
			mPacket.len = MM_HDR_SIZE+3;
					
			mPacket.tStamp = MM_NOW;
			mPacket.data[0] = MCTLCHANGE+channel-1;
			mPacket.data[1] = MCHMODE_ALLNOTESOFF;
			mPacket.data[2] = 0;			
			MIDIWritePacket(outputMMRefNum, &mPacket);		
		}
		else {
			INT16 rc;
			
			MidiNow(MCTLCHANGE+channel-1, MCHMODE_ALLNOTESOFF, 0, (int *)&rc);	/* Should never fail */
		}
}

#endif

/* ----------------------------------------------------------------- AllNotesOff -- */
/*	The so-called "Panic" command: turn all notes off on all channels, regardless of
whether we think they're playing. Do this simply by sending Note Off commands for
every note and channel. N.B. To get the notes shut off as quickly as possible, we
probably should first call SendAllNotesOff, though (as of Dec. 1992) many synths
ignore it so we would still have to follow up with individual Note Offs for every
possible note.

Handles the (trivial) user interface. */

void AllNotesOff()
{
	INT16 noteNum, channel;
	
	WaitCursor();

	if (useWhichMIDI==MIDIDR_BI) MayInitBIMIDI(BIMIDI_SMALLBUFSIZE, BIMIDI_SMALLBUFSIZE);
	
	for (channel = 1; channel<=MAXCHANNEL; channel++)
		for (noteNum = 0; noteNum<=MAX_NOTENUM; noteNum++) {
			if (useWhichMIDI==MIDIDR_MM)
				MMEndNoteAtTime(noteNum, channel, 0L);
			else
				MIDITriple(MNOTEON+channel-1, noteNum, 0);

			/*
			 * Pause a bit, to try to avoid overloading the synth on the receiving end.
			 * It's better to use a simpleminded delay loop instead of "real" timing in
			 * order to avoid conflicts in use of the hardware timer(s)--see comments
			 * in SleepMS.
			 */
			SleepMS(1L);
		}
	
	if (useWhichMIDI==MIDIDR_BI) {
		WaitForQueue();
		MayResetBIMIDI(FALSE);
	}
}