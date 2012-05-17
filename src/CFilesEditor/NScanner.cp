/***************************************************************************
*	FILE:	NScanner.c																			*
*	PROJ:	Nightingale, rev. for v 3.5													*
*	DESC:	Routines for Nightingale scanner interface.								*
/***************************************************************************/

/*											NOTICE
 *
 *	THIS FILE IS PART OF THE NIGHTINGALE™ PROGRAM AND IS CONFIDENTIAL PROPERTY OF
 *	ADVANCED MUSIC NOTATION SYSTEMS, INC.  IT IS CONSIDERED A TRADE SECRET AND IS
 *	NOT TO BE DIVULGED OR USED BY PARTIES WHO HAVE NOT RECEIVED WRITTEN
 *	AUTHORIZATION FROM THE OWNER.
 * Copyright © 1995-99 by Advanced Music Notation Systems, Inc. All Rights Reserved.
 *
 */

#include "Nightingale_Prefix.pch"
#include "Nightingale.appl.h"

#ifndef LIGHT_VERSION

#include "NPIFStructs.h"

typedef struct NScanSystem {
	short		numStaves;
	short		numVisStaves;
	LINK		sysL;
	LINK		staffL;
} NScanSystem, *pNScanSystem;

static enum {
	WholeNoteNSVAL = 1,		/* "NS"=NoteScan */
	HalfNoteNSVAL,
	QuarterNoteNSVAL,
	EighthNoteNSVAL,
	SixteenthNoteNSVAL,
	Thirty2NoteNSVAL
} E_NSVal;

static enum {
	StemDown = 1,
	StemUp
} E_NSStem;

static enum {
	MissingOrProblemWithFILE = 1,
	MissingPAGE,
	MissingSYST,
	MissingSTFF,
	NStavesError
} E_NSErr;

#define NoGood -1

static enum {
	UnknownClefNSVAL = 0,
	TrebleClefNSVAL,
	BassClefNSVAL,
	AltoClefNSVAL,
	TenorClefNSVAL,
	MaxClefNSVAL = TenorClefNSVAL
} E_NSClef;

#define UnknownKeySigNSVAL 999

static short FILEXResolution,
				PAGEpageWidth,
				PAGEpageHt,
				SYSTnumberOfStaves,
				SYSTvisibleStaves,
				STFFspacePixels,
				CLEFhPlacement,
				CLEFclef,
				KEYhPlacement,
				KEYkey,
				TIMEhPlacement,
				TIMEvisible,
				TIMEtopN,
				TIMEwidth,
				TIMEbottomN,
				NOTEhPlacement,
				BEAMnParts,
				BEAMSnoteHPlacement,
				RESThPlacement,
				BARhPlacement;

static char NOTEvSclPos,
				NOTEaccdntl,
				NOTEtimeVal,
				NOTEnDots,
				NOTEdirectn,
				RESTvSclPos,
				RESTtimeVal,
				RESTnDots;

static float scaleFactor = .2727272;	/* Value may be (always is?) reset by GetScaleFactor */
short 		staffNum,
				currSystem,
				debug = 0;			/* (unused) 0 = put barlines from top to bottom of system
												1 = show barlines as found in NPIF file  */

#if !TARGET_API_MAC_CARBON_FILEIO
static Point dialogWhere = { 90, 82 };
static SFReply reply;
#endif

static Boolean firstScoreSys;		/* TRUE=working on the 1st system of the score */
static char *recPtr;

static NScanSystem *scanSystems;

/* --------------------------------------------------------------------------------- */
/* Local prototypes */

static Boolean GetScaleFactor(void);
static Boolean HandleFile(char *recPtr);
static void HandlePage(char *recPtr);
static void HandleSystem(char *recPtr);
static void HandleStaff(char *recPtr);
static void HandleClef(char *recPtr);
static void HandleKeySignature(char *recPtr);
static void HandleTimeSignature(char *recPtr);
static void HandleNote(char *recPtr);
static void HandleBeam(char *recPtr);
static void BeamPart(short i, npifBEAMPtr npifBeam);
static void HandleRest(char *recPtr);
static void HandleBar(char *recPtr);
static void EndOfSystem(void);
static void HandleEndOfFile(char *recPtr);
static void HandleOther(char *recPtr);

static LINK AddScanSystem(Document *doc,LINK prevL,LINK pageL,LINK sysL,DDIST sysTop);

static char *AdvanceRecPtr(char *p);
static short NPIFDur2NightDur(short durCode);
static void ResetVoiceTable(Document *doc);
static void FixXMeasLinks(LINK headL,LINK tailL);

static LINK MakeScanStaff(Document *, LINK, LINK, LINK, DDIST, INT16, DDIST []);
static LINK MakeScanConnect(Document *, LINK, LINK, INT16);

static LINK CreateScanStaff(Document *doc, LINK sysL, LINK *connL, LINK staffA[]);
static LINK CreateScanClef(Document *doc, LINK prevL, LINK staffA[], DDIST spBefore);
static LINK CreateScanKeySig(Document *doc, LINK prevL, LINK staffA[], DDIST spBefore);
static LINK CreateScanTimeSig(Document *doc, LINK prevL, LINK staffA[], DDIST spBefore);
static void SetBarlineVis(LINK measL,Boolean visible);
static Boolean Scan1stMeasInSys(LINK measL);
static LINK CreateScanMeasure(Document *doc, LINK prevL, LINK staffA[], DDIST spBefore,
								Boolean firstMeasure);
static void UpdateMeasureContexts(Document *doc);
static void UpdateStaffContexts(Document *doc);

static Boolean ChkMeasConsistent(Document *doc,LINK staffL[]);
static Boolean StaffDone(LINK syncs[],short i);
static void InitializeStaves(short nstaves,LINK staffA[],LINK syncs[],char addToObj[],char done[]);
static Boolean StavesDone(short nstaves, char done[]);
static void IncrementStaves(short nstaves,LINK syncs[],LINK lastObj[],char addToObj[],char done[]);
static DDIST GetObjToAdd(short nstaves,LINK syncs[],char addToObj[],char done[]);
static void GetScanMeasMultiV(LINK staffA[],short nstaves,char multiv[]);
static short SetupScanNote(Document *doc,LINK pL,LINK aNoteL,char multiv[],short altv[]);
static LINK AddSyncNote(LINK objL,LINK pL,LINK aNoteL);
static short CountNotesOnStaff(LINK pL,short s);

static LINK TraverseScanMeasure(Document *doc, LINK prevL, LINK staffA[],short altv[],short nStaves);
static void DisposeStfLists(Document *doc,LINK staffA[],LINK stfTailL[]);
static void Normalize2FirstMeas(Document *doc,LINK sysL);

static Boolean SetClefDialog(short, short, short *, Boolean [MaxClefNSVAL]);
static Boolean SetKeySigDialog(short, short, short *, Boolean *);
static Boolean CreateMeasScanObjs(Document *, LINK, Boolean, Boolean, Ptr [], LINK []);
static LINK AddSystemContents(Document *doc, LINK sysL, LINK *connL, char *endPtr,
									short altv[], Boolean overrideCl, Boolean overrideKS);

static void FixPageWidth(Document *doc,short width);
static void FixForStaffSize(Document *, short);

static Document *CreateScanDoc(unsigned char *fileName,short vRefNum,FSSpec *pfsSpec,short pageWidth,
									short pageHt,short nStaves,short rastral);
static void SetupScanDoc(Document *newDoc);
static short GetScanFileName(Str255 fn, short *vRef );

static short CheckScanFile(char *buffer,long length, short *pageWidth, short *pageHt, short *nStaves,
							short *rastral);
static LINK InsertPage(Document *doc,LINK prevL);
static Boolean GetScoreParams(INT16 vRefNum,INT16 refNum,short *pageWidth,short *pageHt,short *nStaves,
								short *rastral);
static short ConvertFile(Document *doc,INT16 refNum, INT16 vRefNum, Boolean overrideCl,
								Boolean overrideKS);

/*
 * Set the scaleFactor based on the resolution stored in the FILE object.
 */

static Boolean GetScaleFactor()
{
	if (FILEXResolution>=100 || FILEXResolution<=400) {
		scaleFactor = (float)POINTSPERIN/(float)FILEXResolution;
		return TRUE;
	}
	return FALSE;
}

#define MAXSYSTEMS	20			/* Max. systems per page */
#ifdef LIGHT_VERSION
#define NSMAXSTAVES MAXSTAVES	/* Should never be more than MAXSTAVES! */
#else
#define NSMAXSTAVES 40			/* Should never be more than MAXSTAVES! */
#endif

/* ---------------------------------------------------------- HandleXXX functions -- */

/*
 * Handle a record of type FILE from a scanner file.
 */

static Boolean HandleFile(char *recPtr)
{
	npifFILEPtr npifFile;

	npifFile = (npifFILEPtr)recPtr;

	scanSystems = (pNScanSystem)NewPtr((Size)sizeof(NScanSystem) * 
											npifFile->numberOfPages * MAXSYSTEMS);
	if (!scanSystems)
		{ NoMoreMemory(); return FALSE; }
	
	FILEXResolution	= npifFile->XResolution;
	
	if (GetScaleFactor())
		return TRUE;
	else {
		GetIndCString(strBuf, NOTESCANERRS_STRS, 4);		/* "Resolution is illegal" */
		CParamText(strBuf, "", "", "");
		StopInform(GENERIC_ALRT);
		return FALSE;
	}
}

/*
 * Handle a record of type PAGE from a scanner file.
 */

static void HandlePage(char *recPtr)
{
	npifPAGEPtr npifPage;

	npifPage = (npifPAGEPtr)recPtr;

	PAGEpageWidth	= npifPage->pageWidth;
	PAGEpageHt		= npifPage->pageDepth;
}

/*
 * Handle a record of type SYST from a scanner file.
 */

static void HandleSystem(char *recPtr)
{
	npifSYSTPtr npifSystem;

	npifSystem = (npifSYSTPtr)recPtr;

	SYSTnumberOfStaves = npifSystem->numberOfStaves;
 	SYSTvisibleStaves = npifSystem->visibleStaves;

	if (staffNum == SYSTnumberOfStaves)
		if (!debug) 
			EndOfSystem();

	staffNum = 0;
}

/*
 * Handle a record of type STFF from a scanner file.
 */

static void HandleStaff(char *recPtr)
{
   npifSTFFPtr npifStaff;

	npifStaff = (npifSTFFPtr)recPtr;

	STFFspacePixels         = npifStaff->spacePixels;

   staffNum++;
}

/*
 * Handle a record of type CLEF from a scanner file.
 */

static void HandleClef(char *recPtr)
{
	npifCLEFPtr npifClef;

	npifClef = (npifCLEFPtr)recPtr;

	CLEFhPlacement = npifClef->horizontalPlacement * scaleFactor;
	CLEFclef       = npifClef->clef;
}

/*
 * Handle a record of type KYSG from a scanner file.
 */

static void HandleKeySignature(char *recPtr)
{
	npifKYSGPtr npifKeySignature;

	npifKeySignature = (npifKYSGPtr)recPtr;

	KEYhPlacement = npifKeySignature->horizontalPlacement * scaleFactor;
	KEYkey	= npifKeySignature->key;

}

/*
 * Handle a record of type TIME from a scanner file.
 */

static void HandleTimeSignature(char *recPtr)
{
	npifTMSGPtr npifTimeSignature;

	npifTimeSignature = (npifTMSGPtr)recPtr;

	TIMEhPlacement  = npifTimeSignature->horizontalPlacement;
	TIMEvisible  = npifTimeSignature->visible;
	TIMEtopN	= npifTimeSignature->topNumber;
	TIMEwidth    = npifTimeSignature->width;
	TIMEbottomN  = npifTimeSignature->bottomNumber;
}

/*
 * Handle a record of type NOTE from a scanner file.
 */

static void HandleNote(char *recPtr)
{
	npifNOTEPtr npifNote;

	npifNote = (npifNOTEPtr)recPtr;

	NOTEhPlacement  = npifNote->horizontalPlacement * scaleFactor;
	NOTEvSclPos  = npifNote->verticalScalePos;
	NOTEaccdntl  = npifNote->accidental;
	NOTEtimeVal  = npifNote->timeValue;
	NOTEnDots	= npifNote->numberOfDots;
	NOTEdirectn  = npifNote->stem.direction;
}

/*
 * Handle a record of type REST from a scanner file.
 */

static void HandleRest(char *recPtr)
{
	npifRESTPtr npifRest;
    
	npifRest = (npifRESTPtr)recPtr;
	
	RESThPlacement  = npifRest->horizontalPlacement * scaleFactor;
	RESTvSclPos  = npifRest->verticalScalePos;
	RESTtimeVal  = npifRest->timeValue;
      
	RESTnDots    = npifRest->numberOfDots;
}

/*
 * Handle a record of type 'BAR ' from a scanner file.
 */

static void HandleBar(char *recPtr)
{
	npifBARPtr npifBar;

	npifBar = (npifBARPtr)recPtr;

	BARhPlacement = npifBar->barStart * scaleFactor;
}

static void EndOfSystem()
{
	/* No longer needs to do anything. */
}

static void HandleEndOfFile(char *recPtr)
{
    npifEOFPtr npifEndOfFile;

    npifEndOfFile = (npifEOFPtr)recPtr;
    
    if (!debug)
       EndOfSystem();
}

static void HandleOther(char *recPtr)
{
	npifOtherPtr npifOther;
   
   npifOther = (npifOtherPtr)recPtr;
}


/* -------------------------------------------------------------------- Utilities -- */

static LINK AddScanSystem(Document *doc, LINK prevL, LINK pageL, LINK sysL,
									DDIST sysTop)
{
	short sysWhere;

	if (firstScoreSys)
		sysWhere = firstSystem;
	else if (PageTYPE(prevL))
		sysWhere = firstOnPage;
	else sysWhere = succSystem;

	if (firstScoreSys)
		sysL = SSearch(doc->headL,SYSTEMtype,GO_RIGHT);
	else
		sysL = MakeSystem(doc, prevL, pageL, sysL, sysTop, sysWhere);

	return sysL;
}

/*
 * Functions for AddSystemContents, which add the contents of the system from
 * the intermediate data structures to a system created by AddScanSystem.
 */

static char *AdvanceRecPtr(char *p)
{
	char *lenPtr;
	short recLength;

	lenPtr = &p[4];
	recLength = *(short *)lenPtr;
	p += recLength;
	return p;
}

/*
 * Convert NoteScan duration code to Nightingale equivalent. ??This only covers
 * whole to 32nd durations: Nightingale has both longer and shorter durations,
 * but maybe NoteScan doesn't.
 */

short NPIFDur2NightDur(short durCode)
{
	switch(durCode) {
		case WholeNoteNSVAL:
			return WHOLE_L_DUR;
		case HalfNoteNSVAL:
			return HALF_L_DUR;
		case QuarterNoteNSVAL:
			return QTR_L_DUR;
		case EighthNoteNSVAL:
			return EIGHTH_L_DUR;
		case SixteenthNoteNSVAL:
			return SIXTEENTH_L_DUR;
		case Thirty2NoteNSVAL:
			return THIRTY2ND_L_DUR;
	}
	
	return UNKNOWN_L_DUR;
}

static void FixXMeasLinks(LINK headL, LINK tailL)
{
	LINK pL,rMeas,lMeas;
	
	for (pL=headL; pL!=tailL; pL=RightLINK(pL))
		if (MeasureTYPE(pL)) {
			lMeas = SSearch(LeftLINK(pL),MEASUREtype,GO_LEFT);
			rMeas = SSearch(RightLINK(pL),MEASUREtype,GO_RIGHT);
			
			LinkLMEAS(pL) = lMeas;
			LinkRMEAS(pL) = rMeas;
		}
}

static void ResetVoiceTable(Document *doc)
{
	short v;
	
	for (v=1; v<=MAXVOICES; v++)
		if (VOICE_MAYBE_USED(doc, v)) {
			doc->voiceTab[v].voiceRole = SINGLE_DI;
		}
}


/* ---------------------------------------------------------------- MakeScanStaff -- */
/* Insert a new Staff after prevL in some object list belonging to doc. Return
new Staff's LINK, or NILINK. Does not set the Staff's context fields: the
calling routine must do so. */

static LINK MakeScanStaff(Document *doc,
									LINK prevL, LINK prevStaffL, LINK systemL,
									DDIST staffLength, INT16 /*where*/, DDIST /*staffTop*/[])
{
	LINK pL, nextStaffL, aStaffL, copyStaffL;
	PASTAFF aStaff;
	
	/* Copy the previous Staff, if there is one, otherwise get it from Master Page. */
	
	if (prevStaffL)
			copyStaffL = prevStaffL;
	else	copyStaffL = SSearch(doc->masterHeadL,STAFFtype,GO_RIGHT);

	pL = DuplicateObject(STAFFtype, copyStaffL, FALSE, doc, doc, FALSE);
	if (!pL) {
		NoMoreMemory(); return NILINK;
	}

	/* Pay attention to the requested <staffLength>. */
	
	aStaffL = FirstSubLINK(pL);
	for ( ; aStaffL; aStaffL = NextSTAFFL(aStaffL)) {
		aStaff = GetPASTAFF(aStaffL);
		aStaff->staffRight = staffLength;
	}
	
	InsNodeInto(pL,RightLINK(prevL));
	nextStaffL = SSearch(RightLINK(pL), STAFFtype, GO_RIGHT);
	
	LinkLSTAFF(pL) = prevStaffL;
	LinkRSTAFF(pL) = nextStaffL;
	if (prevStaffL) LinkRSTAFF(prevStaffL) = pL;
	if (nextStaffL) LinkLSTAFF(nextStaffL) = pL;
	
	StaffSYS(pL) = systemL;
	
	return pL;
}

/* ------------------------------------------------------------- MakeScanConnect -- */
/* Insert a new Connect after prevL in some object list belonging to doc. Return
new Connect's LINK, or NILINK. */

LINK MakeScanConnect(Document *doc, LINK prevL, LINK prevConnectL, INT16 /*where*/)
{
	LINK pL, copyConnL;
	
	/* Copy the previous Connect, if there is one, otherwise get it from Master Page. */

	if (prevConnectL)
			copyConnL = prevConnectL;
	else	copyConnL = SSearch(doc->masterHeadL,CONNECTtype,GO_RIGHT);

	pL = DuplicateObject(CONNECTtype, copyConnL, FALSE, doc, doc, FALSE);
	if (!pL) {
		NoMoreMemory(); return NILINK;
	}
	
	InsNodeInto(pL,RightLINK(prevL));
	return pL;
}

/* -------------------------------------------------- CreateScanObject functions -- */

static LINK CreateScanStaff(Document *doc,
										LINK sysL,
										LINK *connL,
										LINK /*staffA*/[])			/* unused */
{
	short sysWhere;
	LINK staffL,aStaffL,connectL,prevStaffL;
	DDIST staffLength,staffTop[NSMAXSTAVES+1];
	PASTAFF aStaff;

	if (firstScoreSys) {
		sysWhere = firstSystem;
		staffL = SSearch(doc->headL,STAFFtype,GO_RIGHT);
		*connL = SSearch(staffL,CONNECTtype,GO_RIGHT);
		
		return staffL;
	}
	else if (PageTYPE(LeftLINK(sysL))) {
		sysWhere = firstOnPage;
		staffLength = MARGWIDTH(doc)-doc->otherIndent;
	}
	else {
		sysWhere = succSystem;
		staffLength = MARGWIDTH(doc)-doc->otherIndent;
	}

	prevStaffL = SSearch(sysL,STAFFtype,GO_LEFT);
	aStaffL = FirstSubLINK(prevStaffL);
	for ( ; aStaffL; aStaffL=NextSTAFFL(aStaffL)) {
		aStaff = GetPASTAFF(aStaffL);
		staffTop[StaffSTAFF(aStaffL)] = aStaff->staffTop;
	}

	staffL = MakeScanStaff(doc, sysL, prevStaffL, sysL, staffLength, sysWhere, staffTop);

	connectL = MakeScanConnect(doc, staffL, *connL, sysWhere);
	*connL = connectL;
	
	return staffL;
}

static LINK CreateScanClef(Document *doc, LINK prevL, LINK staffA[], DDIST spBefore)
{
	short i,j,visible=FALSE,nExpand;
	LINK objL,clefL,aClefL,bClefL,clefList;
	
	if (firstScoreSys) {
		clefL = SSearch(doc->headL,CLEFtype,GO_RIGHT);
		
		for (i=1; i<=SYSTnumberOfStaves; i++) {
			objL = staffA[i]; 
			if (ClefTYPE(objL)) {
				aClefL = ClefOnStaff(clefL, i);
				bClefL = FirstSubLINK(objL);
				
				ClefType(aClefL) = ClefType(bClefL);
				ClefVIS(aClefL) = ClefVIS(bClefL);
				if (ClefVIS(aClefL)) visible = TRUE;

				UpdateBFClefStaff(clefL,i,ClefType(aClefL));
			}
		}

		LinkVIS(clefL) = visible;
		return clefL;
	}

	clefL = DuplicateObject(CLEFtype,staffA[1],FALSE,doc,doc,FALSE);
	if (!clefL) {
		NoMoreMemory(); return NILINK;
	}
	aClefL = FirstSubLINK(clefL);
	if (ClefVIS(aClefL)) visible = TRUE;

	if (SYSTvisibleStaves==SYSTnumberOfStaves) {
		for (i=2; i<=SYSTnumberOfStaves; i++) {
			objL = staffA[i]; 
			if (ClefTYPE(objL)) {
				bClefL = FirstSubLINK(objL);
				FirstSubLINK(objL) = NILINK;
				LinkNENTRIES(objL)--;
				NextCLEFL(aClefL) = bClefL;
				aClefL = bClefL;
				LinkNENTRIES(clefL)++;
				if (ClefVIS(aClefL)) visible = TRUE;
			}
		}
	}
	else {
		for (i=2; i<=SYSTvisibleStaves; i++) {
			objL = staffA[i]; 
			if (ClefTYPE(objL)) {
				bClefL = FirstSubLINK(objL);
				FirstSubLINK(objL) = NILINK;
				LinkNENTRIES(objL)--;
				NextCLEFL(aClefL) = bClefL;
				aClefL = bClefL;
				LinkNENTRIES(clefL)++;
				if (ClefVIS(aClefL)) visible = TRUE;
			}
		}
		
		for (nExpand=0,j=i; i<=SYSTnumberOfStaves; i++) {
			nExpand++;
		}
		
		ExpandNode(clefL, &clefList, nExpand);

		for (aClefL=clefList; aClefL && j<=SYSTnumberOfStaves; j++,aClefL=NextCLEFL(aClefL)) {
			InitClef(aClefL, j, 0, TREBLE_CLEF);
		}		
	}
		
	InsNodeInto(clefL, RightLINK(prevL));
	
	/* ??Don't we want LinkXD(prevL)+spBefore ? (CER comment!) */

	SetObject(clefL, spBefore, 0, FALSE, visible, TRUE);
	ClefINMEAS(clefL) = FALSE;

	return clefL;
}

static LINK CreateScanKeySig(Document *doc, LINK prevL, LINK staffA[], DDIST spBefore)
{
	short i,j,nExpand,visible=FALSE;
	LINK objL,keySigL,aKeySigL,bKeySigL,keySigList;
	PAKEYSIG aKeySig,bKeySig; KSINFO newKSInfo;
	
	if (firstScoreSys) {
		keySigL = SSearch(doc->headL,KEYSIGtype,GO_RIGHT);
		
		for (i=1; i<=SYSTnumberOfStaves; i++) {
			objL = staffA[i]; 
			if (KeySigTYPE(objL)) {
				KeySigINMEAS(keySigL) = FALSE;

				aKeySigL = KeySigOnStaff(keySigL, i);
				bKeySigL = FirstSubLINK(objL);
				
				aKeySig = GetPAKEYSIG(aKeySigL);
				bKeySig = GetPAKEYSIG(bKeySigL);
				
				aKeySig->nKSItems = bKeySig->nKSItems;
				KEYSIG_COPY((PKSINFO)bKeySig->KSItem, (PKSINFO)aKeySig->KSItem);
				KEYSIG_COPY((PKSINFO)aKeySig->KSItem, &newKSInfo);
				aKeySig->visible = (aKeySig->nKSItems!=0);
				if (aKeySig->visible) visible = TRUE;

				UpdateBFKSStaff(keySigL,i,newKSInfo);

			}
		}

		LinkVIS(keySigL) = visible;
		LinkXD(keySigL) = LinkXD(prevL) + spBefore;
		return keySigL;
	}

	keySigL = DuplicateObject(KEYSIGtype,staffA[1],FALSE,doc,doc,FALSE);
	if (!keySigL) {
		NoMoreMemory(); return NILINK;
	}
	aKeySigL = FirstSubLINK(keySigL);
	if (KeySigVIS(aKeySigL)) visible = TRUE;

	if (SYSTvisibleStaves==SYSTnumberOfStaves) {
		for (i=2; i<=SYSTnumberOfStaves; i++) {
			objL = staffA[i];
			if (KeySigTYPE(objL)) {
				bKeySigL = FirstSubLINK(objL);
				FirstSubLINK(objL) = NILINK;
				LinkNENTRIES(objL)--;
				NextKEYSIGL(aKeySigL) = bKeySigL;
				aKeySigL = bKeySigL;
				LinkNENTRIES(keySigL)++;
				aKeySig = GetPAKEYSIG(aKeySigL);
				if (aKeySig->visible) visible = TRUE;
			}
		}
	}
	else {
		for (i=2; i<=SYSTvisibleStaves; i++) {
			objL = staffA[i]; 
			if (KeySigTYPE(objL)) {
				bKeySigL = FirstSubLINK(objL);
				FirstSubLINK(objL) = NILINK;
				LinkNENTRIES(objL)--;
				NextKEYSIGL(aKeySigL) = bKeySigL;
				aKeySigL = bKeySigL;
				LinkNENTRIES(keySigL)++;
				aKeySig = GetPAKEYSIG(aKeySigL);
				if (aKeySig->visible) visible = TRUE;
			}
		}
		
		for (nExpand=0,j=i; i<=SYSTnumberOfStaves; i++) {
			nExpand++;
		}
		
		ExpandNode(keySigL, &keySigList, nExpand);

		for (aKeySigL=keySigList; aKeySigL && j<=SYSTnumberOfStaves; j++,aKeySigL=NextKEYSIGL(aKeySigL)) {
			InitKeySig(aKeySigL, j, 0, 0);
		}		
	}
		
	InsNodeInto(keySigL, RightLINK(prevL));

	SetObject(keySigL, LinkXD(prevL)+spBefore, 0, FALSE, visible, TRUE);
	KeySigINMEAS(keySigL) = FALSE;

	return keySigL;
}

static LINK CreateScanTimeSig(Document *doc, LINK prevL, LINK staffA[], DDIST spBefore)
{
	short i,num,denom,visible=FALSE;
	LINK objL,timeSigL,aTimeSigL,bTimeSigL;
	PATIMESIG aTimeSig,bTimeSig;
	
	if (firstScoreSys) {
		timeSigL = SSearch(doc->headL,TIMESIGtype,GO_RIGHT);
		
		for (i=1; i<=SYSTnumberOfStaves; i++) {
			objL = staffA[i]; 
			if (TimeSigTYPE(objL)) {
				TimeSigINMEAS(timeSigL) = FALSE;
				LinkXD(timeSigL) = LinkXD(prevL)+spBefore;

				aTimeSigL = TimeSigOnStaff(timeSigL, i);
				bTimeSigL = FirstSubLINK(objL);
				
				visible = TimeSigVIS(aTimeSigL) = TimeSigVIS(bTimeSigL);
				
				aTimeSig = GetPATIMESIG(aTimeSigL);
				bTimeSig = GetPATIMESIG(bTimeSigL);
				num = bTimeSig->numerator;
				denom = bTimeSig->denominator;

				InitTimeSig(aTimeSigL, i, 0, DFLT_TSTYPE, num, denom);

				UpdateBFTSStaff(timeSigL,i,DFLT_TSTYPE,num,denom);

			}
		}

		LinkVIS(timeSigL) = visible;
		LinkXD(timeSigL) = LinkXD(prevL) + spBefore;
		return timeSigL;
	}

	timeSigL = DuplicateObject(TIMESIGtype,staffA[1],FALSE,doc,doc,FALSE);
	if (!timeSigL) {
		NoMoreMemory(); return NILINK;
	}
	aTimeSigL = FirstSubLINK(timeSigL);
	if (TimeSigVIS(aTimeSigL)) visible = TRUE;

	for (i=2; i<=SYSTnumberOfStaves; i++) {
		objL = staffA[i];
		if (TimeSigTYPE(objL)) {
			bTimeSigL = FirstSubLINK(objL);
			FirstSubLINK(objL) = NILINK;
			LinkNENTRIES(objL)--;
			NextTIMESIGL(aTimeSigL) = bTimeSigL;
			aTimeSigL = bTimeSigL;
			LinkNENTRIES(timeSigL)++;
			if (TimeSigVIS(aTimeSigL)) visible = TRUE;
		}
	}
	
	InsNodeInto(timeSigL, RightLINK(prevL));

	SetObject(timeSigL, LinkXD(prevL)+spBefore, 0, FALSE, visible, TRUE);
	TimeSigINMEAS(timeSigL) = FALSE;

	return timeSigL;
}

static void SetBarlineVis(LINK measL, Boolean visible)
{
	LINK aMeasL;

	LinkVIS(measL) = visible;
	
	aMeasL = FirstSubLINK(measL);
	for ( ; aMeasL; aMeasL = NextMEASUREL(aMeasL))
		MeasureVIS(aMeasL) = visible;
}

static Boolean Scan1stMeasInSys(LINK measL)
{
	LINK lMeas;

	/* If not 1st system in score, will be first in system if in diff system
		from lMeas. Otherwise, lMeas is before it in its system. ??CER: SAY WHAT? */

	lMeas = LinkLMEAS(measL);
	if (lMeas) {
		return (MeasSYSL(lMeas) != MeasSYSL(measL));
	}

	return TRUE;						/* first in score => first in sys */
}

/*
 * If we are at the first measure of the first system of the score, update fields
 * for this measure object, which is already in the data structure, and return it.
 * Otherwise, duplicate the previous measure object, update its fields, and return
 * it.
 */

static LINK CreateScanMeasure(Document *doc, LINK prevL, LINK staffA[], DDIST spBefore,
										Boolean firstMeasure)
{
	LINK objL,measureL,prevMeasL,endMeasL;

	if (firstScoreSys && firstMeasure) {
		measureL = SSearch(doc->headL,MEASUREtype,GO_RIGHT);
		LinkXD(measureL) = LinkXD(prevL)+spBefore;
		MeasureTIME(measureL) = 0L;
		
		return measureL;
	}

	prevMeasL = SSearch(prevL, MEASUREtype, GO_LEFT);
	
	measureL = DuplicateObject(MEASUREtype,prevMeasL,FALSE,doc,doc,FALSE);
	if (!measureL) {
		NoMoreMemory(); return NILINK;
	}
	InsNodeInto(measureL, RightLINK(prevL));
	
	FixStructureLinks(doc,doc,doc->headL,doc->tailL);

	objL = staffA[1];
	LinkXD(measureL) = LinkXD(objL);
	prevMeasL = LinkLMEAS(measureL);

	if (prevMeasL==NILINK)
		MeasureTIME(measureL) = 0L;
	else {
		endMeasL = EndMeasSearch(doc, prevMeasL);
		MeasureTIME(measureL) = MeasureTIME(prevMeasL)+GetMeasDur(doc, endMeasL);
	}

	SetBarlineVis(measureL,!Scan1stMeasInSys(measureL));

	return measureL;
}

static void UpdateMeasureContexts(Document *doc)
{
	LINK measL,aMeasL;
	CONTEXT context;
	
	measL = SSearch(doc->headL,MEASUREtype,GO_RIGHT);

	for ( ; measL; measL = LinkRMEAS(measL)) {
		aMeasL=FirstSubLINK(measL);
		for ( ; aMeasL; aMeasL=NextMEASUREL(aMeasL)) {
			GetContext(doc, LeftLINK(measL), MeasureSTAFF(aMeasL), &context);
			FixMeasureContext(aMeasL, &context);
		}
	}
}

static void UpdateStaffContexts(Document *doc)
{
	LINK staffL,aStaffL;
	CONTEXT context;
	
	staffL = SSearch(doc->headL,STAFFtype,GO_RIGHT);

	for ( ; staffL; staffL = LinkRSTAFF(staffL)) {
		aStaffL=FirstSubLINK(staffL);
		for ( ; aStaffL; aStaffL=NextSTAFFL(aStaffL)) {
			GetContext(doc, LeftLINK(staffL), StaffSTAFF(aStaffL), &context);
			FixStaffContext(aStaffL, &context);
		}
	}
}

/* -------------------------------------------------------------------------------- */
/* Functions for TraverseScanMeasure, which adds the contents of a single
measure from the NPIF file to the score object list. */


/*
 * To be called at a point where all current LINKs in the array of list of LINKs
 * staffL[] are expected to be MEASUREs. If all are (??CER: or if *none* are: why?),
 *	return TRUE; else return FALSE.
 */

static Boolean ChkMeasConsistent(Document *doc, LINK staffL[])
{
	short i;
	Boolean hasMeas=FALSE;

	for (i=1; i<=doc->nstaves; i++)
		if (MeasureTYPE(staffL[i]))
			hasMeas = TRUE;
			
	if (!hasMeas)
		return TRUE;

	for (i=1; i<=doc->nstaves; i++)
		if (!MeasureTYPE(staffL[i]))
			return FALSE;
	
	return TRUE;
}

static Boolean StaffDone(LINK syncs[], short i)
{
	if (!syncs[i] || MeasureTYPE(syncs[i]) || TailTYPE(syncs[i]))
		return TRUE;

	return FALSE;
}

static void InitializeStaves(short nstaves, LINK staffA[], LINK syncs[],
										char addToObj[], char done[])
{
	short i;

	for (i=1; i<=nstaves; i++) {
		syncs[i] = staffA[i];
		addToObj[i] = FALSE;
		done[i] = FALSE;						/* ??CER: THIS IS IMMEDIATELY OVERRIDDEN--?? */
		
		done[i] = StaffDone(syncs,i);
	}
}

static Boolean StavesDone(short nstaves, char done[])
{
	short i;

	for (i=1; i<=nstaves; i++)
		if (!done[i]) return FALSE;
		
	return TRUE;
}

static void IncrementStaves(short nstaves, LINK syncs[], LINK lastObj[],
										char addToObj[], char done[])
{
	short i;

	for (i=1; i<=nstaves; i++)
		if (!done[i]) {
			if (addToObj[i]) {
				syncs[i] = RightLINK(lastObj[i]);
				done[i] = StaffDone(syncs,i);
			}
		}

	for (i=1; i<=nstaves; i++)
		if (addToObj[i])
			addToObj[i] = FALSE;
}

/* Find for the next object or objects to add to the object list by looking at
<syncs> for every staff for which <!done>. If any such staff has a Beamset in <syncs>,
that Beamset alone is the object. Otherwise, find the object in <syncs> that's
furthest left. Then the objects we want are all those in <syncs> of that object's
type that are at most config.noteScanEpsXD to the right of it.

Return with <addToObj> for every staff with an object we want. Also, if the object
isn't a Beamset, return as function value the xd of the furthest-left object; if
it is a Beamset, return 0. If we find nothing, return ERROR_INT. */

static DDIST GetObjToAdd(short nstaves, LINK syncs[], char addToObj[], char done[])
{
	short i,type=STAFFtype;
	DDIST minxd=SHRT_MAX;
	Boolean gotSomething;

	for (i=1; i<=nstaves; i++)
		if (!done[i]) {
			if (BeamsetTYPE(syncs[i])) {
				addToObj[i] = TRUE; return 0;
			}
		}

	gotSomething = FALSE;
	for (i=1; i<=nstaves; i++)
		if (!done[i]) {
			if (!BeamsetTYPE(syncs[i])) {
				minxd = n_min(LinkXD(syncs[i]), minxd);
				if (LinkXD(syncs[i]) == minxd)
					type = ObjLType(syncs[i]);
			}
		}
	
	for (i=1; i<=nstaves; i++) {
		if (!done[i] && ObjLType(syncs[i]) == type)
			if (LinkXD(syncs[i]) <= minxd + pt2d(config.noteScanEpsXD)) {
				gotSomething = TRUE;
				addToObj[i] = TRUE;
			}
	}
	
	return (gotSomething? minxd : ERROR_INT);
}

/* For every staff, determine if that staff has at least one stem-up and one stem-
down note in notes that will end up in a single Sync in the measure: if so, report
that it's multivoice. NB: This function must use the same criteria for syncing as
will actually be used (by GetObjToAdd, I think) or we'll be in trouble! */

static void GetScanMeasMultiV(
						LINK staffA[],			/* Staff-by-staff object lists */
						short nstaves,
						char multiv[] 			/* Output: multivoice flags (TRUE=staff is multivoice) */
						)		
{
	short i;
	LINK pL,aNoteL,qL,bNoteL;
	Boolean hasDown,hasUp;
	DDIST syncXD,nextXD,epsXD;

	for (i=1; i<=nstaves; i++)
		multiv[i] = FALSE;

	epsXD = pt2d(config.noteScanEpsXD);
	for (i=1; i<=nstaves; i++) {
		pL = staffA[i];
		hasDown = hasUp = FALSE;

		for ( ; pL && !MeasureTYPE(pL) && !TailTYPE(pL); pL=RightLINK(pL))
			if (SyncTYPE(pL)) {
				aNoteL = FirstSubLINK(pL);
				
				/*	Use direction flags we've previously stored in the NoteYSTEM. */
				if (NoteYSTEM(aNoteL) == StemDown) hasDown = TRUE;
				if (NoteYSTEM(aNoteL) == StemUp) hasUp = TRUE;
				
				syncXD = LinkXD(pL);
				qL = RightLINK(pL);
				for ( ; qL && !MeasureTYPE(qL) && !TailTYPE(qL); qL=RightLINK(qL)) {
					if (SyncTYPE(qL)) {
						nextXD = LinkXD(qL);
						if (nextXD <= syncXD + epsXD && nextXD >= syncXD - epsXD) {
							
							bNoteL = FirstSubLINK(qL);
	
							if (NoteYSTEM(bNoteL) == StemDown) hasDown = TRUE;
							if (NoteYSTEM(bNoteL) == StemUp) hasUp = TRUE;
						}
						else if (nextXD > syncXD + epsXD) {			/* ??CER: WHY ANY CONDITION? */
							break;
						}
					}
				}

				if (hasDown && hasUp) {
					multiv[i] = TRUE; break;
				}
				
				hasUp = hasDown = FALSE;
			}
	}
}

static short SetupScanNote(
						Document *doc,
						LINK pL, LINK aNoteL,
						char multiv[],		/* Multivoice flags (TRUE=staff is multivoice) */
						short altv[] 		/* Input AND output: alternate (2nd) voice no. for staff */
						)
{
	PANOTE	aNote;
	INT16		halfLn;								/* Relative to the top of the staff */
	char		lDur;
	INT16		ndots,staffn;
	char		voice;
	Boolean	isRest;
	INT16		accident;
	LINK 		partL;
	short		userVoice;

	aNote = GetPANOTE(aNoteL);
	halfLn = aNote->yd;
	lDur = aNote->subType;
	ndots = aNote->ndots;
	staffn = aNote->staffn;

	/* Handle voice numbers, including multiple voices on the staff and their effects
		on voice role. If the staff has multiple voices in the vicinity, then:
			If this note is stem up, put it in the staff's default voice; otherwise
				put it in the staff's alternate voice
		If the staff doesn't have multiple voices, put the note in the staff's default
			voice. */
	
	if (multiv[staffn]) {
		if (aNote->ystem == StemUp) {
			voice = staffn;
			doc->voiceTab[voice].voiceRole = UPPER_DI;
		}
		else {
			if (altv[staffn]==NOONE) {
				partL = FindPartInfo(doc, Staff2Part(doc, staffn));
				userVoice = NewVoiceNum(doc, partL);
				
				voice = User2IntVoice(doc, userVoice, partL);
				doc->voiceTab[voice].voiceRole = LOWER_DI;
				altv[staffn] = voice;
			}
			else {
				voice = altv[staffn];
			}
		}
	}
	else
		voice = staffn;

	aNote = GetPANOTE(aNoteL);
	isRest = aNote->rest;
	accident = aNote->accident;
	
	SetupNote(doc, pL, aNoteL, staffn, halfLn, lDur, ndots, voice, isRest,
						accident, 0);

	return voice;
}

/* AddSyncNote removes the first subobject of <fromSyncL> and adds it to <toSyncL>
after <aNoteL>. If <fromSyncL> has only one subobject (typical in NoteScan file
processing), that makes its subobject list empty, and the degenerate Sync should
be removed later. All this seems to be work safely, at least now (v.1.5b8), but
caveat. */

static LINK AddSyncNote(LINK fromSyncL, LINK toSyncL, LINK aNoteL)
{
	LINK bNoteL;

	bNoteL = FirstSubLINK(fromSyncL);
	FirstSubLINK(fromSyncL) = NILINK;
	LinkNENTRIES(fromSyncL)--;
	NextNOTEL(aNoteL) = bNoteL;
	aNoteL = bNoteL;
	LinkNENTRIES(toSyncL)++;
	
	return aNoteL;
}

static short CountNotesOnStaff(LINK pL, short s)
{
	LINK aNoteL;
	short notes=0;
	
	aNoteL = FirstSubLINK(pL);
	for ( ; aNoteL; aNoteL = NextNOTEL(aNoteL)) {
		if (NoteSTAFF(aNoteL)==s) notes++;
	}
	return notes;
}			

/* TraverseScanMeasure adds the contents of one measure of the NoteScan file, as
transformed and stored in staff-by-staff object lists, to the Nightingale score
object list. It merges subobjects as necessary to do so. */

static LINK TraverseScanMeasure(
						Document *doc,
						LINK prevL,			/* Object AFTER which to insert the measure content */
						LINK staffA[],		/* Staff-by-staff object lists */
						short altv[],		/* Input AND output: alternate (2nd) voice no. for staff */
						short nstaves
						)
{
	LINK pL,qL,rL,aNoteL,insertL,measL,
			syncs[NSMAXSTAVES+1],					/* Despite the name, not necessarily syncs! */
			lastObj[NSMAXSTAVES+1];
	LINK aClefL,bClefL,aTimeSigL,bTimeSigL,aKeySigL,bKeySigL,objL,bNoteL;
	short i,j,k,m,notes,dfNotes,nAltNotes,iAltv,nInBeam,v,nBeamable,s;
	PANOTE aNote;
	char addToObj[NSMAXSTAVES+1],done[NSMAXSTAVES+1],multiv[NSMAXSTAVES+1];
	DDIST minxd,epsXD;

	qL = prevL;										/* CER? Init. in case we don't find anything */
	insertL = RightLINK(prevL);
	ResetVoiceTable(doc);

	GetScanMeasMultiV(staffA,nstaves,multiv);
	InitializeStaves(nstaves,staffA,syncs,addToObj,done);

	for ( ; !StavesDone(nstaves,done); IncrementStaves(nstaves,syncs,lastObj,addToObj,done)) {
		
		minxd = GetObjToAdd(nstaves,syncs,addToObj,done);
		if (minxd==ERROR_INT) {
			MayErrMsg("TraverseScanMeasure: GetObjToAdd failed. prevL=%d", prevL);
			goto broken;
		}
		
		epsXD = pt2d(config.noteScanEpsXD);
		for (i=1; i<=nstaves; i++)
			if (addToObj[i]) {

				lastObj[i] = pL = syncs[i];
				switch (ObjLType(pL)) {
					case SYNCtype:
						aNoteL = FirstSubLINK(pL);
						aNote = GetPANOTE(aNoteL);
						aNote->firstMod = NILINK;
		
						qL = DuplicateObject(SYNCtype,pL,FALSE,doc,doc,FALSE);
						if (!qL) {
							NoMoreMemory(); goto broken;
						}
						InsNodeInto(qL, insertL);	/* ??CER: SHOULD BE InsNodeIntoSlot? */
		
						measL = SSearch(qL,MEASUREtype,GO_LEFT);
						LinkXD(qL) -= LinkXD(measL);

						aNoteL = FirstSubLINK(qL);
						SetupScanNote(doc,qL,aNoteL,multiv,altv);

						/* Scan forward on this staff for notes on this staff to be added
							to this sync: notes in the same chord or notes/chords in another
							voice */

						rL=RightLINK(pL);
						while (rL && SyncTYPE(rL) && LinkXD(rL) <= minxd+epsXD) {
							lastObj[i] = rL;
							aNoteL = AddSyncNote(rL,qL,aNoteL);
							SetupScanNote(doc,qL,aNoteL,multiv,altv);
							
							rL = RightLINK(rL);
						}

						/* Scan forward for notes on other staves to be added to this sync */
							
						for (j=i+1; j<=nstaves; j++) {
							if (addToObj[j]) {
								lastObj[j] = objL = syncs[j];

								aNoteL = AddSyncNote(objL,qL,aNoteL);
								SetupScanNote(doc,qL,aNoteL,multiv,altv);

								rL=RightLINK(objL);
								while (rL && SyncTYPE(rL) && LinkXD(rL) <= minxd+epsXD) {
									lastObj[j] = rL;
									aNoteL = AddSyncNote(rL,qL,aNoteL);
									SetupScanNote(doc,qL,aNoteL,multiv,altv);
									
									rL = RightLINK(rL);
								}
							}
						}
						
						/* For each staff, fix chord in default voice and alternate voice. */
						
						if (LinkNENTRIES(qL) > 1)
							for (k=1; k<=nstaves; k++) {
								if (addToObj[k]) {
									notes = CountNotesOnStaff(qL,k);
	
									if (notes>1) {
										bNoteL = FirstSubLINK(qL);
										for (dfNotes=nAltNotes=0; bNoteL; bNoteL=NextNOTEL(bNoteL))
											if (NoteSTAFF(bNoteL)==k) {
												if (NoteVOICE(bNoteL) == k)
													dfNotes++;
												else {
													iAltv = NoteVOICE(bNoteL); nAltNotes++;
												}
											}

										if (dfNotes>1) {
											/* ??CER, WHY MAKE STEMS ZERO-LENGTH HERE? */
											bNoteL = FirstSubLINK(qL);
											for ( ; bNoteL; bNoteL=NextNOTEL(bNoteL)) {
												if (NoteVOICE(bNoteL) == k)
													NoteYSTEM(bNoteL) = NoteYD(bNoteL);
											}

											FixSyncForChord(doc,qL,k,FALSE,0,0,NULL);
										}
										
										if (nAltNotes>1) {

											FixSyncForChord(doc,qL,iAltv,FALSE,0,0,NULL);
										}
									}
								}
							}

						break;
					case CLEFtype:
						qL = DuplicateObject(CLEFtype,pL,FALSE,doc,doc,FALSE);
						if (!qL) {
							NoMoreMemory(); goto broken;
						}
						InsNodeInto(qL, insertL);
						measL = SSearch(qL,MEASUREtype,GO_LEFT);
						LinkXD(qL) -= LinkXD(measL);
						ClefINMEAS(qL) = TRUE;
						aClefL = FirstSubLINK(qL);

						for (j=i+1; j<=nstaves; j++)
							if (addToObj[j]) {
								lastObj[j] = objL = syncs[j];
								bClefL = FirstSubLINK(objL);

								FirstSubLINK(objL) = NILINK;
								LinkNENTRIES(objL)--;
								NextCLEFL(aClefL) = bClefL;
								aClefL = bClefL;
								LinkNENTRIES(qL)++;
								
							}
						break;
					case KEYSIGtype:
						qL = DuplicateObject(KEYSIGtype,pL,FALSE,doc,doc,FALSE);
						if (!qL) {
							NoMoreMemory(); goto broken;
						}
						InsNodeInto(qL, insertL);
						measL = SSearch(qL,MEASUREtype,GO_LEFT);
						LinkXD(qL) -= LinkXD(measL);
						KeySigINMEAS(qL) = TRUE;
						aKeySigL = FirstSubLINK(qL);

						for (j=i+1; j<=nstaves; j++)
							if (addToObj[j]) {
								lastObj[j] = objL = syncs[j];
								bKeySigL = FirstSubLINK(objL);

								FirstSubLINK(objL) = NILINK;
								LinkNENTRIES(objL)--;
								NextKEYSIGL(aKeySigL) = bKeySigL;
								aKeySigL = bKeySigL;
								LinkNENTRIES(qL)++;
								
							}
						break;
					case TIMESIGtype:
						qL = DuplicateObject(TIMESIGtype,pL,FALSE,doc,doc,FALSE);
						if (!qL) {
							NoMoreMemory(); goto broken;
						}
						InsNodeInto(qL, insertL);
						measL = SSearch(qL,MEASUREtype,GO_LEFT);
						LinkXD(qL) -= LinkXD(measL);
						TimeSigINMEAS(qL) = TRUE;
						aTimeSigL = FirstSubLINK(qL);

						for (j=i+1; j<=nstaves; j++)
							if (addToObj[j]) {
								lastObj[j] = objL = syncs[j];
								bTimeSigL = FirstSubLINK(objL);

								FirstSubLINK(objL) = NILINK;
								LinkNENTRIES(objL)--;
								NextTIMESIGL(aTimeSigL) = bTimeSigL;
								aTimeSigL = bTimeSigL;
								LinkNENTRIES(qL)++;
								
							}
						break;
					case BEAMSETtype:
						nInBeam = LinkNENTRIES(pL);
						rL=LeftLINK(insertL);
						v = s = BeamSTAFF(pL);
						for (m=0; m<nInBeam && rL && !StaffTYPE(rL); rL = LeftLINK(rL)) {
							if (SyncTYPE(rL))
								if (NoteOnStaff(rL,v)) {
									m += CountNotesOnStaff(rL,v);
								}
							if (StaffTYPE(rL)) break;
						}
						
						nBeamable = CountBeamable(doc, RightLINK(rL), insertL, v, FALSE);
						if (nBeamable>1) {
							if (!CreateBEAMSET(doc, RightLINK(rL), insertL, v, nBeamable, FALSE,
															doc->voiceTab[v].voiceRole))
								goto broken;
						}
						break;
					default:
						break;
					
				}
				
				break;
			}
	}

	return qL;
	
broken:
	return NILINK;
}

static void DisposeStfLists(Document *doc, LINK staffA[], LINK stfTailL[])
{
	short s;

	for (s=1; s<=NSMAXSTAVES; s++)
		if (staffA[s] && stfTailL[s])
			DisposeNodeList(doc,&staffA[s],&stfTailL[s]);
}


static void Normalize2FirstMeas(Document *doc, LINK sysL)
{
	LINK firstMeasL,firstObjL, rSys, nextMeasL, lastMeasL;
	DDIST spBefore,xd2Move;
	
	firstMeasL = SSearch(sysL,MEASUREtype,GO_RIGHT);
	rSys = SSearch(RightLINK(sysL),SYSTEMtype,GO_RIGHT);

	firstObjL = FirstValidxd(RightLINK(firstMeasL),GO_RIGHT);
	if (firstObjL==NILINK ||
			PageTYPE(firstObjL) ||
			SystemTYPE(firstObjL) ||
			TailTYPE(firstObjL)) return;					/* Avoid goofy objects */
	
	spBefore = STHEIGHT/(STFLINES-1);

	if (MeasureTYPE(firstObjL)) {							/* Catch those consecutive measures */
		if (LinkXD(firstObjL) < LinkXD(firstMeasL) + spBefore) {
			xd2Move = LinkXD(firstMeasL) + spBefore - LinkXD(firstObjL);
			nextMeasL = firstObjL;
			if (rSys) {
				lastMeasL = SSearch(LeftLINK(rSys),MEASUREtype,GO_LEFT);
			}
			else {
				lastMeasL = SSearch(doc->tailL,MEASUREtype,GO_LEFT);
			}
			MoveMeasures(nextMeasL,RightLINK(lastMeasL),xd2Move);
		}
	}

	/* If there is a next measure, MoveInMeasure until it; otherwise until the tail.
		If there is an rSys, move remaining measures until it; else until the tail. */

	if (LinkXD(firstObjL) < spBefore) {					/* Notes, clefs and things */
		nextMeasL = SSearch(RightLINK(firstMeasL),MEASUREtype,GO_RIGHT);
		xd2Move = spBefore - LinkXD(firstObjL);

		if (nextMeasL) {
			MoveInMeasure(RightLINK(firstMeasL), nextMeasL, xd2Move);
			if (rSys) {
				lastMeasL = SSearch(LeftLINK(rSys),MEASUREtype,GO_LEFT);
			}
			else {
				lastMeasL = SSearch(doc->tailL,MEASUREtype,GO_LEFT);
			}
			MoveMeasures(nextMeasL,RightLINK(lastMeasL),xd2Move);
		}
		else {
			MoveInMeasure(RightLINK(firstMeasL), doc->tailL, xd2Move);
		}
	}
}


static enum {
	OK_DI=1,
	Cancel_DI,		/* (not used by SetClefDialog) */
	STAFFN_DI=3,
	TREBLE_DI=5,
	ALTO_DI,
	TENOR_DI,
	BASS_DI,
	ALLOW_TREBLE_DI=10,
	ALLOW_ALTO_DI,
	ALLOW_TENOR_DI,
	ALLOW_BASS_DI
} E_SetClefItems;

/* Return FALSE if an error occurs. Otherwise return the Nightingale code for the
desired initial clef, and set <clefChanges> to indicate acceptable clef changes. */

Boolean SetClefDialog(
					short staffn,
					short oldClefVal,							/* Initial clef according to NoteScan */
					short *pNewClefVal,						/* Initial clef to use */
					Boolean canChangeTo[MaxClefNSVAL]
					)
{
	INT16 itemHit,radio;
	Boolean keepGoing=TRUE;
	DialogPtr dlog; GrafPtr oldPort;
	ModalFilterUPP	filterUPP;

	/* Build dialog window and install its item values */
	
	filterUPP = NewModalFilterUPP(OKButFilter);
	if (filterUPP == NULL) {
		MissingDialog(SETCLEF_DLOG);
		return FALSE;
	}
	GetPort(&oldPort);
	dlog = GetNewDialog(SETCLEF_DLOG, NULL, BRING_TO_FRONT);
	if (!dlog) {
		DisposeModalFilterUPP(filterUPP);
		MissingDialog(SETCLEF_DLOG);
		return FALSE;
	}
	SetPort(GetDialogWindowPort(dlog));

	PutDlgWord(dlog, STAFFN_DI, staffn, FALSE);
	
	if (oldClefVal==TrebleClefNSVAL) radio = TREBLE_DI;
	else if (oldClefVal==AltoClefNSVAL) radio = ALTO_DI;
	else if (oldClefVal==TenorClefNSVAL) radio = TENOR_DI;
	else if (oldClefVal==BassClefNSVAL) radio = BASS_DI;
	else radio = TREBLE_DI;
	PutDlgChkRadio(dlog, radio, TRUE);

	PutDlgChkRadio(dlog, ALLOW_TREBLE_DI, FALSE);
	PutDlgChkRadio(dlog, ALLOW_ALTO_DI, FALSE);
	PutDlgChkRadio(dlog, ALLOW_TENOR_DI, FALSE);
	PutDlgChkRadio(dlog, ALLOW_BASS_DI, FALSE);

	PlaceWindow(GetDialogWindow(dlog),(WindowPtr)NULL,0,80);
	ShowWindow(GetDialogWindow(dlog));
	ArrowCursor();

	/* Entertain filtered user events until dialog is dismissed */
	
	while (keepGoing) {
		ModalDialog(filterUPP, &itemHit);
		switch(itemHit) {
			case OK_DI:
				keepGoing = FALSE;
				if (radio==TREBLE_DI) *pNewClefVal = TrebleClefNSVAL;
				else if (radio==ALTO_DI) *pNewClefVal = AltoClefNSVAL;
				else if (radio==TENOR_DI) *pNewClefVal = TenorClefNSVAL;
				else *pNewClefVal = BassClefNSVAL;
				
				canChangeTo[TrebleClefNSVAL] = GetDlgChkRadio(dlog, ALLOW_TREBLE_DI);
				canChangeTo[AltoClefNSVAL] = GetDlgChkRadio(dlog, ALLOW_ALTO_DI);
				canChangeTo[TenorClefNSVAL] = GetDlgChkRadio(dlog, ALLOW_TENOR_DI);
				canChangeTo[BassClefNSVAL] = GetDlgChkRadio(dlog, ALLOW_BASS_DI);
				break;
			case TREBLE_DI:
			case ALTO_DI:
			case TENOR_DI:
			case BASS_DI:
				if (itemHit!=radio) SwitchRadio(dlog, &radio, itemHit);
				break;
			case ALLOW_TREBLE_DI:
			case ALLOW_ALTO_DI:
			case ALLOW_TENOR_DI:
			case ALLOW_BASS_DI:
				PutDlgChkRadio(dlog, itemHit, !GetDlgChkRadio(dlog, itemHit));
				break;
			}
		}
	
	DisposeModalFilterUPP(filterUPP);
	DisposeDialog(dlog);
	SetPort(oldPort);
	
	return TRUE;
}


/* Return FALSE if an error occurs. Otherwise return the NoteScan (not Nightingale!)
code for the desired initial keysig, and set <*pCanChange> to indicate if keysig
changes are allowed. */

Boolean SetKeySigDialog(
					short staffn,
					short oldKeySigVal,				/* Initial keysig according to NoteScan */
					short *pNewKeySigVal,			/* Initial keysig to use */
					Boolean *pCanChange
					)
{
	INT16 sharpParam, flatParam; Boolean okay;
	
	sharpParam = flatParam = 0;
	if (oldKeySigVal>0) {
		if (oldKeySigVal<=7)
			sharpParam = oldKeySigVal;
		else
			flatParam = oldKeySigVal-7;
	}
	okay = SetKSDialogGuts(staffn, &sharpParam, &flatParam, pCanChange);
	if (okay) {
		*pNewKeySigVal = 0;
		if (sharpParam>0) *pNewKeySigVal = sharpParam;
		else if (flatParam>0) *pNewKeySigVal = flatParam+7;
	}
	return okay;
}


Boolean CreateMeasScanObjs(
					Document *doc,
					LINK sysL,
					Boolean overrideCl,	/* TRUE=override clef changes */
					Boolean overrideKS,	/* TRUE=override key sig. changes */
					Ptr staffPtr[],		/* the start of the sequence of records for each staff */
					LINK stfTailL[]
					)
{
	LINK pL,aStfL,aClefL,aKeySigL,aTimeSigL,aMeasL,aNoteL,aRestL;
	Ptr p,staffEndPtr;
	static short clefInStaff[NSMAXSTAVES+1], keySigInStaff[NSMAXSTAVES+1];
	static Boolean canChangeClefTo[NSMAXSTAVES+1][MaxClefNSVAL],
						canChangeKeySig[NSMAXSTAVES+1];
	Boolean firstClefInStaff, firstKeySigInStaff;
	short i,s,top,sharpsOrFlats,durCode,useClef,useKeySig;
	long recType;
	DDIST staffLen;
	PANOTE aNote,aRest;

	/*
	 * If the first system, initialize the current clef and keysig for each staff to
	 * unknown, and initialize the recognized clef-change possibilities to allow all
	 * keysig changes.
	 */
	
	if (firstScoreSys) {
		for (s=0; s<=NSMAXSTAVES; s++) {
			clefInStaff[s] = UnknownClefNSVAL;
			for (i=1; i<=MaxClefNSVAL; i++)
				/* ??Should probably be FALSE, but it doesn't matter now. */
				canChangeClefTo[s][i] = TRUE;

			keySigInStaff[s] = UnknownKeySigNSVAL;
			canChangeKeySig[s] = FALSE;
		}
	}
		
	for (s=1; s<=doc->nstaves; s++) {

		p = staffPtr[s];
		staffEndPtr = staffPtr[s+1];
		firstClefInStaff = TRUE;
		firstKeySigInStaff = TRUE;

		while (p<staffEndPtr) {
	
			recType = *(long *)p;
	
			switch (recType) {
				case 'STFF':
					HandleStaff(p);
					pL = InsertNode(doc, stfTailL[s], STAFFtype, 1);
					aStfL = FirstSubLINK(pL);

					if (firstScoreSys) {
						staffLen = MARGWIDTH(doc)-doc->firstIndent;
					}
					else if (PageTYPE(LeftLINK(sysL))) {
						staffLen = MARGWIDTH(doc)-doc->otherIndent;
					}
					else {
						staffLen = MARGWIDTH(doc)-doc->otherIndent;
					}
					
					/* staffNum is gotten from the NPIF file by HandleStaff.
						staffNum is zeroed by HandleSystem and then incremented with
						each succeeding call to HandleStaff; therefore it is
							imperative that HandleSystem be called once before the
							loop reading staff records and not again during this loop. */

					if (staffNum==1) top = initStfTop1;
					else if (staffNum==2) top = initStfTop1+initStfTop2;
					else top = initStfTop1 + (staffNum-1) * (initStfTop2);

					InitStaff(aStfL, staffNum, top, 0, staffLen, STHEIGHT, STFLINES, SHOW_ALL_LINES);
					break;
				case 'CLEF':
					/* If we're trying to ignore spurious clef changes,
						(1) if this is the staff's initial clef of the first system, give
							the user a chance to change it, and a chance to say what clefs
							should be allowed on the staff from now on;
						(2) if this is a clef change within a system, if it's not an allowed
							clef for the staff or if it's the same as the previous clef, do
							nothing;
						(3) if it's the staff's initial clef, substitute the staff's last
							allowed clef. */
					
					HandleClef(p);
					if (overrideCl) {
						if (firstScoreSys && firstClefInStaff)	{							/* Rule 1 */
							if (SetClefDialog(s, CLEFclef, &useClef, &(canChangeClefTo[s][0]) ))
								CLEFclef = useClef;
							WaitCursor();
						}
						if (!firstClefInStaff) {
							if (!canChangeClefTo[s][CLEFclef]) break;						/* Rule 2a */
							if (CLEFclef==clefInStaff[s]) break;							/* Rule 2b */
						}

						if (firstClefInStaff && clefInStaff[s]!=UnknownClefNSVAL)	/* Rule 3 */
							CLEFclef = clefInStaff[s];
					}
					
					clefInStaff[s] = CLEFclef;
					firstClefInStaff = FALSE;
					
					pL = InsertNode(doc, stfTailL[s], CLEFtype, 1);
					aClefL = FirstSubLINK(pL);

					switch (CLEFclef) {
						case TrebleClefNSVAL:
							InitClef(aClefL, staffNum, 0, TREBLE_CLEF);
							break;
						case BassClefNSVAL:
							InitClef(aClefL, staffNum, 0, BASS_CLEF);
							break;
						case AltoClefNSVAL:
							InitClef(aClefL, staffNum, 0, ALTO_CLEF);
							break;
						case TenorClefNSVAL:
							InitClef(aClefL, staffNum, 0, TENOR_CLEF);
							break;
						default:													/* Should never happen */
							InitClef(aClefL, staffNum, 0, TREBLE_CLEF);
					}

					SetObject(pL, p2d(CLEFhPlacement), 0, FALSE, TRUE, TRUE);
					break;
				case 'KYSG':
					/* If we're trying to ignore spurious key sig. changes,
						(1) if this is the staff's initial key sig. of the first system, give
							the user a chance to change it, and a chance to say whether key sigs
							should be allowed on the staff from now on;
						(2) if this is a key sig. change within a system, if we're not allowing
							key sig. changes for the staff or if it's the same as the previous
							key sig., do nothing;
						(3) if it's the staff's initial key sig., substitute the staff's last
							allowed key sig. */
					
					HandleKeySignature(p);
					if (overrideKS) {
						if (firstScoreSys && firstKeySigInStaff)	{						/* Rule 1 */
							if (SetKeySigDialog(s, KEYkey, &useKeySig, &canChangeKeySig[s]))
								KEYkey = useKeySig;
							WaitCursor();
						}
						if (!firstKeySigInStaff) {
							if (!canChangeKeySig[s]) break;									/* Rule 2a */
							if (KEYkey==keySigInStaff[s]) break;							/* Rule 2b */
						}

						if (firstKeySigInStaff && keySigInStaff[s]!=UnknownKeySigNSVAL) /* Rule 3 */
							KEYkey = keySigInStaff[s];
					}
					
					keySigInStaff[s] = KEYkey;
					firstKeySigInStaff = FALSE;
					pL = InsertNode(doc, stfTailL[s], KEYSIGtype, 1);
					aKeySigL = FirstSubLINK(pL);

					sharpsOrFlats = 0;
					if (KEYkey != 0) {
						if (KEYkey >= 1 && KEYkey <= 7) {			/* sharps */
							sharpsOrFlats = KEYkey;
						}
						else {												/* flats */
							sharpsOrFlats = -(KEYkey - 7);
						}
					}
					InitKeySig(aKeySigL, staffNum, 0, sharpsOrFlats);
					SetupKeySig(aKeySigL, sharpsOrFlats);
					SetObject(pL, p2d(KEYhPlacement), 0, FALSE, TRUE, TRUE);
					break;
				case 'TMSG':
					HandleTimeSignature(p);
					pL = InsertNode(doc, stfTailL[s], TIMESIGtype, 1);
					aTimeSigL = FirstSubLINK(pL);
					InitTimeSig(aTimeSigL, staffNum, 0, DFLT_TSTYPE, TIMEtopN, TIMEbottomN);
					SetObject(pL, p2d(TIMEhPlacement), 0, FALSE, TRUE, TRUE);
					LinkVIS(pL) = TIMEvisible;
					TimeSigVIS(aTimeSigL) = TIMEvisible;
					break;
				case 'BAR ':
					HandleBar(p);
					pL = InsertNode(doc, stfTailL[s], MEASUREtype, 1);
					SetObject(pL, p2d(BARhPlacement), 0, FALSE, TRUE, TRUE);
					aMeasL = FirstSubLINK(pL);
					if (staffNum==1)
						InitMeasure(aMeasL, 1, pt2d(0), pt2d(0), 0,
													STHEIGHT, TRUE, FALSE, doc->nstaves, 0);
					else if (doc->nstaves>1) {
						InitMeasure(aMeasL, staffNum, pt2d(0), initStfTop1+(staffNum-1)*STHEIGHT, 0,
													STHEIGHT, /* FALSE */ TRUE, TRUE, 0, 0);
					}
					break;
				case 'NOTE':
					HandleNote(p);
					pL = InsertNode(doc, stfTailL[s], SYNCtype, 1);
					SetObject(pL, p2d(NOTEhPlacement), 0, FALSE, TRUE, TRUE);
					durCode = NPIFDur2NightDur(NOTEtimeVal);
					aNoteL = FirstSubLINK(pL);
					
					/* Set the note's fields. Many will be reset, esp. by SetupScanNote. */
					 
					aNote = GetPANOTE(aNoteL);
					aNote->staffn = staffNum;
					aNote->voice = staffNum;			/* Default voice for the staff */
					aNote->rest = FALSE;
					aNote->ndots = NOTEnDots;
					aNote->subType = durCode;
					aNote->yd = 4 - NOTEvSclPos;
					aNote->firstMod = NILINK;
					aNote->accident = 0;
					aNote->ystem = NOTEdirectn;		/* Temp. flag: 1=stem down, 2=stem up */

					/* LG: 0=none, 1--5=dbl. flat--dbl. sharp (unused for rests) */

					if (NOTEaccdntl != 0) {
						switch (NOTEaccdntl) {
							case 1:
								aNote->accident = 4;		/* sharp */
								break;
							case 2:
								aNote->accident = 3;		/* natural */
								break;
							case 3:
								aNote->accident = 5;		/* double sharp */
								break;
							case 4:
								aNote->accident = 2;		/* flat */
								break;
						}
					}

					break;
				case 'REST':
					HandleRest(p);
#if 1
					/* In most music, nearly all whole rests are really whole-measure
						rests and are centered in their measures. Since we currently decide
						syncing solely by horizontal position, we mishandle these badly.
						for now, just ignore whole rests--the user can add them later.
						??In OrchScore2, this fails horribly: it results in skipping all
						notes till the last system. But, for now, NoteScan "fixes" this for
						us--not the best solution. */
					
					if (CapsLockKeyDown() && ShiftKeyDown() && RESTtimeVal==WholeNoteNSVAL)
						break;
#endif
					pL = InsertNode(doc, stfTailL[s], SYNCtype, 1);
					SetObject(pL, p2d(RESThPlacement), 0, FALSE, TRUE, TRUE);
					durCode = NPIFDur2NightDur(RESTtimeVal);
					aRestL = FirstSubLINK(pL);
					
					/* Set the rest's fields. Many will be reset, esp. by SetupScanNote. */
					 
					aRest = GetPANOTE(aRestL);
					aRest->staffn = staffNum;
					aRest->voice = staffNum;			/* Default voice for the staff */
					aRest->rest = TRUE;
					aRest->ndots = RESTnDots;
					aRest->subType = durCode;
					aRest->ystem = 0;						/* Temp. flag: neither stem down nor stem up */
					break;
				case 'BEAM': {
					npifBEAMPtr npifBeam;
					struct startEnd *startEndPtr;
					struct beamTerm *startPtr;
					LINK rL,sL,startL=NILINK; short k,nInBeam=0;

						npifBeam = (npifBEAMPtr) p;
						BEAMnParts = npifBeam->numberOfParts;

						for (k=0; k < npifBeam->numberOfParts && k < 10; k++) {
							if (k==0) {
								startEndPtr	= &npifBeam->beamLine[k];
								startPtr = &startEndPtr->start;
								BEAMSnoteHPlacement = startPtr->noteHPlacement * scaleFactor;

								for (rL = stfTailL[s]; rL && !SystemTYPE(rL); rL = LeftLINK(rL)) {
									if (SyncTYPE(rL))
										if (SyncInVoice(rL,s)) {
											if (LinkXD(rL) >= p2d(BEAMSnoteHPlacement))
												startL = rL;
											else
												break;
										}
								}
							}

							break;
						}

						if (startL) {
							for (sL = startL; sL && sL!=stfTailL[s]; sL=RightLINK(sL)) {
								if (SyncTYPE(sL))
									if (SyncInVoice(sL,s))
										nInBeam++;
							}
						}

						if (nInBeam>1) {
							pL = InsertNode(doc, stfTailL[s], BEAMSETtype, nInBeam);
							BeamSTAFF(pL) = BeamVOICE(pL) = s;
						}
					}

					break;
				default:
					break;
			}

			p = AdvanceRecPtr(p);
		}
	}

	return TRUE;
}

/*
 * Add to the given score the contents of a single system of music. sysL is
 * the system whose contents are being created; recPtr (global) is the position
 * in the scan file buffer beginning the system; endPtr is the end of the
 * entire buffer.
 *
 * Returns the last LINK of the system, or NILINK if there's an error.
 */

static LINK AddSystemContents(
					Document *doc,
					LINK sysL, LINK *connL,
					char *endPtr,
					short altv[],			/* Input AND output: alternate (2nd) voice no. for staff */
					Boolean overrideCl,	/* TRUE=override clef changes */
					Boolean overrideKS 	/* TRUE=override key sig. changes */
					)
{
	LINK stfStartL[NSMAXSTAVES+1],stfTailL[NSMAXSTAVES+1],staffL[MAXSTAVES+1],pL,
			prevL,aStfL,aKeySigL,aMeasL,stfL,prevMeasL,nextMeasL,measL;
	Ptr staffPtr[NSMAXSTAVES+2],p,staffEndPtr;
	short i,s,k,staffn,nStaves,maxSofon=0,maxSofonStf=0;
	short typeFound;
	long recType;
	Boolean inMeasure=FALSE,foundClef=FALSE,foundKeySig=FALSE,foundTimeSig=FALSE,
				foundObj,firstMeasure=TRUE;
	DDIST dLineSp,xd,spBefore=0;
	DDIST spBeforeClef=0,spAfterClef=0,spAfterKS=0,spAfterTS=0;
	PAKEYSIG aKeySig;
	CONTEXT context;
	char fmtStr[256];

	/*
	 * Initialize an array of temporary object lists, one list to hold each
	 * staff's worth of Nightingale objects for this system. Note that these
	 * are not legal object lists as far as most of Nightingale is concerned:
	 * they have no header, PAGE, etc.
	 */

	for (s=0; s<=NSMAXSTAVES; s++) {
		stfStartL[s] = stfTailL[s] = NILINK;
		staffPtr[s] = NULL;
	}

	for (i=1; i<=SYSTnumberOfStaves; i++) {
		stfStartL[i] = NewNode(doc, SYSTEMtype, 0);
		if (!stfStartL[i])
			goto errReturn;
		stfTailL[i] = NewNode(doc, TAILtype, 0);
		if (!stfTailL[i])
			goto errReturn;
		LeftLINK(stfStartL[i]) = NILINK;
		RightLINK(stfStartL[i]) = stfTailL[i];
		LeftLINK(stfTailL[i]) = stfStartL[i];
		RightLINK(stfTailL[i]) = NILINK;
	}
	
	/*
	 * Traverse the scan file buffer, filling in the staffPtr array of ptrs
	 * to the start of the sequence of records for each staff.
	 */

	recType = *(long *)recPtr;
	if (recType!='STFF')
		goto errReturn;

	staffn = 1;
	p = recPtr;
	while (p<endPtr) {

		recType = *(long *)p;

		switch (recType) {
			case 'FILE':
				goto errReturn;
			case 'PAGE':
			case 'SYST':
				staffEndPtr = recPtr = p;					/* recPtr is incremented here */
				goto done1;
			case 'EOF ':
				staffEndPtr = p;
				recPtr = endPtr;								/* recPtr is incremented here */
				goto done1;
			case 'STFF':
				staffPtr[staffn++] = p;
				break;
			case 'CLEF':
			case 'KYSG':
			case 'TMSG':
			case 'BAR ':
			case 'NOTE':
			case 'REST':
			case 'BEAM':
			default:
				break;
		}
		p = AdvanceRecPtr(p);
	}	
	
done1:

	staffPtr[staffn] = staffEndPtr;

	/* staffn is now one more than the number of staves found; set nStaves from it.
		Check it for agreement with either the number of visible staves or the total
		number of staves, for compatibility with both old and new (July 1994) NoteScan. */
		
	nStaves = staffn-1;
	if (nStaves!=SYSTvisibleStaves && nStaves!=SYSTnumberOfStaves) {
		GetIndCString(strBuf, NOTESCANERRS_STRS, 6);		/* "Unexpected no. of staves" */
		CParamText(strBuf, "", "", "");
		StopInform(GENERIC_ALRT);
		goto errReturn;
	}

	/* Create objects for the individual staves and put them into separate object
		lists. We'll merge them into the object list for the score later (in
		TraverseScanMeasure). */
		
	if (nStaves!=doc->nstaves) MayErrMsg("AddSystemContents: nStaves=%ld!=doc->nstaves=%ld",
													(long)nStaves, (long)doc->nstaves);
	CreateMeasScanObjs(doc,sysL,overrideCl,overrideKS,staffPtr,stfTailL);

	for (s=1; s<=nStaves; s++)
		FixXMeasLinks(stfStartL[s],stfTailL[s]);

	for (s=1; s<=nStaves; s++) {
		staffL[s] = RightLINK(stfStartL[s]);
	}

	stfL = CreateScanStaff(doc, sysL, connL, staffL);
	pL = *connL;
	for (s=1; s<=nStaves; s++) {
		staffL[s] = RightLINK(staffL[s]);
	}
	
	aStfL=FirstSubLINK(stfL);
	for (s=1; aStfL && s<=nStaves; s++,aStfL=NextSTAFFL(aStfL)) {
		GetContext(doc, LeftLINK(stfL), StaffSTAFF(aStfL), &context);
		FixStaffContext(aStfL, &context);
	}

	spBefore = dLineSp = STHEIGHT/(STFLINES-1);					/* Space between staff lines */
	spBeforeClef = spBefore;

	/* prevL is used to track the object before the current object. If one of
			the reserved-area objects is not found, prevL will not be incremented
			in the traversal, so that the next thing which is found will be correctly
			located relative to the most recently found object. That is to say, prevL
			will not be incremented in the (already existing) data structure of the
			score if the object is not encountered in the data structure produced
			from the NoteScan file. This also applies to the following measure, which
			will be correctly located relative to the last thing found. ?? CER EXPLAIN
	 If we never found an object, but one will appear in the score anyway, it
			will be handled under the cases for !foundClef, etc. If this is the case,
			and nothing subsequent to the appearing-not-found object, prevL and spBefore
			must be updated for this case as well. ?? CER EXPLAIN */

	prevL = pL;
	while (staffL[1]!=stfTailL[1] && !inMeasure)	{
#ifndef PUBLIC_VERSION
		if (ControlKeyDown()) {
			short kk; DebugPrintf("staffL[],ObjLType() ");
			for (kk=1; kk<=n_min(4,doc->nstaves); kk++)
				DebugPrintf("%d=%d:%d ", kk, staffL[kk], ObjLType(staffL[kk]));
			DebugPrintf("\n");
		}
#endif

		/* NB: we're getting the object type from staff 1. If reserved-area objects
			are on other staves but not staff 1, this won't work. As of the moment
			(8/94), hidden staves contain nothing but measures, but staff 1 can never
			be hidden, so this should be safe. */
			
		switch (ObjLType(staffL[1])) {
			case CLEFtype:
				if (foundClef) {
					inMeasure = TRUE; break;
				}

				prevL = pL = CreateScanClef(doc, pL, staffL, spBefore);
				foundClef = foundObj = TRUE;
				spBefore = spAfterClef = 7*dLineSp/2;
				break;
			case KEYSIGtype:
				if (foundKeySig) {
					inMeasure = TRUE; break;
				}

				prevL = pL = CreateScanKeySig(doc, pL, staffL, spBefore);
				aKeySigL = FirstSubLINK(pL);
				for (k = 1; k<=doc->nstaves; k++,aKeySigL = NextKEYSIGL(aKeySigL)) {
					aKeySig = GetPAKEYSIG(aKeySigL);
					if (aKeySig->nKSItems>maxSofon) {
						maxSofonStf = k;
						maxSofon = aKeySig->nKSItems;
					}
				}

				foundKeySig = foundObj = TRUE;
				if (maxSofonStf!=0) {
					spBefore = dLineSp/2;
					spBefore += std2d(SymWidthRight(doc, pL, maxSofonStf, FALSE), STHEIGHT, 5);
				}
				else {
					spBefore = 0;
				}
				if (!LinkVIS(pL))
					spBefore = 0;
				spAfterKS = spBefore;

				break;
			case TIMESIGtype:
				if (foundTimeSig) {
					inMeasure = TRUE; break;
				}

				prevL = pL = CreateScanTimeSig(doc, pL, staffL, spBefore);
				foundTimeSig = foundObj = TRUE;
				spBefore = LinkVIS(pL) ? 3*dLineSp : 0;
				spAfterTS = spBefore;
				break;
			default:
				inMeasure = TRUE;
				foundObj = FALSE;
				break;
		}

		/* If we found a clef, keysig, or timesig on staff 1, advance past it on every
			staff that has the same type of object. For example, in NoteScan at the
			moment (7/94), hidden staves won't have any such objects. */
			
		if (foundObj) {
			typeFound = ObjLType(staffL[1]);
			for (s=1; s<=nStaves; s++) {
				if (ObjLType(staffL[s])==typeFound)
					staffL[s] = RightLINK(staffL[s]);
			}
		}
	}

	/* Fix up reserved area of system for any objects that are required but were
			not present in the scan file. */

	if (!foundClef) {
		LINK firstClefL,beforeL;

		firstClefL = SSearch(sysL,CLEFtype,GO_RIGHT);

		if (firstClefL) {
			beforeL = FirstValidxd(LeftLINK(firstClefL),GO_LEFT);
			LinkXD(firstClefL) = LinkXD(beforeL)+spBeforeClef;
	
			spAfterClef = 7*dLineSp/2;

			if (!foundKeySig && !foundTimeSig && LinkVIS(firstClefL)) {
				prevL = firstClefL;
				spBefore = spAfterClef;
			}
		}
	}
	if (!foundKeySig) {
		LINK firstKeySigL,beforeL,aKeySigL;
		
		firstKeySigL = SSearch(sysL,KEYSIGtype,GO_RIGHT);

		if (firstKeySigL!=NILINK) {
			beforeL = FirstValidxd(LeftLINK(firstKeySigL),GO_LEFT);
			LinkXD(firstKeySigL) = LinkXD(beforeL)+spAfterClef;
	
			aKeySigL = FirstSubLINK(firstKeySigL);
			for (k = 1; k<=doc->nstaves; k++,aKeySigL = NextKEYSIGL(aKeySigL)) {
				aKeySig = GetPAKEYSIG(aKeySigL);
				if (aKeySig->nKSItems>maxSofon) {
					maxSofonStf = k;
					maxSofon = aKeySig->nKSItems;
				}
			}
	
			if (maxSofonStf!=0) {
				spAfterKS = dLineSp/2;
				spAfterKS += std2d(SymWidthRight(doc, firstKeySigL, maxSofonStf, FALSE), STHEIGHT, 5);
			}
			else {
				spAfterKS = 0;
			}
			if (!LinkVIS(firstKeySigL))
				spAfterKS = 0;
				
			if (!foundTimeSig && LinkVIS(firstKeySigL)) {
				prevL = firstKeySigL;
				spBefore = spAfterKS;
			}
		}
	}
	if (!foundTimeSig) {
		LINK firstTimeSigL,beforeL;
		
		firstTimeSigL = SSearch(sysL,TIMESIGtype,GO_RIGHT);

		if (firstTimeSigL!=NILINK) {
			beforeL = FirstValidxd(LeftLINK(firstTimeSigL),GO_LEFT);
			LinkXD(firstTimeSigL) = LinkXD(beforeL)+spAfterKS;
			
			if (LinkVIS(firstTimeSigL)) {
				prevL = firstTimeSigL;
				spBefore = 3*dLineSp;
			}
		}
	}

	/* If this was the first system of the score, the context fields of the staff
		were updated before the reserved area objects were created; this will leave
		the first system with possibly illegitimate default context unless we fix
		the staff context again here.
		pL is the most recently inserted reserved area object, the LeftLINK of the
		first invis measure; if we get the context at that measure, we will just pull
		default values out of that measure's context fields, and not fix what we
		intended to. */

	if (firstScoreSys) {
		aStfL=FirstSubLINK(stfL);
		for (s=1; aStfL && s<=nStaves; s++,aStfL=NextSTAFFL(aStfL)) {
			GetContext(doc, pL, StaffSTAFF(aStfL), &context);
			FixStaffContext(aStfL, &context);
		}
	}

	/*
	 * First invisible measure is not present. Insert a measure object on all staves
	 * and traverse the contents of the first measure. If this is the first system
	 * of the score, then its first measure is already in the data structure; update
	 * some of its fields and just leave it in place.
	 */

	if (!MeasureTYPE(staffL[1])) {
		if (firstScoreSys) {
			pL = SSearch(doc->headL,MEASUREtype,GO_RIGHT);
			LinkXD(pL) = LinkXD(prevL)+spBefore;
			MeasureTIME(pL) = 0L;

			aMeasL=FirstSubLINK(pL);
			for (s=1; aMeasL && s<=nStaves; s++,aMeasL=NextMEASUREL(aMeasL)) {
				GetContext(doc, LeftLINK(pL), MeasureSTAFF(aMeasL), &context);
				FixMeasureContext(aMeasL, &context);
			}
		}
		else {

			prevMeasL = SSearch(pL,MEASUREtype,GO_LEFT);
			
			measL = DuplicateObject(MEASUREtype,prevMeasL,FALSE,doc,doc,FALSE);
			if (!measL) {
				NoMoreMemory(); goto errReturn;
			}
			InsNodeInto(measL,RightLINK(pL));

			prevMeasL = SSearch(LeftLINK(measL),  MEASUREtype, GO_LEFT);
			nextMeasL = SSearch(RightLINK(measL), MEASUREtype, GO_RIGHT);
		
			LinkLMEAS(measL) = prevMeasL;
			LinkRMEAS(measL) = nextMeasL;
			if (prevMeasL!=NILINK) LinkRMEAS(prevMeasL) = measL;
			if (nextMeasL!=NILINK) LinkLMEAS(nextMeasL) = measL;
			
			MeasSYSL(measL) = sysL;
			MeasSTAFFL(measL) = stfL;
	
			xd = LinkXD(prevL)+spBefore;
			SetObject(measL, xd, 0, FALSE, FALSE, TRUE);
			SetBarlineVis(measL,FALSE);

			aMeasL=FirstSubLINK(measL);
			for (s=1; aMeasL && s<=nStaves; s++,aMeasL=NextMEASUREL(aMeasL)) {
				GetContext(doc, LeftLINK(measL), MeasureSTAFF(aMeasL), &context);
				FixMeasureContext(aMeasL, &context);
			}

			pL = measL;
		}
		firstMeasure = FALSE;

		pL = TraverseScanMeasure(doc, pL, staffL, altv, nStaves);
		for (s=1; s<=nStaves; s++) {
			staffL[s] = SSearch(staffL[s],MEASUREtype,GO_RIGHT);
		}
	}

	/*
	 * While there are measures left, create measure objects out of the bars
	 * created from the scan file, and then traverse and fill in the contents of
	 * those measures.
	 */

#ifndef PUBLIC_VERSION
	if (ControlKeyDown()) {
		LINK dL; INT16 kount, ss;
		
		for (ss=1; ss<=doc->nstaves; ss++) {
			for (kount=0, dL=staffL[ss]; dL; dL=RightLINK(dL))
				kount++;
			DebugPrintf("-- %d objs on staff %d of sys %d:\n", kount, ss, currSystem);
			for (kount=0, dL=staffL[ss]; dL; dL=RightLINK(dL)) {
				if (kount<100)				/* Avoid boredom if obj list is huge or bad */
					DisplayNode(NILINK, dL, kount, TRUE, FALSE, TRUE);
				kount++;
			}
		}
	}
#endif

	while (staffL[1]) {
		if (!ChkMeasConsistent(doc,staffL)) {

			/* Have reached a measure obj not extending across all staves: create a
				measure to mark it, and return without adding any more objects. All
				objects will be out of sync from now on. ??CER? */
				
			pL = CreateScanMeasure(doc, pL, staffL, spBefore, firstMeasure);

			Normalize2FirstMeas(doc, sysL);

			GetIndCString(fmtStr, NOTESCANERRS_STRS, 15);    /* "Found a measure not extending across all staves: skipping the rest of system %d." */
			sprintf(strBuf, fmtStr, currSystem+1); 

#ifndef PUBLIC_VERSION
			DebugPrintf(strBuf);
			DebugPrintf("\n");
#endif /* PUBLIC_VERSION */
			CParamText(strBuf, "", "", "");
			StopInform(GENERIC_ALRT);
			WaitCursor();
			goto okReturn;
		}

		pL = CreateScanMeasure(doc, pL, staffL, spBefore, firstMeasure);
		firstMeasure = FALSE;

		aMeasL=FirstSubLINK(pL);
		for (s=1; aMeasL && s<=nStaves; s++,aMeasL=NextMEASUREL(aMeasL)) {
			GetContext(doc, LeftLINK(pL), MeasureSTAFF(aMeasL), &context);
			FixMeasureContext(aMeasL, &context);
		}
	
		for (s=1; s<=nStaves; s++) {
			staffL[s] = RightLINK(staffL[s]);
		}
		pL = TraverseScanMeasure(doc, pL, staffL, altv, nStaves);
		for (s=1; s<=nStaves; s++) {
			staffL[s] = SSearch(staffL[s],MEASUREtype,GO_RIGHT);
		}
	}
			
	/* Initial system objects are created independently of the contents; initial
		system objects are set up when system is created; contents are positioned
		from actual scanned values.
		Normalize the contents of the system here, so that the first object after
		the first invisible measure does not have a negative xd and begin in the
		reserved area; move the contents of the system to the right if there is
		not at least SPAFTER between the first invisible measure and the FirstValidxd
		obj after it. */
		
	Normalize2FirstMeas(doc, sysL);
	
	DisposeStfLists(doc,stfStartL,stfTailL);

okReturn:
	return pL;

errReturn:
	DisposeStfLists(doc,stfStartL,stfTailL);

	return NILINK;
}

/* -------------------------------------------------------------------------------- */
/* Functions to create Nightingale documents from NPIF Files. */

#ifdef NOMORE

static void FixPageWidth(Document *doc,
									short width)	/* points by which to increase page width */
{
	LINK sysL,staffL,aStaffL;
	PASTAFF aStaff;

	/* These values are points */

	doc->paperRect.right += width;
	doc->origPaperRect.right += width;
	doc->marginRect.right += width;

	sysL = SSearch(doc->headL,SYSTEMtype,GO_RIGHT);
	for ( ; sysL; sysL = LinkRSYS(sysL)) {
		SystemRECT(sysL).right += pt2d(width);
	}
	
	staffL = SSearch(doc->headL,STAFFtype,GO_RIGHT);
	for ( ; staffL; staffL = LinkRSTAFF(staffL)) {
		aStaffL = FirstSubLINK(staffL);
		for ( ; aStaffL; aStaffL = NextSTAFFL(aStaffL)) {
			aStaff = GetPASTAFF(aStaffL);
			aStaff->staffRight += pt2d(width);
		}
	}
}

/* Do everything necessary to change the given score's staff size to <newRastral>. */

static void NewStaffSize(Document *doc, short newRastral)
{
	FASTFLOAT fact; INT16 i;
	LINK sysL,staffL,aStaffL;
	DRect sysRect;
	DDIST sysSize, sysOffset, staffTop[NSMAXSTAVES+1];
	PASTAFF aStaff;

	if (newRastral==doc->srastral) return;
	
	staffL = SSearch(doc->masterHeadL, STAFFtype, GO_RIGHT);
	aStaffL = FirstSubLINK(staffL);
	for ( ; aStaffL; aStaffL=NextSTAFFL(aStaffL)) {
		aStaff = GetPASTAFF(aStaffL);
		staffTop[StaffSTAFF(aStaffL)] = aStaff->staffTop;
	}

	fact = (FASTFLOAT)drSize[newRastral]/drSize[doc->srastral];

	for (i = 2; i<=doc->nstaves; i++)
		staffTop[i] = staffTop[1] + (fact * (staffTop[i]-staffTop[1]));
			
	SetStaffSize(doc, doc->headL, doc->tailL, doc->srastral, newRastral);
	UpdateStaffTops(doc, doc->headL, doc->tailL, staffTop);

	SetStaffSize(doc, doc->masterHeadL, doc->masterTailL, doc->srastral, newRastral);
	UpdateStaffTops(doc, doc->masterHeadL, doc->masterTailL, staffTop);

	/* Adjust heights of systemRects, both in Master Page and in the score proper. */
	sysL = SSearch(doc->masterHeadL, SYSTEMtype, FALSE);
	sysRect = SystemRECT(sysL);
	
	sysSize = sysRect.bottom-sysRect.top;
	sysOffset = (fact-1.0)*sysSize;
	
	SystemRECT(sysL).bottom += sysOffset;
	LinkOBJRECT(sysL).bottom += d2p(sysOffset);
	
	sysL = SSearch(doc->headL, SYSTEMtype, FALSE);
	for ( ; sysL; sysL = LinkRSYS(sysL)) {
		SystemRECT(sysL).bottom += sysOffset;
		LinkOBJRECT(sysL).bottom += d2p(sysOffset);
	}
	
	FixMeasRectYs(doc, NILINK, FALSE, TRUE, FALSE);		/* Fix measure tops & bottoms */

	doc->srastral = newRastral;
}

#endif

/*
 * Create a document to hold the score converted from an NPIF file.
 */

static Document *CreateScanDoc(unsigned char *fileName, short vRefNum, FSSpec *pfsSpec, short pageWidth,
											short pageHt, short nStaves, short rastral)
{
	Document *newDoc;
	WindowPtr w; long fileVersion;

	newDoc = FirstFreeDocument();
	if (newDoc == NULL) {
		TooManyDocs(); return(NULL);
	}
	
	w = GetNewWindow(docWindowID,NULL,BottomPalette);
	if (!w) return(NULL);
	
	newDoc->theWindow = w;
	SetDocumentKind(w);
	//((WindowPeek)w)->spareFlag = TRUE;
	ChangeWindowAttributes(w, kWindowFullZoomAttribute, kWindowNoAttributes);
	
	newDoc->inUse = TRUE;
	Pstrcpy(newDoc->name,fileName);
	newDoc->vrefnum = vRefNum;

	/* Create newDoc, the document for the scanned score. */
	
	if (!BuildNightScanDoc(newDoc,fileName,vRefNum,pfsSpec,&fileVersion,TRUE,
										pageWidth,pageHt,nStaves,rastral,FILEXResolution)) {
		DoCloseDocument(newDoc);
		return(NULL);
	}

	return(newDoc);
}

/*
 * Set up and display the newly created scan document.
 */

static void SetupScanDoc(Document *newDoc)
{
	short palWidth,palHeight;
	Rect box,bounds;
	WindowPtr w; 
	short	changeFirstIndent;

	/* Replace the Master Page: delete the old Master Page data structure
		here and replace it with a new structure to reflect the part-staff
		structure of the scanned score. */

	// Score2MasterPage(newDoc);
	newDoc->docNew = newDoc->changed = TRUE;		/* Has to be set after BuildDocument */
	newDoc->readOnly = FALSE;
	
	/* Strip the instrument name from the left end of the first system and
		move the first system all the way to the left margin */

	newDoc->firstNames = NONAMES;
	
	changeFirstIndent = pt2d(0)-newDoc->firstIndent;
	if (changeFirstIndent!=0) {
		newDoc->firstIndent = pt2d(0);
		IndentSystems(newDoc, changeFirstIndent, TRUE);
	}
	
	/* Place new document window in a non-conflicting position */

	w = newDoc->theWindow;
	WindowPtr palPtr = (*paletteGlobals[TOOL_PALETTE])->paletteWindow;
	GetWindowPortBounds(palPtr,&box);
	palWidth = box.right - box.left;
	palHeight = box.bottom - box.top;
	GetGlobalPort(w,&box);						/* set bottom of window near screen bottom */
	bounds = GetQDScreenBitsBounds();
	if (box.left < bounds.left+4) box.left = bounds.left+4;
	if (palWidth < palHeight)
		box.left += palWidth;

	MoveWindow(newDoc->theWindow,box.left,box.top,FALSE);
	AdjustWinPosition(w);
	GetGlobalPort(w,&box);
	bounds = GetQDScreenBitsBounds();
	box.bottom = bounds.bottom - 4;
	if (box.right > bounds.right-4) box.right = bounds.right - 4;
	SizeWindow(newDoc->theWindow,box.right-box.left,box.bottom-box.top,FALSE);

	SetOrigin(newDoc->origin.h,newDoc->origin.v);
	GetAllSheets(newDoc);
	RecomputeView(newDoc);
	SetControlValue(newDoc->hScroll,newDoc->origin.h);
	SetControlValue(newDoc->vScroll,newDoc->origin.v);
	SetWTitle(w,newDoc->name);
	
	MEAdjustCaret(newDoc,FALSE);
	InstallMagnify(newDoc);
	ShowDocument(newDoc);
	
#if TARGET_API_MAC_CARBON_FILEIO
	DoUpdate(newDoc->theWindow);
#endif
}

#if TARGET_API_MAC_CARBON_FILEIO

static short GetScanFileName(Str255, short *)
{
	return (0);
}

#else

static short GetScanFileName(Str255 fn, short *vRef)
{
	SFTypeList	myTypes;
	
	myTypes[0] = 'Scan';
	myTypes[1] = 'TEXT';
	myTypes[2] = 'TIFF';
	myTypes[3] = 'Midi';			/* ??CER: LOOKS LIKE WE ONLY USE THE 1ST ELEMENT! */
	
	SFGetFile(dialogWhere, "\p", 0L, 1, myTypes, 0L, &reply);
	if (reply.good) {
		PStrCopy(reply.fName, fn);
		*vRef = reply.vRefNum;
		return(1);
	}

	return(0);
}

#endif // TARGET_API_MAC_CARBON_FILEIO
/*
 * Do preprocessing needed to verify integrity of file.
 * Return values that must be used to prepare the document before the score
 * is actually built. At this point, this just includes staff rastral size.
 * If we succeed, return 0; if not, return a positive error code.
 */

static short CheckScanFile(
							char *buffer,
							long length,							/* File length, in bytes */
							short *pageWidth, short *pageHt,
							short *nStaves,
							short *rastral
							)
{
	long recType;
	char *recPtr,*endPtr,*lenPtr;
	short recLength,prevNStaves=0,stfRastral=-2;
	Boolean needPage=FALSE,needSys=FALSE,needStf=FALSE,gotNStaves=FALSE,
				usePreferred=TRUE;
	float stfHtIn;								/* staff height in fractions of inch */

	recPtr = buffer;
	endPtr = buffer + length;
   
   recType = *(long *)recPtr;
   if (recType != 'FILE')
   	return MissingOrProblemWithFILE;
 
	scanSystems = NULL;
	while (recPtr<endPtr) {

		recType = *(long *)recPtr;

		if (needPage && recType != 'PAGE')
			return MissingPAGE;

		if (needSys && recType != 'SYST')
			return MissingSYST;

		if (needStf && recType != 'STFF')
			return MissingSTFF;

		switch (recType) {

			case 'FILE':
				if (!HandleFile(recPtr))				/* Allocates scanSystems */
					return MissingOrProblemWithFILE;	/* Really resolution problem or no memory */

				needPage = TRUE;
				break;
			case 'PAGE':
				HandlePage(recPtr);
				needPage = FALSE;
				needSys = TRUE;
				*pageWidth = PAGEpageWidth;
				*pageHt = PAGEpageHt;
				break;
			case 'SYST':
				needSys = FALSE;
				needStf = TRUE;

				HandleSystem(recPtr);
				if (gotNStaves) {
					if (SYSTnumberOfStaves!=prevNStaves)
						return NStavesError;
				}
				else {
					*nStaves = prevNStaves = SYSTnumberOfStaves;
					gotNStaves = TRUE;
				}
				break;
			case 'STFF':
				if (stfRastral<0) {
					HandleStaff(recPtr);
					stfHtIn = (float)STFFspacePixels*4/(float)FILEXResolution;
					
					if (stfHtIn >= .38) 			stfRastral = 0;
					else if (stfHtIn >= .33)	stfRastral = 1;
					else if (stfHtIn >= .27)	stfRastral = 2;
					else if (stfHtIn >= .26)	stfRastral = 3;
					else if (stfHtIn >= .25)	stfRastral = 4;
					else if (stfHtIn >= .22)	stfRastral = 5;
					else if (stfHtIn >= .19)	stfRastral = 6;
					else if (stfHtIn >= .16)	stfRastral = 7;
					else								stfRastral = 8;
					
					if (usePreferred) {
						switch (stfRastral) {
							case 0:
							case 1:
							case 2:
							case 5:
							case 7:
								*rastral = stfRastral;
								break;
							case 3:
								*rastral = 2;
								break;
							case 4:
								*rastral = 5;
								break;
							case 6:
							case 8:
								*rastral = 7;
								break;
						}
					}
					else
						*rastral = stfRastral;
				}
				needStf = FALSE;
				break;
			case 'CLEF':
			case 'KYSG':
			case 'TMSG':
			case 'BAR ':
			case 'NOTE':
			case 'REST':
			case 'BEAM':
			case 'EOF ':
			default:
				break;
	  
		}

		lenPtr = &recPtr[4];
		recLength = *(short *)lenPtr;
		recPtr += recLength;
  }
  
  if (scanSystems) DisposePtr((Ptr)scanSystems);
  if (*rastral<0) *rastral = 1;		/* rastral was never set */
  return noErr;
}

static LINK InsertPage(Document *doc, LINK prevL)
{
	LINK		pageL, lPage, rPage, sysL;
	
	lPage = LSSearch(prevL, PAGEtype, ANYONE, TRUE, FALSE);
	rPage = LSSearch(RightLINK(prevL), PAGEtype, ANYONE, FALSE, FALSE);

	pageL = InsertNode(doc, RightLINK(prevL), PAGEtype, 0);
	if (pageL==NILINK) { NoMoreMemory(); return NILINK; }
	SetObject(pageL, 0, 0, FALSE, TRUE, TRUE);
	LinkTWEAKED(pageL) = FALSE;

	LinkLPAGE(pageL) = lPage;
	if (lPage) LinkRPAGE(lPage) = pageL;

	if (rPage) LinkLPAGE(rPage) = pageL;
	LinkRPAGE(pageL) = rPage;

	sysL = LSSearch(pageL, SYSTEMtype, ANYONE, TRUE, FALSE);
	doc->currentSystem = sysL ? SystemNUM(sysL) : 1;
	SheetNUM(pageL) = lPage ? SheetNUM(lPage)+1 : 0;
	doc->numSheets++;

	return pageL;
}


/* GetScoreParams reads the entire file in, checks it for validity, and gets
information about it. If it finds an error, it reports it to the user and returns
FALSE; else it returns TRUE. */

static Boolean GetScoreParams(
						INT16 /*vRefNum*/, INT16 refNum,
						short *pageWidth, short *pageHt,
						short *nStaves, short *rastral)
{
	OSErr errCode;
	long length, fileEOF;
	char *buffer;
	char fmtStr[256];
	
	errCode = GetEOF(refNum,&fileEOF);
	if (errCode!=noError) {
		ReportIOError(errCode, READNOTESCAN_ALRT);
		goto haveError;
	}
	
	buffer = NewPtr((Size)fileEOF + 4);
	if (!GoodNewPtr(buffer)) {
		NoMoreMemory();
		goto haveError;
	}
	
	errCode = SetFPos(refNum, fsFromStart, 0x0000);
	if (errCode!=noError) {
		ReportIOError(errCode, READNOTESCAN_ALRT);
		goto haveError;
	}
	
	length = fileEOF;
	errCode = FSRead(refNum, &length, buffer);
	if (errCode!=noError) {
		ReportIOError(errCode, READNOTESCAN_ALRT);
		goto haveError;
	}
	   
   errCode = CheckScanFile(buffer, length, pageWidth, pageHt, nStaves, rastral);
	if (errCode!=noError) {
		GetIndCString(strBuf, NOTESCANERRS_STRS, 1);			/* "File appears to be damaged" */
		CParamText(strBuf, "", "", "");
		StopInform(READNOTESCAN_ALRT);
		goto haveError;
	}
	
	if (*nStaves<1 || *nStaves>NSMAXSTAVES) {
		GetIndCString(fmtStr, NOTESCANERRS_STRS, 7);			/* "no. of staves is illegal" */
		sprintf(strBuf, fmtStr, *nStaves);
		CParamText(strBuf, "", "", "");
		StopInform(READNOTESCAN_ALRT);
		goto haveError;
	}

	if (buffer) DisposePtr(buffer);
	return TRUE;

haveError:
	if (buffer) DisposePtr(buffer);
	return FALSE;
}


/*
 * Read an NPIF file and convert it into a Nightingale score stored in document
 * <doc>. If all is well, return noErr, else return NoGood.
 * Need:
 *		System: sysTop
 *		Staff: staffLength,staffTop[]; init context properly
 *		Clef: context
 *		KeySig,TimeSig: distinguish beforeFirstMeas, use spBefore accordingly
 */

#define EXTRA_BYTES 200

static short ConvertFile(Document *doc, INT16 refNum, INT16 vRefNum, Boolean overrideCl,
									Boolean overrideKS)
{
	char *buffer,*endPtr;
	short i,nStaves,altv[NSMAXSTAVES+1],rastral;
	long recType, length, fileEOF;
	LINK prevL=NILINK,pageL=NILINK,sysL=NILINK,staffL,connL=NILINK,firstMeasL,aStaffL,
			partL;
	DDIST sysTop=0,spBefore=0,sysHt;
	OSErr errCode;
	short pageWidth,pageHt;
	Boolean fileOpen=TRUE, allStavesVisible;

	errCode = GetEOF(refNum,&fileEOF);
	if (errCode!=noError) goto haveError;

	buffer = NewPtr((Size)fileEOF + EXTRA_BYTES);
	if (!GoodNewPtr(buffer)) { NoMoreMemory(); goto haveError; }

	errCode = SetFPos(refNum, fsFromStart, 0x0000);
	if (errCode!=noError) goto haveError;

	length = fileEOF;
	errCode = FSRead(refNum, &length, buffer);
	if (errCode!=noError) goto haveError;
   
	if ((errCode = FSClose(refNum))!=noErr)
		 goto haveError;
	fileOpen = FALSE;

	if ((errCode = FlushVol(NULL, vRefNum))!=noErr)
		 goto haveError;

	/* We should have checked the file before getting here: following is to get params. */
	
   errCode = CheckScanFile(buffer, length, &pageWidth, &pageHt, &nStaves, &rastral);
	if (errCode!=noError) goto haveError;

	/*
	 *	We now have a default score with one part of two staves. Add a part of
	 *	nStaves staves below the default part to contain all the staves from
	 * the NPIF File (if there are many staves, it'll look silly, but the user
	 * can easily split them into separate parts). Then remove the default part.
	 */

	partL = AddPart(doc, 2, nStaves, SHOW_ALL_LINES);
	if (partL==NILINK) goto haveError;
	InitPart(partL, 3, 2+nStaves);

	DeletePart(doc, 1, 2);

	WaitCursor();
	
	FixMeasRectYs(doc, NILINK, TRUE, TRUE, FALSE);		/* Fix measure & system tops & bottoms */
	Score2MasterPage(doc);

	currSystem = 0;
	recPtr = buffer;
	endPtr = buffer + length;
   firstScoreSys = TRUE;
	sysTop = SYS_TOP(doc)+pt2d(config.titleMargin);
	doc->yBetweenSys = 0;

	for (i=1; i<=nStaves; i++)
		altv[i] = NOONE;

	/* Main loop: add the objects in the NoteScan file to the score. This function
		handles only the system level and above; it calls AddSystemContents for
		lower-level objects. */
	
	while (recPtr<endPtr) {
		recType = *(long *)recPtr;

		switch (recType) {

			case 'FILE':
				if (!HandleFile(recPtr))					/* Allocates scanSystems */
					goto haveError;

				recPtr = AdvanceRecPtr(recPtr);
				break;
			case 'PAGE':
				HandlePage(recPtr);
				recPtr = AdvanceRecPtr(recPtr);
				
				if (firstScoreSys)
					prevL = pageL = SSearch(doc->headL,PAGEtype,GO_RIGHT);
				else
					prevL = pageL = InsertPage(doc,LeftLINK(doc->tailL));
				if (!pageL)
					goto haveError;

				break;
			case 'SYST':
				if (!pageL) goto haveError;

				HandleSystem(recPtr);

				if (currSystem>=MAXSYSTEMS) {
					GetIndCString(strBuf, NOTESCANERRS_STRS, 5);		/* "Too many systems" */
					CParamText(strBuf, "", "", "");
					StopInform(GENERIC_ALRT);
					goto haveError;
				}
				scanSystems[currSystem].numStaves = SYSTnumberOfStaves;
				scanSystems[currSystem].numVisStaves = SYSTvisibleStaves;
				
				if (firstScoreSys)
					doc->nstaves = SYSTnumberOfStaves;
				else {
					if (doc->nstaves != SYSTnumberOfStaves)
						goto haveError;
				}

				recPtr = AdvanceRecPtr(recPtr);
				prevL = sysL = AddScanSystem(doc,prevL,pageL,sysL,sysTop);
				if (!sysL)
					goto haveError;
				
				scanSystems[currSystem].sysL = sysL;

				prevL = AddSystemContents(doc,sysL,&connL,endPtr,altv,overrideCl,overrideKS);
				if (!prevL)
					goto haveError;
   			firstScoreSys = FALSE;
				
				scanSystems[currSystem].staffL = SSearch(sysL,STAFFtype,GO_RIGHT);
				if (!scanSystems[currSystem].staffL)
					goto haveError;

				sysHt = GetSysHeight(doc, sysL, firstSystem);
				sysTop += sysHt;
				
				currSystem++;
				
				break;
			default:
				break;
		}
	}

	/* Everything is in the object list. Now clean up and make all consistent. */
	
	FixStructureLinks(doc,doc,doc->headL,doc->tailL);

	if (!FixTimeStamps(doc, doc->headL, NILINK))		/* Recompute all play times */
		goto haveError;

	FixSystemRectYs(doc, FALSE);
	firstMeasL = LSSearch(doc->headL,MEASUREtype,ANYONE,GO_RIGHT,FALSE);
	FixMeasRectXs(firstMeasL,NILINK);
	FixMeasRectYs(doc, NILINK, FALSE, FALSE, FALSE);

	UpdatePageNums(doc);
	UpdateSysNums(doc, doc->headL);
	UpdateMeasNums(doc, NILINK);
	UpdateMeasureContexts(doc);
	UpdateStaffContexts(doc);
	
	DeselRangeNoHilite(doc,doc->headL,doc->tailL);

	/* Hide any staves that are supposed to be invisible. Assume all invisible staves
		are at the bottom of the system: that's true for now, but hopefully NoteScan
		will become smarter in the future, at which point we'll have to pay attention
		to the staff record's topRow (-1=invisible). */
	
	allStavesVisible = TRUE;
	for (i=0; i<currSystem; i++) {
		if (scanSystems[i].numVisStaves < scanSystems[i].numStaves) {
			allStavesVisible = FALSE;
			staffL = scanSystems[i].staffL;
			doc->selStartL = staffL;
			doc->selEndL = RightLINK(staffL);

			LinkSEL(staffL) = TRUE;
			aStaffL = FirstSubLINK(scanSystems[i].staffL);
			for ( ; aStaffL; aStaffL = NextSTAFFL(aStaffL)) {
				if (StaffSTAFF(aStaffL) <= scanSystems[i].numVisStaves)
					StaffSEL(aStaffL) = FALSE;
				else
					StaffSEL(aStaffL) = TRUE;
			}
			
			NSInvisify(doc);
			DeselRangeNoHilite(doc,staffL,RightLINK(staffL));
		}
	}

	if (!allStavesVisible && !overrideCl)
		CautionInform(HIDDENSTAVES_ALRT);
	SetDefaultSelection(doc);
	DisposePtr((Ptr)scanSystems);
	if (buffer) DisposePtr(buffer);

	return noErr;											/* file closed at top of function */
  
haveError:
	if (buffer) DisposePtr(buffer);
	if (fileOpen) FSClose(refNum);					/* error before file closed */
	return NoGood;
}


static enum {
	NSTAVES_DI=4,
	OVERRIDECL_DI=7,
	NOOVERRIDECL_DI,
	OVERRIDEKS_DI,
	NOOVERRIDEKS_DI
} E_NSInfoItems;

/* Return FALSE if error or user cancels. Otherwise returns TRUE, with parameters
saying whether to override the clefs and key sigs. NoteScan recognized. */

Boolean NSInfoDialog(short, Boolean *, Boolean *);
Boolean NSInfoDialog(short nStaves, Boolean *pOverrideClefs, Boolean *pOverrideKS)
{
	INT16 itemHit,keepGoing=TRUE, radio1, radio2;
	DialogPtr dlog; GrafPtr oldPort;
	ModalFilterUPP	filterUPP;

	/* Build dialog window and install its item values */
	
	filterUPP = NewModalFilterUPP(OKButFilter);
	if (filterUPP == NULL) {
		MissingDialog(NSINFO_DLOG);
		return FALSE;
	}
	GetPort(&oldPort);
	dlog = GetNewDialog(NSINFO_DLOG, NULL, BRING_TO_FRONT);
	if (!dlog) {
		DisposeModalFilterUPP(filterUPP);
		MissingDialog(NSINFO_DLOG);
		return FALSE;
	}
	SetPort(GetDialogWindowPort(dlog));

	PutDlgWord(dlog, NSTAVES_DI, nStaves, FALSE);
	
	radio1 = (*pOverrideClefs? OVERRIDECL_DI : NOOVERRIDECL_DI);
	PutDlgChkRadio(dlog, radio1, TRUE);
	radio2 = (*pOverrideKS? OVERRIDEKS_DI : NOOVERRIDEKS_DI);
	PutDlgChkRadio(dlog, radio2, TRUE);

	PlaceWindow(GetDialogWindow(dlog), (WindowPtr)NULL, 0, 80);
	ShowWindow(GetDialogWindow(dlog));
	ArrowCursor();

	/* Entertain filtered user events until dialog is dismissed */
	
	while (keepGoing) {
		ModalDialog(filterUPP, &itemHit);
		switch(itemHit) {
			case OK_DI:
				keepGoing = FALSE;
				*pOverrideClefs = GetDlgChkRadio(dlog, OVERRIDECL_DI);
				*pOverrideKS = GetDlgChkRadio(dlog, OVERRIDEKS_DI);
				break;
			case Cancel_DI:
				keepGoing = FALSE;
				break;
			case OVERRIDECL_DI:
			case NOOVERRIDECL_DI:
				if (itemHit!=radio1) SwitchRadio(dlog, &radio1, itemHit);
				break;
			case OVERRIDEKS_DI:
			case NOOVERRIDEKS_DI:
				if (itemHit!=radio2) SwitchRadio(dlog, &radio2, itemHit);
				break;
			default:
				;
			}
		}
	
	DisposeModalFilterUPP(filterUPP);
	DisposeDialog(dlog);
	SetPort(oldPort);
	
	return (itemHit==OK_DI);
}

#if TARGET_API_MAC_CARBON_FILEIO

short OpenScanFile()
{
	Document *doc;
	char str[256];
	Str255 fn;
	NSClientData nscd; FSSpec fsSpec;
	INT16 returnCode; 
	short vRef; short refNum; short io;
	Boolean fileOpen = FALSE;
	short pageWidth,pageHt,nStaves,rastral,totalErrors,firstErrMeas,firstErrStf;
	static Boolean overrideCl = FALSE, overrideKS = FALSE;
	
	if (!NSEnabled()) return NoGood;
	
	UseStandardType('Scan');
	
	returnCode = GetInputName(str,FALSE,tmpStr,&vRef,&nscd);
	if (returnCode == OP_Cancel) {	
		return NoGood;	
	}
	
	fsSpec = nscd.nsFSSpec;
	vRef = nscd.nsFSSpec.vRefNum;
	Pstrcpy(fn,tmpStr);
	
	io = FSpOpenDF (&fsSpec, fsRdWrPerm, &refNum );	/* Open the file */
	if (io) {
		ReportIOError(io, READNOTESCAN_ALRT);
		return io;
	}
	fileOpen = TRUE;

	if (!GetScoreParams(vRef,refNum,&pageWidth,&pageHt,&nStaves,&rastral)) {
		/* GetScoreParams gives its own error messages */
		io = NoGood; goto ForgetIt;
	}

	if (!NSInfoDialog(nStaves, &overrideCl, &overrideKS)) goto ForgetIt;

	if (!(doc = CreateScanDoc(fn,vRef,&fsSpec,pageWidth,pageHt,nStaves,rastral))) {
		GetIndCString(strBuf, NOTESCANERRS_STRS, 2);		/* "Error creating score" */
		CParamText(strBuf, "", "", "");
		StopInform(READNOTESCAN_ALRT);
		io = NoGood; goto ForgetIt;
	}

	if ((io = ConvertFile(doc, refNum, vRef, overrideCl, overrideKS))!=noErr) {
		GetIndCString(strBuf, NOTESCANERRS_STRS, 3);		/* "Error converting NoteScan file" */
		CParamText(strBuf, "", "", "");
		StopInform(READNOTESCAN_ALRT);
		fileOpen = FALSE; goto ForgetIt;
	}

	SetupScanDoc(doc);
	
	ArrowCursor();
	if (NSProblems(doc, &totalErrors, &firstErrMeas, &firstErrStf))	/* 3rd and 4th params unused! */
#ifdef PUBLIC_VERSION
		NULL;
#else
		DebugPrintf("Problems converting. %ld inconsistencies.\n", (long)totalErrors);
#endif

	return noErr;
	
ForgetIt:
	if (fileOpen) FSClose(refNum);	/* Closed by ConvertFile; otherwise close here */
	return io;
}

#else

short OpenScanFile()
{
	short vRef, refNum, io;
	Str255 fn;
	Document *doc;
	short pageWidth,pageHt,nStaves,rastral,totalErrors,firstErrMeas,firstErrStf;
	Boolean fileOpen = FALSE;
	static Boolean overrideCl = FALSE, overrideKS = FALSE;
	FSSpec fsSpec;

	if (!NSEnabled()) return NoGood;
	if (!GetScanFileName(fn,&vRef,&fsSpec))
		return NoGood;

	if ((io = FSOpen(fn, vRef, &refNum))!=noErr)	{							
		ReportIOError(io, READNOTESCAN_ALRT);
		return io;
	}
	fileOpen = TRUE;

	if (!GetScoreParams(vRef,refNum,&pageWidth,&pageHt,&nStaves,&rastral)) {
		/* GetScoreParams gives its own error messages */
		io = NoGood; goto ForgetIt;
	}

	if (!NSInfoDialog(nStaves, &overrideCl, &overrideKS)) goto ForgetIt;

	if (!(doc = CreateScanDoc(fn,vRef,&fsSpec,pageWidth,pageHt,nStaves,rastral))) {
		GetIndCString(strBuf, NOTESCANERRS_STRS, 2);		/* "Error creating score" */
		CParamText(strBuf, "", "", "");
		StopInform(READNOTESCAN_ALRT);
		io = NoGood; goto ForgetIt;
	}

	if ((io = ConvertFile(doc, refNum, vRef, overrideCl, overrideKS))!=noErr) {
		GetIndCString(strBuf, NOTESCANERRS_STRS, 3);		/* "Error converting NoteScan file" */
		CParamText(strBuf, "", "", "");
		StopInform(READNOTESCAN_ALRT);
		fileOpen = FALSE; goto ForgetIt;
	}

	SetupScanDoc(doc);
	
	ArrowCursor();
	if (NSProblems(doc, &totalErrors, &firstErrMeas, &firstErrStf))	/* 3rd and 4th params unused! */
#ifdef PUBLIC_VERSION
		NULL;
#else
		DebugPrintf("Problems converting. %ld inconsistencies.\n", (long)totalErrors);
#endif

	return noErr;
	
ForgetIt:
	if (fileOpen) FSClose(refNum);	/* Closed by ConvertFile; otherwise close here */
	return io;
}

#endif // TARGET_API_MAC_CARBON_FILEIO


#endif /* !LIGHT_VERSION */
