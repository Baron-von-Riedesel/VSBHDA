/* AUX.C: aux device of VSBHDA.DRV - the master volume of the sound card
 * (set by VSBVXD.386 in the card's mixer). */

#include "VSBDRV.H"

static DWORD volume = 0xFFFFFFFF;

DWORD FAR PASCAL __export __loadds auxMessage( UINT id, UINT msg, DWORD user, DWORD p1, DWORD p2 )
{
	if ( id != 0 && msg != AUXDM_GETNUMDEVS )
		return MMSYSERR_BADDEVICEID;
	switch ( msg ) {
	case AUXDM_GETNUMDEVS:
		return vxdok ? 1 : 0;
	case AUXDM_GETDEVCAPS: {
		AUXCAPS caps;
		UINT i, size = (UINT)p2;
		caps.wMid = 0;
		caps.wPid = 0;
		caps.vDriverVersion = 0x0100;
		copyname( caps.szPname, "VSBHDA Master" );
		caps.wTechnology = AUXCAPS_AUXIN;
		caps.dwSupport = AUXCAPS_VOLUME | AUXCAPS_LRVOLUME;
		if ( size > sizeof(caps) )
			size = sizeof(caps);
		for ( i = 0; i < size; i++ )
			((char FAR *)p1)[i] = ((char *)&caps)[i];
		return MMSYSERR_NOERROR;
	}
	case AUXDM_GETVOLUME:
		*(DWORD FAR *)p1 = volume;
		return MMSYSERR_NOERROR;
	case AUXDM_SETVOLUME:
		volume = p1;
		vcall( VW_SETMASTER, volume, 0, 0, 0, 0 );
		return MMSYSERR_NOERROR;
	default:
		return MMSYSERR_NOTSUPPORTED;
	}
}
