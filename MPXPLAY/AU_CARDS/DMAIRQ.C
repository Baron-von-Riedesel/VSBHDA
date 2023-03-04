//**************************************************************************
//*                     This file is part of the                           *
//*                      Mpxplay - audio player.                           *
//*                  The source code of Mpxplay is                         *
//*        (C) copyright 1998-2008 by PDSoft (Attila Padar)                *
//*                http://mpxplay.sourceforge.net                          *
//*                  email: mpxplay@freemail.hu                            *
//**************************************************************************
//*  This program is distributed in the hope that it will be useful,       *
//*  but WITHOUT ANY WARRANTY; without even the implied warranty of        *
//*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.                  *
//*  Please contact with the author (with me) if you want to use           *
//*  or modify this source.                                                *
//**************************************************************************
//function: DMA & IRQ handling
//based on the MPG123 (DOS)

int dbgprintf(const char *fmt, ... );
#define dbgprintf

#include <stdlib.h>
#include <stdio.h>
#include <conio.h>
#include <io.h>
#include <dos.h>
#include <string.h>

#include "mpxplay.h"
#include "dmairq.h"

//declared in control.c
extern struct mpxplay_audioout_info_s au_infos;
extern unsigned int intsoundconfig,intsoundcontrol;

//**************************************************************************
// DMA functions
//**************************************************************************

//-----------------------------------------------------------------------
//common (ISA & PCI)
#ifdef __DOS__
cardmem_t *MDma_alloc_cardmem(unsigned int buffsize)
{
    dbgprintf("MDma_alloc_cardmem\n");
	cardmem_t *dm;
	dm=calloc(1,sizeof(cardmem_t));
	if(!dm)
		exit(MPXERROR_XMS_MEM);
#ifndef DJGPP
	if(!pds_dpmi_dos_allocmem(dm,buffsize)){
#else
	if(!pds_dpmi_xms_allocmem(dm,buffsize)){
#endif
		free(dm);
		exit(MPXERROR_CONVENTIONAL_MEM);
	}
	memset(dm->linearptr,0,buffsize);
	dbgprintf("MDma_alloc_cardmem: %X\n", dm->linearptr);
	return dm;
}

void MDma_free_cardmem(cardmem_t *dm)
{
    dbgprintf("MDma_free_cardmem\n");
	if(dm){
#ifndef DJGPP
		pds_dpmi_dos_freemem(dm);
#else
		pds_dpmi_xms_freemem(dm);
#endif
		free(dm);
	}
}
#endif

unsigned int MDma_get_max_pcmoutbufsize(struct mpxplay_audioout_info_s *aui,unsigned int max_bufsize,unsigned int pagesize,unsigned int samplesize,unsigned long freq_config)
{
	unsigned int bufsize;
    dbgprintf("MDma_get_max_pcmoutbufsize\n");
	if(!max_bufsize)
		max_bufsize=AUCARDS_DMABUFSIZE_MAX;
	if(!pagesize)
		pagesize=AUCARDS_DMABUFSIZE_BLOCK;
	if(samplesize<2)
		samplesize=2;
	bufsize=AUCARDS_DMABUFSIZE_NORMAL/2; // samplesize/=2;

	if(freq_config)
		bufsize=(int)((float)bufsize*(float)freq_config/44100.0);

	if(aui->card_controlbits&AUINFOS_CARDCNTRLBIT_DOUBLEDMA)
		bufsize*=2;                  // 2x bufsize at -ddma
	bufsize*=samplesize;          // 2x bufsize at 32-bit output (1.5x at 24)
	bufsize+=(pagesize-1);        // rounding up to pagesize
	bufsize-=(bufsize%pagesize);  //
	if(bufsize>max_bufsize)
		bufsize=max_bufsize-(max_bufsize%pagesize);
	return bufsize;
}

unsigned int MDma_init_pcmoutbuf(struct mpxplay_audioout_info_s *aui,unsigned int maxbufsize,unsigned int pagesize,unsigned long freq_config)
{
	unsigned int dmabufsize,bit_width,tmp;
	float freq;

	//dbgprintf("fc:%d mb:%d fr:%d bc:%d",freq_config,maxbufsize,aui->freq_card,aui->bits_card);

    dbgprintf("MDma_init_pcmoutbuf\n");
	freq=(freq_config)? freq_config:44100;
	switch(aui->card_wave_id){
	case MPXPLAY_WAVEID_PCM_FLOAT:bit_width=32;break;
	default:bit_width=aui->bits_card;break;
	}

	dmabufsize=(unsigned int)((float)maxbufsize
							  *(float)aui->freq_card/freq);
	// *(float)bit_width/16.0);
	dmabufsize+=(pagesize-1);           // rounding up to pagesize
	dmabufsize-=(dmabufsize%pagesize);  //
	if(dmabufsize<(pagesize*2))
		dmabufsize=(pagesize*2);
	if(dmabufsize>maxbufsize){
		dmabufsize=maxbufsize;
		dmabufsize-=(dmabufsize%pagesize);
	}

	funcbit_smp_int32_put(aui->card_bytespersign,aui->chan_card*((bit_width+7)/8));

	//dmabufsize-=(dmabufsize%aui->card_bytespersign);

	funcbit_smp_int32_put(aui->card_dmasize,dmabufsize);

	if(!aui->card_outbytes)
		funcbit_smp_int32_put(aui->card_outbytes,PCM_OUTSAMPLES*aui->card_bytespersign); // not exact

	tmp=(long) ( (float)aui->freq_card*(float)aui->card_bytespersign
				/((float)INT08_DIVISOR_DEFAULT*(float)INT08_CYCLES_DEFAULT/(float)INT08_DIVISOR_NEW) );

	tmp+=aui->card_bytespersign-1;                              // rounding up
	tmp-=(aui->card_dmaout_under_int08%aui->card_bytespersign); // to pcm_samples
	funcbit_smp_int32_put(aui->card_dmaout_under_int08,tmp);

	funcbit_smp_int32_put(aui->card_dma_lastgoodpos,0); // !!! the soundcard also must to do this
	tmp =aui->card_dmasize/2;
	tmp-=aui->card_dmalastput%aui->card_bytespersign; // round down to pcm_samples
	funcbit_smp_int32_put(aui->card_dmalastput,tmp);
	funcbit_smp_int32_put(aui->card_dmafilled,aui->card_dmalastput);
	funcbit_smp_int32_put(aui->card_dmaspace,aui->card_dmasize-aui->card_dmafilled);

	freq=(aui->freq_song<22050)? 22050.0:(float)aui->freq_song;
	funcbit_smp_int32_put(aui->int08_decoder_cycles,(long)(freq/(float)PCM_OUTSAMPLES)*(float)INT08_DIVISOR_NEW/(float)(INT08_CYCLES_DEFAULT*INT08_DIVISOR_DEFAULT)+1);

	return dmabufsize;
}

void MDma_clearbuf(struct mpxplay_audioout_info_s *aui)
{
	if(aui->card_DMABUFF && aui->card_dmasize)
		pds_memset(aui->card_DMABUFF,0,aui->card_dmasize);
}

void MDma_writedata(struct mpxplay_audioout_info_s *aui,char *src,unsigned long left)
{
	unsigned int todo;

    //dbgprintf("MDma_writedata( buffer=%X, src=%X)\n", aui->card_DMABUFF+aui->card_dmalastput, src);
	todo=aui->card_dmasize-aui->card_dmalastput;

	if(todo<=left){
		pds_memcpy(aui->card_DMABUFF+aui->card_dmalastput,src,todo);
		aui->card_dmalastput=0;
		left-=todo;
		src+=todo;
	}
	if(left){
		pds_memcpy(aui->card_DMABUFF+aui->card_dmalastput,src,left);
		aui->card_dmalastput+=left;
	}
}

// *************** called from int08 **********************************

// checks the DMA buffer and if it's empty, fills with zeroes
void MDma_interrupt_monitor(struct mpxplay_audioout_info_s *aui)
{
	if(aui->card_dmafilled<(aui->card_dmaout_under_int08*2)){
		if(!(aui->card_infobits&AUINFOS_CARDINFOBIT_DMAUNDERRUN)){
			MDma_clearbuf(aui);
			funcbit_smp_enable(aui->card_infobits,AUINFOS_CARDINFOBIT_DMAUNDERRUN);
		}
	}else{
		aui->card_dmafilled-=aui->card_dmaout_under_int08;
		funcbit_smp_disable(aui->card_infobits,AUINFOS_CARDINFOBIT_DMAUNDERRUN);
	}
}

//**************************************************************************
// soundcard IRQ functions
//**************************************************************************
#ifdef __DOS__
static char *newstack_pm_sc;
static farptr new_stack_sc;
static farptr old_stack_sc;
static void (*new_irq_routine_sc)(struct mpxplay_audioout_info_s *);
int_handler_t oldhandler_sc;
static unsigned char sc_irq_controller_mask;
//static unsigned char sc_irq_used;

void loades();
#pragma aux loades = "push ds" "pop es"

void savefpu();
#pragma aux savefpu = "sub esp,200" "fsave [esp]"

void clearfpu();
#pragma aux clearfpu = "finit"

void restorefpu();
#pragma aux restorefpu = "frstor [esp]" "add esp,200"

void stackcall_sc(struct mpxplay_audioout_info_s *,void *);
#pragma aux stackcall_sc parm [eax] [edx] modify[eax]= \
  "mov word ptr old_stack_sc+4,ss" \
  "mov dword ptr old_stack_sc+0,esp" \
  "lss esp,new_stack_sc" \
  "sti" \
  "call edx" \
  "cli"\
  "lss esp,old_stack_sc"

#if defined(DJGPP)

#define loades() { asm("push %ds\n\t pop %es");}
#define savefpu() {asm("sub $200, %esp\n\t fsave (%esp)");}
#define clearfpu() {asm("finit");}
#define restorefpu() {asm("frstor (%esp)\n\t add $200, %esp");}
#define cld() {asm("cld");}

void stackcall_sc(struct mpxplay_audioout_info_s* aui, void* pfun)
{
	asm(
		"movw %%ss, %1 \n\t"
		"movl %%esp, %0 \n\t"
		"lss %2, %%esp \n\t"
		"sti \n\t"
		"push %3 \n\t"
		"call *%4 \n\t"
		"add $4, %%esp \n\t"
		"cli \n\t"
		"lss %0, %%esp \n\t"
		:"+m"(old_stack_sc.off), "+m"(old_stack_sc.sel)
		:"m"(new_stack_sc), "m"(aui), "m"(pfun)
	   );
}
#endif

static void __interrupt __loadds newhandler_sc_special(void)
{
	struct mpxplay_audioout_info_s *aui;
	unsigned int intsoundcntrl_save;

	dbgprintf("newhandler_sc_special\n");

	MPXPLAY_INTSOUNDDECODER_DISALLOW;  // GUS needs this!
	//savefpu();
	//clearfpu();
	loades();
	aui=&au_infos;
	if(!(aui->card_infobits&AUINFOS_CARDINFOBIT_IRQSTACKBUSY)){
		funcbit_enable(aui->card_infobits,(AUINFOS_CARDINFOBIT_IRQSTACKBUSY|AUINFOS_CARDINFOBIT_IRQRUN));
		stackcall_sc(aui,new_irq_routine_sc);
		funcbit_disable(aui->card_infobits,AUINFOS_CARDINFOBIT_IRQSTACKBUSY);
	}
	//restorefpu();
	//if(sc_irq_used && oldhandler_sc){ // irq sharing, run previous handler
	// oldhandler_sc()
	//}else{
	if(aui->card_irq&8)
		outp(0xa0,0x20);
	outp(0x20,0x20);
	//}
	MPXPLAY_INTSOUNDDECODER_ALLOW;
}

static void __interrupt __loadds newhandler_sc_normal(void)
{
	struct mpxplay_audioout_info_s *aui;
	unsigned int intsoundcntrl_save;
	dbgprintf("newhandler_sc_normal\n");
	//loades();
	MPXPLAY_INTSOUNDDECODER_DISALLOW;
	//ENTER_CRITICAL;
	aui=&au_infos;
	funcbit_enable(aui->card_infobits,AUINFOS_CARDINFOBIT_IRQRUN);
	new_irq_routine_sc(aui);
	//if(sc_irq_used && oldhandler_sc){ // irq sharing, run previous handler
	// oldhandler_sc()
	//}else{
	if(aui->card_irq&8)
		outp(0xa0,0x20);
	outp(0x20,0x20);
	//}
	MPXPLAY_INTSOUNDDECODER_ALLOW;
	//LEAVE_CRITICAL;
}

static unsigned int setvect_soundcard_newirq(unsigned int irq_num,void (*irq_routine)(struct mpxplay_audioout_info_s *),unsigned long *card_infobits)
{
	unsigned int irq_controller_port,bitnum;

	dbgprintf("setvect_soundcard_newirq\n");
	funcbit_enable(*card_infobits,AUINFOS_CARDINFOBIT_IRQSTACKBUSY);
	//allocate own stack for long irq routines
	if((*card_infobits)&SNDCARD_SPECIAL_IRQ){
		newstack_pm_sc=(char *)pds_malloc(IRQ_STACK_SIZE);
		if(newstack_pm_sc==NULL)
			return 0;
		new_stack_sc= pds_fardata((char far *)(newstack_pm_sc+IRQ_STACK_SIZE));
	}

	//set values
	new_irq_routine_sc=irq_routine;

	if(irq_num>7){
		bitnum=irq_num-8;
		irq_num+=0x68;
		irq_controller_port=0xa1;
	}else{
		bitnum=irq_num;
		irq_num+=0x08;
		irq_controller_port=0x21;
	}
	bitnum=(1<<bitnum);

	// enable irq for (PCI) card on the irq controller
	if((*card_infobits)&SNDCARD_LOWLEVELHAND){
		sc_irq_controller_mask=inp(irq_controller_port); // saves the irq config
		//sc_irq_used=!(sc_irq_controller_mask&bitnum);
		outp(irq_controller_port,sc_irq_controller_mask&(~bitnum)); // inversed! (0=enable)
		//outp(irq_controller_port,inp(irq_controller_port)&(~bitnum));
	}

	// install protected mode vector
	oldhandler_sc=(int_handler_t)pds_dos_getvect(irq_num);
	if((*card_infobits)&SNDCARD_SPECIAL_IRQ)
		pds_dos_setvect(irq_num,pds_int_handler(newhandler_sc_special));
	else
		pds_dos_setvect(irq_num,pds_int_handler(newhandler_sc_normal));

	funcbit_disable(*card_infobits,AUINFOS_CARDINFOBIT_IRQSTACKBUSY);
	return 1;
}

static void setvect_soundcard_oldirq(unsigned int irq_num,unsigned long *card_infobits)
{
	unsigned int irq_controller_port,bitnum;

	dbgprintf("setvect_soundcard_oldirq\n");
	if(irq_num>7){
		bitnum=irq_num-8;
		irq_num+=0x68;
		irq_controller_port=0xa1;
	}else{
		bitnum=irq_num;
		irq_num+=0x08;
		irq_controller_port=0x21;
	}
	bitnum=(1<<bitnum);

	//restore the previous irq-controller setting
	if((*card_infobits)&SNDCARD_LOWLEVELHAND){
		//outp(irq_controller_port,sc_irq_controller_mask | bitnum);
		//restores the original bit value
		outp(irq_controller_port,(inp(irq_controller_port)&(~bitnum)) | (sc_irq_controller_mask&bitnum)); // inversed! (1=disable)
		//outp(irq_controller_port,inp(irq_controller_port)|bitnum); // inversed! (1=disable)
	}

	//restore the old vector (pmode)
	pds_dos_setvect(irq_num,oldhandler_sc);
	if(newstack_pm_sc){
		free(newstack_pm_sc);
		newstack_pm_sc=NULL;
	}
}
#endif // __DOS__

//--------------------------------------------------------------------------
#ifdef __DOS__

#define OCR1    0x20
#define IMR1    0x21
#define OCR2    0xA0
#define IMR2    0xA1

static void MIrq_OnOff(unsigned int irqnum,unsigned int start)
{
	unsigned int imr=(irqnum>7) ? IMR2 : IMR1;
	unsigned int ocr=(irqnum>7) ? OCR2 : OCR1;
	unsigned int msk=1<<(irqnum&7);
	unsigned int eoi=0x60|(irqnum&7);

	if(start){
		outp(imr,inp(imr) & ~msk);
		outp(ocr,eoi);
		if(irqnum>7)
			MIrq_OnOff(2,1);
	}else{
		outp(imr,inp(imr) | msk);
	}
}

unsigned int MIrq_Start(unsigned int irq_num,void (*irq_routine)(struct mpxplay_audioout_info_s *),unsigned long *card_infobits)
{
	if(irq_routine && (irq_num>=2) && (irq_num<=13)){
		if(!setvect_soundcard_newirq(irq_num,irq_routine,card_infobits))
			return 0;
		MIrq_OnOff(irq_num,1);
		return 1;
	}
	return 0;
}

void MIrq_Stop(unsigned int irq_num,unsigned long *card_infobits)
{
	if(irq_num>=2 && irq_num<=13){
		setvect_soundcard_oldirq(irq_num,card_infobits);
		MIrq_OnOff(irq_num,0);
	}
}

static unsigned int MIrq_is_used(unsigned int irqnum) // is the irq allocated (busy)?
{
	unsigned int irq_controller_port,bitnum,irq_controller_mask;

	if(irqnum>7){
		bitnum=irqnum-8;
		irq_controller_port=0xa1;
	}else{
		bitnum=irqnum;
		irq_controller_port=0x21;
	}
	bitnum=(1<<bitnum);
	irq_controller_mask=inp(irq_controller_port);

	return !(irq_controller_mask&bitnum);
}

static unsigned int MIrq_test(struct mpxplay_audioout_info_s *aui)
{
	unsigned int i;
	funcbit_disable(aui->card_infobits,AUINFOS_CARDINFOBIT_IRQRUN);
	aui->card_handler->card_start(aui);
	for(i=0;i<500;i++){
		if(aui->card_infobits&AUINFOS_CARDINFOBIT_IRQRUN)
			break;
		delay(10);
	}
	aui->card_handler->card_stop(aui);
	if(i<500)
		return 1;
	return 0;
}

unsigned int MIrq_autodetect(struct mpxplay_audioout_info_s *aui)
{
	unsigned int i;

	if(!aui->card_handler)
		return 0;
	if(aui->card_handler->card_setrate)
		aui->card_handler->card_setrate(aui);

	for(i=2;i<14;i++){  // 0,1,14,15 irqs are always busy (I think so)
		if(!MIrq_is_used(i)){
			aui->card_irq=i;
			if(MIrq_test(aui))
				return 1;
		}
	}

	aui->card_irq=0;
	return 0;
}
#endif // __DOS__
