/*
 * Editor
 */
#include <stdio.h>
#include <signal.h>
#include <setjmp.h>

/* make BLKSIZE and LBSIZE 512 for smaller machines */
#define	ESIZE	256
#define	NBRA	5

#define	CBRA	1
#define	CCHR	2
#define	CDOT	4
#define	CCL	6
#define	NCCL	8
#define	CDOL	10
#define	CEOF	11
#define	CKET	12
#define	CBACK	14
#define	CCIRC	15

#define	STAR	01

char	Q[]	= "";

int	peekc;
char	expbuf[ESIZE+4];
char	*braslist[NBRA];
char	*braelist[NBRA];
int	nbra;
char	*loc1;
char	*loc2;


void error(char *s);
int advance(char *lp, char *ep);
int backref(int i, char *lp);
int cclass(char *set, int c, int af);
void compile(char* wordSearch);
int execute(char* line);


jmp_buf	savej;

typedef void	(*SIG_TYP)(int);
SIG_TYP	oldhup;
SIG_TYP	oldquit;
/* these two are not in ansi, but we need them */
#define	SIGHUP	1	/* hangup */
#define	SIGQUIT	3	/* quit (ASCII FS) */

int main(int argc, char *argv[])
{	
	compile(argv[1]);
	FILE* toRead = fopen(argv[2], "r");
	int counter = 0;
    char c;
    char line[256];
	memset(line, 0, sizeof(line));
    	while((c = fgetc(toRead)) != EOF){
        	line[counter++] = c;
        		if(c == '\n'){
            		if(execute(line))
						printf("%s", line);
					memset(line, 0, sizeof(line));
					counter = 0;
        		}
   			}
	fclose(toRead);
	return 0;
}


void compile(char *wordSearch) {
	int c;
	char *ep, *lastep;
	char bracket[NBRA], *bracketp;
	int cclcnt;

	ep = expbuf;
	bracketp = bracket;
	if ((c = (*wordSearch)) == '\n') {
		peekc = c;
		c = '\0';
	}
	if (c == '\0') {
		return;
	}
	nbra = 0;
	if (*wordSearch =='^') {
		++wordSearch;
		*ep++ = CCIRC;
	}
	peekc = *wordSearch;
	lastep = 0;
	--wordSearch;
	for (;;) {
		if (ep >= &expbuf[ESIZE])
			++wordSearch;
		if (*wordSearch == '\n') {
			peekc = *wordSearch;
			*wordSearch = '\0';
		}
		if (*wordSearch=='\0') {
			*ep++ = CEOF;
			return;
		}
		if (*wordSearch !='*')
			lastep = ep;
		switch (c) {

		case '\\':
			if (*++wordSearch =='(') {
				*bracketp++ = nbra;
				*ep++ = CBRA;
				*ep++ = nbra++;
				continue;
			}
			if (*wordSearch == ')') {
				*ep++ = CKET;
				*ep++ = *--bracketp;
				continue;
			}
			if (*wordSearch >='1' && c<'1'+NBRA) {
				*ep++ = CBACK;
				*ep++ = *wordSearch -'1';
				continue;
			}
			*ep++ = CCHR;
			*ep++ = *wordSearch;
			continue;

		case '.':
			*ep++ = CDOT;
			continue;

		case '*':
			if (lastep==0 || *lastep==CBRA || *lastep==CKET)
				goto defchar;
			*lastep |= STAR;
			continue;

		case '$':
			if ((peekc= (*++wordSearch)) != '\0' && peekc!='\n')
				goto defchar;
			*ep++ = CDOL;
			continue;

		case '[':
			*ep++ = CCL;
			*ep++ = 0;
			cclcnt = 1;
			if ((c=(*++wordSearch)) == '^') {
				c = (*wordSearch++);
				ep[-2] = NCCL;
			}
			do {
				if (*wordSearch=='-' && ep[-1]!=0) {
					if (*wordSearch++ ==']') {
						*ep++ = '-';
						cclcnt++;
						break;
					}
					while (ep[-1]<*wordSearch) {
						*ep = ep[-1]+1;
						ep++;
						cclcnt++;
					}
				}
				*ep++ = *wordSearch;
				cclcnt++;
			} while (*++wordSearch != ']');
			lastep[1] = cclcnt;
			continue;

		defchar:
		default:
			*ep++ = CCHR;
			*ep++ = c;
		}
	}
}

int execute(char *line) {
	char *p1, *p2;
	int c;

	for (c=0; c<NBRA; c++) {
		braslist[c] = 0;
		braelist[c] = 0;
	}
	p2 = expbuf;
    p1 = line;
	if (*p2==CCIRC) {
		loc1 = p1;
		return(advance(p1, p2+1));
	}
	/* fast check for first character */
	if (*p2==CCHR) {
		c = p2[1];
		do {
			if (*p1!=c)
				continue;
			if (advance(p1, p2)) {
				loc1 = p1;
				return(1);
			}
		} while (*p1++);
		return(0);
	}
	/* regular algorithm */
	do {
		if (advance(p1, p2)) {
			loc1 = p1;
			return(1);
		}
	} while (*p1++);
	return(0);
}

int advance(char *lp, char *ep) {
	char *curlp;
	int i;

	for (;;) switch (*ep++) {

	case CCHR:
		if (*ep++ == *lp++)
			continue;
		return(0);

	case CDOT:
		if (*lp++)
			continue;
		return(0);

	case CDOL:
		if (*lp==0)
			continue;
		return(0);

	case CEOF:
		loc2 = lp;
		return(1);

	case CCL:
		if (cclass(ep, *lp++, 1)) {
			ep += *ep;
			continue;
		}
		return(0);

	case NCCL:
		if (cclass(ep, *lp++, 0)) {
			ep += *ep;
			continue;
		}
		return(0);

	case CBRA:
		braslist[*ep++] = lp;
		continue;

	case CKET:
		braelist[*ep++] = lp;
		continue;

	case CBACK:
		if (backref(i, lp)) {
			lp += braelist[i] - braslist[i];
			continue;
		}
		return(0);

	case CBACK|STAR:
		curlp = lp;
		while (backref(i, lp))
			lp += braelist[i] - braslist[i];
		while (lp >= curlp) {
			if (advance(lp, ep))
				return(1);
			lp -= braelist[i] - braslist[i];
		}
		continue;

	case CDOT|STAR:
		curlp = lp;
		while (*lp++ == *ep)
			;
		goto star;

	case CCHR|STAR:
		curlp = lp;
		while (*lp++ == *ep)
			;
		ep++;
		goto star;

	case CCL|STAR:
	case NCCL|STAR:
		curlp = lp;
		while (cclass(ep, *lp++, ep[-1]==(CCL|STAR)))
			;
		ep += *ep;
		goto star;

	star:
		do {
			lp--;
			if (advance(lp, ep))
				return(1);
		} while (lp > curlp);
		return(0);

	default:
		return 0;
	}
}

int backref(int i, char *lp) {
	char *bp;

	bp = braslist[i];
	while (*bp++ == *lp++)
		if (bp >= braelist[i])
			return(1);
	return(0);
}

int cclass(char *set, int c, int af) {
	int n;

	if (c==0)
		return(0);
	n = *set++;
	while (--n)
		if (*set++ == c)
			return(af);
	return(!af);
}
