/* Application.h
   For "Sheets" generic application shell for editing multiple documents,
   by Douglas McKenna
   Nightingale version
*/

#ifndef say
#define say DebugPrintf
#endif

#define	NIL			((void *)0)			/* ??This should go away: change all NIL to NULL! */
#ifndef TRUE
	#define TRUE		1
#endif
#ifndef FALSE
	#define FALSE		0
#endif

#define BRING_TO_FRONT	((WindowPtr)(-1L))

#define SCROLLBAR_WIDTH	15

/* WindowKind values for our own windows */

enum {
		OURKIND = 100,
		DOCUMENTKIND = OURKIND,
		PALETTEKIND,
		TOPKIND
	};

enum {
		TOOL_PALETTE = 0,
		HELP_PALETTE,
		CLAVIER_PALETTE,
		TOTAL_PALETTES								/* 1 more than last palette index */
	};

#define MAX_UPDATE_RECTS	(2 + TOTAL_PALETTES*2)
