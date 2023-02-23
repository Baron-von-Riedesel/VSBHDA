#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <dos.h>
#include <fcntl.h>
#include <assert.h>
#include <dpmi.h>
#ifdef DJGPP
#include <sys/ioctl.h>
#endif
#include "QEMM.h"
#include <DPMI/DPMI.H>
#include <DPMI/DBGUTIL.H>
#include <UNTRAPIO.H>

#define HANDLE_IN_388H_DIRECTLY 1

int dbgprintf(const char *fmt, ... );
#define dbgprintf dbgprintf


static QEMM_IODT_LINK QEMM_IODT_header;
static QEMM_IODT_LINK* QEMM_IODT_Link = &QEMM_IODT_header;
static uint16_t QEMM_EntryIP;
static uint16_t QEMM_EntryCS;

//static BOOL QEMM_InCallback;
static uint16_t QEMM_OldCallbackIP;
static uint16_t QEMM_OldCallbackCS;
static __dpmi_raddr rmcb;

static void __NAKED QEMM_RM_Wrapper()
{//al=data,cl=out,dx=port
    _ASM_BEGIN16
#if HANDLE_IN_388H_DIRECTLY
        _ASM(cmp dx, 0x388)
        _ASM(je is388)
        _ASM(cmp dx, 0x389)
        _ASM(je is389)
    _ASMLBL(normal:)
        _ASM(jmp dword ptr cs:[0])
    _ASM(is388:)
        _ASM(test cl, 4)     // is it OUT?
        _ASM(jnz OUT388H)
        _ASM(mov al, cs:[5]) //in 388h
        _ASM(and al, 0x03)
        _ASM(test al, 0x01)
        _ASM(jz nexttimer)
        _ASM(mov al, 0xC0)
        _ASM(retf)
    _ASMLBL(nexttimer:)
        _ASM(test al, 0x02)
        _ASM(jz ret0)
        _ASM(mov al, 0xA0)
        _ASM(retf)
    _ASMLBL(ret0:)
        _ASM(xor al,al)
        _ASM(retf)
    _ASMLBL(OUT388H:)
        _ASM(mov cs:[4], al)
        _ASM(jmp normal)        
    _ASMLBL(is389:)
        _ASM(test cl, 4)     // is it OUT?
        _ASM(jz normal)
        _ASM(cmp byte ptr cs:[4], 4) //timer reg?
        _ASM(jne normal)
        _ASM(mov cs:[5], al)
        _ASM(jmp normal)        
#endif
    _ASM_END16
}
static void __NAKED QEMM_RM_WrapperEnd() {}

static DPMI_REG QEMM_TrapHandlerREG; /* real-mode callback register struct */

static void QEMM_TrapHandler()
{
    uint16_t port = QEMM_TrapHandlerREG.w.dx;
    uint8_t val = QEMM_TrapHandlerREG.h.al;
    uint8_t out = QEMM_TrapHandlerREG.h.cl;
    QEMM_IODT_LINK* link = QEMM_IODT_header.next;

    //_LOG("Port trap: %s %x\n", out ? "out" : "in", port);
//    QEMM_InCallback = TRUE;
    while(link)
    {
        for(int i = 0; i < link->count; ++i)
        {
            if((link->iodt[i].port&0xFFFF) == port)
            {
                QEMM_TrapHandlerREG.w.flags &= ~CPU_CFLAG;
                //uint8_t val2 = link->iodt[i].handler(port, val, out);
                //QEMM_TrapHandlerREG.h.al = out ? QEMM_TrapHandlerREG.h.al : val2;
                QEMM_TrapHandlerREG.h.al = link->iodt[i].handler(port, val, out);
                return;
            }
        }
        link = link->next;
    }
//    QEMM_InCallback = FALSE;

    /* this should never be reached. */

    //QEMM_TrapHandlerREG.w.flags |= CPU_CFLAG;
    DPMI_REG r = QEMM_TrapHandlerREG;
    r.w.cs = QEMM_OldCallbackCS;
    r.w.ip = QEMM_OldCallbackIP;
    r.w.ss = 0; r.w.sp = 0;
    DPMI_CallRealModeRETF(&r);
    QEMM_TrapHandlerREG.w.flags |= r.w.flags&CPU_CFLAG;
    QEMM_TrapHandlerREG.h.al = r.h.al;
}

//https://www.cs.cmu.edu/~ralf/papers/qpi.txt
//https://fd.lod.bz/rbil/interrup/memory/673f_cx5145.html
//http://mirror.cs.msu.ru/oldlinux.org/Linux.old/docs/interrupts/int-html/rb-7414.htm
uint16_t QEMM_GetVersion(void)
{
    //http://mirror.cs.msu.ru/oldlinux.org/Linux.old/docs/interrupts/int-html/rb-2830.htm
    int fd = 0;
    unsigned int result = _dos_open("QEMM386$", O_RDONLY, &fd);
    uint32_t entryfar = 0;
    //ioctl - read from character device control channel
    DPMI_REG r = {0};
    if (result == 0) //QEMM detected
    {
        int count = ioctl(fd, DOS_RCVDATA, 4, &entryfar);
        _dos_close(fd);
        if(count != 4)
            return 0;
        r.w.cs = entryfar>>16;
        r.w.ip = entryfar&0xFFFF;
    }
    else //check QPIEMU for JEMM
    {
        /* getting the entry point of QPIEMU is non-trivial in protected-mode, since
         * the int 2Fh must be executed as interrupt ( not just "simulated" ). Here
         * a small ( 3 bytes ) helper proc is constructed on the fly, at 0040:00D0,
         * which is the INT 2Fh, followed by an RETF.
         */
        asm(
            "push ds \n\t"
            "push $0x40 \n\t"
            "pop ds \n\t"
            "mov $0xd0, bx \n\t"
            "movl $0xcb2fcd, (bx) \n\t"
            "pop ds \n\t"
           );
        r.w.ax = 0x1684;
        r.w.bx = 0x4354;
        r.w.sp = 0; r.w.ss = 0;
        r.w.cs = 0x40;
        r.w.ip = 0xd0;
        if( DPMI_CallRealModeRETF(&r) != 0 || (r.w.ax & 0xff))
            return 0;
        r.w.ip = r.w.di;
        r.w.cs = r.w.es;
    }
    r.h.ah = 0x03;
    QEMM_EntryIP = r.w.ip;
    QEMM_EntryCS = r.w.cs;
    if( DPMI_CallRealModeRETF(&r) == 0)
        return r.w.ax;
    return 0;
}

BOOL QEMM_Prepare_IOPortTrap()
{
    DPMI_REG r = {0};
    r.w.cs = QEMM_EntryCS;
    r.w.ip = QEMM_EntryIP;
    r.w.ax = 0x1A06;
    /* get current trap handler */
    if(DPMI_CallRealModeRETF(&r) != 0 || (r.w.flags&CPU_CFLAG))
        return FALSE;
    QEMM_OldCallbackIP = r.w.es;
    QEMM_OldCallbackCS = r.w.di;
    //printf("QEMM old callback: %04x:%04x\n",r.w.es, r.w.di);

    /* get a realmode callback */
    if ( !DPMI_AllocateRMCB_RETF(&QEMM_TrapHandler, &QEMM_TrapHandlerREG, &rmcb ) )
        return FALSE;

#if HANDLE_IN_388H_DIRECTLY
	/* copy 16-bit code to DOS memory
	 * bytes 0-3 are the realmode callback
     * bytes 4-5 are used as data
	 */
    uint32_t codesize = (uintptr_t)&QEMM_RM_WrapperEnd - (uintptr_t)&QEMM_RM_Wrapper;
    uint32_t dosmem = DPMI_HighMalloc((codesize + 4 + 2 + 15)>>4, TRUE);
    DPMI_CopyLinear(DPMI_SEGOFF2L(dosmem, 0), DPMI_PTR2L(&rmcb), 4);
    void* buf = malloc(codesize);
    memcpy_c2d(buf, &QEMM_RM_Wrapper, codesize); //copy to ds seg in case cs&ds are not same
    DPMI_CopyLinear(DPMI_SEGOFF2L(dosmem, 4+2), DPMI_PTR2L(buf), codesize);
    free(buf);

    /* set new trap handler ES:DI */
    r.w.es = dosmem&0xFFFF;
    r.w.di = 4+2;
    r.w.ax = 0x1A07;
    if( DPMI_CallRealModeRETF(&r) != 0 || (r.w.flags&CPU_CFLAG))
    {
        DPMI_HighFree(dosmem);
        return FALSE;
    }
#else
    r.w.es = rmcb.segment;
    r.w.di = rmcb.offset16;
    r.w.ax = 0x1A07;
    if( DPMI_CallRealModeRETF(&r) != 0 || (r.w.flags&CPU_CFLAG))
        return FALSE;
#endif
    return TRUE;
}


BOOL QEMM_Install_IOPortTrap(QEMM_IODT* inputp iodt, uint16_t count, QEMM_IOPT* outputp iopt, QEMM_IODT_LINK* oldlink )
{
    static DPMI_REG r = {0};
    assert(QEMM_IODT_Link->next == NULL);
    QEMM_IODT_LINK* newlink = oldlink;
    if ( !newlink ) {
        newlink = (QEMM_IODT_LINK *)malloc( sizeof( QEMM_IODT_LINK ) + sizeof( QEMM_IODT ) * count );
        memcpy( newlink + 1, iodt, sizeof( QEMM_IODT)*count );
        iopt->memory = (uintptr_t)newlink;
    }
    QEMM_IODT* mem = (QEMM_IODT *)( newlink + 1 );

    for(int i = 0; i < count; ++i)
    {
        r.w.cs = QEMM_EntryCS;
        r.w.ip = QEMM_EntryIP;
        if ( !oldlink ) {
            r.w.ax = 0x1A08;
            r.w.dx = mem[i].port;
            DPMI_CallRealModeRETF(&r);
            mem[i].port |= (r.h.bl)<<16; //previously trapped state
        }
        r.w.ax = 0x1A09;
        r.w.dx = mem[i].port & 0xFFFF;
        DPMI_CallRealModeRETF(&r); //set port trapped
    }

    newlink->iodt = mem;
    newlink->count = count;
    newlink->prev = QEMM_IODT_Link;
    newlink->next = NULL;
    /* this function may be called from inside ISR, don't modify IF! */
	//CLIS();
    QEMM_IODT_Link->next = newlink;
    QEMM_IODT_Link = newlink;
    //STIL();
    return TRUE;
}

BOOL QEMM_Uninstall_IOPortTrap(QEMM_IOPT* inputp iopt, BOOL bFree )
{
    static DPMI_REG r = {0};
    /* may be called from inside ISR, don't modify IF! */
    //CLIS();
    QEMM_IODT_LINK* link = (QEMM_IODT_LINK*)iopt->memory;
    if (!link)
        return FALSE;
    link->prev->next = link->next;
    if(link->next) link->next->prev = link->prev;
    if(QEMM_IODT_Link == link)
        QEMM_IODT_Link = link->prev;
    //STIL();

    for(int i = 0; i < link->count; ++i)
    {
        if(!(link->iodt[i].port&0xFFFF0000L)) //previously not trapped
        {
            r.w.cs = QEMM_EntryCS;
            r.w.ip = QEMM_EntryIP;
            r.w.ax = 0x1A0A; //clear trapped
            r.w.dx = link->iodt[i].port&0xFFFF;
            DPMI_CallRealModeRETF(&r);
        }
    }

    //free(link->iodt);
    if ( bFree )
		free(link);
    
    if(QEMM_IODT_header.next == NULL) //empty
    {
        r.w.cs = QEMM_EntryCS;
        r.w.ip = QEMM_EntryIP;
        r.w.ax = 0x1A07;
        r.w.es = QEMM_OldCallbackCS;
        r.w.di = QEMM_OldCallbackIP;
        if( DPMI_CallRealModeRETF(&r) != 0) //restore old handler
            return FALSE;
        DPMI_FreeRMCB( &rmcb );
    }
    return TRUE;
}

