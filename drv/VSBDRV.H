/* VSBDRV.H: VSBHDA.DRV - Windows 3.1 multimedia driver for VSBVXD.386
 * (wave output, FM MIDI output, master volume) */

#include <windows.h>
#include <mmsystem.h>

/* --- from the DDK (mmddk.h) -------------------------------------------- */
#define WODM_GETNUMDEVS      3
#define WODM_GETDEVCAPS      4
#define WODM_OPEN            5
#define WODM_CLOSE           6
#define WODM_PREPARE         7
#define WODM_UNPREPARE       8
#define WODM_WRITE           9
#define WODM_PAUSE           10
#define WODM_RESTART         11
#define WODM_RESET           12
#define WODM_GETPOS          13
#define WODM_GETPITCH        14
#define WODM_SETPITCH        15
#define WODM_GETVOLUME       16
#define WODM_SETVOLUME       17
#define WODM_GETPLAYBACKRATE 18
#define WODM_SETPLAYBACKRATE 19
#define WODM_BREAKLOOP       20

#define MODM_GETNUMDEVS      1
#define MODM_GETDEVCAPS      2
#define MODM_OPEN            3
#define MODM_CLOSE           4
#define MODM_PREPARE         5
#define MODM_UNPREPARE       6
#define MODM_DATA            7
#define MODM_LONGDATA        8
#define MODM_RESET           9
#define MODM_GETVOLUME       10
#define MODM_SETVOLUME       11
#define MODM_CACHEPATCHES    12
#define MODM_CACHEDRUMPATCHES 13

#define AUXDM_GETNUMDEVS     3
#define AUXDM_GETDEVCAPS     4
#define AUXDM_GETVOLUME      5
#define AUXDM_SETVOLUME      6

typedef struct {
	HWAVE hWave;
	const WAVEFORMAT FAR *lpFormat;
	DWORD dwCallback;
	DWORD dwInstance;
} WAVEOPENDESC, FAR *LPWAVEOPENDESC;

typedef struct {
	HMIDI hMidi;
	DWORD dwCallback;
	DWORD dwInstance;
} MIDIOPENDESC, FAR *LPMIDIOPENDESC;

BOOL WINAPI DriverCallback( DWORD dwCallback, UINT uFlags, HANDLE hDevice, UINT uMessage,
							DWORD dwUser, DWORD dwParam1, DWORD dwParam2 );
DWORD WINAPI GetSelectorBase( UINT uSelector );

/* --- VSBVXD protected mode API (vxd/VXDWAVE.C) ------------------------- */
#define VW_GETVERSION 0
#define VW_OPEN       1
#define VW_CLOSE      2
#define VW_WRITE      3
#define VW_GETDONE    4
#define VW_RESET      5
#define VW_PAUSE      6
#define VW_RESTART    7
#define VW_GETPOS     8
#define VW_SETVOLUME  9
#define VW_SETMASTER  10

extern BOOL vxdok;
DWORD vcall( DWORD fn, DWORD ebx, DWORD ecx, DWORD edx, DWORD esi, DWORD FAR *out );
void copyname( char FAR *dst, const char *src );

DWORD FAR PASCAL __export __loadds wodMessage( UINT, UINT, DWORD, DWORD, DWORD );
DWORD FAR PASCAL __export __loadds modMessage( UINT, UINT, DWORD, DWORD, DWORD );
DWORD FAR PASCAL __export __loadds auxMessage( UINT, UINT, DWORD, DWORD, DWORD );
