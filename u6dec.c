#include <u.h>
#include <libc.h>
#include <bio.h>

typedef struct Dict Dict;
struct Dict{
	uchar root;
	int word;
};
Dict dict[4096], *dictp;
int sz, top;
Biobuf *bf, *bfo;

u16int
get16(void)
{
	int n;
	uchar u[2];

	n = Bread(bf, u, sizeof u);
	if(n < 0)
		sysfatal("get16: %r");
	else if(n != sizeof u)
		exits(nil);
	return u[1] << 8 | u[0];
}

void
put8(u8int v)
{
	if(Bwrite(bfo, &v, sizeof v) != sizeof v)
		sysfatal("put8: %r");
}

uchar
putstr(int v)
{
	uchar buf[4096], *p;

	p = buf;
	while(v > 0xff){
		*p++ = dict[v].root;
		v = dict[v].word;
	}
	put8(v);
	while(--p >= buf)
		put8(*p);
	return v;
}

void
pushnode(uchar r, int u)
{
	if(dictp == dict + nelem(dict))
		sysfatal("pushnode: dict overflow");
	dictp->root = r;
	dictp->word = u;
	dictp++;
	if(++top >= 1 << sz){
		if(++sz > 12)
			sysfatal("pushnode: word size overflow");
	}
}

u16int
getword(int sz)
{
	static int nbit;
	static u32int u;
	u16int v;

	if(sz < 9 || sz > 12)
		sysfatal("getword: invalid word size");
	if(nbit < sz){
		u &= (1 << nbit) - 1;
		u |= get16() << nbit;
		nbit += 16;
	}
	v = u & (1 << sz) - 1;
	u >>= sz;
	nbit -= sz;
	return v;
}

void
init(void)
{
	sz = 9;
	top = 0x102;
	dictp = dict + 0x102;
}

void
main(int, char**)
{
	int u, v;
	uchar r;

	bf = Bfdopen(0, OREAD);
	bfo = Bfdopen(1, OWRITE);
	if(bf == nil || bfo == nil)
		sysfatal("Bfdopen: %r");
	Bseek(bf, 4, 1);
	init();
	u = 0;
	v = getword(sz);
	if(v != 0x100)
		sysfatal("unknown format");
	for(;;){
		switch(v){
		case 0x100:
			init();
			v = getword(sz);
			put8(v);
			break;
		case 0x101:
			exits(nil);
		default:
			if(v < top){
				r = putstr(v);
				pushnode(r, u);
				break;
			}
			if(v != top)
				sysfatal("invalid word");
			r = putstr(u);
			put8(r);
			pushnode(r, u);
			break;
		}
		u = v;
		v = getword(sz);
	}
}
