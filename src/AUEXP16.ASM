
;--- 16-bit only: export table for sndcard.drv

	.386
	.model small

CONST segment dword public 'DATA'
CONST ends

DGROUP group CONST

_TEXT   segment dword public 'CODE'
externdef _AU_init       :near32
;externdef _AU_prestart   :near32
externdef _AU_start      :near32
externdef _AU_stop       :near32
externdef _AU_close      :near32
externdef _AU_getirq     :near32
externdef _AU_isirq      :near32
externdef _AU_getfreq    :near32
externdef _AU_getshortname  :near32
externdef _AU_setoutbytes   :near32
externdef _AU_setrate       :near32
externdef _AU_setmixer_init :near32
externdef _AU_setmixer_one  :near32
externdef _AU_setmixer_outs :near32
externdef _AU_setmixer_all  :near32
externdef _AU_writedata     :near32
externdef _AU_cardbuf_space :near32
_TEXT ends

CONST segment

SCDEXP struct
AU_init     dd offset _AU_init
;AU_prestart dd offset _AU_prestart
AU_start    dd offset _AU_start
AU_stop     dd offset _AU_stop
AU_close    dd offset _AU_close
AU_getirq   dd offset _AU_getirq
AU_isirq    dd offset _AU_isirq
AU_getfreq  dd offset _AU_getfreq
AU_getshortname  dd offset _AU_getshortname
AU_setoutbytes   dd offset _AU_setoutbytes
AU_setrate       dd offset _AU_setrate
AU_setmixer_init dd offset _AU_setmixer_init
AU_setmixer_one  dd offset _AU_setmixer_one
AU_setmixer_outs dd offset _AU_setmixer_outs
AU_setmixer_all  dd offset _AU_setmixer_all
AU_writedata     dd offset _AU_writedata
AU_cardbuf_space dd offset _AU_cardbuf_space
SCDEXP ends

	public ExpTable

ExpTable SCDEXP <>

CONST ends

end
