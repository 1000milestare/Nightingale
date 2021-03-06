typedef struct {
	TEHandle			teH;
	ControlHandle	scrollH;
	Rect				bounds;		/* rect enclosing TE field and scrollbar */
	Boolean			active;		/* is activated? */
} EDITFIELD, *PEDITFIELD;

Boolean CreateEditField(WindowPtr, Rect, short, short, short, Handle, Boolean, PEDITFIELD);
void DisposeEditField(PEDITFIELD);
CharsHandle GetEditFieldText(PEDITFIELD);
Boolean SetEditFieldText(PEDITFIELD, Handle, unsigned char *, Boolean);
void ScrollToSelection(PEDITFIELD);
Boolean DoEditFieldClick(PEDITFIELD, EventRecord *);
void DoTEEdit(PEDITFIELD, short);						/* see enum below for legal values */
void DoTEFieldKeyEvent(PEDITFIELD, EventRecord *);
void DoTEFieldUpdate(PEDITFIELD);
void DoTEFieldActivateEvent(PEDITFIELD, EventRecord *);
Boolean DoTEFieldIdle(PEDITFIELD, EventRecord *);
void ReadDeskScrap(void);
void WriteDeskScrap(void);

enum {											/* for DoTEEdit */
	TE_CUT=0,
	TE_COPY,
	TE_PASTE,
	TE_CLEAR
};

