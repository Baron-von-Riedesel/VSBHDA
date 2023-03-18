//**************************************************************************
//*                     This file is part of the                           *
//*                      Mpxplay - audio player.                           *
//*                  The source code of Mpxplay is                         *
//*        (C) copyright 1998-2011 by PDSoft (Attila Padar)                *
//*                http://mpxplay.sourceforge.net                          *
//*                  email: mpxplay@freemail.hu                            *
//**************************************************************************
//*  This program is distributed in the hope that it will be useful,       *
//*  but WITHOUT ANY WARRANTY; without even the implied warranty of        *
//*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.                  *
//*  Please contact with the author (with me) if you want to use           *
//*  or modify this source.                                                *
//**************************************************************************
//function: string handling

#include <string.h>
#include "mpxplay.h"

unsigned int pds_strcpy(char *dest,char *src)
{
 char *begin;
 if(!dest)
  return 0;
 if(!src){
  *dest=0;
  return 0;
 }
 begin=src;
 do{
  char c=*src;
  *dest=c;
  if(!c)
   break;
  dest++;src++;
 }while(1);
 return (src-begin); // returns the lenght of string, not the target pointer!
}

unsigned int pds_strmove(char *dest,char *src)
{
 unsigned int len,count;
 if(!dest)
  return 0;
 if(!src){
  *dest=0;
  return 0;
 }
 if(dest<src)
  return pds_strcpy(dest,src);
 count=len=pds_strlen(src)+1;
 src+=len;
 dest+=len;
 do{
  src--;dest--;
  *dest=*src;
 }while(--count);
 return len; // returns the lenght of string
}

unsigned int pds_strncpy(char *dest,char *src,unsigned int maxlen)
{
 char *begin;
 if(!dest || !maxlen)
  return 0;
 if(!src){
  *dest=0;
  return 0;
 }
 begin=src;
 do{
  char c=*src;
  *dest=c;
  if(!c)
   break;
  dest++;src++;
 }while(--maxlen);
 return (src-begin); // returns the lenght of string, not the target pointer!
}

unsigned int pds_strcat(char *strp1,char *strp2)
{
 if(!strp1 || !strp2)
  return 0;
 return pds_strcpy(&strp1[pds_strlen(strp1)],strp2);
}

static int pds_strchknull(char *strp1,char *strp2)
{
 register const unsigned char *s1 = (const unsigned char *) strp1;
 register const unsigned char *s2 = (const unsigned char *) strp2;

 if(!s1 || !s1[0])
  if(s2 && s2[0])
   return -1;
  else
   return 0;

 if(!s2 || !s2[0])
  if(s1 && s1[0])
   return 1;
  else
   return 0;

 return 2;
}

int pds_strcmp(char *strp1,char *strp2)
{
 register const unsigned char *s1 = (const unsigned char *) strp1;
 register const unsigned char *s2 = (const unsigned char *) strp2;
 unsigned char c1,c2;
 int retcode=pds_strchknull(strp1,strp2);
 if(retcode!=2)
  return retcode;

  do{
   c1 = (unsigned char) *s1++;
   c2 = (unsigned char) *s2++;
   if(!c1)
    break;
  }while (c1 == c2);

  return c1 - c2;
}

int pds_stricmp(char *strp1,char *strp2)
{
 register const unsigned char *s1 = (const unsigned char *) strp1;
 register const unsigned char *s2 = (const unsigned char *) strp2;
 unsigned char c1,c2;
 int retcode=pds_strchknull(strp1,strp2);
 if(retcode!=2)
  return retcode;

 do{
  c1 = (unsigned char) *s1++;
  c2 = (unsigned char) *s2++;
  if(!c1)
   break;
  if(c1>='a' && c1<='z')  // convert to uppercase
   c1-=32;                // c1-='a'-'A'
  if(c2>='a' && c2<='z')
   c2-=32;
 }while(c1 == c2);
 return (c1 - c2);
}

//faster (no pointer check), returns 1 if equal
unsigned int pds_stri_compare(char *strp1,char *strp2)
{
 char c1,c2;
 do{
  c1=*strp1;
  c2=*strp2;
  if(c1!=c2){
   if(c1>='a' && c1<='z')  // convert to uppercase
    c1-=32;                // c1-='a'-'A'
   if(c2>='a' && c2<='z')
    c2-=32;
   if(c1!=c2)
    return 0;
  }
  strp1++;strp2++;
 }while(c1 && c2);
 return 1;
}

int pds_strricmp(char *str1,char *str2)
{
 char *pstr1=str1,*pstr2=str2;
 int retcode=pds_strchknull(str1,str2);
 if(retcode!=2)
  return retcode;

 while(pstr1[0]!=0)
  pstr1++;
 while(pstr1[0]==0 || pstr1[0]==32)
  pstr1--;
 if(pstr1<=str1)
  return 1;
 while(pstr2[0]!=0)
  pstr2++;
 while(pstr2[0]==0 || pstr2[0]==32)
  pstr2--;
 if(pstr2<=str2)
  return -1;
 while(pstr1>=str1 && pstr2>=str2){
  char c1=pstr1[0];
  char c2=pstr2[0];
  if(c1>='a' && c1<='z')  // convert to uppercase
   c1-=32;
  if(c2>='a' && c2<='z')
   c2-=32;
  if(c1!=c2){
   if(c1<c2)
    return -1;
   else
    return 1;
  }
  pstr1--;pstr2--;
 }
 return 0;
}

int pds_strlicmp(char *str1,char *str2)
{
 char c1,c2;
 int retcode=pds_strchknull(str1,str2);
 if(retcode!=2)
  return retcode;

 do{
  c1=*str1;
  c2=*str2;
  if(!c1 || !c2)
   break;
  if(c1!=c2){
   if(c1>='a' && c1<='z')  // convert to uppercase
    c1-=32;
   if(c2>='a' && c2<='z')
    c2-=32;
   if(c1!=c2){
    if(c1<c2)
     return -1;
    else
     return 1;
   }
  }
  str1++;str2++;
 }while(1);
 return 0;
}

int pds_strncmp(char *strp1,char *strp2,unsigned int counter)
{
 char c1,c2;
 int retcode=pds_strchknull(strp1,strp2);
 if(retcode!=2)
  return retcode;
 if(!counter)
  return 0;
 do{
  c1=*strp1;
  c2=*strp2;
  if(c1!=c2)
   if(c1<c2)
    return -1;
   else
    return 1;
  strp1++;strp2++;
 }while(c1 && c2 && --counter);
 return 0;
}

int pds_strnicmp(char *strp1,char *strp2,unsigned int counter)
{
 char c1,c2;
 int retcode=pds_strchknull(strp1,strp2);
 if(retcode!=2)
  return retcode;
 if(!counter)
  return 0;
 do{
  c1=*strp1;
  c2=*strp2;
  if(c1!=c2){
   if(c1>='a' && c1<='z')
    c1-=32;
   if(c2>='a' && c2<='z')
    c2-=32;
   if(c1!=c2){
    if(c1<c2)
     return -1;
    else
     return 1;
   }
  }
  strp1++;strp2++;
 }while(c1 && c2 && --counter);
 return 0;
}

unsigned int pds_strlen(char *strp)
{
 char *beginp;
 if(!strp || !strp[0])
  return 0;
 beginp=strp;
 do{
  strp++;
 }while(*strp);
 return (unsigned int)(strp-beginp);
}

unsigned int pds_strlenc(char *strp,char seek)
{
 char *lastnotmatchp,*beginp;

 if(!strp || !strp[0])
  return 0;

 lastnotmatchp=NULL;
 beginp=strp;
 do{
  if(*strp!=seek)
   lastnotmatchp=strp;
  strp++;
 }while(*strp);

 if(!lastnotmatchp)
  return 0;
 return (unsigned int)(lastnotmatchp-beginp+1);
}

/*unsigned int pds_strlencn(char *strp,char seek,unsigned int len)
{
 char *lastnotmatchp,*beginp;

 if(!strp || !strp[0] || !len)
  return 0;

 lastnotmatchp=NULL;
 beginp=strp;
 do{
  if(*strp!=seek)
   lastnotmatchp=strp;
  strp++;
 }while(*strp && --len);

 if(!lastnotmatchp)
  return 0;
 return (unsigned int)(lastnotmatchp-beginp+1);
}*/

char *pds_strchr(char *strp,char seek)
{
 if(!strp)
  return NULL;
 do{
  char c=strp[0];
  if(c==seek)
   return strp;
  if(!c)
   break;
  strp++;
 }while(1);
 return NULL;
}

char *pds_strrchr(char *strp,char seek)
{
 char *foundp=NULL,curr;

 if(!strp)
  return foundp;

 curr=*strp;
 if(!curr)
  return foundp;
 do{
  if(curr==seek)
   foundp=strp;
  strp++;
  curr=*strp;
 }while(curr);
 return foundp;
}

char *pds_strnchr(char *strp,char seek,unsigned int len)
{
 if(!strp || !strp[0] || !len)
  return NULL;
 do{
  if(*strp==seek)
   return strp;
  strp++;
 }while(*strp && --len);
 return NULL;
}

unsigned int pds_strcutspc(char *src)
{
 char *dest,*dp;

 if(!src)
  return 0;

 dest=src;

 while(src[0] && (src[0]==32))
  src++;

 if(!src[0]){
  dest[0]=0;
  return 0;
 }
 if(src>dest){
  char c;
  dp=dest;
  do{
   c=*src++; // move
   *dp++=c;  //
  }while(c);
  dp-=2;
 }else{
  while(src[1])
   src++;
  dp=src;
 }
 while((*dp==32) && (dp>=dest))
  *dp--=0;

 if(dp<dest)
  return 0;

 return (dp-dest+1);
}

//-------------------------------------------------------------------------------------------------------

static unsigned int dekadlim[10]={1,10,100,1000,10000,100000,1000000,10000000,100000000,1000000000};

static unsigned int pds_log10(long value)
{
 unsigned int dekad=1;
 while((value>=dekadlim[dekad]) && (dekad<10))
  dekad++;
 return dekad;
}

void pds_ltoa(int value,char *ltoastr)
{
 unsigned int dekad=pds_log10(value);
 do{
  *ltoastr++=(value/dekadlim[dekad-1])%10+0x30;
 }while(--dekad);
 *ltoastr=0;
}

/*void pds_ltoa16(int value,char *ltoastr)
{
 static int dekadlim[9]={1,0x10,0x100,0x1000,0x10000,0x100000,0x1000000,0x10000000,0x100000000};
 int dekad;

 dekad=1;
 while(value>dekadlim[dekad] && dekad<9)
  dekad++;
 do{
  int number=(value/dekadlim[dekad-1])%16;
  if(number>9)
   ltoastr[0]=number-10+'A';
  else
   ltoastr[0]=number+'0';
  dekad--;
  ltoastr++;
 }while(dekad>0);
 ltoastr[0]=0;
}*/

long pds_atol(char *strp)
{
 long number=0;
 unsigned int negative=0;

 if(!strp || !strp[0])
  return number;

 while(*strp==' ')
  strp++;

 if(*strp=='-'){
  negative=1;
  strp++;
 }else{
  if(*strp=='+')
   strp++;
 }

 do{
  if((strp[0]<'0') || (strp[0]>'9'))
   break;
  number=(number<<3)+(number<<1);     // number*=10;
  number+=(unsigned long)strp[0]-'0';
  strp++;
 }while(1);
 if(negative)
  number=-number;
 return number;
}

long pds_atol16(char *strp)
{
 unsigned long number=0;

 if(!strp || !strp[0])
  return number;

 while(*strp==' ')
  strp++;

 if(*((unsigned short *)strp)==(((unsigned short)'x'<<8)|(unsigned short)'0')) // C format
  strp+=2;
 else if(*strp=='#') // HTML format
  strp++;

 do{
  char c=*strp++;
  if(c>='0' && c<='9')
   c-='0';
  else
   if(c>='a' && c<='f')
    c-=('a'-10);
   else
    if(c>='A' && c<='F')
     c-=('A'-10);
    else
     break;

  number<<=4;     // number*=16;
  number+=(unsigned long)c;
 }while(1);

 return number;
}

void pds_str_to_hexs(char *src,char *dest,unsigned int destlen)
{
 if(!src || !dest || !destlen)
  return;
 do{
  unsigned char cl=*((unsigned char *)src),ch;
  if(!cl)
   break;
  ch=cl>>4;
  if(ch<=9)
   ch+='0';
  else
   ch+='A'-10;
  *((unsigned char *)dest)=ch;
  dest++;
  cl&=0x0f;
  if(cl<=9)
   cl+='0';
  else
   cl+='A'-10;
  *((unsigned char *)dest)=cl;
  dest++;
  src++;
  destlen-=2;
 }while(destlen>=2);
 *dest=0;
}

void pds_hexs_to_str(char *src,char *dest,unsigned int destlen)
{
 unsigned int i=0,d=0;
 if(!src || !dest || !destlen)
  return;
 do{
  unsigned char c=*((unsigned char *)src++);
  if(!c)
   break;
  if((c>='0') && (c<='9'))
   c-='0';
  else if((c>='A') && (c<='F'))
   c-='A'-10;
  else if((c>='a') && (c<='a'))
   c-='a'-10;
  else
   break;
  d=(d<<4)|c;
  if(i&1){
   *dest++=d;
   destlen--;
   d=0;
  }
  i++;
 }while(destlen>1);
 *dest=0;
}

//-----------------------------------------------------------------------

