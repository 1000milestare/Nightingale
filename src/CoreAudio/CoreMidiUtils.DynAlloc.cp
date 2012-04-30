/*  * CoreMIDIUtils.c *  * Implementation of OMSUtils functionality for CoreMIDI */ #include "Nightingale_Prefix.pch"#include "Nightingale.appl.h"#include "CoreMidiUtils.h"#include "NTimeMgr.h"#ifdef CM_DYNAMIC_ALLOC#include <vector.h> // #include <vector>#endifconst MIDIUniqueID kInvalidMIDIUniqueID = 0;const int kCMBufLen 			= 0x4000;			// 16384;const int kCMWriteBufLen 	= 0x0200;	// 512const int kActiveSensingLen = 1;const int kNoteOnLen = 3;const int kNoteOffLen = 3;const Byte kActiveSensingData = 0xFE;const UInt64 kNanosToMillis = 1000000;Byte				gMIDIPacketListBuf[kCMBufLen];Byte				gMIDIWritePacketListBuf[kCMWriteBufLen];MIDIPacketList	*gMIDIPacketList;MIDIPacketList	*gMIDIWritePacketList;MIDIPacket		*gCurrentPacket;MIDIPacket		*gCurrPktListBegin;MIDIPacket		*gCurrPktListEnd;MIDIPacket		*gCurrPktListTerm;MIDIPacket 		*gPeekedPkt = NULL;MIDIPacket		*gCurrentWritePacket;Boolean			gSignedIntoCoreMIDI;Boolean			appOwnsBITimer;MIDIClientRef 		gClient = NULL;MIDIPortRef			gInPort = NULL;MIDIPortRef			gOutPort = NULL;MIDIEndpointRef	gDest = NULL;MIDIUniqueID		gDefaultInputDevID = 0;MIDIUniqueID		gDefaultOutputDevID = 0;MIDIUniqueID		gDefaultMetroDevID = 0;int					gDefaultChannel = 1;long					gCMBufferLength;	/* CM MIDI Buffer size in bytes */Boolean				gCMMIDIBufferFull;MIDIUniqueID		gSelectedInputDevice;int					gSelectedInputChannel;MIDIUniqueID		gSelectedThruDevice;int					gSelectedThruChannel;MIDIUniqueID		gSelectedOutputDevice;int					gSelectedOutputChannel;static long MIDIPacketSize(int len);static Boolean CMIsNoteOnPacket(MIDIPacket *p);static Boolean CMIsNoteOffPacket(MIDIPacket *p);static Boolean CMIsActiveSensingPacket(MIDIPacket *p);static Boolean AllocCMPacketList(void);#ifdef CM_DYNAMIC_ALLOC// None of this is testedtypedef MIDIPacketList *MIDIPacketListPtr;typedef struct MIDIPacketListElt {	MIDIPacketListPtr pktListPtr;	size_t				pktListSize;} MIDIPacketListElt, *MIDIPacketListEltPtr;typedef vector<MIDIPacketListPtr> MIDIPktListPtrVector;static MIDIPktListPtrVector *midiReadPktListVec;size_t MIDIPacketListSize(const MIDIPacketList *pktlist){	const MIDIPacket *pkt = &pktlist->packet[0];	int npackets = pktlist->numPackets;	while (--npackets >= 0)		pkt = MIDIPacketNext(pkt);	size_t len = (Byte *)pkt - (Byte *)pktlist;	return len;}size_t MIDIPacketListVectorSize(MIDIPktListPtrVector *mplv){	MIDIPktListPtrVector::const_iterator i = mplv->begin();	size_t pktSize = 0;		for( ; i != mplv->end(); i++) {		pktSize += MIDIPacketListSize((const MIDIPacketList *)(*i));	}		return pktSize;}OSStatus InsertMIDIPacketList(const MIDIPacketList *pktlist){	size_t pktListSize = MIDIPacketListSize(pktlist);		MIDIPacketList *listPtr = (MIDIPacketList *)NewPtr(pktListSize);	if (listPtr == NULL)		return memFullErr;		BlockMove(pktlist, listPtr, pktListSize);		midiReadPktListVec->push_back(listPtr);		return noErr;}MIDIPacketList *CoalesceMIDIPacketLists(){	size_t pktVecSize = MIDIPacketListVectorSize(midiReadPktListVec);	pktVecSize += sizeof(UInt32);	Byte *pkts = (Byte *)NewPtr(pktVecSize);	if (pkts == NULL)		return NULL;			MIDIPacketList *midiPktList;	Byte *p = pkts + sizeof(UInt32);		midiPktList = (MIDIPacketList *)pkts;	MIDIPacketListInit(midiPktList);		MIDIPktListPtrVector::const_iterator iC = midiReadPktListVec->begin();		for ( ; iC != midiReadPktListVec->end(); iC++)	{		MIDIPacketList *pktList = (MIDIPacketList *)(*iC);		size_t pktListSize = MIDIPacketListSize(pktList);				midiPktList->numPackets += pktList->numPackets;				BlockMove(pktList->packet, p, pktListSize);		p += pktListSize;		DisposePtr((Ptr)pktList);	// How do we dispose of this??	}		MIDIPktListPtrVector::iterator i = midiReadPktListVec->begin();	MIDIPktListPtrVector::iterator j = midiReadPktListVec->end();		midiReadPktListVec->erase(i,j);		return midiPktList;}#endif#ifdef CM_DYNAMIC_ALLOC1const size_t kCMBuferLen 			= 0x4000;			// 16384;const size_t kCMBufferIncr			= 0x2000;			//  8192;static size_t gCurrBuffSize;Byte *gMIDIPacketListBuffer;static Boolean CMBufferOverflow(MIDIPacket *pkt, size_t len){	return ((char*)pkt + len) > 				((char*)gMIDIPacketListBuffer + gCurrBuffSize);}size_t MIDIPacketListSize(const MIDIPacketList *pktlist){	const MIDIPacket *pkt = &pktlist->packet[0];	int npackets = pktlist->numPackets;	while (--npackets >= 0)		pkt = MIDIPacketNext(pkt);	size_t len = (Byte *)pkt - (Byte *)pktlist;	return len;}// This strategy doesn't filter out the packets we don't wantstatic void	NightCMReadProc(const MIDIPacketList *pktlist, void *refCon, void *connRefCon){	MIDIPacket *packetToAdd = gCurrPktListEnd;	if (gOutPort != NULL && gDest != NULL) {				size_t pktListSize = MIDIPacketListSize(pktlist);				MIDIPacket *packet = (MIDIPacket *)pktlist->packet;	// remove const (!)				if (CMBufferOverflow(packetToAdd, pktListSize)) {			// Allocate new buffer		}		else {			Byte *src = (Byte*)&pktlist->packet[0];			Byte *dst = (Byte*)gCurrPktListTerm; 			BlockMove(src, dst, pktListSize);			gMIDIPacketList->numPackets += pktlist->numPackets;			dst += pktListSize;			gCurrPktListTerm = (MIDIPacket *)dst;		}	}		gCurrPktListEnd = packetToAdd;	gCurrPktListTerm = MIDIPacketNext(gCurrPktListEnd);}#endif#define CM_DYNAMIC_ALLOC2#ifdef CM_DYNAMIC_ALLOC2//const size_t kCMBuferLen 			= 0x4000;			// 16384;//const size_t kCMBufferIncr			= 0x2000;			//  8192;const size_t kCMBuferLen 			= 0x80;			// 16384;const size_t kCMBufferIncr			= 0x80;			//  8192;static size_t gCurrBuffSize;Byte *gMIDIPacketListBufDyn;static long gBufLeft;/* ------------------------------------------------------------ AllocCMPacketList -- */static Boolean AllocCMPktListDyn(void){	gCurrBuffSize = kCMBuferLen;		gMIDIPacketListBufDyn = (Byte *)NewPtr(gCurrBuffSize);	if (!gMIDIPacketListBufDyn)		return FALSE;		gMIDIPacketList = (MIDIPacketList *)gMIDIPacketListBufDyn;	gCurrentPacket = MIDIPacketListInit(gMIDIPacketList);		gCurrPktListBegin = gCurrPktListEnd = gCurrPktListTerm = gCurrentPacket;		return (gCurrentPacket != NULL);}/* ------------------------------------------------------------ ResetMIDIPacketList -- *//* Same implementation as AllocCMPacketList. Fulfill different functions for which * implementation will diverge when we dynamically allocate the buffer. */Boolean ResetMIDIPktListDyn(){	gMIDIPacketList = (MIDIPacketList *)gMIDIPacketListBufDyn;	gCurrentPacket = MIDIPacketListInit(gMIDIPacketList);		gCurrPktListBegin = gCurrPktListEnd = gCurrPktListTerm = gCurrentPacket;		return (gCurrentPacket != NULL);}MIDIPacket *AllocNewPktListBuffDyn(MIDIPacket *currPkt){	Byte *p = (Byte *)currPkt;		size_t currPktOffset = p - gMIDIPacketListBufDyn;	Byte *gOldMIDIPktListBufDyn = gMIDIPacketListBufDyn;		gMIDIPacketListBufDyn = (Byte *)NewPtrClear(gCurrBuffSize + kCMBufferIncr);	if (!gMIDIPacketListBufDyn) {		gMIDIPacketListBufDyn = gOldMIDIPktListBufDyn;		return currPkt;	}		BlockMove(gOldMIDIPktListBufDyn, gMIDIPacketListBufDyn, gCurrBuffSize);		DisposePtr((char *)gOldMIDIPktListBufDyn);		gMIDIPacketList = (MIDIPacketList *)gMIDIPacketListBufDyn;	gCurrentPacket = (MIDIPacket *)(gMIDIPacketListBufDyn + sizeof(UInt32));	gCurrPktListBegin = gCurrPktListEnd = gCurrPktListTerm = gCurrentPacket;		gCurrBuffSize += kCMBufferIncr;	currPkt = (MIDIPacket *)(gMIDIPacketListBufDyn + currPktOffset);	return currPkt;}static Boolean CMEndOfBuffDyn(MIDIPacket *pkt, int len){	gBufLeft = (gMIDIPacketListBufDyn + gCurrBuffSize) - ((Byte *)pkt + MIDIPacketSize(len));		return gBufLeft < 0;		//return ((Byte *)pkt + MIDIPacketSize(len)) > 	//			(gMIDIPacketListBufDyn + gCurrBuffSize);}static void	NightCMDynReadProc(const MIDIPacketList *pktlist, void *refCon, void *connRefCon){	if (!gCurrPktListEnd) return;		MIDIPacket *packetToAdd = gCurrPktListEnd;		if (gOutPort != NULL && gDest != NULL) {		MIDIPacket *packet = (MIDIPacket *)pktlist->packet;	// remove const (!)				unsigned int j = 0;				for ( ; j < pktlist->numPackets && packetToAdd != NULL; j++) {					// Active sensing is sent once every 300 Milliseconds						if (!CMIsActiveSensingPacket(packet)) {							if (CMEndOfBuffDyn(MIDIPacketNext(packetToAdd), packet->length))				{						packetToAdd = AllocNewPktListBuffDyn(packetToAdd);				}								if (packetToAdd != NULL)				{					packetToAdd = MIDIPacketListAdd(gMIDIPacketList, gCurrBuffSize, packetToAdd, 																packet->timeStamp, packet->length, packet->data);				}			}						packet = MIDIPacketNext(packet);		}	}		gCurrPktListEnd = packetToAdd;		if (gCurrPktListEnd)		gCurrPktListTerm = MIDIPacketNext(gCurrPktListEnd);}#endif// -------------------------------------------------------------------------------------// Midi Callbacks/* -------------------------------------------------------------- NightCMReadProc -- */static long MIDIPacketSize(int len){	return CMPKT_HDR_SIZE + len;}static Boolean CMEndOfBuffer(MIDIPacket *pkt, int len){	return ((char*)pkt + MIDIPacketSize(len)) > 				((char*)gMIDIPacketListBuf + kCMBufLen);}static void	NightCMReadProc(const MIDIPacketList *pktlist, void *refCon, void *connRefCon){	MIDIPacket *packetToAdd = gCurrPktListEnd;		if (gOutPort != NULL && gDest != NULL) {		MIDIPacket *packet = (MIDIPacket *)pktlist->packet;	// remove const (!)		unsigned int j = 0;		for ( ; j < pktlist->numPackets && !CMEndOfBuffer(packetToAdd, packet->length); j++) {					// Active sensing is sent once every 300 Milliseconds						if (!CMIsActiveSensingPacket(packet)) {				packetToAdd = MIDIPacketListAdd(gMIDIPacketList, sizeof(gMIDIPacketListBuf), packetToAdd, 																packet->timeStamp, packet->length, packet->data);									}			packet = MIDIPacketNext(packet);		}	}		gCurrPktListEnd = packetToAdd;	gCurrPktListTerm = MIDIPacketNext(gCurrPktListEnd);}// -------------------------------------------------------------------------------------// Midi Packets/* ------------------------------------------------------------ AllocCMPacketList -- */static Boolean AllocCMPacketList(void){	gMIDIPacketList = (MIDIPacketList *)gMIDIPacketListBuf;	gCurrentPacket = MIDIPacketListInit(gMIDIPacketList);		gCurrPktListBegin = gCurrPktListEnd = gCurrPktListTerm = gCurrentPacket;		return (gCurrentPacket != NULL);}/* ------------------------------------------------------------ ResetMIDIPacketList -- *//* Same implementation as AllocCMPacketList. Fulfill different functions for which * implementation will diverge when we dynamically allocate the buffer. */Boolean ResetMIDIPacketList(){	gMIDIPacketList = (MIDIPacketList *)gMIDIPacketListBuf;	gCurrentPacket = MIDIPacketListInit(gMIDIPacketList);		gCurrPktListBegin = gCurrPktListEnd = gCurrPktListTerm = gCurrentPacket;		return (gCurrentPacket != NULL);}Boolean CMCurrentPacketValid(){	return (gCurrentPacket != NULL && 				gCurrentPacket < gCurrPktListTerm);}void SetCMMIDIPacket(){	gCurrentPacket = gCurrPktListBegin;}void ClearCMMIDIPacket(){	gCurrentPacket = NULL;}/* ------------------------------------------------------------ GetCMMIDIPacket -- *//* ASSUMPTION, since buffer element is returned to Queue, it is unlikely to be reused  before caller has a chance to use the packet */  MIDIPacket *GetCMMIDIPacket(void){	MIDIPacket *pmPkt = NULL;		if (CMCurrentPacketValid()) {		pmPkt = gCurrentPacket;		gCurrentPacket = MIDIPacketNext(gCurrentPacket);	}		return pmPkt;}/* ----------------------------------------------------- PeekAtNextOMSMIDIPacket -- */MIDIPacket *PeekAtNextCMMIDIPacket(Boolean first){/*	if (!CMCurrentPacketValid())		gPeekedPkt = NULL;	else if (first)		gPeekedPkt = gCurrentPacket;	else if (!CMIsActiveSensingPacket(gPeekedPkt))		gPeekedPkt = MIDIPacketNext(gPeekedPkt);	else		gPeekedPkt = NULL;*/	if (!CMCurrentPacketValid())		gPeekedPkt = NULL;	else if (first)		gPeekedPkt = gCurrentPacket;	else if (gPeekedPkt < gCurrPktListTerm)		gPeekedPkt = MIDIPacketNext(gPeekedPkt);	else		gPeekedPkt = NULL;			return gPeekedPkt;/*				if (CMValidCurrentPacket()) {		if (first) {			gPeekedPkt = gCurrentPacket;		}		else if (!IsActiveSensingPacket(gPeekedPkt)) {			gPeekedPkt = MIDIPacketNext(gPeekedPkt);		}		else {			gPeekedPkt = NULL;		}			}	else {		gPeekedPkt = NULL;	}			return gPeekedPkt;*/}/* -------------------------------------------------- DeletePeekedAtOMSMIDIPacket -- */void DeletePeekedAtCMMIDIPacket(void){	if (gPeekedPkt != NULL) {		gPeekedPkt->data[0] = 0;	}}// -------------------------------------------------------------------------------------// Packet Typesstatic Boolean CMIsNoteOnPacket(MIDIPacket *p){	Byte command = p->data[0] & MCOMMANDMASK;	return (p->length == kNoteOnLen && command == MNOTEON);}static Boolean CMIsNoteOffPacket(MIDIPacket *p){	Byte command = p->data[0] & MCOMMANDMASK;	return (p->length == kNoteOffLen && command == MNOTEOFF);}static Boolean CMIsActiveSensingPacket(MIDIPacket *p){	return (p->length == kActiveSensingLen && p->data[0] == kActiveSensingData);}/* -------------------------------------------------- DeletePeekedAtOMSMIDIPacket -- */static MIDIPacket *AddActiveSensingPacket(MIDIPacket *p){	MIDIPacket mPkt;		mPkt.timeStamp = 0;	mPkt.length = kActiveSensingLen;	mPkt.data[0] = kActiveSensingData;	p = MIDIPacketListAdd(gMIDIPacketList, sizeof(gMIDIPacketListBuf), p, 									mPkt.timeStamp, mPkt.length, mPkt.data);														return p;}/* -------------------------------------------------- DeletePeekedAtOMSMIDIPacket -- */long CMTimeStampToMillis(MIDITimeStamp timeStamp){	UInt64 tsNanos = AudioConvertHostTimeToNanos(timeStamp);		UInt64 tsMillis = tsNanos / kNanosToMillis;		long tsLongMillis = (long)tsMillis;		return tsLongMillis;} /* -------------------------------------------------- DeletePeekedAtOMSMIDIPacket -- */void CMNormalizeTimeStamps(){	MIDITimeStamp firstTimeStamp;	Boolean haveTimeStamp = false;		MIDIPacket *packet = gCurrPktListBegin;	while (packet != gCurrPktListTerm) {			if (CMIsNoteOnPacket(packet) || CMIsNoteOffPacket(packet) ) {			if (!haveTimeStamp) {				haveTimeStamp = TRUE;				firstTimeStamp = packet->timeStamp;				packet->timeStamp = 0;			}			else {				packet->timeStamp -= firstTimeStamp;			}		}		packet = MIDIPacketNext(packet);	}} /* -------------------------------------------------- CloseCoreMidiInput -- */void CloseCoreMidiInput(void){//	gCurrPktListEnd = AddActiveSensingPacket(gCurrPktListEnd);}// -------------------------------------------------------------------------------------// Write packet list (for single note on / note offs./* -------------------------------------------------------- AllocCMWritePacketList -- */static Boolean AllocCMWritePacketList(void){	gMIDIWritePacketList = (MIDIPacketList *)gMIDIWritePacketListBuf;	gCurrentWritePacket = MIDIPacketListInit(gMIDIWritePacketList);		return (gCurrentWritePacket != NULL);}Boolean ResetMIDIWritePacketList(){	gMIDIWritePacketList = (MIDIPacketList *)gMIDIWritePacketListBuf;	gCurrentWritePacket = MIDIPacketListInit(gMIDIWritePacketList);		return (gCurrentWritePacket != NULL);}// ----------------------------------------------------------- Timing calls for CoreMidi// Use the Nightingale Time Manager.void CMInitTimer(void){	//OMSClaimTimer(myClientID);		appOwnsBITimer = TRUE;	NTMInit();                              /* initialize timer */}void CMLoadTimer(INT16 interruptPeriod){	if (appOwnsBITimer) {			;												/* There's nothing to do here. */	}}void CMStartTime(void){	//OMSClaimTimer(myClientID);		appOwnsBITimer = TRUE;	NTMStartTimer(1);}long CMGetCurTime(void){	long time;	time = NTMGetTime();								/* use built-in */	return time;}void CMStopTime(void){	if (appOwnsBITimer) {						/* use built-in */		NTMStopTimer();							/* we're done with the millisecond timer */		NTMClose();		appOwnsBITimer = FALSE;	}}static void InitCoreMidiTimer(){	CMInitTimer();}// -------------------------------------------------------------------------------------// Sending Notes// Need a 1 packet long list to send// 	Set the packet's timeStamp to zero [= now, see Core Midi Doc, MIDIPacket]//		Call MIDISend//// Need separate packet list (not gMIDIPacketList)//		That way we don't have to alloc and init a separate packet list inside these//			routines//		Add 1 packet to it//		Call MIDISend//		Make sure that packet is released//// Need to figure out how to set up the packet//  -------------------------------------------------------------- CM End/Start Note Now OSStatus CMWritePacket(MIDITimeStamp tStamp, UInt16 pktLen, Byte *data){	gCurrentWritePacket = MIDIPacketListAdd(gMIDIWritePacketList, 															sizeof(gMIDIWritePacketListBuf), 															gCurrentWritePacket, tStamp, pktLen, data);	OSStatus err = MIDISend(gOutPort, gDest, gMIDIWritePacketList);		if (err == noErr) {		Boolean ok = ResetMIDIWritePacketList();				if (!ok) err = memFullErr;	}	return err;}OSStatus CMEndNoteNow(INT16 noteNum, char channel){	Byte noteOff[] = { 0x90, 60, 0 };	MIDITimeStamp tStamp = 0;				// Indicates perform NOW.	noteOff[0] |= gDefaultChannel;	noteOff[1] = noteNum;	OSStatus err = CMWritePacket(tStamp, 3, noteOff);		return err;}OSStatus CMStartNoteNow(INT16 noteNum, char channel, char velocity){	Byte noteOn[] = { 0x90, 60, 64 };	MIDITimeStamp tStamp = 0;				// Indicates perform NOW.	noteOn[0] |= gDefaultChannel;	noteOn[1] = noteNum;	noteOn[2] = velocity;		OSStatus err = CMWritePacket(tStamp, 3, noteOn);	return err;}//  ------------------------------------------------------------------------ CM Feedback void CMFBOff(Document *doc){	if (doc->feedback) {		if (appOwnsBITimer) {					/* use built-in */			CMStopTime();		}	}}void CMFBOn(Document *doc){	if (doc->feedback) {		//OMSClaimTimer(myClientID);				/* use built-in */		appOwnsBITimer = TRUE;		CMStartTime();	}}/* ---------------------------------------------------------- OMSMIDIFBNoteOn/Off -- *//* Start MIDI "feedback" note by sending a MIDI NoteOn command for thespecified note and channel. */void CMFBNoteOn(Document *doc, INT16 noteNum, INT16 channel, short ioRefNum){	if (doc->feedback) {		CMStartNoteNow(noteNum, channel, config.feedbackNoteOnVel);		SleepTicks(2L);	}}/* End MIDI "feedback" note by sending a MIDI NoteOn command for thespecified note and channel with velocity 0 (which acts as NoteOff). */void CMFBNoteOff(Document *doc, INT16 noteNum, INT16 channel, short ioRefNum){	if (doc->feedback) {		CMEndNoteNow(noteNum, channel);		SleepTicks(2L);	}}// -------------------------------------------------------------------------------------static MIDIUniqueID GetMIDIObjectId(MIDIObjectRef obj){	MIDIUniqueID id = kInvalidMIDIUniqueID;	OSStatus err = noErr;		if (obj != NULL) {		err = MIDIObjectGetIntegerProperty(obj, kMIDIPropertyUniqueID, &id);	}		if (err != noErr) {		id = kInvalidMIDIUniqueID;	}		return id;}static MIDIEndpointRef GetMIDIEndpointByID(MIDIUniqueID id){	if (id == kInvalidMIDIUniqueID)		return NULL;	int m = MIDIGetNumberOfSources();	for (int i = 0; i < m; ++i) {		MIDIEndpointRef src = MIDIGetSource(i);		MIDIUniqueID srcID = GetMIDIObjectId(src);		if (srcID == id)			return src;	}		int n = MIDIGetNumberOfDestinations();	for (int i = 0; i < n; ++i) {		MIDIEndpointRef dest = MIDIGetDestination(i);		MIDIUniqueID destID = GetMIDIObjectId(dest);		if (destID == id)			return dest;	}		return NULL;}static void GetInitialDefaultInputDevice(){	MIDIEndpointRef src = NULL;	OSStatus err = noErr;		int n = MIDIGetNumberOfSources();	for (int i = 0; i < n; ++i) {		src = MIDIGetSource(i); break;	}		gDefaultInputDevID = GetMIDIObjectId(src);}Boolean CMRecvChannelValid(MIDIUniqueID endPtID, int channel){	MIDIEndpointRef endpt = GetMIDIEndpointByID(endPtID);	long channelMap, channelValid = 0;	OSStatus err = -1;		if (endpt != NULL) {		err = MIDIObjectGetIntegerProperty(endpt, kMIDIPropertyReceiveChannels, &channelMap);	}		if (err == noErr) {			// For one-based channel:		channelValid = (channelMap << (channel-1)) & 0x01;				// For one-based channel:		//channelValid = (channelMap << (channel)) & 0x01;	}		return channelValid;}Boolean CMTransmitChannelValid(MIDIUniqueID endPtID, int channel){	MIDIEndpointRef endpt = GetMIDIEndpointByID(endPtID);	long channelMap, channelValid = 0;	OSStatus err = -1;		if (endpt != NULL) {		err = MIDIObjectGetIntegerProperty(endpt, kMIDIPropertyTransmitChannels, &channelMap);	}		if (err == noErr) {			// For one-based channel:		channelValid = (channelMap << (channel-1)) & 0x01;				// For zero-based channel:		//channelValid = (channelMap << (channel)) & 0x01;	}		return channelValid;}static void CheckDefaultInputDevice(){	MIDIEndpointRef src;			if (gDefaultInputDevID == kInvalidMIDIUniqueID)	{		GetInitialDefaultInputDevice();	}		src = GetMIDIEndpointByID(gDefaultInputDevID);		if (!CMRecvChannelValid(gDefaultInputDevID, gDefaultChannel))	{		int n = MIDIGetNumberOfSources();		for (int i = 0; i < n; ++i) {			MIDIEndpointRef endpt = MIDIGetSource(i);			MIDIUniqueID id = GetMIDIObjectId(endpt);						if (CMRecvChannelValid(id, gDefaultChannel)) {				gDefaultInputDevID = id;				break;			}		}	}}static void GetInitialDefaultOutputDevice(){	MIDIEndpointRef dest = NULL;	OSStatus err = noErr;		int n = MIDIGetNumberOfDestinations();	for (int i = 0; i < n; ++i) {		dest = MIDIGetDestination(i); break;	}		gDefaultOutputDevID = GetMIDIObjectId(dest);}static void CheckDefaultOutputDevice(){	MIDIEndpointRef dest;			if (gDefaultOutputDevID == kInvalidMIDIUniqueID)	{		GetInitialDefaultOutputDevice();	}		dest = GetMIDIEndpointByID(gDefaultOutputDevID);		if (!CMTransmitChannelValid(gDefaultOutputDevID, gDefaultChannel))	{		int n = MIDIGetNumberOfDestinations();		for (int i = 0; i < n; ++i) {			MIDIEndpointRef endpt = MIDIGetDestination(i);			MIDIUniqueID id = GetMIDIObjectId(endpt);						if (CMTransmitChannelValid(id, gDefaultChannel)) {				gDefaultOutputDevID = id;				break;			}		}	}}static void CheckDefaultMetroDevice(){	if (gDefaultMetroDevID == kInvalidMIDIUniqueID)	{		gDefaultMetroDevID = gDefaultOutputDevID;	}	else if (!CMTransmitChannelValid(gDefaultMetroDevID, gDefaultChannel))	{		gDefaultMetroDevID = gDefaultOutputDevID;	}}static void CheckDefaultDevices(){	CheckDefaultInputDevice();		CheckDefaultOutputDevice();		CheckDefaultMetroDevice();}// These calls use a selected input / midi thru device & channel, check if the// device / channel combos are valid, and get IORefNums for each for the MidiReadProc.//// Does Core Midi have an IORefNum for its read proc that needs to do this?void CoreMidiSetSelectedInputDevice(MIDIUniqueID inputDevice, INT16 inputChannel){	gSelectedInputDevice = inputDevice;	gSelectedInputChannel = inputChannel;}void CoreMidiSetSelectedMidiThruDevice(MIDIUniqueID thruDevice, INT16 thruChannel){	gSelectedInputDevice = thruDevice;	gSelectedInputChannel = thruChannel;}void CoreMidiSetSelectedOutputDevice(MIDIUniqueID outputDevice, INT16 outputChannel){	gSelectedOutputDevice = outputDevice;	gSelectedOutputChannel = outputChannel;}MIDIUniqueID CoreMidiGetSelectedOutputDevice(INT16 *outputChannel){	*outputChannel = gSelectedOutputChannel;	return gSelectedOutputDevice;}OSStatus OpenCoreMidiInput(MIDIUniqueID inputDevice){	return noErr;}// --------------------------------------------------------------------------------------/* ---------------------------------------------------------- CreateOMSInputMenu --OMSDeviceMenuH CreateOMSInputMenu(Rect *menuBox){	if (!gSignedIntoOMS) return NULL;	return NewOMSDeviceMenu(					NULL,					odmFrameBox,					menuBox,					omsIncludeInputs + omsIncludeReal,					NULL);} *//* ---------------------------------------------------------- GetCMDeviceForPart -- */MIDIUniqueID GetCMDeviceForPartn(Document *doc, INT16 partn){	if (!doc) doc = currentDoc;	return doc->cmPartDeviceList[partn];}MIDIUniqueID GetCMDeviceForPartL(Document *doc, LINK partL){	INT16 partn;		if (!doc) doc = currentDoc;	partn = (INT16)PartL2Partn(doc, partL);	return doc->cmPartDeviceList[partn];}/* ---------------------------------------------------------- SetCMDeviceForPart -- */void SetCMDeviceForPartn(Document *doc, INT16 partn, MIDIUniqueID device){	if (!doc) doc = currentDoc;	doc->cmPartDeviceList[partn] = device;}void SetCMDeviceForPartL(Document *doc, LINK partL, MIDIUniqueID device){	INT16 partn;		if (!doc) doc = currentDoc;	partn = (INT16)PartL2Partn(doc, partL);	doc->cmPartDeviceList[partn] = device;}/* --------------------------------------------------------- InsertPartnCMDevice -- *//* Insert a null device in front of partn's device */void InsertPartnCMDevice(Document *doc, INT16 partn, INT16 numadd){	register INT16 i, j;	INT16 curLastPartn;		if (!doc) doc = currentDoc;	curLastPartn = LinkNENTRIES(doc->headL) - 1;	/* shift list from partn through curLastPartn up by numadd */	for (i=curLastPartn, j=i+numadd;i >= partn;)		doc->cmPartDeviceList[j--] = doc->cmPartDeviceList[i--];	for (i=partn;i<partn+numadd;)		doc->cmPartDeviceList[i++] = 0;}void InsertPartLCMDevice(Document *doc, LINK partL, INT16 numadd){	INT16 partn;		if (!doc) doc = currentDoc;	partn = (INT16)PartL2Partn(doc, partL);	InsertPartnCMDevice(doc, partn, numadd);}/* --------------------------------------------------------- DeletePartnCMDevice -- */void DeletePartnCMDevice(Document *doc, INT16 partn){	register INT16 i, j;	INT16 curLastPartn;		if (!doc) doc = currentDoc;	curLastPartn = LinkNENTRIES(doc->headL) - 1;	/* Shift list from partn+1 through curLastPartn down by 1 */	for (i=partn+1, j=i-1;i >= curLastPartn;)		doc->cmPartDeviceList[i++] = doc->cmPartDeviceList[j++];	doc->cmPartDeviceList[curLastPartn] = 0;}void DeletePartLCMDevice(Document *doc, LINK partL){	INT16 partn;	if (!doc) doc = currentDoc;	partn = (INT16)PartL2Partn(doc, partL);	DeletePartnCMDevice(doc, partn);}// --------------------------------------------------------------------------------------// MIDI Programs & Patches#define CM_PATCHNUM_BASE 1			/* Some synths start numbering at 1, some at 0 */void CMMIDIProgram(Document *doc, unsigned char *partPatch, unsigned char *partChannel){	INT16 i;	Byte programChange[] = { 0xC0, 0x00 };	MIDITimeStamp tStamp = 0;				// Indicates perform NOW.	OSStatus err = noErr;		if (doc->polyTimbral && !doc->dontSendPatches) {		for (i = 1; i<=LinkNENTRIES(doc->headL) - 1 && err == noErr; i++) {		/* skip dummy part */			programChange[0] = MPGMCHANGE + partChannel[i] - 1;			programChange[1] = partPatch[i] - CM_PATCHNUM_BASE;			err = CMWritePacket(tStamp, 2, programChange);		}	}}// --------------------------------------------------------------------------------------/* ----------------------------------------------------------- GetCMPartPlayInfo -- *//* Similar to the non-CM GetPartPlayInfo. */Boolean GetCMPartPlayInfo(Document *doc, short partTransp[], Byte partChannel[],							Byte partPatch[], SignedByte partVelo[], short partIORefNum[],							MIDIUniqueID partDevice[]){	INT16 i, channel; LINK partL; PARTINFO aPart;		partL = FirstSubLINK(doc->headL);	for (i = 0; i<=LinkNENTRIES(doc->headL)-1; i++, partL = NextPARTINFOL(partL)) {		if (i==0) continue;					/* skip dummy partn = 0 */		aPart = GetPARTINFO(partL);		partVelo[i] = aPart.partVelocity;		partChannel[i] = UseMIDIChannel(doc, i);		partPatch[i] = aPart.patchNum;		partTransp[i] = aPart.transpose;		if (doc->polyTimbral || doc->cmInputDevice != 0)			partDevice[i] = GetCMDeviceForPartn(doc, i);		else			partDevice[i] = doc->cmInputDevice;		/* Validate device / channel combination. */		if (!CMTransmitChannelValid(partDevice[i], (INT16)partChannel[i])) {			partDevice[i] = CoreMidiGetSelectedOutputDevice(&channel);			//partDevice[i] = config.cmDefaultOutputDevice;		}		/* It's possible our device has changed, so validate again (and again, if necc.). */		if (!CMTransmitChannelValid(partDevice[i], (INT16)partChannel[i])) {			partChannel[i] = config.defaultChannel;			if (!CMTransmitChannelValid(partDevice[i], (INT16)partChannel[i])) {				if (CautionAdvise(NO_OMS_DEVS_ALRT)==1) return FALSE;			/* Cancel playback button (item 1 in this ALRT!) */			}		}				partIORefNum[i] = CMInvalidRefNum;		// No IORefNums for CoreMIDI	}	return TRUE;}/* ----------------------------------------------------------- GetCMNotePlayInfo -- *//* Given a note and tables of part transposition, channel, and offset velocity, returnthe note's MIDI note number, including transposition; channel number; and velocity,limited to legal range. Similar to the non-CM GetNotePlayInfo. */void GetCMNotePlayInfo(				Document *doc, LINK aNoteL, short partTransp[],				Byte partChannel[], SignedByte partVelo[], short partIORefNum[],				INT16 *pUseNoteNum, INT16 *pUseChan, INT16 *pUseVelo, short *puseIORefNum){	INT16 partn;	PANOTE aNote;	partn = Staff2Part(doc,NoteSTAFF(aNoteL));	*pUseNoteNum = UseMIDINoteNum(doc, aNoteL, partTransp[partn]);	*pUseChan = partChannel[partn];	aNote = GetPANOTE(aNoteL);	*pUseVelo = doc->velocity+aNote->onVelocity;	if (doc->polyTimbral) *pUseVelo += partVelo[partn];		if (*pUseVelo<1) *pUseVelo = 1;	if (*pUseVelo>MAX_VELOCITY) *pUseVelo = MAX_VELOCITY;	*puseIORefNum = partIORefNum[partn];}// --------------------------------------------------------------------------------------static void DisplayMidiDevices(){	CFStringRef pname, pmanuf, pmodel;	char name[64], manuf[64], model[64];		int n = MIDIGetNumberOfDevices();	for (int i = 0; i < n; ++i) {		MIDIDeviceRef dev = MIDIGetDevice(i);				MIDIObjectGetStringProperty(dev, kMIDIPropertyName, &pname);		MIDIObjectGetStringProperty(dev, kMIDIPropertyManufacturer, &pmanuf);		MIDIObjectGetStringProperty(dev, kMIDIPropertyModel, &pmodel);				CFStringGetCString(pname, name, sizeof(name), 0);		CFStringGetCString(pmanuf, manuf, sizeof(manuf), 0);		CFStringGetCString(pmodel, model, sizeof(model), 0);		CFRelease(pname);		CFRelease(pmanuf);		CFRelease(pmodel);		printf("name=%s, manuf=%s, model=%s\n", name, manuf, model);	}	}Boolean InitCoreMIDI(){	int i, n;		if (!gCoreMIDIInited) {		gCMBufferLength = kCMBufLen;		/* CoreMidi MIDI Buffer size in bytes */		gCMMIDIBufferFull = FALSE;#ifdef CM_DYNAMIC_ALLOC2		if (!AllocCMPktListDyn())			return FALSE;#else		if (!AllocCMPacketList())			return FALSE;#endif					if (!AllocCMWritePacketList())			return FALSE;								// create client and ports		MIDIClientCreate(CFSTR("MIDI Echo"), NULL, NULL, &gClient);		#ifdef CM_DYNAMIC_ALLOC2		MIDIInputPortCreate(gClient, CFSTR("Input port"), NightCMDynReadProc, NULL, &gInPort);#else		MIDIInputPortCreate(gClient, CFSTR("Input port"), NightCMReadProc, NULL, &gInPort);#endif		MIDIOutputPortCreate(gClient, CFSTR("Output port"), &gOutPort);		CheckDefaultDevices();				// open connections from all sources		n = MIDIGetNumberOfSources();		//printf("%d sources\n", n);		for (i = 0; i < n; ++i) {			MIDIEndpointRef src = MIDIGetSource(i);			MIDIPortConnectSource(gInPort, src, NULL);		}				// find the first destination		n = MIDIGetNumberOfDestinations();		if (n > 0)			gDest = MIDIGetDestination(0);				InitCoreMidiTimer();				gSelectedInputDevice = 0L;		gSelectedInputChannel = 0;		gSelectedThruDevice = 0L;		gSelectedThruChannel = 0;				CoreMidiSetSelectedInputDevice(gDefaultInputDevID, gDefaultChannel);				CoreMidiSetSelectedMidiThruDevice(gDefaultOutputDevID, gDefaultChannel);		CoreMidiSetSelectedOutputDevice(gDefaultOutputDevID, gDefaultChannel);		#ifdef CM_DYNAMIC_ALLOC		midiReadPktListVec = new MIDIPktListPtrVector();#endif				//CoreMidiSetSelectedInputDevice(config.cmDefaultInputDevice, config.defaultChannel);				//CoreMidiSetSelectedMidiThruDevice(config.cmMetroDevice, config.defaultChannel);				//DisplayMidiDevices();		gCoreMIDIInited = true;	}		return TRUE;}