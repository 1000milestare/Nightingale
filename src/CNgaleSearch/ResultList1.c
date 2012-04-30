/* ResultList.c for Nightingale Search, by Donald Byrd. Incorporates modeless-dialoghandling code from Apple sample source code by Nitin Ganatra. */#include "Nightingale.precomp.h"#include "Nightingale.appl.h"#include "SearchScorePrivate.h"#include <string.h>#include <Dialogs.h>#include <Lists.h>#include <Sound.h>#ifdef SEARCH_DBFORMAT_MEFvoid DoDialogEvent(EventRecord *theEvent){	AlwaysErrMsg("DoDialogEvent called in the MEF version!");}#elseextern Boolean gDone;extern Boolean gInBackground;typedef struct {	ListHandle hndl;	Rect bounds,scroll,content,dataBounds;	Point cSize,cell;	short nCells;} UserList;static INT16 itemCount, maxItemCount, sPatLen;static UserList theList;static char *resultStr = NULL;static INT16 *docNumA = NULL;static LINK *foundLA = NULL;static INT16 *foundVoiceA = NULL;static DB_LINK *matchedObjFA = NULL;static DB_LINK *matchedSubobjFA = NULL;#define MAX_LINELEN 100static enum {	BUT1_OK = 1,	LIST2,	LABEL_QMATCHESQ,	LABEL_MATCHES,	LASTITEM} E_ResultListItems;static Boolean EventFilter(EventRecord *theEvent, WindowPtr theFrontWindow);static Boolean HandleListEvent(EventRecord *theEvent);/* ------------------------------------------------------------------------------------- *//*	DoDialogEvent - After checking that EventFilter's result is TRUE, this calls 	DialogSelect to check if any controls were hit, and if so this acts accordingly.*/void DoDialogEvent(EventRecord *theEvent){	//DialogPtr	theDialog = (DialogPtr)FrontWindow();	WindowPtr frontWindow = FrontWindow();	short		theItem;	GrafPtr		origPort;	Point		where;	INT16		modifiers;	GetPort(&origPort);	//SetPort(GetDialogWindowPort(theDialog));		SetPort(GetWindowPort(frontWindow));		if (EventFilter(theEvent, frontWindow)) {		TextFont(0);									/* Set to the system font */		TextSize(12);		DialogPtr theDialog = GetDialogFromWindow(frontWindow);		if (DialogSelect(theEvent, &theDialog, &theItem)) {			if (theDialog!=NULL) {				switch (theItem) {									case BUT1_OK:						/* Just throw out the dialog and return. */						DisposeDialog(theDialog);						break;								default:						break;							}			}		}	}Finish:	SetPort(origPort);	return;BailOut:	DebugStr((StringPtr)"\pGetCtlHandle in DoDialogEvent failed");}#define RESULTLIST_DLOG 966/* ------------------------------------------------------------------------------------- *//* DoResultList - Creates the modeless dialog (from a DLOG resource) and setsup the initial state of the controls and user items.  Note that it also allocatestwo globals that will always point to the user item proc routines.  This is neededonly once and can be called by all sebsequent calls to DoResultList, becausethe routines won't change.  In the 68K world, these routines are redefined to just be plain old proc pointers; on PowerPC builds they use real UPPs.*//* Display the Result List modeless dialog.  Return TRUE if all OK, FALSE on error. */Boolean DoResultList1(char label[]){	short 					tempType;	Handle					tempHandle;	Rect						tempRect;	DialogPtr				mlDlog;	static UserItemUPP	procForBorderUserItem = NULL;	static UserItemUPP	procForListUserItem = NULL;	mlDlog = GetNewDialog(RESULTLIST_DLOG, NULL, BRING_TO_FRONT);	if (mlDlog==NULL) { MissingDialog(RESULTLIST_DLOG); return FALSE; }		gResultListDlog = mlDlog;		WindowPtr w = GetDialogWindow(mlDlog);	ShowWindow(w);	SelectWindow(w);		int kind = GetWindowKind(w);	int dlogKind = dialogKind;		return TRUE;}/* EventFilter - This is the first thing done if a dialog event is received, giving you a chance to perform any special stuff before passing control on to DialogSelect.If this routine returns TRUE, the event processing will continue, and DialogSelectwill be called to perform hit detection on the controls.  If FALSE is returned,it means the event is already handled and the main event loop will continue. */	Boolean EventFilter(EventRecord *theEvent, WindowPtr theFrontWindow){	char 		theKey;	short 		theHiliteCode;		switch (theEvent->what)	{			case keyDown:		case autoKey:					theKey = theEvent->message & charCodeMask;				if ((theEvent->modifiers & cmdKey)!=0) {	#if 0					long menuResult;									AdjustMenus();					menuResult = MenuKey(theKey);								if ((menuResult >> 16) != 0) {						Boolean editOpPerformed = MenuCommand(menuResult);												if (editOpPerformed)							/* You may ask yourself, "Why are we exiting when an 	*/							/* edit operation is performed?"  Well, DialogSelect 	*/							/* performs some automatic handling for any editText 	*/							/* items that may be in the Dialog, and since we've 	*/							/* already handled them in MenuCommand() we don't 		*/							/* want  DialogSelect to do anything		 			*/							return FALSE;					}#endif				}								break;						case mouseDown:			WindowPtr w = (WindowPtr)theEvent->message;			if (w == NULL) w = theFrontWindow;						WindowPartCode part = FindWindow(theEvent->where,&w);						//w==GetDialogWindow(dlog);			Rect bounds = GetQDScreenBitsBounds();			switch (part) {				case inContent:					//if (HandleListEvent(theEvent)) {					//	return TRUE;					//}					break;				case inDrag:					DragWindow(w,theEvent->where,&bounds);					return TRUE;					break;									default:					break;								}			return  FALSE;			break;		case activateEvt:								/* This is where we take care of hiliting the		*/				/* controls according to whether or not the dialog 	*/				/* is frontmost. 									*/				theFrontWindow = (WindowPtr)theEvent->message;				if (theEvent->modifiers & activeFlag)					theHiliteCode = CTL_ACTIVE;				else					theHiliteCode = CTL_INACTIVE;				//HiliteAllControls(GetDialogFromWindow(theFrontWindow), theHiliteCode);								//if (theHiliteCode == CTL_ACTIVE)				//	CloseToolPalette();								return FALSE;	}	/* If we haven't returned FALSE by now, go ahead	*/	/* and return TRUE so DialogSelect can do its 		*/	/* thing, like update the window, deal with an 		*/	/* item hit, etc.									*/	return TRUE;}#endif /* !SEARCH_DBFORMAT_MEF */