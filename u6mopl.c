#include <u.h>
#include <libc.h>
#include <bio.h>
#include <fcall.h>

typedef struct Channel Channel;
struct Channel{
	int o1;
	u16int f;
	char Δf;
	char fmA;
	int fmΔA;
	uchar fmmax;
	uchar fmfact;
	uchar lvl;
	int Δlvl;
	uchar Δlvlt;
	uchar Δlvldt;
};
Channel ch[9] = {
	{.o1=0x00}, {.o1=0x01}, {.o1=0x02}, {.o1=0x08}, {.o1=0x09},
	{.o1=0x0a}, {.o1=0x10}, {.o1=0x11}, {.o1=0x12}
};

typedef struct Sect Sect;
struct Sect{
	uchar *p;
	int n;
	uchar *ret;
};
Sect sect[32], *secp = sect;

u16int fnum[] = {
	0x000, 0x158, 0x182, 0x1b0, 0x1cc, 0x203, 0x241, 0x286,
	0x000, 0x16a, 0x196, 0x1c7, 0x1e4, 0x21e, 0x25f, 0x2a8,
	0x000, 0x147, 0x16e, 0x19a, 0x1b5, 0x1e9, 0x224, 0x266
};

uchar buf[8192], *bufp = buf, *bufe = buf, *loop = buf;
uchar *instp[16];
uchar outbuf[1024], *outp = outbuf;
int Δtc, doloop;

void
flush(void)
{
	if(outp == outbuf)
		return;
	PBIT16(outp-2, 1);
	write(1, outbuf, outp-outbuf);
	memset(outbuf, 0, outp-outbuf);
	outp = outbuf;
}

void
out(uchar r, uchar v)
{
	if(outp >= outbuf + sizeof outbuf)
		flush();
	outp[0] = r;
	outp[1] = v;
	outp += 4;
}

void
barf(void)
{
	if(outp == outbuf)
		out(0, 0);
	flush();
}

uchar
get8(void)
{
	if(bufp == bufe)
		sysfatal("premature eof");
	return *bufp++;
}

void
setinst(Channel *c)
{
	uchar *p;

	p = instp[get8()];
	out(0x20 + c->o1, *p++);
	out(0x40 + c->o1, *p++);
	out(0x60 + c->o1, *p++);
	out(0x80 + c->o1, *p++);
	out(0xe0 + c->o1, *p++);
	out(0x20 + c->o1+3, *p++);
	out(0x40 + c->o1+3, *p++);
	out(0x60 + c->o1+3, *p++);
	out(0x80 + c->o1+3, *p++);
	out(0xe0 + c->o1+3, *p++);
	out(0xc0 + c - ch, *p);
}

void
setΔlvl(int dir)
{
	uchar v;
	Channel *c;

	v = get8();
	c = ch + (v >> 4);
	v = (v & 0xf) + 1;
	c->Δlvl = dir;
	c->Δlvlt = v;
	c->Δlvldt = v;
}

void
setf(int n, int f)
{
	out(0xa0 + n, f);
	out(0xb0 + n, f >> 8);
}

void
setc(Channel *c, int on)
{
	int f;
	uchar v;

	v = get8();
	f = fnum[v & 0x1f] | (v & 0xe0) << 5 | on << 13;
	if(on)
 		setf(c - ch, f & ~(1 << 13));
	setf(c - ch, c->f = f);
}

void
up(void)
{
	u16int w;
	Channel *c;

	for(c=ch; c<ch+nelem(ch); c++){
		if(c->Δf != 0)
			setf(c - ch, c->f += c->Δf);
		else if(c->fmfact != 0 && c->f & 1<<13){
			if(c->fmA >= c->fmmax)
				c->fmΔA = -1;
			else if(c->fmA == 0)
				c->fmΔA = 1;
			c->fmA += c->fmΔA;
			w = c->fmfact * (int)(c->fmA - c->fmmax / 2);
			setf(c - ch, c->f + w & 0xffff);
		}
		if(c->Δlvl == 0 || --c->Δlvlt > 0)
			continue;
		c->Δlvlt = c->Δlvldt;
		c->lvl += c->Δlvl;
		if(c->lvl > 0x3f){
			c->lvl = 0x3f;
			c->Δlvl = 0;
		}else if(c->lvl & 1<<7){
			c->lvl = 0;
			c->Δlvl = 0;
		}
		out(0x40 + c->o1+3, c->lvl);
	}
}

void
ev(void)
{
	uchar v;
	Sect *s;
	Channel *c;

	for(;;){
		v = get8();
		c = ch + (v & 0xf);
		switch(v >> 4 & 0xf){
		case 0:
			setc(c, 0);
			break;
		case 1:
			c->fmΔA = 1;
			c->fmA = 0;
			/* wet floor */
		case 2:
			setc(c, 1);
			break;
		case 3:
			v = get8();
			c->Δlvl = 0;
			c->lvl = v;
			out(0x40 + c->o1+3, v);
			break;
		case 4:
			out(0x40 + c->o1, get8());
			break;
		case 5:
			c->Δf = (int)(char)get8();
			break;
		case 6:
			v = get8();
			c->fmmax = v >> 4;
			c->fmfact = v & 0xf;
			break;
		case 7:
			setinst(c);
			break;
		case 8:
			switch(v & 0xf){
			case 1:
				if(secp == sect + nelem(sect)){
					fprint(2, "sect overflow off=%#zux\n", bufp-buf);
					break;
				}
				secp->n = get8();
				v = get8();
				secp->p = buf + (get8() << 8 | v);
				secp->ret = bufp;
				if(secp->p >= bufe)
					sysfatal("sect pos past eob off=%#zux", bufp-buf);
				bufp = secp++->p;
				break;
			case 2:
				Δtc = get8();
				return;
			case 3:
				v = get8();
				if(v >= nelem(instp))
					sysfatal("inst overflow off=%#zux\n", bufp-buf);
				instp[v] = bufp;
				bufp += 11;
				break;
			case 5:
				setΔlvl(1);
				break;
			case 6:
				setΔlvl(-1);
				break;
			}
			break;
		case 14:
			loop = bufp;
			break;
		case 15:
			if(secp == sect){
				bufp = loop;
				if(!doloop && bufp == buf){
					barf();
					exits(nil);
				}
				break;
			}
			s = secp - 1;
			if(--s->n == 0){
				bufp = s->ret;
				secp--;
				break;
			}
			bufp = s->p;
			break;
		}
	}
}

void
usage(void)
{
	fprint(2, "usage: %s [-l] [mfile]\n", argv0);
	exits("usage");
}

void
main(int argc, char **argv)
{
	int n;

	ARGBEGIN{
	case 'l': doloop++; break;
	default: usage();
	}ARGEND
	while(n = read(0, bufe, buf + sizeof(buf) - bufe), n > 0){
		if(bufe >= buf + sizeof buf)
			sysfatal("file too large");
		bufe += n;
	}
	if(n < 0)
		sysfatal("read: %r");
	out(0x01, 1<<5);
	for(;;){
		if(--Δtc <= 0)
			ev();
		up();
		barf();
	}
}
