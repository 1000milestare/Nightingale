/*	MusicFont.h for Nightingale */

void PrintMusFontTables(Document *);
Byte MapMusChar(INT16, Byte);
void MapMusPString(INT16, Byte []);
DDIST MusCharXOffset(INT16, Byte, DDIST);
DDIST MusCharYOffset(INT16, Byte, DDIST);
Boolean MusFontHas16thFlag(INT16);
Boolean MusFontHasCurlyBraces(INT16);
Boolean MusFontHasRepeatDots(INT16);
Boolean MusFontUpstemFlagsHaveXOffset(INT16);
INT16 MusFontStemSpaceWidthPixels(Document *, INT16, DDIST);
DDIST MusFontStemSpaceWidthDDIST(INT16, DDIST);
DDIST UpstemExtFlagLeading(INT16, DDIST);
DDIST DownstemExtFlagLeading(INT16, DDIST);
DDIST Upstem8thFlagLeading(INT16, DDIST);
DDIST Downstem8thFlagLeading(INT16, DDIST);
void InitDocMusicFont(Document *);

void SetTextSize(Document *);
void BuildCharRectCache(Document *);
Rect CharRect(INT16);

void NumToSonataStr(long, unsigned char *);
void GetMusicAscDesc(Document *, unsigned char *, INT16, INT16 *, INT16 *);
INT16 GetMFontSizeIndex(INT16);
INT16 GetMNamedFontSize(INT16);
INT16 Staff2MFontSize(DDIST);
INT16 GetActualFontSize(INT16);

INT16 GetYHeadFudge(INT16 fontSize);
INT16 GetYRestFudge(INT16 fontSize, INT16 durCode);
