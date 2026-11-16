#include "cd_guard_packet.h"
#ifdef __CC65__
#include "main.h"
#pragma code-name(push,"BNK08")
#pragma rodata-name(push,"BNK08")
#endif
static unsigned short address_at(const CdGuardPacket *s,unsigned char i) {
    return (unsigned short)s->rows[i*3] | ((unsigned short)s->rows[i*3+1]<<8);
}
static unsigned short sigstart(const CdGuardPacket *s) {
    return (unsigned short)s->siglo | ((unsigned short)s->sighi<<8);
}
static unsigned char allowed(const CdGuardPacket *s,unsigned short a) {
    unsigned short sig=sigstart(s);
    return a>=0x2000 && a<=0x3fff && !(a>=0x2100 && a<=0x21ff)
        && !(a>=sig && a<sig+8);
}
static unsigned char add_impl(CdGuardPacket *s,unsigned short address,unsigned char value) {
    unsigned char i,n;
    if(s->count>=16 || !allowed(s,address))return 1;
    for(i=0;i<s->count;++i)if(address_at(s,i)==address)return 1;
    n=s->count*3;s->rows[n]=(unsigned char)address;s->rows[n+1]=(unsigned char)(address>>8);s->rows[n+2]=value;++s->count;return 0;
}
static unsigned char body_size(const CdGuardPacket *s,unsigned char guarded) {
    unsigned char i,n=0,v;unsigned short a;
    for(i=0;i<s->count;++i) {
        a=address_at(s,i);v=s->rows[i*3+2];
        if(guarded && ((s->bha==1 && a==0x2961 && v==0x23) || (s->bha==2 && a==0x296a && v==0x46))) {n+=10;continue;}
        n+=(unsigned char)((v?2:0)+(a<0x2100?2:3));
    }
    return n;
}
static unsigned char validate_impl(const CdGuardPacket *s) {
    unsigned short sig=sigstart(s),a,correction=0;unsigned char i,j;
    if(sig<0x2200 || sig>0x3ff8 || s->count>16 || s->bha>2)return 1;
    for(i=0;i<s->count;++i) {
        a=address_at(s,i);if(!allowed(s,a))return 1;
        for(j=0;j<i;++j)if(address_at(s,j)==a)return 1;
        if(s->bha==1 && a==0x2961 && s->rows[i*3+2]==0x23)correction=0x2965;
        if(s->bha==2 && a==0x296a && s->rows[i*3+2]==0x46)correction=0x296e;
    }
    if(correction) {
        if(s->count==16)return 1;
        if(!allowed(s,correction))return 1;
        for(i=0;i<s->count;++i)if(address_at(s,i)==correction)return 1;
    }
    /* Adapter includes two eventual JMP operands in its 121-byte code limit. */
    if(s->count && (unsigned short)body_size(s,1)+71>126)return 1;
    return 0;
}
static unsigned char size_impl(const CdGuardPacket *s,unsigned char guarded) {
    if(s->count>16)return 0;
    if(guarded && validate_impl(s))return 0;
    if(!s->count)return 7;
    return (unsigned char)(body_size(s,guarded)+(guarded?71:10));
}
/* Resolve just the requested byte; never materialize the packet in RAM. */
static unsigned char store_byte(unsigned short a,unsigned char v,unsigned char pos) {
    if(v) {
        if(!pos)return 0xa9;
        if(pos==1)return v;
        pos-=2;
    }
    if(!pos)return a<0x2100?(v?0x85:0x64):(v?0x8d:0x9c);
    if(pos==1)return (unsigned char)a;
    return (unsigned char)(a>>8);
}
static unsigned char byte_impl(const CdGuardPacket *s,unsigned char guarded,unsigned char wanted) {
    unsigned char i,n=size_impl(s,guarded),v,width,part;unsigned short a;
    if(!n || wanted>=n)return 0;
    if(wanted<6)return s->vectors[wanted];
    if(wanted==6)return s->count?n:0;
    if(wanted==7)return 0x48;
    if(guarded) {
        if(wanted==8)return 0x08;
        if(wanted<65) {
            part=(unsigned char)(wanted-9);i=0;
            while(part>=7){part-=7;++i;}
            a=sigstart(s)+i;
            if(part==0)return 0xad;
            if(part==1)return (unsigned char)a;
            if(part==2)return (unsigned char)(a>>8);
            if(part==3)return 0xc9;
            if(part==4)return s->signature[i];
            if(part==5)return 0xd0;
            return (unsigned char)(n-19-7*i);
        }
        if(wanted==65)return 0x28;
        wanted-=66;
    }else wanted-=8;
    for(i=0;i<s->count;++i) {
        a=address_at(s,i);v=s->rows[i*3+2];
        if(guarded && ((s->bha==1 && a==0x2961 && v==0x23) || (s->bha==2 && a==0x296a && v==0x46))) {
            if(wanted<5)return store_byte(a,s->bha==1?0xdf:0xce,wanted);
            if(wanted<10)return store_byte(s->bha==1?0x2965:0x296e,s->bha==1?3:7,(unsigned char)(wanted-5));
            wanted-=10;
        }else {
            width=(unsigned char)((v?2:0)+(a<0x2100?2:3));
            if(wanted<width)return store_byte(a,v,wanted);
            wanted-=width;
        }
    }
    if(guarded) {
        if(!wanted)return 0x80;
        if(wanted==1)return 1;
        if(wanted==2)return 0x28;
        wanted-=3;
    }
    return wanted?0x4c:0x68;
}
#ifdef __CC65__
#pragma code-name(pop)
#pragma rodata-name(pop)
#define ENTER unsigned char bank=sysGetBank();sysSetBank(12)
#define LEAVE sysSetBank(bank)
#else
#define ENTER
#define LEAVE
#endif
/* The source pointers can live in the caller's bank. Copy while it is mapped. */
void cd_packet_init(CdGuardPacket *s,unsigned short a,const unsigned char *sig,const unsigned char *v,unsigned char bha){
    unsigned char i;
    for(i=0;i<8;++i)s->signature[i]=sig[i];
    for(i=0;i<6;++i)s->vectors[i]=v[i];
    s->siglo=(unsigned char)a;s->sighi=(unsigned char)(a>>8);s->count=0;s->bha=bha;
}
unsigned char cd_packet_add(CdGuardPacket *s,unsigned short a,unsigned char v){unsigned char r;ENTER;r=add_impl(s,a,v);LEAVE;return r;}
unsigned char cd_packet_validate(const CdGuardPacket *s){unsigned char r;ENTER;r=validate_impl(s);LEAVE;return r;}
unsigned char cd_packet_size(const CdGuardPacket *s,unsigned char g){unsigned char r;ENTER;r=size_impl(s,g);LEAVE;return r;}
unsigned char cd_packet_byte(const CdGuardPacket *s,unsigned char g,unsigned char o){unsigned char r;ENTER;r=byte_impl(s,g,o);LEAVE;return r;}
