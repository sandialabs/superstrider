/* Superstrider Simulator v. 1.0 Beta:

Copyright 2017 Sandia Corporation. Under the terms of Contract
DE-AC04-94AL85000 with Sandia Corporation, the U.S. Government
retains certain rights in this software.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * The names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL COPYRIGHT HOLDERS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
/* This software has been assigned SCR# 2200.0. This number is a Sandia
internal tracking number and is a useful reference when contacting the
Legal Technology Transfer Center or Licensing and IP Management.

This software was originally assigned Export Control Classification Number
(ECCN) EAR 99. However, since this software is to be released as OSS, the
software is deemed to be Publicly Available.
*/
#define WIN32_LEAN_AND_MEAN		// Exclude rarely-used stuff from Windows headers
#include <stdio.h>
#include <time.h>
#include <float.h>
#include <math.h>

#include <vector>
#include <algorithm>
#include <map>
#include <string>
using namespace std;

#if __GNUC__ == 0
#pragma warning(disable : 4996)
#include <process.h>
#include <direct.h>
#include <winsock.h>
#else
// compile with g++ SuperStrider.cpp -lpthread
// on Windows 10 do sudo bash first, so you can access the internet 
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>
#include <string.h>
#define SOCKET int
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1

#define stricmp(a, b) strcasecmp(a, b)
#define min(a, b) ((a) < (b) ? (a) : (b))
#define max(a, b) ((a) > (b) ? (a) : (b))

#define BYTE unsigned char
#define DWORD unsigned long
#define WORD unsigned short
#define __stdcall

typedef struct RGBQUAD {
BYTE rgbBlue;
BYTE rgbGreen;
BYTE rgbRed;
BYTE rgbReserved;
} RGBQUAD;

#define isdigit(c) ((c) >= '0' && (c) <= '9')
#define isalpha(c) ((c) >= 'a' && (c) <= 'z' || (c) >= 'A' && (c) <= 'Z')
#define iscsym(c) (isdigit(c) || isalpha(c) || (c) == '_')

#define _finite(f) isfinite(f)    // Jeanine changed to isfinite().  Apparently finite() has been deprecated since osx 10.9

// see https://gcc.gnu.org/onlinedocs/gcc/_005f_005fatomic-Builtins.html
long InterlockedExchange(long *ptr, long val) { __atomic_exchange_n(ptr, val, __ATOMIC_RELAXED); }
long InterlockedExchangeAdd(long *ptr, long val) { __atomic_fetch_add(ptr, val, __ATOMIC_RELAXED); }
long InterlockedIncrement(long *ptr) { return __atomic_fetch_add(ptr, 1L, __ATOMIC_RELAXED)+1; }

#define _mkdir(s) mkdir(s, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH )
#define closesocket close
#define strnicmp strncasecmp 
#define Sleep(s) usleep(s*1000)
#define RGB(r,g,b)          ((unsigned long)(((unsigned char)(r)|((unsigned short)((unsigned char)(g))<<8))|(((unsigned long)(unsigned char)(b))<<16)))

int CloseHandle (pthread_t) { } // fixme

typedef struct _FILETIME {
    unsigned long dwLowDateTime;
    unsigned long dwHighDateTime;
} FILETIME, *PFILETIME, *LPFILETIME;
#define FileTimeToSystemTime(a, b)

#define HANDLE int

typedef union _LARGE_INTEGER {
    struct {
        unsigned long LowPart;
        long HighPart;
    };
    struct {
        unsigned long LowPart;
        long HighPart;
    } u;
} LARGE_INTEGER;

unsigned long WaitForSingleObject(pthread_t hHandle, int dwMilliseconds) { }
#define STILL_ACTIVE 1
#define SYSTEMTIME int
#define SOCKADDR_IN sockaddr_in
#define _CrtCheckMemory()
#define WSACleanup()
#define _CrtDumpMemoryLeaks()
#endif									// }

// modes area
#define STATS 0				// 1 to enable statistics
#define BITSMODE 0			// 0->int64, 1->long, 2->2 unsigned longs (mode 1 won't work)
#define INLINECOMPLEX 0			// 1 for inline implementation of complex numbers; 0 for debugging
#define USEGIF 1			// define either of these to be nonzero to generate output in the appropriate graphics format
#define USEBMP 0

#define COMMANDS 5
#define MACHINES 5
#define PERFECTION 2
#define NUMPLOTS (COMMANDS+4+7)
#define APPPLOT COMMANDS
#define MFPLOT1 (COMMANDS+1)
#define MFPLOT2 (COMMANDS+2)
#define MFPLOT3 (COMMANDS+3)

#define PLOTFACTOR 10

#define FANCYPAGE 1			// set to 1 to include fancypage formatting
#define SAMPLECOMMANDS 0		// set to 1 to include sample commands

enum eDataValidity {			// system can accommodate real and extrapolated points
	Ephemeral=0,
	Real=1,
};

const char *Out1 = "<font face=arial size=3>";
const char *Out2 = "</font>";
const char *NOut1 = "<font face=arial size=0 color=white>";
const char *NOut2 = "</font>";

class DataPoint {
public:
	double X, Y;
	char *Text;
};

class HTMLGraph {
	int NumPoints, MaxPoints;
	DataPoint *Data;
	int color;			// color of the "real" lines in a graph
	int bgcolor;			// color of "unreal" lines, or projected values. If -1 then no line drawn

	void *payload;
	double minx, maxx, miny, maxy;
	int width, height;
public:
	enum xxeDataValidity {		// system can accommodate real and extrapolated points
		Ephemeral=0,
		Real=1,
	};

	enum eDataMark {		// should data points be marked with a cross or box?
		NoMark=0,
		Square=1,
		NoLineNoMark=2,
		NoLineSquare=3,
	};

	enum eSpecMinX {		// minimum X value in chart
		CalcMinX=0,
		SpecMinX=1,
	};
		
	enum eSpecMaxX {		// maxiumum X value in chart
		CalcMaxX=0,
		SpecMaxX=1,
	};

	enum eSpecMinY {		// minimum Y value in chart
		CalcMinY=0,
		SpecMinY=1,
	};
		
	enum eSpecMaxY {		// maximum Y value in chart
		CalcMaxY=0,
		SpecMaxY=1,
	};
		
	HTMLGraph();

	void SetName(int c, int bgc);

	void Clear();

	~HTMLGraph();

	void AddPoint(double d, double t, char *tx, void *p, eDataValidity real);

	void WriteToBitmap(int **image, int logx, int logy, double ix, double ax, double iy, double ay, int ii, int jj, HTMLGraph::eDataMark cross);

	void Range(double &minx, double &maxx, double &miny, double &maxy) ;

	int backtranslate(FILE *output, int x, int y, int dist);

	friend void HTMLGraph_Print(FILE *output, const char *title, int n, int logx, int fmtx, int logy, int fmty_unused, HTMLGraph *dope, int *Color, int bgcolor, HTMLGraph::eDataMark cross,
			HTMLGraph::eSpecMinX fixminx, double aminx,
			HTMLGraph::eSpecMaxX fixmaxx, double amaxx,
			HTMLGraph::eSpecMinY fixminy, double aminy,
			HTMLGraph::eSpecMaxY fixmaxy, double amaxy);
};

// print a data set as an HTML included graphic
// can optionally print multiple traphs for comparison
// scaling can be specified for { min, max } { x, y } where each combination can be fixed or an extremum5 value can be specified
void HTMLGraph_Print(FILE *output, const char *title, int n, int logx, int fmtx, int logy, int fmty_unused, HTMLGraph *dope, int *Color, int bgcolor, HTMLGraph::eDataMark cross,
			HTMLGraph::eSpecMinX fixminx = HTMLGraph::CalcMinX, double aminx = 1e100,
			HTMLGraph::eSpecMaxX fixmaxx = HTMLGraph::CalcMaxX, double amaxx = -1e100,
			HTMLGraph::eSpecMinY fixminy = HTMLGraph::CalcMinY, double aminy = 1e100,
			HTMLGraph::eSpecMaxY fixmaxy = HTMLGraph::CalcMaxY, double amaxy = -1e100) ;

// This is a buffer for bitmap images to be rendered semi-independently of pages
// It has some accommodation to multi-threading
// The MIME pointer is atomically set to non-NULL when the file has been fully set up for serving
// Variable BuffersTop is atomically set to the number of cells to check, except it should be clipped to IMAGESTORELIMIT-1
// I think there could be a timing bug when the image store is cleared, but this is not done except at program exit
#define IMAGESTORELIMIT 1000

extern struct ImageStore {
	char Name[50];			// this will be the pathname for access, as in http://127.0.0.1/bm0.bmp
	int Len;			// length of the image file (below)
	const char *MIME;		// MIME type of data (NOT allocated from storage pool)
	char *Dope;
} Buffer[IMAGESTORELIMIT];
extern long BuffersTop;			// atomic increment

extern int ImageStoreSearch(char *n);	// find a file
extern void ImageStoreClear();		// reset (may not be timing stable)

extern FILE *ArchiveFile(const char *notation, const char *extension, int link, char *stash);

extern HTMLGraph PlotData[NUMPLOTS];
extern void Plotfcn(void *base, double amp, double horiz, eDataValidity real);

// image buffer capability
#define IMAGESTORELIMIT 1000
extern class IStore {
public:
	struct {
		char Name[50];		// this will be the pathname for access, as in http://127.0.0.1/bm0.bmp
		int Len;		// length of the image file (below)
		const char *MIME;	// MIME type of data (NOT allocated from storage pool)
		char *Dope;
	} Buffer[IMAGESTORELIMIT];
	long BuffersTop;		// atomic increment

	int ImageStoreSearch(char *n);
	void ImageStoreClear();
	FILE *ArchiveFile(const char *notation, const char *extension, int link, char *stash);
} ImageStore;

// this is a class with a web browser query and a socket with which to respond to it
#define SENDBUFSZ 512
class ServeData {
	friend int main(int argc, char* argv[]);

	char *URI;			// file name relative to this server
	FILE *index;			// place to send data
	char *p;			// input buffer
	int lastwrite;			// next position to write (for incremental output)
	unsigned int theClient;		// socket for output
public:
	int argc;			// number of arguments
	char **name;			// pointer to array of asciz names created by malloc/malloc
	char **value;			// pointer to array of asciz values created by malloc/malloc

	ServeData() { }
	ServeData(ServeData *s) { *this = *s; }
	void Set(unsigned int s);
	void FreeUp();
	void CloseAndFreeUp();
	void Serve404();
	void Serve200();
	void TitleTextNoDiskBackup(char *title);
	FILE *TitleTextOpenBufferFile(const char *title, const char *notation, const char *extension, int link, char *stash, int reload = -1);
	void IncrementalWrite();
	void CloseBufferFile();
	void BasicServe(const char *title, const char *msg);

	// unused virtuals an anticipation of the need to parse the browser-supplied input
	char *URLDecode(char *x);
	void SD(double &d, char *v);
	void SI(int &n, char *v);
	void SS(char *&p, char *v);
	void SB(int &b, char *v) ;
};

// local thread manager
// NOTE: Does not include the main thread
#define MAXTHREADS 100
class cThreads {
public:
	// initialize data structure -- like a creator function
	virtual void Initialize() { };

	// launch a thread, putting a reference to it in local storage
	virtual void Add(void (__stdcall *f) (ServeData *), ServeData *p) { };

	// print system status
	virtual void Print(FILE *fd, int html) { };

	// close everything, waiting until active threads terminate
	virtual void AllDone() { };

};

extern cThreads *Threads;

// Assertion support
#define ASSERTALWAYS(c) if (!(c)) { fprintf(stderr, "assert failed"); fflush(stderr); botchassert(); }
#undef ASSERT
#define FPASSERT(a, b) if ((a)/(b) < .9999 || (a)/(b) > 1.0001) { fprintf(stderr, "FP assert failed"); fflush(stderr); botchassert(); }
#define ASSERT(x) if (!(x)) asserterror(#x);

void botchassert() {
	double pi=3.14;
}

void asserterror(const char *msg) {
	printf("assert error %s\n", msg);
	getchar();
}

#if __GNUC__ == 0
// see https://en.wikipedia.org/wiki/Linear_congruential_generator for constants
// The linear congruential generator is mod 2^64, but only 31 bits are used
// the constants are used in "Newlib, Musl" -- whatever they are
ULARGE_INTEGER myrandstate = { 123, };
#define MYRANDMAX 0x7fffffff

static int myrand() {
    myrandstate.QuadPart = 6364136223846793005ULL*myrandstate.QuadPart + 1ULL;
    int rval = myrandstate.HighPart & MYRANDMAX;
    return rval;
}

static void mysrand(int s) {
    myrandstate.QuadPart = s;
}
#else
static unsigned myrandstate = 123;
#define MYRANDMAX 0x7fff

static int myrand() {
    int rval = (myrandstate = 214013L*myrandstate + 2531011L);
    return (rval >> 16)&MYRANDMAX;
}

static void mysrand(int s) {
    srand(s);
    myrandstate = s;
}
#endif

// Print a floating number with the SI suffix, as in 10.4ns
// Returns a ASCIZ string from a rotating static buffer (generally sufficient for printf)
// Second argument is the number of significant digits, defaulting to three
// Third argument controls weather the micro suffix will be a Roman "u" or an
//   HTML [ISO 8859-1 (Latin-1) Entities] micro sign &#181;
// Fourth argument controlls the power that will be applied to the SI prefix;
//   Pwr=2 causes m to mean 10**-6 instead of 10**-3, as in
//   a square inch is .000645m^2 or 645mm^2.
char *SISuffix(double x, int digits = 3, int html = 1, int pwr = 1) {
	static char dope[20][50];
	static int next = 0;
	static struct g {
		double limit;
		int basedigits;
		const char *suffix;
		double divide;
	} table[] = {
		{ 1e-23, 2, "y", 1e-24, },
		{ 1e-22, 1, "y", 1e-24, },
		{ 1e-21, 0, "y", 1e-24, },
		{ 1e-20, 2, "z", 1e-21, },
		{ 1e-19, 1, "z", 1e-21, },
		{ 1e-18, 0, "z", 1e-21, },
		{ 1e-17, 2, "a", 1e-18, },
		{ 1e-16, 1, "a", 1e-18, },
		{ 1e-15, 0, "a", 1e-18, },
		{ 1e-14, 2, "f", 1e-15, },
		{ 1e-13, 1, "f", 1e-15, },
		{ 1e-12, 0, "f", 1e-15, },
		{ 1e-11, 2, "p", 1e-12, },
		{ 1e-10, 1, "p", 1e-12, },
		{ 1e-9, 0, "p", 1e-12, },
		{ 1e-8, 2, "n", 1e-9, },
		{ 1e-7, 1, "n", 1e-9, },
		{ 1e-6, 0, "n", 1e-9, },
		{ 1e-5, 2, "u", 1e-6, },
		{ 1e-4, 1, "u", 1e-6, },
		{ 1e-3, 0, "u", 1e-6, },
		{ 1e-2, 2, "m", 1e-3, },
		{ 1e-1, 1, "m", 1e-3, },
		{ 1e0, 0, "m", 1e-3, },
		{ 1e1, 2, "", 1, },
		{ 1e2, 1, "", 1, },
		{ 1e3, 0, "", 1, },
		{ 1e4, 2, "K", 1e3, },
		{ 1e5, 1, "K", 1e3, },
		{ 1e6, 0, "K", 1e3, },
		{ 1e7, 2, "M", 1e6, },
		{ 1e8, 1, "M", 1e6, },
		{ 1e9, 0, "M", 1e6, },
		{ 1e10, 2, "G", 1e9, },
		{ 1e11, 1, "G", 1e9, },
		{ 1e12, 0, "G", 1e9, },
		{ 1e13, 2, "T", 1e12, },
		{ 1e14, 1, "T", 1e12, },
		{ 1e15, 0, "T", 1e12, },
		{ 1e16, 2, "P", 1e15, },
		{ 1e17, 1, "P", 1e15, },
		{ 1e18, 0, "P", 1e15, },
		{ 1e19, 2, "E", 1e18, },
		{ 1e20, 1, "E", 1e18, },
		{ 1e21, 0, "E", 1e18, },
		{ 1e22, 2, "Z", 1e21, },
		{ 1e23, 1, "Z", 1e21, },
		{ 1e24, 0, "Z", 1e21, },
		{ 1e25, 2, "Y", 1e24, },
		{ 1e26, 1, "Y", 1e24, },
		{ 1e27, 0, "Y", 1e24, },
	};
	char format[50];

	char *p = dope[next%20];

	// handle negatives
	if (x < 0) {
		x = -x;
		*p++ = '-';
	}

	// zero has only one representation -- "0"
	if (x == 0) {
		sprintf(dope[next%20], "0");
		return dope[next++%20];
	}

	// numbers between .1 and 1 can be represented with a leading decimal, as in .1, .2, .2004
	else if (x >= .1 && x < 1) {
//		sprintf(p, ".%.0f", x*pow(10, digits));
		sprintf(p, "%.*f", digits, x);
		for (int i = strlen(p); --i > 1 && p[i] == '0'; )
			p[i] = 0;
		return dope[next++%20];
	}

	// other numbers to be represented by a number n, 1 <= n <= 999.999
	if (x >= 1e-26) for (unsigned int i = 0; i < sizeof(table)/sizeof(g); i++) {
		double limit = table[i].limit, divide = table[i].divide;
		if (1) for (int j = 1; j < pwr; j++) {
			limit *= table[i].limit;
			divide *= table[i].divide;
		}

		if (x < limit) {
			const char *s = table[i].suffix;
			if (s[0] == 'u' && html) s = "&#181;";
			int d = table[i].basedigits + digits - 3;
			if (d < 0) d = 0;
			sprintf(format, "%%.%dlf", d);
			sprintf(p, format, x/divide);

			// now suppress trailing zeros
			if (d > 0) {
				int j;
				for (j = strlen(p); --j >= 1 && p[j] == '0'; )
					p[j] = 0;
				if (p[j = strlen(p)-1] == '.')
					p[j] = 0;
			}

			strcat(p, s);				// add suffix
			return dope[next++%20];
		}
	}
	sprintf(format, "%%.%dle", digits-1);
	sprintf(p, format, x);
	return dope[next++%20];
}

char *SISuffix2(double x, int digits = 3, int html = 1) {
	return SISuffix(x, digits, html, 2);
}

char *SISuffixC(double re, double im, int digits = 3, int html = 1, int pwr = 1) {
	static char dope[20][103];
	static int next = 0;
	char *p = dope[next%20];
	*p = 0;

//sprintf(p, "%.4f+%.4fi", re, im);
//return p;

	const char *rt = SISuffix(re, digits, html, pwr);
	const char *it = SISuffix(im, digits, html, pwr);

//	sprintf(p, "(%.4f, %.4f, %s, %s)", re, im, rt, it);
//	return p;

	if (im == 0)
		sprintf(p, "(%s)", rt);
	else if (re == 0)
		sprintf(p, "(%si)", it);
	else if (im > 0)
		sprintf(p, "(%s+%si)", rt, it);
	else
		sprintf(p, "(%s%si)", rt, it);
	return p;
}

double SIRound(double v) {
	double t;
	static double table[] = { 1, 1.5, 2, 3, 5, 7, 9, };
	for (int e = 100; --e >= -100;)
		for (int f = 7; --f >= 0;)
			if ((t = table[f] * pow(10., e)) < v)
				return t;
	return 1.0;
}

#define MinPlot 1e-3
#define MaxPlot 1e22

#define SCALE 1				// 1 for small graphics; proportionately larger
#define SUBDIVIDE 4			// oversample each pixel this many times in x and y

#define GRAPHPOINTS ((int)(600*SCALE))
#define GRAPHHEIGHT ((int)(300*SCALE))
#define GRAPHWIDTH ((int)(2*SCALE))

#define PICTUREPOINTS ((int)(350*SCALE))
#define PICTUREHEIGHT ((int)(250*SCALE))
#define PICTUREWIDTH ((int)(2*SCALE))

#define SQUARESIZE ((int)(2*SCALE))
#define SEGHT ((int)(4*SCALE))		// "7 segment" line height (1/2 character height)
#define SEGWD ((int)(3*SCALE))		// "7 segment" line width
#define SEGSP ((int)(5*SCALE))		// character spacing
#define SEGLW ((int)(1+SCALE/3))	// width of drawing line
#define VMAR ((int)(10*SCALE))		// graph vertical margin (at bottom) for axis
#define HMAR ((int)(27*SCALE))		// graph horizontal margin (at left) for axis

// Allocate an archival disk file, returning an open FILE * for writing its contents
// If link == 1, create a hyperlink to this file from archive.htm
// If stash != NULL, store the (relative) file name
FILE *ArchiveFile(const char *notation, const char *extension, int link, char *stash) {

	time_t long_time;
	time(&long_time);

	// additional segment
	char fraction[10];
	fraction[0] = 0;

	for (int i = 0; i < 50; i++) {

		struct tm *newtime = localtime(&long_time);
		const char *mon[12] = { "jan", "feb", "mar", "apr", "may", "jun", "jul", "aug", "sep", "oct", "nov", "dec", };
		char fn[200], fn2[200];
		sprintf(fn, "%s-%02d%s%02d-%02dh%02dm%02ds%s.%s", notation,
			newtime->tm_mday, mon[newtime->tm_mon], newtime->tm_year-100, newtime->tm_hour, newtime->tm_min, newtime->tm_sec, fraction, extension);

		sprintf(fn2, "archive/%s", fn);

		if (stash != NULL) sprintf(stash, "%s", fn);

		// create the directory
		_mkdir("archive");

		// see if the file exists
		FILE *rval = fopen(fn2, "r");

		// doesn't exist, see if we can make it
		if (rval == NULL) {
			rval = fopen(fn2, "wb+");
			//fclose(rval);
			//rval = fopen(fn2, "wb+");
		}

		// oops, exists, close it and try another
		else {
			fclose(rval);
			rval = NULL;
		}

		// got one
		if (rval != NULL) {

			if (link != 0) {
				FILE *ff = fopen("archive.htm", "a");
				fprintf(ff, "<a href=\"archive/%s\">%s</a><br>\n", fn, fn);
				fclose(ff);
			}

			return rval;
		}

		sprintf(fraction, ".%02x", unsigned(myrand()&0xff));
	}

	ASSERTALWAYS(0);

	return NULL;
}

// clear buffers
// atomically destroy the MIME type pointer to indicate unavailable
// then atomically set the BuffersTop pointer to permit reuse
void ImageStoreClear() {
	for (int i = 0; i < IMAGESTORELIMIT; i++) {
		InterlockedExchange((long *)&ImageStore.Buffer[i].MIME, 0);
		ImageStore.Buffer[i].Name[0] = 0;
		ImageStore.Buffer[i].Len = 0;
#define IMAGESTOREPOINTER 1
#if IMAGESTOREPOINTER != 0
		delete ImageStore.Buffer[i].Dope;
#endif
	}
	InterlockedExchange(&ImageStore.BuffersTop, 0);
}

// search image store for an image matching the url
int ImageStoreSearch(char *n) {
	for (int i = 0, e = InterlockedExchangeAdd(&ImageStore.BuffersTop, 0); i < e; i++) {
		if (i >= IMAGESTORELIMIT) break;
		if (InterlockedExchangeAdd((long *)&ImageStore.Buffer[i].MIME, 0) != 0)
			if (strcmp(ImageStore.Buffer[i].Name, n) == 0)
				return i;
	}
	return -1;
}

void Dither(int x, int y, int **image, int lvr, int lvg, int lvb) {

	unsigned char maptabr[256], maptabg[256], maptabb[256];
	if (1) for (int i = 0; i < 256; i++) {
		maptabr[i] = (i+255/2/lvr) * lvr / 255 * 255 / lvr;
		maptabg[i] = (i+255/2/lvg) * lvg / 255 * 255 / lvg;
		maptabb[i] = (i+255/2/lvb) * lvb / 255 * 255 / lvb;
	}

	if (1) for (int i = 0; i < x; i++) {
		for (int j = 0; j < y; j++) {
			int c = image[i][j];		// get the color
			unsigned char cr = c>>16;
			unsigned char cg = c>>8;
			unsigned char cb = c;

			unsigned char nr = maptabr[cr];	// cut the range
			unsigned char ng = maptabg[cg];
			unsigned char nb = maptabb[cb];

			image[i][j] = nr<<16 | ng<<8 | nb;

			char er = cr - nr;			// the error

			char eg = cg - ng;
			char eb = cb - nb;

			unsigned long rn = myrand();

			for (int n = 0; n < 15; n++) {
				int ii = i, jj = j;
				switch (n&3) {
					case 0:
						jj++;
						break;

					case 1:
						ii++;
						break;

					case 2:
						ii++;
						jj--;
						break;

					case 3:
						ii++;
						jj++;
						break;
				}

				if (ii < 0 || ii >= x || jj < 0 || jj >= y)
					continue;

				if ((rn & 1<<n) == 0)
					continue;

				c = image[ii][jj];
				cr = c>>16;
				cg = c>>8;
				cb = c;
				if (er != 0) nr = min(255, max(0, cr + (er>>2|1))), er -= nr - cr; else nr = cr;
				if (eg != 0) ng = min(255, max(0, cg + (eg>>2|1))), eg -= ng - cg; else ng = cg;
				if (eb != 0) nb = min(255, max(0, cb + (eb>>2|1))), eb -= nb - cb; else nb = cb;
				image[ii][jj] = nr<<16 | ng<<8 | nb;

				if (er == 0 && eg == 0 && eb == 0)
					break;
			}
		}
	}
}

// add an image to the store
char *ImageStoreAdd(int x, int y, int **image) {
	int i = InterlockedIncrement(&ImageStore.BuffersTop);
	if (--i >= IMAGESTORELIMIT)  return NULL;

	FILE *f = ArchiveFile("image", "bmp", 0, ImageStore.Buffer[i].Name);

	int bytes_per_line=(x*24+31)/32*4;
	ImageStore.Buffer[i].Len = 54 + bytes_per_line * y;
	char *p = (char *)malloc(ImageStore.Buffer[i].Len);
	ImageStore.Buffer[i].Dope = p;

	*p++ = 'B';
	*p++ = 'M';
	*(*(long **)&p)++ = 14+40;	// file size
	*(*(short **)&p)++ = 0;		// reserved
	*(*(short **)&p)++ = 0;		// reserved
	*(*(long **)&p)++ = 14+40;	// offset bits
	*(*(long **)&p)++ = 40;		// size
	*(*(long **)&p)++ = x;		// width
	*(*(long **)&p)++ = y;		// height
	*(*(short **)&p)++ = 1;		// planes
	*(*(short **)&p)++ = 24;	// bit count
	*(*(long **)&p)++ = 0;		// compression
	*(*(long **)&p)++ = bytes_per_line*y;// image size
	*(*(long **)&p)++ = 75*39;	// x_pixels
	*(*(long **)&p)++ = 75*39;	// y_pixels
	*(*(long **)&p)++ = 0;		// number colors
	*(*(long **)&p)++ = 0;		// colors important

	if (1)
		for (int j = 0; j < y; j++) {
			if (1) for (int i = 0; i < x; i++) {
				*p++ = image[i][j];
				*p++ = image[i][j]>>8;
				*p++ = image[i][j]>>16;
			}
			if (1) for (int i = x*3; i < bytes_per_line; i++)
				*p++ = 0;
		}

	fwrite(ImageStore.Buffer[i].Dope, 1, ImageStore.Buffer[i].Len, f);
	fclose(f);

	const char *mime = "image/bmp";
	InterlockedExchange((long *)&ImageStore.Buffer[i].MIME, (long)mime);

	return ImageStore.Buffer[i].Name;
}

// begin GIF Code
#define LEVR 6				// number of levels of red in 256 color palette
#define LEVG 6				// number of levels of green in 256 color palette
#define LEVB 6				// number of levels of blue in 256 color palette

void Putword(int w, FILE *f) {
	fputc(w, f);
	fputc(w>>8, f);
}

void EncodeHeader(int x, int y, RGBQUAD *pPal, FILE *f) {
	int Colors = 256;		// number of colors
	fwrite("GIF89a", 1, 6, f);	// GIF Header
	Putword(x, f);			// logical screen width
	Putword(y, f);			// logical screen height

	unsigned char field;		// Packed fields
	if (Colors == 0)
		field=0x11;
	else {
		field = 0x80;		// global color table flag
		field |= 7 << 5;	// color resolution
		field |= 7;		// size of global color table
	}
	fputc(field, f);		// Packed fields
	fputc(0, f);			// Background color index
	fputc(0, f);			// Pixel aspect ratio
	if (Colors != 0)
		for (int i = 0; i < Colors; i++) {
			fputc(pPal[i].rgbRed, f);
			fputc(pPal[i].rgbGreen, f);
			fputc(pPal[i].rgbBlue, f);
		}
}

void EncodeExtension(int delay, FILE *f) {

	fputc('!', f);			// extension introducer
	fputc(0xF9, f);			// graphic control label

	struct anonymous_structure {	// 4 bytes
		unsigned char transpcolflag:1;
		unsigned char userinputflag:1;
		unsigned char dispmeth:3;
		unsigned char res:3;
		unsigned char delaylow;
		unsigned char delayhigh;
		unsigned char transpcolindex;
	} gg;

	gg.transpcolflag = 0;
	gg.userinputflag = 0;
	gg.dispmeth = 0;
	gg.res = 0;
	gg.delaylow = (unsigned)delay;
	gg.delayhigh = (unsigned)(delay)>>8;
	gg.transpcolindex = -1;
	fputc(sizeof(gg), f);		// block size (4)
	fwrite(&gg, sizeof(gg), 1, f);	// packed fields
	fputc(0, f);			// block terminator
}

void EncodeLoopExtension(int n, FILE *f) {
	fputc('!', f);			// Extension introducer
	fputc(255, f);			// Extension label
	fputc(11, f);			// Block size
	fwrite("NETSCAPE2.0", 11, 1, f);
	fputc(3, f);			// Block size
	fputc(1, f);			// App-dependent code?
	Putword(min(65536, n), f);	// Loop length
	fputc(0, f);			// Block terminator 
}

void EncodeComment(const char *comment, FILE *f) {
	int len = strlen(comment);
	if (len > 255)
		len = 255;
	if (len) {
		fputc('!', f);		// Extension introducer
		fputc(254, f);		// Comment label
		fputc(len, f);		// Size of comment
		fwrite(comment, len, 1, f);	// Comment data
		fputc(0, f);		// Block terminator
	}
}

class GIFEncoderState {

#define BUFMAX 255			// can be as large as 255

	class BitOut {
		unsigned int Acc;	// bit accumulator for bit-oriented encoding format
		int Bits;		// number of bits valid in Acc
		int CodeSize;		// number of bits/code

		class BufGif {
			char Buffer[BUFMAX];	// output buffer
			int Count;	// number of pending characters in output buffer
			FILE *Out;	// write output here

		public:
			BufGif(FILE *f) {
				Count = 0;
				Out = f;				// save pointer to output file
			}

			// transmit contents of the output buffer
			void Flush() {
				if (Count > 0) {
					fputc(Count, Out);	// GIF format block size
					fwrite(Buffer, 1, Count, Out);	// block of size just specified
					Count = 0;
				}
			}

			void Output(int Chr) {
				Buffer[Count++] = Chr;	// output a character 
				if (Count >= BUFMAX)	// transmit buffer as a (cnt, buffer) block as needed
					Flush();
			}

			~BufGif() {
				Flush();
				fflush(Out);	// fflush for good measure
			}

		} GifBlk;

	public:
		// Set up the necessary values
		BitOut(FILE *f): GifBlk(f) {
			Acc = 0;
			Bits = 0;
			CodeSize = 9;
		}

		// Accept the supplied code for output in bit mode,
		// buffering up to 7 bits of an incomplete byte.
		// Initially output 9 bit bytes, but increase this
		// when the caller specifies that the max possible
		// code exceeds the largest representable code.
		// Request to output code 257 only occurs as the
		// last character of the transmission sequence and
		// can be used as an indication to flush pending bits.
		void Output(int Code, int FreeCode, int ClearFlag) {
			Acc |= Code << Bits;	// add to accumulator (high bits will be zero)
			Bits += CodeSize;

			while (Bits >= 8) { // transmit 8 bits at a time until less than 8 stored
				GifBlk.Output(Acc);
				Acc >>= 8;
				Bits -= 8;
			}

			if (ClearFlag)	// resetting block at 9 bit codes
				CodeSize = 9;

			else if (CodeSize < 12 && FreeCode >= 1<<CodeSize)	// bump codeword size so largest possible code will be representable
				CodeSize++;

			if (Code == 257) {  // only occurs at end
				while (Bits > 0) {  // transmit 8 bits at a time until all gone
					GifBlk.Output(Acc);
					Acc >>= 8;
					Bits -= 8;
				}

			}
		}
	};

	// Content addressible memory for (prefix, character) ordered pairs.
	// The entries in the CAM will be numbered 258..4095.
	// Function Lookup seeks to translate a (prefix, character) pair to an index.
	// If a lookup fails, it saves the arguments for a subsequent call to EnterLast.
	// EnterLast assigns the next code to the ordered pair, unless the CAM is
	// full. In this case, EnterLast returns 0.
	// FreeEntry is an official export: it is the number of the first unassigned code.
	// Uses an unbalanced tree, but scrambles order based on discussion in Knuth 6.4 p. 509
	class HashTable {
	    map<long, short> SymTab;
	    int KeySave;
	public:
	    int FreeEntry;
	    HashTable() { FreeEntry = 258; SymTab.clear(); }

	    int Lookup(int Prefix, int Ch) {
		KeySave = Prefix<<8|Ch;
		map<long, short>::iterator it = SymTab.find(KeySave);
		if (it != SymTab.end())
		    return it->second;
		return -1;
	    }

	    int EnterLast() {
		if (FreeEntry < 4096) {
		    SymTab[KeySave] = FreeEntry++;
		    return 1;
		}
		FreeEntry = 258;
		SymTab.clear();
		return 0;
	    }
	};

	long CountDown;			// raster scan of image

	// Get the next sequential pixel from the image
	// Does not dither, but reduces to a multi-level 256 color palete
	int GifNextPixel(int x, int y, int **image) {
		if (CountDown == 0) return EOF;
		CountDown--;

		int rgb = image[x - 1 - CountDown%x][CountDown/x];

		unsigned char r = rgb>>16;
		unsigned char g = rgb>>8;
		unsigned char b = rgb;
		return (r * (LEVR-1) / 255)*LEVG*LEVB + (g * (LEVG-1) / 255)*LEVB + (b * (LEVB-1) / 255);
	}

	// LZW conpression
	void compressLZW(int x, int y, int **image, FILE *f) {
	    HashTable Table;
	    BitOut B(f);
	    B.Output(256, Table.FreeEntry, 1);

	    int Prefix = GifNextPixel(x, y, image), Ch;
	    while ((Ch = GifNextPixel(x, y, image)) != EOF) {
		int Symbol = Table.Lookup(Prefix, Ch);
		if (Symbol >= 0)
		    Prefix = Symbol;
		else {
		    B.Output(Prefix, Table.FreeEntry, 0);
		    Prefix = Ch;
		    if (!Table.EnterLast())
			B.Output(256, Table.FreeEntry, 1);
		}
	    }
	    B.Output(Prefix, Table.FreeEntry, 0);
	    B.Output(257, Table.FreeEntry, 0);
	}

public:
	void EncodeBody(int x, int y, int **image, FILE *f) {
		CountDown = (long)x * (long)y;
		fputc(',', f);
		Putword(0, f);
		Putword(0, f);
		Putword(x, f);
		Putword(y, f);
		fputc(0, f);		// flags
		fputc(8, f);		// initial code size
		compressLZW(x, y, image, f);
		fputc(0, f);		// zero length packet signals end
	}

	GIFEncoderState() { }
};

void EncodeGIF(int x, int y, int ***image, int numimage, RGBQUAD *pPal, const char *Comment, FILE *f) {
	GIFEncoderState es;
	EncodeHeader(x, y, pPal, f);
	if (numimage > 1) {
		EncodeLoopExtension(100000, f);
		for (int i = 0; i < numimage; i++) {
			EncodeExtension(50, f);
			es.EncodeBody(x, y, image[i], f);
		}
	}
	else {
		EncodeExtension(0, f);
		es.EncodeBody(x, y, image[0], f);
	}
	EncodeComment(Comment, f);
	fputc(';', f);
}

// add a GIF image to the store
// for compatibility with old version, pass a pointer to the image array and count one
char *GifImageStoreAdd(int x, int y, int ***image, int numimage = 1) {
	int bi = InterlockedIncrement(&ImageStore.BuffersTop);
	if (--bi >= IMAGESTORELIMIT)  return NULL;

	if (1) for (int i = 0; i < numimage; i++)
		Dither(x, y, image[i], LEVR, LEVG, LEVB);

	FILE *f = ArchiveFile("image", "gif", 0, ImageStore.Buffer[bi].Name);

	// Synthesize a 256 (or less) color palette with some number of levels in each of rgb
	// The standard Internet palette has 6 levels in each color -- for a total of 216 colors
	RGBQUAD Pal[256];
	{
		RGBQUAD rgb;
		rgb.rgbRed = 0;
		rgb.rgbGreen = 0;
		rgb.rgbBlue = 0;
		rgb.rgbReserved = 0;
		for (int i = 0; i < 255; i++)
			Pal[i] = rgb;

		for (int r = 0; r < LEVR; r++)
			for (int g = 0; g < LEVG; g++)
				for (int b = 0; b < LEVB; b++) {
					rgb.rgbRed = r * 255 / (LEVR-1);
					rgb.rgbGreen = g * 255 / (LEVG-1);
					rgb.rgbBlue = b * 255 / (LEVB-1);
					rgb.rgbReserved = 0;
					Pal[r*LEVG*LEVB + g*LEVB + b] = rgb;
				}
	}

	{
		EncodeGIF(x, y, image, numimage, Pal, "EPD Framework", f);
		fclose(f);
		char buf[200];
		sprintf(buf, "archive/%s", ImageStore.Buffer[bi].Name);
		f = fopen(buf, "rb");
		fseek(f, 0, SEEK_END);
		int len = ftell(f);
		ImageStore.Buffer[bi].Dope = (char *)malloc(5000+(ImageStore.Buffer[bi].Len = len));
		fseek(f, 0, SEEK_SET);
		fread(ImageStore.Buffer[bi].Dope, 1, len, f);
	}
	fclose(f);

	const char *mime = "image/gif";
	char *foo = (char *)InterlockedExchange((long *)&ImageStore.Buffer[bi].MIME, (long)mime);

	return ImageStore.Buffer[bi].Name;
}
// end GIF code


void PolygonWrite(int **image, int maxi, int maxj, double *polygon, int npts, int color) {
	double *dope = new double[npts + 1000];
	int nn;

	for (int i = 0; i < maxi; i++) {

		nn = 0;
		for (int n = 0; n < npts; n++) {

			double *p1 = &polygon[2*n];
			double *p2 = &polygon[2*(n != npts-1 ? n+1 : 0)];

			// skip if no intersection
			if (i < p1[0] && i < p2[0]) continue;
			if (i > p1[0] && i > p2[0]) continue;

			// skip if parallel to scan line
			if (p1[0] == p2[0]) continue;

			// skip top vertex
			if (p1[0] > p2[0] && i == p1[0]) continue;
			if (p1[0] < p2[0] && i == p2[0]) continue;

			// compute intersection
			double frac = (i-p1[0])/(p2[0]-p1[0]);
			double intersec = p1[1] + frac*(p2[1]-p1[1]);

			dope[nn++] = intersec;
		}

		// sort
		if (1) for (int ii = 0; ii < nn-1; ii++) {
			for (int jj = ii+1; jj < nn; jj++)
				if (dope[ii] > dope[jj]) {
					double t = dope[ii];
					dope[ii] = dope[jj];
					dope[jj] = t;
				}
		}

		// write
		if (1) for (int ii = 0; ii < nn; ii+=2)
			if (dope[ii] != dope[ii+1]) {
				int start = 5000 - (int)(5000.-dope[ii]);
				int end = 5000 - (int)(5000.-dope[ii+1]);

				while (start != end) {
					if (start >= 0 && start < maxj)
						image[i][start] = color;
					start++;
				}
		}

	}

	delete dope;

}

// write a line with a color, width, and possibly rounded ends
// argument pts is the number of verticies at the end
// pts == 2 is a square end
// pts == 5 has 45 degree turns
void LineWrite(int **image, int maxi, int maxj, double x1, double y1, double x2, double y2, int color, int wid = GRAPHWIDTH, unsigned int pts = 7) {

	double poly[160], *p = poly;

	// control number of end segments -- permits static buffer allocation
	if (pts < 2)
		pts = 2;
	else if (pts > sizeof(poly)/sizeof(double)/2)
		pts = sizeof(poly)/sizeof(double)/2;

	// compute parameters of line
	double len = sqrt((x2-x1)*(x2-x1)+(y2-y1)*(y2-y1));
	if (len == 0) return;
	double vecx = (x2-x1)/len*wid*.5;
	double vecy = (y2-y1)/len*wid*.5;

	if (1) for (unsigned int i = 0; i < pts; i++) {
		double theta = (-90. + 180.*i/(pts-1))/(180./3.141592653589793238);
		*p++ =  x2 + cos(theta) * vecx - sin(theta) * vecy;
		*p++ =  y2 + sin(theta) * vecx + cos(theta) * vecy;
	}

	if (1) for (unsigned int i = 0; i < pts; i++) {
		double theta = (90. + 180.*i/(pts-1))/(180./3.141592653589793238);
		*p++ =  x1 + cos(theta) * vecx - sin(theta) * vecy;
		*p++ =  y1 + sin(theta) * vecx + cos(theta) * vecy;
	}

	PolygonWrite(image, maxi, maxj, poly, pts*2, color);
}

void PolyLineWrite(int **image, int maxi, int maxj, double *polygon, int npts, int color, int wid = 1) {
	for (int i = 0; i < npts; i++) {
		int j = i+1 < npts ? i+1 : 0;
		LineWrite(image, maxi, maxj, polygon[2*i], polygon[2*i+1], polygon[2*j], polygon[2*j+1], color, wid);
	}
}

// Line drawing for characters:
// ABCDE
// FGHIJ
// KLMNO
// PQRST
// UVWXY
// -------------- baseline
// 01234
// 56789
// Z -- Pen up

void TextWrite(int **image, int maxi, int maxj, double x, double y, const char *txt, int color, double hj = .5, double vj = .5) {
	double v = SEGHT;
	double h = SEGWD;
	double sp = SEGSP;
	int w = SEGLW;

	x = x + 1 - (2 + strlen(txt)*sp) * hj;
	y = y + 1 - (2 + 2*v) * vj;

	for (; *txt != 0; txt++) {

		static const char *CharForm[256] = {
			NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
			NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
			NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
			// ()*+'-./
			"XRHD", "VRHB", "FTXPJXWC", "RHZLN", "MD", "KO", "VW", "VD",
			// 01234567
			"BFPVXTJDBZUE", "GCWZVX", "FBDJPUY", "AEKNTXU", "AKOZDX", "EAKNTXU", "DCKPVXTNL", "AEV",
			// 89:;<=>?
			"BFLNJDBZLPVXTN", "VWOJDBFLMO", NULL, NULL, "EKY", "FJZPT", "AOU", NULL,
			// @ABCDEFG
			NULL, "UCYNL", "ADJNKNTXUA", "JDBFPVXT", "AUXTJDA", "EAUYZKM", "EAUZKM", "JDBFPVXTOM",
			// HIJKLMNO
			"AUKOEY", "BDZVXZWC", "PVWSDZCE", "AUZKLZMEZMY", "AUY", "UAWEY", "UAYE", "BDJTXVPFB",
			// PQRSTUVW
			"UADJNK", "SYZXTJDBFPVX", "UADJNKZMY", "JDBFLNTXVP", "AEZCW", "APVXTE", "AWE", "AUCYE",
			// XYZ
			"AYZUE", "AMEZMW", "AEUY", NULL, NULL, NULL, NULL, NULL,
			// `abcdefg
			NULL, "YTNMLPVXT", "AUZPVWXTNLP", "OLPVY", "EYZTNLPVXT", "PTNLPVX", "JDCFVZKM", "O482ZTNLPVXT",
			// hijklmno
			"AUZPLNTY", "WMZGI", "M260ZGI", "AUZPOZPY", "CW", "UKZPLRWRNTY", "KUZPLNTY", "LPVXTNL",
			// pqrstuvw
			"5KZPLNTXVP", "N84ZSMLPVWS", "KUZPLNT", "OLPTXU", "CWZLN", "KPVXTZYO", "KWO", "KVMXO",
			// xyz{|}~
			"KYZUO", "KWZOW5", "KOUY", NULL, "WC", NULL, NULL, NULL,
			NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 
			NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 
		};

		const char *pat = CharForm[*txt];
		if (pat != NULL) {
			double oldh = x;
			double oldv = y;

			int suppress = 1;
			for (; *pat != 0; pat++) {
				int c = *pat;
				if (c >= 'A' && c <= 'Z') c += 0 - 'A';
				else if (c >= '0' && c <= '9') c += 25 - '0';

				double v = y + SEGHT*2 - (double)(c/5)*SEGHT/2;
				double h = x + (double)(c%5) * SEGWD/5;

				if (suppress <= 0)
					LineWrite(image, maxi, maxj, oldh, oldv, h, v, color, SEGLW);
				oldh = h;
				oldv = v;

				if (pat[1] == 'Z')
					suppress = 2;
				else
					suppress--;
			}
		}

		x += sp;
	}
}

static int CvtColor(const char *name, int def) {
	if (stricmp(name, "Red") == 0)
		return 255<<16 | 0<<8 | 0;
	else if (stricmp(name, "Green") == 0)
		return 0<<16 | 255<<8 | 0;
	else if (stricmp(name, "Blue") == 0)
		return 0<<16 | 0<<8 | 255;
	else if (stricmp(name, "Black") == 0)
		return 0<<16 | 0<<8 | 0;
	else if (stricmp(name, "Purple") == 0)
		return 255<<16 | 0<<8 | 255;
	else if (stricmp(name, "Gray") == 0)
		return 128<<16 | 128<<8 | 128;
	else if (stricmp(name, "Brown") == 0)
		return 128<<16 | 128<<8 | 64;
	return def;
}
inline int advance(char *&p, char *&e) {
	while (*e != 0 && !isalpha(*e))
		e++;

	if (*e == 0)
		return 0;

	p = e;

	while (iscsym(*e))
		e++;

	return 1;
}


int advancen(char *&p, char *&e) {
	while (*e != 0 && !isdigit(*e))
		e++;

	if (*e == 0)
		return 0;

	p = e;

	while (isdigit(*e))
		e++;

	return 1;
}

void WriteMark(int **image, int ii, int jj, double x, double y, int mode, int color) {
//	if (mode == 0) return;

	double square[8], size = SQUARESIZE;
	if (mode == 2) size *= 1.5;

	square[0] = x-size;
	square[1] = y-size;
	square[2] = x+size;
	square[3] = y-size;
	square[4] = x+size;
	square[5] = y+size;
	square[6] = x-size;
	square[7] = y+size;

	PolygonWrite(image, ii, jj, square, 4, color);
}

HTMLGraph::HTMLGraph() {
	NumPoints = MaxPoints = 0;
	Data = 0;
}

void HTMLGraph::SetName(int c, int bgc) {
	color = c;
	bgcolor = bgc;
}

void HTMLGraph::Clear() {
	NumPoints = MaxPoints = 0;
	if (Data != 0)
		delete Data;
	Data = 0;
}

HTMLGraph::~HTMLGraph() {
	Clear();
}

void HTMLGraph::AddPoint(double d, double t, char *tx, void *p, eDataValidity real) {

	if (p != NULL) payload = p;
	ASSERTALWAYS(_finite(d) && _finite(t));

	if (NumPoints == MaxPoints) {
		MaxPoints = NumPoints + 100 + NumPoints/5;
		DataPoint *old = Data;
		Data = new DataPoint[MaxPoints];
		if (1) for (int i = 0; i < NumPoints; i++)
			Data[i] = old[i];
		delete old;
	}
	Data[NumPoints].X = t;
	Data[NumPoints].Y = d;
	Data[NumPoints].Text = tx;

	*(char *)&Data[NumPoints].Y &= ~3;
	*(char *)&Data[NumPoints++].Y |= real & 3;
}

void HTMLGraph::WriteToBitmap(int **image, int logx, int logy, double ix, double ax, double iy, double ay, int ii, int jj, HTMLGraph::eDataMark cross) {
	minx = ix;
	maxx = ax;
	miny = iy;
	maxy = ay;
	width = ii;
	height = jj;
	for (int i = 0; i < NumPoints-1; i++) {
		double x1 = ((logx!=0?log10(Data[i].X):Data[i].X)-minx)*(ii-HMAR)/(maxx-minx) + HMAR;
		double y1 = ((logy!=0?log10(Data[i].Y):Data[i].Y)-miny)*(jj-VMAR)/(maxy-miny) + VMAR;
		double x2 = ((logx!=0?log10(Data[i+1].X):Data[i+1].X)-minx)*(ii-HMAR)/(maxx-minx) + HMAR;
		double y2 = ((logy!=0?log10(Data[i+1].Y):Data[i+1].Y)-miny)*(jj-VMAR)/(maxy-miny) + VMAR;

		int real1 = *(char *)&Data[i].Y & 3;
		int real2 = *(char *)&Data[i+1].Y & 3;

		unsigned char r = color>>16;
		unsigned char br = bgcolor>>16;
		unsigned char g = color>>8;
		unsigned char bg = bgcolor>>8;
		unsigned char b = color;
		unsigned char bb = bgcolor;
		r = (int)r*2/4 + (int)br*2/4;
		g = (int)g*2/4 + (int)bg*2/4;
		b = (int)b*2/4 + (int)bb*2/4;
		int othercolor = r<<16 | g<<8 | b;

		if (cross&2) ;
		else if (bgcolor < 0 && (!real1 || !real2)) ;
		else LineWrite(image, ii, jj, x1, y1, x2, y2, real1 && real2 ? color : othercolor, real1 && real2 ? GRAPHWIDTH : GRAPHWIDTH*2/3);

		if (cross&1) {
			WriteMark(image, ii, jj, x1, y1, real1, color);
			WriteMark(image, ii, jj, x2, y2, real2, color);
		}

		if (i == 0 && Data[0].Text != NULL)
			TextWrite(image, ii, jj, x1, y1, Data[0].Text, color);

		if (Data[i+1].Text != NULL)
			TextWrite(image, ii, jj, x2, y2, Data[i+1].Text, color, 0., 0.);

	}
}

void HTMLGraph::Range(double &minx, double &maxx, double &miny, double &maxy) {
	for (int i = 0; i < NumPoints; i++) {
		if (minx > Data[i].X) minx = Data[i].X;
		if (maxx < Data[i].X) maxx = Data[i].X;
		if (miny > Data[i].Y) miny = Data[i].Y;
		if (maxy < Data[i].Y) maxy = Data[i].Y;
	}
}

// print a data set as an HTML included graphic
// can optionally print multiple traphs for comparison
// scaling can be specified for { min, max } { x, y } where each combination can be fixed or an extremum5 value can be specified
// 1 output --> File buffer for HTML stream
// 2 title --> Caption
// 3 n --> Number of plot lines
// 4 logx x scale logarithmic
// 5 fmtx 0 means x scale uses SISuffix 1 means x scale uses printf
// 6 logy y scale logarithmic
// 7 fmty 0 means y scale uses SISuffix 1 means y scale uses printf
// 8 dope
// 9 Color
// 10 bgcolor
// 11 cross
// 12 13 minimum x calculation
// 14 15 maximum x calculation
// 16 17 minimum y calculation
// 18 19 maximum y calculation
void HTMLGraph_Print(FILE *output, const char *title, int n, int logx, int fmtx, int logy, int fmty_unused, HTMLGraph *dope, int *Color, int bgcolor, HTMLGraph::eDataMark cross,
			HTMLGraph::eSpecMinX fixminx, double aminx,
			HTMLGraph::eSpecMaxX fixmaxx, double amaxx,
			HTMLGraph::eSpecMinY fixminy, double aminy,
			HTMLGraph::eSpecMaxY fixmaxy, double amaxy) {
	int color1 = Color[0];
	int color2 = (n > 1) ? Color[1] : 0;

	// calculate overall range of all data
	double minx = aminx, maxx = amaxx, miny = aminy, maxy = amaxy;
	if (1) for (int ii = 0; ii < n; ii++)
		dope[ii].Range(minx, maxx, miny, maxy);

	// provide an overrun margin in the event of automatic dimensions
	// let user override automatic dimensions
	if (fixmaxx != 0) maxx = amaxx;
	else if (logx == 0) maxx += (maxx-minx) * .02;
	else maxx *= 1.1;

	if (fixminx != 0) minx = min(aminx, maxx);
	else if (logx == 0)  minx -= (maxx-minx) * .02;
	else minx /= 1.1;

	if (fixmaxy != 0) maxy = amaxy;
	else if (logy == 0)  maxy += (maxy-miny) * .02;
	else maxy *= 1.1;

	if (fixminy != 0) miny = min(aminy, maxy);
	else if (logy == 0)  miny -= (maxy-miny) * .02;
	else miny /= 1.1;

	int *imagedata[GRAPHPOINTS], id[GRAPHPOINTS][GRAPHHEIGHT];
	if (1) for (int i = 0; i < GRAPHPOINTS; i++)
		imagedata[i] = id[i];

	if (1) for (int i = 0; i < GRAPHPOINTS; i++)
		for (int j = 0; j < GRAPHHEIGHT; j++)
			imagedata[i][j] = bgcolor;

	// horizontal grid lines
	{
		if (logy != 0) {
			miny = log10(miny); if (!_finite(miny)) miny = 1e-10;
			maxy = log10(maxy); if (!_finite(maxy)) maxy = 1e10;
		}
		double ss = pow(10., ceil(log10(maxy-miny))-1);	// pick a power of ten interval with 1-10 intervals over the data
		if ((maxy-miny)/ss < 3) ss *= .1;	// assure at least 3 marks

		double firsttick = floor(miny/ss)-2;
		for (int ll = 0; ll < 40; ll++) {
			double yval = (firsttick+ll)*ss;
			if (!_finite(yval) || yval < miny || yval > maxy) continue;
			double vpos = (yval-miny)*(GRAPHHEIGHT-VMAR)/(maxy-miny) + VMAR;
			LineWrite(imagedata, GRAPHPOINTS, GRAPHHEIGHT, HMAR, vpos, GRAPHPOINTS, vpos, 0xc0c0c0, 1);
			TextWrite(imagedata, GRAPHPOINTS, GRAPHHEIGHT, HMAR, vpos,
				logy != 0 ? SISuffix(pow(10., yval), 3, 0) :	SISuffix(yval, 3, 0),
				0x000000, 1, .5);
		}
	}

	// vertical grid lines
	{
		if (logx != 0) {
			minx = log10(minx); if (!_finite(minx)) minx = 1e-10;
			maxx = log10(maxx); if (!_finite(maxx)) maxx = 1e10;
		}
		double ss = pow(10., ceil(log10(maxx-minx))-1);	// pick a power of ten interval with 1-10 intervals over the data
		if ((maxx-minx)/ss < 3) ss *= .1;	// assure at least 3 marks

		double firsttick = floor(minx/ss)-2;
		for (int ll = 0; ll < 40; ll++) {
			double xval = (firsttick+ll)*ss;
			if (!_finite(xval) || xval < minx || xval > maxx) continue;
			double hpos = (xval-minx)*(GRAPHPOINTS-HMAR)/(maxx-minx) + HMAR;
			LineWrite(imagedata, GRAPHPOINTS, GRAPHHEIGHT, hpos, VMAR, hpos, GRAPHHEIGHT, 0xc0c0c0, 1);

			char buf[1050], *bp = buf;
			if (logx == 0 && fmtx == 0) bp = SISuffix(xval, 3, 0);
			else if (logx == 1 && fmtx == 0) bp = SISuffix(pow(10., xval), 3, 0);
			else if (logx == 0 && fmtx == 1) sprintf(buf, "%.0f", xval);
			else sprintf(buf, "%.0f", xval);
			TextWrite(imagedata, GRAPHPOINTS, GRAPHHEIGHT, hpos, VMAR, bp, 0x000000, .5, 1);
		}
	}

	if (1) for (int ii = 0; ii < n; ii++)
		dope[ii].WriteToBitmap(imagedata, logx, logy, minx, maxx, miny, maxy, GRAPHPOINTS, GRAPHHEIGHT, cross);

//		double tick = SIRound(maxy);
//		LineWrite(imagedata, GRAPHPOINTS, GRAPHHEIGHT, 0, tick*GRAPHHEIGHT/(maxy-miny), 10, tick*GRAPHHEIGHT/(maxy-miny), 0x404000);

	if (logy != 0) {
		miny = pow(10., miny);
		maxy = pow(10., maxy);
	}

	if (logx != 0) {
		minx = pow(10., minx);
		maxx = pow(10., maxx);
	}

	fprintf(output, "<table>\n");

	// top legend only if a second graph present
#if TOPLEGEND != 0			// {
	if (n > 1)
		fprintf(output, "<tr><td></td><td>%s<font color=\"#%6x\">K=%s</font>%s</td><td></td><td align=right>%s<font color=\"#%6x\">K=%s</font>%s</td></tr>\n", Out1, color2, SISuffix(pow(10, minx)), Out2, Out1, color2, SISuffix(pow(10, maxx)), Out2);
#endif					// }

	// main row
	fprintf(output, "<tr><td align=right valign=top>%s<font color=\"#%6x\">%s</font>%s</td>", Out1, color1, SISuffix(maxy), Out2);

	fprintf(output, "<td colspan=3 rowspan=3>");
	fprintf(output, "<table border=2><tr><td bgcolor=\"#%6x\">", bgcolor);
#if USEBMP				// {
	fprintf(output, "<a href=/ismap target=detail><img src=\"%s\" ismap></a>", ImageStoreAdd(GRAPHPOINTS, GRAPHHEIGHT, imagedata));
#endif					// }
#if USEGIF && USEBMP			// {
	fprintf(output, "<br>");
#endif					// }
#if USEGIF				// {
	int **ip = imagedata;
	fprintf(output, "<a href=/ismap target=detail><img src=\"%s\" ismap border=0></a>", GifImageStoreAdd(GRAPHPOINTS, GRAPHHEIGHT, &ip));
	//fprintf(output, "<a href=/ismap target=detail><img src=\"d:\\erik\\q\\archive\\%s\" ismap border=0></a>", GifImageStoreAdd(GRAPHPOINTS, GRAPHHEIGHT, &ip));
	//fprintf(output, "<a href=/ismap target=detail><img src=\"file:///d:/erik/q/archive/%s\" ismap border=0></a>", GifImageStoreAdd(GRAPHPOINTS, GRAPHHEIGHT, &ip));
#endif					// }
	fprintf(output, "</td></tr></table>");

	fprintf(output, "</td>");

#if TOPLEGEND != 0			// {
	if (n > 1)
		fprintf(output, "<td valign=top>%s<font color=\"#%6x\">%s</font>%s</td>", Out1, color2, SISuffix(maxy), Out2);
#endif					// }
	fprintf(output, "</tr>\n");

	// lower side legend
	fprintf(output, "<tr></tr><tr><td align=right valign=bottom>%s<font color=\"#%6x\">0.0</font>%s</td>", Out1, color1, Out2);
#if TOPLEGEND != 0			// {
	if (n > 1)
		fprintf(output, "<td valign=bottom>%s<font color=\"#%6x\">0.0</font>%s</td></tr>", Out1, color2, Out2);
#endif					// }
	fprintf(output, "</tr>\n");

	// bottom legend
	fprintf(output, "<tr><td></td><td>%s<font color=\"#%6x\">K=%s</font>%s</td><td></td><td align=right>%s<font color=\"#%6x\">K=%s</font>%s</td></tr>\n", Out1, color1, SISuffix(minx), Out2, Out1, color1, SISuffix(maxx), Out2);

	// legend
#if LEGEND != 0				// {
	fprintf(output, "<tr><td colspan=4 align=center>%s", Out1);
	if (1) for (int ii = 0; ii < n; ii++)
		fprintf(output, "<font color=\"#%0x\">%s</font> ", Color[ii], dope[ii].Name != NULL ? dope[ii].Name->Name : "Measurement");
	fprintf(output, "%s<td></tr>\n", Out2);
#endif					// }

	// caption
	fprintf(output, "<tr><td colspan=4 align=center>%s%s%s</td></tr></table>\n", Out1, title, Out2);

	// print csf table for Excel
	fprintf(output, "<table><tr><td>\n");

	if (0) {
		int MaxNumPoints = 0;
		if (1) for (int i = 0; i < n; i++)
			if (MaxNumPoints < dope[i].NumPoints)
				MaxNumPoints = dope[i].NumPoints;

		if (1) for (int i = 0; i < MaxNumPoints; i++) {
			int newline = 1;
			for (int j = 0; j < n; j++) {
				if (dope[j].NumPoints > 0) {
					if (newline == 0)
						fprintf(output, ", ");
					else
						newline = 0;
					if (i < dope[j].NumPoints)
						fprintf(output, "%.4f, %.4f", dope[j].Data[i].X, log10(dope[j].Data[i].Y));
					else
						fprintf(output, " , ");
				}
			}
			fprintf(output, "<br>\n");
		}
	}
	fprintf(output, "</td></tr></table>\n");


	fprintf(output, "<br>\n");
}

int HTMLGraph::backtranslate(FILE *output, int x, int y, int dist) {
	int logx = 0, logy = 1;
	double xx = logx == 0 ? (x-HMAR)*(maxx-minx)/(width-HMAR) + minx :
			pow(10., (x-HMAR)*(maxx-minx)/(width-HMAR) + minx);
	double yy = logy == 0 ? ((height-y)-VMAR)*(maxy-miny)/(height-VMAR) + miny :
			pow(10., ((height-y)-VMAR)*(maxy-miny)/(height-VMAR) + miny);

	int ty = height-y-1;
	int tx = x-1;
#define LX(x) (logx != 0 ? log10(x) : (x))
#define LY(y) ((y))
	for (int i = 0; i < NumPoints; i++) {
		double rx = logx == 0 ? Data[i].X : log10(Data[i].X);
		double ry = logy == 0 ? Data[i].Y : log10(Data[i].Y);
		double x1 = (rx-LX(minx))*(width-HMAR)/(LX(maxx)-LX(minx)) + HMAR;
		double y1 = (ry-LY(miny))*(height-VMAR)/(LY(maxy)-LY(miny)) + VMAR;
		if (x1-tx <= dist && tx-x1 <= dist && y1-ty <= dist && ty-y1 <= dist) {
			//fprintf(output, "meef %s %s<br>\n", SISuffix(X[i]), SISuffix(Y[i]));
			//payload->HTMLDescription(output, X[i]);
			return 1;
		}
	}
	return 0;
}

HTMLGraph PlotData[NUMPLOTS];

#define FOREACHWORD(s, p, e) for (char *(p) = NULL, *(e) = (s); advance((p), (e)); )
#define FOREACHNUMBER(s, p, e) for (char *(p) = NULL, *(e) = (s); advancen((p), (e)); )

void Plotfcn(void *base, double amp, double horiz, eDataValidity real) {
	((HTMLGraph *)base)->AddPoint(amp, horiz, NULL, NULL, real);	// Ephemeral or Real
}



#define DIAGHDRMODE 0
#if DIAGHDRMODE != 0
#include "header.cpp"
#endif

// image buffer capability
//struct ImageStore Buffer[IMAGESTORELIMIT];
IStore ImageStore;

// search image store for an image matching the url
int IStore::ImageStoreSearch(char *n) {
	for (int i = 0, e = InterlockedExchangeAdd(&BuffersTop, 0); i < e; i++) {
		if (i >= IMAGESTORELIMIT) break;
		if (InterlockedExchangeAdd((long *)&Buffer[i].MIME, 0) != 0)
			if (strcmp(Buffer[i].Name, n) == 0)
				return i;
	}
	return -1;
}

// clear buffers
// atomically destroy the MIME type pointer to indicate unavailable
// then atomically set the BuffersTop pointer to permit reuse
void IStore::ImageStoreClear() {
	for (int i = 0; i < IMAGESTORELIMIT; i++) {
		InterlockedExchange((long *)&Buffer[i].MIME, 0);
		Buffer[i].Name[0] = 0;
		Buffer[i].Len = 0;
#define IMAGESTOREPOINTER 1
#if IMAGESTOREPOINTER != 0
		delete Buffer[i].Dope;
#endif
	}
	InterlockedExchange(&BuffersTop, 0);
}

// Allocate an archival disk file, returning an open FILE * for writing its contents
// If link == 1, create a hyperlink to this file from archive.htm
// If stash != NULL, store the (relative) file name
FILE *IStore::ArchiveFile(const char *notation, const char *extension, int link, char *stash) {

	time_t long_time;
	time(&long_time);

	// additional segment
	char fraction[10];
	fraction[0] = 0;

	for (int i = 0; i < 50; i++) {
		struct tm *newtime = localtime(&long_time);
		const char *mon[12] = { "jan", "feb", "mar", "apr", "may", "jun", "jul", "aug", "sep", "oct", "nov", "dec", };
		char fn[200], fn2[200];
		sprintf(fn, "%s-%02d%s%02d-%02dh%02dm%02ds%s.%s", notation,
			newtime->tm_mday, mon[newtime->tm_mon], newtime->tm_year-100, newtime->tm_hour, newtime->tm_min, newtime->tm_sec, fraction, extension);

		sprintf(fn2, "archive/%s", fn);

		if (stash != NULL) sprintf(stash, "%s", fn);

		// create the directory
		_mkdir("archive");

		// see if the file exists
		FILE *rval = fopen(fn2, "r");

		// doesn't exist, see if we can make it
		if (rval == NULL) {
			rval = fopen(fn2, "wb+");
			//fclose(rval);
			//rval = fopen(fn2, "wb+");
		}

		// oops, exists, close it and try another
		else {
			fclose(rval);
			rval = NULL;
		}

		// got one
		if (rval != NULL) {

			if (link != 0) {
				FILE *ff = fopen("archive.htm", "a");
				fprintf(ff, "<a href=\"archive/%s\">%s</a><br>\n", fn, fn);
				fclose(ff);
			}
//printf("file %s\n", fn2);
			return rval;
		}

		sprintf(fraction, ".%02x", myrand()&0xff);
	}

	ASSERTALWAYS(0);

	return NULL;
}

void ServeData::Set(unsigned int s) {
	URI = NULL;
	index = NULL;
	p = NULL;
	lastwrite = 0;
	theClient = s;
	argc = 0;
	name = NULL;
	value = NULL;
}

void ServeData::FreeUp() {
	if (URI != NULL) free(URI);
	if (argc > 0) {
		while (argc > 0) {
			free(name[argc - 1]);
			free(value[argc-- - 1]);
		}
		free(name);
		free(value);
	}
	if (p != NULL) free(p);
	Set(0);
}

void ServeData::CloseAndFreeUp() {
	closesocket(theClient);
	FreeUp();
}

void ServeData::Serve404() {
	send(theClient, "HTTP/1.0 404\r\n", 14, 0);
	send(theClient, "Content-type: ", 14, 0);
	send(theClient, "text/html", 9, 0);
	send(theClient, "\r\n", 2, 0);
	send(theClient, "\r\n", 2, 0);
	send(theClient, "404 not found\n", 14, 0);
	CloseAndFreeUp();
}

void ServeData::Serve200() {
	send(theClient, "HTTP/1.0 200\r\n", 14, 0);
	send(theClient, "Content-type: ", 14, 0);
	send(theClient, "text/html", 9, 0);
	send(theClient, "\r\n", 2, 0);
	send(theClient, "\r\n", 2, 0);
}

void ServeData::TitleTextNoDiskBackup(char *title) {
	Serve200();
	send(theClient, "<html><head><title>", 19, 0);
	send(theClient, title, strlen(title), 0);
	send(theClient, "</title></head><body>\n", 22, 0);
}

FILE *ServeData::TitleTextOpenBufferFile(const char *title, const char *notation, const char *extension, int link, char *stash, int reload) {
	Serve200();
	index = ImageStore.ArchiveFile(notation, extension, link, stash);
	fprintf(index, "<html><head>");
	if (reload > 0)  fprintf(index, "<meta http-equiv=refresh content=%d>", reload);
	fprintf(index, "</head><title>%s</title></head><body>\n", title);
	return index;
}

void ServeData::IncrementalWrite() {
	fseek(index, lastwrite, SEEK_SET);
	char buf[SENDBUFSZ];
	int i;
	do {
		i = fread(buf, 1, sizeof buf, index);
		if (i > 0) send(theClient, buf, i, 0);
	} while (i == sizeof buf);
	lastwrite = ftell(index);
}

void ServeData::CloseBufferFile() {
	fseek(index, lastwrite, SEEK_SET);
	char buf[SENDBUFSZ];
	int i;
	do {
		i = fread(buf, 1, sizeof buf, index);
		if (i > 0) send(theClient, buf, i, 0);
	} while (i == sizeof buf);

	fclose(index);
	CloseAndFreeUp();
}

void ServeData::BasicServe(const char *title, const char *msg) {
	FILE *index = TitleTextOpenBufferFile(title, "index", "htm", 0, NULL);
	fprintf(index, "%s\n", msg);
	fprintf(index, "</body></html>\n");
	CloseBufferFile();
}

// URL encoding decoder
// this will change %xx to the equivalent character
// ** overwrites the input buffer **
char *ServeData::URLDecode(char *x) {
	char *out = x, *rval = x;
	int acc;
	int state = 0;
	for (int c; (c = *x++) != 0; ) {
		if (state == 0 && c == '+')
			*out++ = ' ';
		else if (state == 0 && c != '%')
			*out++ = c;
		else if (state == 0) {
			state = 1;
			acc = 0;
		}
		else {
			acc = (acc<<4) + ((c >= '0' && c <= '9') ? c - '0' : (c >= 'A' && c <= 'F') ? c - 'A' + 10 : (c >= 'a' && c <= 'f') ? c - 'a' + 10 : 0);
			if (state == 1)
				state = 2;
			else {
				*out++ = acc;
				state = 0;
			}
		}
	}
	*out = 0;
	return rval;
}

// convert a double, possibly with a SISuffix suffix
void ServeData::SD(double &d, char *v) {
	sscanf(v, "%lf", &d);
	for (int i = strlen(v); --i >= 0; ) {
		if (v[i] == ' ') continue;
		else if (v[i] == 'y') d *= 1e-24;
		else if (v[i] == 'z') d *= 1e-21;
		else if (v[i] == 'a') d *= 1e-18;
		else if (v[i] == 'f') d *= 1e-15;
		else if (v[i] == 'p') d *= 1e-12;
		else if (v[i] == 'n') d *= 1e-9;
		else if (v[i] == 'u') d *= 1e-6;
		else if (v[i] == 'm') d *= 1e-3;
		else if (v[i] == 'k' || v[i] == 'K') d *= 1e3;
		else if (v[i] == 'M') d *= 1e6;
		else if (v[i] == 'g' || v[i] == 'G') d *= 1e9;
		else if (v[i] == 't' || v[i] == 'T') d *= 1e12;
		else if (v[i] == 'P') d *= 1e15;
		else if (v[i] == 'E') d *= 1e18;
		else if (v[i] == 'Z') d *= 1e21;
		else if (v[i] == 'Y') d *= 1e24;
		else continue;
		break;
	}
}

// convert an integer, possibly with a SI suffix
void ServeData::SI(int &n, char *v) {
	double d;
	sscanf(v, "%lf", &d);
	for (int i = strlen(v); --i >= 0; ) {
		if (v[i] == ' ') continue;
		else if (v[i] == 'y') d *= 1e-24;
		else if (v[i] == 'z') d *= 1e-21;
		else if (v[i] == 'a') d *= 1e-18;
		else if (v[i] == 'f') d *= 1e-15;
		else if (v[i] == 'p') d *= 1e-12;
		else if (v[i] == 'n') d *= 1e-9;
		else if (v[i] == 'u') d *= 1e-6;
		else if (v[i] == 'm') d *= 1e-3;
		else if (v[i] == 'k' || v[i] == 'K') d *= 1e3;
		else if (v[i] == 'M') d *= 1e6;
		else if (v[i] == 'g' || v[i] == 'G') d *= 1e9;
		else if (v[i] == 't' || v[i] == 'T') d *= 1e12;
		else if (v[i] == 'P') d *= 1e15;
		else if (v[i] == 'E') d *= 1e18;
		else if (v[i] == 'Z') d *= 1e21;
		else if (v[i] == 'Y') d *= 1e24;
		else continue;
		break;
	}
	n = (int)d;
}

void ServeData::SS(char *&p, char *v) {
	if (p != NULL) free(p);
	p = strdup(URLDecode(v));
}

void ServeData::SB(int &b, char *v) {
	b = stricmp(v, "on") == 0;
}

// local thread manager
// NOTE: Does not include the main thread
//#define MAXTHREADS 100
class cThreadsSubclass: public cThreads {
	int ActiveThreads;		// number of threads being tracked, including still running and completed
	struct MyThread {
#if __GNUC__ == 0
		HANDLE hThread;		// handle
#else
		pthread_t hThread;
#endif
		unsigned long ec;	// exit code
		FILETIME t[4];		// create, end, kernel, user
		double dt[4];		// double precision version of time

		void Clear() { hThread = 0; ec = 0; for (int i = 0; i < 4; i++) t[i].dwLowDateTime = t[i].dwHighDateTime = 0, dt[i] = 0.; }

		// close operating system's thread handle and clear local data structure
		void Close() { WaitForSingleObject(hThread, 0); CloseHandle(hThread); Clear(); }

		// query operating system and load data to local storage
		void Load() {
#if __GNUC__ == 0
			GetExitCodeThread(hThread, &ec);
			GetThreadTimes(hThread, &t[0], &t[1], &t[2], &t[3]);
			for (int i = 0; i < 4; i++) { LARGE_INTEGER li; li.LowPart = t[i].dwLowDateTime; li.HighPart = t[i].dwHighDateTime; dt[i] = double(li.QuadPart)*100e-9; }
#else
#endif
		}

		// print a line fragment on the thread
		void Print(FILE *fd, int html) {
			fprintf(fd, "%s0x%08x%s", html?"<td>":"", 123, html?"</td>":"");
			char ecbuf[50];
			if (ec == STILL_ACTIVE)
				sprintf(ecbuf, "STILL_ACTIVE");
			else
				sprintf(ecbuf, "%08x", unsigned(ec));

			SYSTEMTIME sc, se;
			FileTimeToSystemTime(&t[0], &sc);
			FileTimeToSystemTime(&t[1], &se);

			//char *ks = SISuffix(dt[2], 3, 1);
			//char *ds = SISuffix(dt[3], 3, 1); 
			//fprintf(fd, "%s%02d:%02d:%02d%s%02d:%02d:%02d%s%ss%s%ss%s%s%s", html?"<td>":" ", sc.wHour, sc.wMinute, sc.wSecond, html?"</td><td>":" ", se.wHour, se.wMinute, se.wSecond, html?"</td><td>":" ", SISuffix(dt[2], 3, 1), html?"</td><td>":" ", SISuffix(dt[3], 3, 1), html?"</td><td>":" ", ecbuf, html?"</td>":"");
		}

		// Sort Threads-> Lowest ranked thread become candidate for replacement by a new thread. Rank is used for printing.
		int operator>(struct MyThread &x) {
			// still active tasks at top of list
			if ((ec == STILL_ACTIVE) != (x.ec == STILL_ACTIVE))
				return ec == STILL_ACTIVE;

			// task with most runtime at top of list
			if (ec == STILL_ACTIVE)
				return dt[2]+dt[3] > x.dt[2]+x.dt[3];

			// task ended longest ago at bottom of list of completed tasks
			else
				return dt[1] > x.dt[1];
		}

	} Array[MAXTHREADS];

	// internal function
	int LoadAndSort() {
		for (int i = 0; i < ActiveThreads; i++)
			Array[i].Load();

		// shell sort
		int flag = 1, d = ActiveThreads;
		while (flag || (d>1)) {	// boolean flag (true when not equal to 0)
			flag = 0;	// reset flag to 0 to check for future swaps
			d = (d+1)/2;
			for (int i = 0; i < (ActiveThreads - d); i++) {
				if (Array[i + d] > Array[i]) {
					struct MyThread temp = Array[i + d];      // swap items at positions i+d and d
					Array[i + d] = Array[i];
					Array[i] = temp;
					flag = 1;	// indicate that a swap has occurred
				}
			}
		}
		return ActiveThreads;
	}
public:
	// initialize data structure -- like a creator function
	void Initialize() {
		Threads = this;
		ActiveThreads = 0;
		for (int i = 0; i < ActiveThreads; i++)
			Array[i].Clear();
	}

	// launch a thread, putting a reference to it in local storage
	void Add(void (__stdcall *f) (ServeData *), ServeData *p) {
	    LoadAndSort();

		// we are full; close and recycle last entry
		if (ActiveThreads == MAXTHREADS)
			Array[--ActiveThreads].Close();
#if __GNUC__ == 0
		Array[ActiveThreads++].hThread = (HANDLE)_beginthreadex(NULL, 0, (unsigned int (__stdcall *)(void *))f, (void *)p, 0, NULL);
#else
		pthread_create(&Array[ActiveThreads++].hThread, NULL, (void* (*)(void *))f, p); 
#endif
	}

	// print system status
	void Print(FILE *fd, int html) {
		fprintf(fd, "%sThread status %d tasks%sTask%shThread%sCreate%sEnd%sKernel%sUser%sExit Code%s", html?"<table border><tr><td colspan=7>":"", LoadAndSort(), html?"</td></tr>\n<tr><td>":":\n", html?"</td><td>":" ", html?"</td><td>":" ", html?"</td><td>":" ", html?"</td><td>":" ", html?"</td><td>":" ", html?"</td><td>":" ", html?"</td></tr>\n":" ");
		for (int i = 0; i < ActiveThreads; i++) {
			fprintf(fd, "%s%d%s", html?"<tr><td>":"", i, html?"</td>":" ");
			Array[i].Print(fd, html);
			fprintf(fd, "%s\n", html?"</tr>":"");
		}
		fprintf(fd, "%s", html?"</table>":"");
	}

	// close everything, waiting until active threads terminate
	void AllDone() {
		for (int i = 0; i < ActiveThreads; i++)
			Array[i].Close();
	}

};

cThreads *Threads;

#if FANCYPAGE != 0

void FancyPageBegin(FILE *index, ServeData &qx) {

    fprintf(index, "<html><head><title>Quantum Computer Simulator</title></head>\n");
    fprintf(index, "<body background=\"bkgrnd.gif\" text=\"#000000\" link=\"#003366\" vlink=\"#cc0033\" alink=\"#000000\">\n");
    fprintf(index, "<a name=\"TOP\"></a>\n");
#if DIAGHDRMODE != 0
    fprintf(index, "<table border=\"0\". width=\"100%%\"><tr><td BACKGROUND=\"diag.gif\" align=center>&nbsp;</td></tr><tr><td align=center>%s</td></tr></table>\n", HEADERMESSAGE);
#endif
    fprintf(index, "<table border=\"0\". width=\"100%%\">\n");
    fprintf(index, "<tr valign=\"top\"><td>\n");
    fprintf(index, "<table border=\"0\" width=\"140\" valign=\"top\">\n");
    fprintf(index, "<tr valign=\"top\"><td width=\"140\" valign=\"top\">\n");
    fprintf(index, "\n");
//	fprintf(index, "1 nav");
    fprintf(index, "</td></tr>\n");
    fprintf(index, "</table>\n");
    fprintf(index, "\n");
    fprintf(index, "<table border=\"0\" width=\"140\" valign=\"top\">\n");
    fprintf(index, "<tr valign=\"top\" align=\"center\"><td width=\"114\" valign=\"top\">\n");
    fprintf(index, "<!----------- #1 - secondary navigation------------>\n");
//	fprintf(index, "2 nav");
	fprintf(index, "<a href=/index.htm target=query>%sIndex%s</a><br>", NOut1, NOut2);
	fprintf(index, "<a href=/SuperStrider>%sSuperStrider%s</a><br>", NOut1, NOut2);
#if SAMPLECOMMANDS != 0
	fprintf(index, "<a href=/test>%sTest%s</a><br>", NOut1, NOut2);
	fprintf(index, "<a href=/status>%sStatus%s</a><br>", NOut1, NOut2);
	fprintf(index, "<a href=/y>%sY%s</a><br>", NOut1, NOut2);
#endif
	fprintf(index, "<a href=/help.htm>%sHelp%s</a><br>", NOut1, NOut2);
	fprintf(index, "<a href=/exit>%sExit%s</a>", NOut1, NOut2);
    fprintf(index, "</td></tr>\n");
    fprintf(index, "\n");
    fprintf(index, "<tr valign=\"top\" align=\"center\"><td width=\"114\" valign=\"top\">\n");
    fprintf(index, "<!----------- #2 - secondary navigation------------>\n");
//	fprintf(index, "3 nav");
    fprintf(index, "</td></tr></table>\n");
    fprintf(index, "\n");
    fprintf(index, "</td>\n");
    fprintf(index, "<td valign=\"top\" align=\"center\" width=\"100%%\">\n");
    fprintf(index, "\n");
    fprintf(index, "<table border=\"0\" width=\"445\" align=\"center\" valign=\"top\">\n");
    fprintf(index, "<tr width=\"445\"><td width=\"445\">\n");
    fprintf(index, "\n");
    fprintf(index, "<table border=\"0\" width=\"100%%\" align=\"center\" valign=\"top\" cellspacing=\"0\" cellpadding=\"0\">\n");
    fprintf(index, "<tr>\n");
    fprintf(index, "<td valign=\"top\">\n");
    fprintf(index, "<!--------- top banner graphic/text goes here --------->\n");
    fprintf(index, "<img src=\"quantumsimulator.gif\" width=\"274\" height=\"62\" hspace=\"0\" vspace=\"4\" alt=\"Programs\"></td>\n");
    fprintf(index, "\n");
    fprintf(index, "<td valign=\"top\" align=\"right\">\n");
    fprintf(index, "<a href=\"http://www.sandia.gov/Main.html\"><img border=\"0\" src=\"logo.gif\" align=\"center\" width=\"151\" hspace=\"0\" height=\"60\" vspace=\"0\" alt=\"Sandia National Laboratories\"></a></td></tr>\n");
    fprintf(index, "<tr><td colspan=\"2\">\n");
    fprintf(index, "<hr>\n");
    fprintf(index, "</td></tr>\n");
    fprintf(index, "</table>\n");
    fprintf(index, "\n");
    fprintf(index, "<center>\n");
    fprintf(index, "\n");
    fprintf(index, "<!---------- body begins here --------->\n");
    fprintf(index, "<table border=\"0\" width=\"100%%\" valign=\"top\" cellspacing=\"0\" cellpadding=\"0\">\n");
    fprintf(index, "<tr>\n");
    fprintf(index, "<td>\n");
//	fprintf(index, "body");
}

void FancyPageEnd(FILE *index, ServeData &qx) {

//fprintf(index, "body end");
    fprintf(index, "</td></tr>\n");
    fprintf(index, "</table>\n");
    fprintf(index, "</center>\n");
    fprintf(index, "\n");
    fprintf(index, "<!---------- body ends here --------->\n");
    fprintf(index, "</td></tr></table>\n");
    fprintf(index, "\n");
    fprintf(index, "</td></tr></table>\n");
    fprintf(index, "\n");
    fprintf(index, "<hr width=\"100%%\">\n");
    fprintf(index, "\n");
    fprintf(index, "<table align=\"top\" border=\"0\" width=\"100%%\">\n");
    fprintf(index, "<tr align=\"center\">\n");
    fprintf(index, "<td valign=\"top\" width=\"140\">\n");
    fprintf(index, "<table border=\"0\" width=\"140\">\n");
    fprintf(index, "<tr><td align=\"center\" width=\"120\" valign=\"top\">\n");
    fprintf(index, "<font size=\"-1\" color=\"#FFFFFF\">\n");
    fprintf(index, "<br></font>\n");
    fprintf(index, "</td>\n");
    fprintf(index, "<td width=\"20\">\n");
    fprintf(index, "<br>\n");
    fprintf(index, "</td></tr></table>\n");
    fprintf(index, "\n");
    fprintf(index, "</td>\n");
    fprintf(index, "\n");
    fprintf(index, "<td valign=\"top\" align=\"center\" width=\"100%%\">\n");
    fprintf(index, "\n");
    fprintf(index, "<table border=\"0\" width=\"100%%\" align=\"center\" valign=\"top\">\n");
    fprintf(index, "<tr valign=top><td>\n");
    fprintf(index, "<font size=\"-1\"><center>\n");
    fprintf(index, "<a href=\"#TOP\">Back to top of page</a> || \n");
    fprintf(index, "<a href=\"http://www.sandia.gov/feedback.htm\">Questions and Comments</a> || \n");
    fprintf(index, "<a href=\"http://www.sandia.gov/dis.htm\">Acknowledgment and Disclaimer</a></center></font>\n");
    fprintf(index, "</td></tr></table>\n");
    fprintf(index, "\n");
    fprintf(index, "</td></tr></table>\n");
    fprintf(index, "\n");
#if DIAGHDRMODE != 0
    fprintf(index, "<table border=\"0\". width=\"100%%\"><tr><td align=center>%s</td></tr><tr><td BACKGROUND=\"diag.gif\" align=center>&nbsp</td></tr></table>\n", HEADERMESSAGE);
#endif
    fprintf(index, "</html>\n");
    fprintf(index, "\n");
}

#else
int FancyPage(FILE *index, ServeData &qx, int WhatToDo)  {
	fprintf(index, "fancy page\n");
	return 0;
}

#endif

// entity encode an asciz string, returning a malloc'd buffer
char *EntityEncode(const char *x) {

	// protect from null input string
	if (x == NULL)
		x = "";

	// a long string of """" grows 6x to &quot;&quot;&quot;&quot;
	char *rval = (char *)malloc(6 * strlen(x) + 1), *p = rval, c;

	for (; (c = *x) != 0; x++)
		if (c == '&') *p++ = '&', *p++ = 'a', *p++ = 'm', *p++ = 'p', *p++ = ';';
		else if (c == '<') *p++ = '&', *p++ = 'l', *p++ = 't', *p++ = ';';
		else if (c == '>') *p++ = '&', *p++ = 'g', *p++ = 't', *p++ = ';';
		else if (c == '"') *p++ = '&', *p++ = 'q', *p++ = 'u', *p++ = 'o', *p++ = 't', *p++ = ';';
		else *p++ = c;

	*p++ = 0;

	// set to correct length
	rval = (char *)realloc(rval, strlen(rval) + 1);

	return rval;
}

// these are the multi-threaded command handlers and have to be added manually
void __stdcall IndexPageThread(ServeData *qx) {

	FILE *index = qx->TitleTextOpenBufferFile("Index Page", "index", "htm", 0, NULL);
#if FANCYPAGE != 0
	FancyPageBegin(index, *qx);
#else
	fprintf(index, "Fancy Page Stub<br>\n");
#endif
	fprintf(index, "<a href=/index.htm target=query>Index</a><br>");
	fprintf(index, "<a href=/SuperStrider>SuperStrider</a><br>");
#if SAMPLECOMMANDS != 0
	fprintf(index, "<a href=/test>Test</a><br>");
	fprintf(index, "<a href=/status>Status</a><br>");
	fprintf(index, "<a href=/y>Y</a><br>");
#endif
	fprintf(index, "<a href=/help.htm>Help</a><br>");
	fprintf(index, "<a href=/exit>Exit</a>");
#if FANCYPAGE != 0
	FancyPageEnd(index, *qx);
#else
	fprintf(index, "</body></html>\n");
#endif
	qx->CloseBufferFile();

	delete qx;
}

#if SAMPLECOMMANDS != 0
void __stdcall StatusPageThread(ServeData *qx) {

// Part 1: define variables, default values, and HTML variable names
// d for double, i for integer
// variable name
// default, do not surround with quotes
// HTML variable names
#define HTMLVARS \
			I(ReloadTime, 3, f1)		\
			I(Factor2, 2, f2)			\
			D(Alpha, 1.23, a)			\
			D(LaserPowerdBm, -30, db)

#define D(a, b, c) char *a = strdup(#b); double d##a;
#define I(a, b, c) int a = b;
HTMLVARS
#undef D
#undef I

// Part 2: Parse input (no specific programmer involvement here)
	int cmd = -1;
	// process input, if any
	if (1) for (int i = 0; i < qx->argc; i++) {
		char *n = qx->name[i];
		char *v = qx->value[i];
		if (0) ;
#define D(a, b, c) else if (strcmp(n, #c) == 0) qx->SS(a, v), qx->SD(d##a, v);
#define I(a, b, c) else if (strcmp(n, #c) == 0) qx->SI(a, v);
HTMLVARS
#undef D
#undef I
		else if (strcmp(n, "cmd") == 0)
			cmd = 0;
	}

	// print the header
	FILE *index = qx->TitleTextOpenBufferFile("Status Page", "test", "htm", 0, NULL, ReloadTime);
#if FANCYPAGE != 0
	FancyPageBegin(index, *qx);
#else
	fprintf(index, "Fancy Page Stub<br>\n");
#endif

	// now execute
	Threads->Print(index, 1);

// Part 3: Do entity encoding as needed (no specific programmer involvement here)
#define D(a, b, c) char *encoded_##a = EntityEncode(a);
#define I(a, b, c)
HTMLVARS
#undef D
#undef I

// Part 4: Output the HTML form
// For reals, hard code the HTML variable name and append encoded_ in front of the corresponding program variable and use %s
// For ints, hard code the HTML varibale name but just use the program variable and use %d
	fprintf(index, "<form method=get action=experiment><br><table>");

	fprintf(index, "<tr><td>Reload Time</td><td><input type=text size=10 name=f1 value=\"%d\"></td></tr>", ReloadTime);
	fprintf(index, "<tr><td>Unused</td><td><input type=text size=10 name=f2 value=\"%d\"></td></tr>", Factor2);
	fprintf(index, "<tr><td>Unused</td><td><input type=text size=10 name=a value=\"%s\"></td></tr>", encoded_Alpha);

	fprintf(index, "<tr><td colspan=2><input type=submit name=cmd value=Run></td></tr>");
	fprintf(index, "</table></form>"); 

// Part 5: Free temporaries
#define D(a, b, c) free(a); free(encoded_##a);
#define I(a, b, c)
HTMLVARS
#undef D
#undef I
#undef HTMLVARS

#if FANCYPAGE != 0
	FancyPageEnd(index, *qx);
#else
	fprintf(index, "</body></html>\n");
#endif
	qx->CloseBufferFile();

	delete qx;
}

void __stdcall TestPageThread(ServeData *qx) {

	// defaults
	int Factor1 = 1;
	int Factor2 = 2;
	char *Alpha = strdup("1.23"); double dAlpha;	// floats in both ascii and double precision
	int cmd = -1;

	// process input, if any
	if (1) for (int i = 0; i < qx->argc; i++) {
		char *n = qx->name[i];
		char *v = qx->value[i];

		if (strcmp(n, "f1") == 0) qx->SI(Factor1, v);
		else if (strcmp(n, "f2") == 0) qx->SI(Factor2, v);
		else if (strcmp(n, "a") == 0) qx->SS(Alpha, v), qx->SD(dAlpha, v);
		else if (strcmp(n, "cmd") == 0) qx->SI(cmd, v);
	}

	// print the header
	FILE *index = qx->TitleTextOpenBufferFile("Test Page", "test", "htm", 0, NULL);

	// now execute
	if (cmd == 0) {
		fprintf(index, "running the command");
		time_t base = time(0L);
	
		if (1) for (time_t i = base; i < base + 10; i++) {
			while (time(0L) < i) ;
			fprintf(index, "%d<br>\n", i);
			qx->IncrementalWrite();
		}
	}

#if FANCYPAGE != 0
	// input form at bottom of page.
	char *t0 = EntityEncode(Alpha);
	fprintf(index, "<form method=get action=test><br>"); 
	fprintf(index, "<input type=text size=10 name=f1 value=\"%d\"><br>", Factor1); 
	fprintf(index, "<input type=text size=10 name=f2 value=\"%d\"><br>", Factor2); 
	fprintf(index, "<input type=text size=10 name=a value=\"%s\"><br>", t0); 
	fprintf(index, "<input type=submit name=cmd value=Run>");
	fprintf(index, "</form>"); 
	free(t0);
#endif

	fprintf(index, "</body></html>\n");
	qx->CloseBufferFile();

	free(Alpha);
	delete qx;
}

#endif

double PerformanceTime() {
#if __GNUC__ == 0
	LARGE_INTEGER f, t;
	QueryPerformanceFrequency(&f);
	QueryPerformanceCounter(&t);
	double tt = double(t.QuadPart)/double(f.QuadPart);
	return double(t.QuadPart)/double(f.QuadPart);
#else
	//struct timespec now;
	//clock_gettime(CLOCK_MONOTONIC, &now);
	//return now.tv_sec + now.tv_nsec / 1000000000.0;
#endif
}

void __stdcall YThread(ServeData *sd) {

	// print the header
	FILE *index = sd->TitleTextOpenBufferFile("Y Thread Background Activity", "tsssssest", "htm", 0, NULL);

	// NOTE: For some reason browsers don't render incrementally with FANCYPAGE
	// Maybe this is because the incremental output is in a table??
#if 0 //FANCYPAGE != 0
	FancyPageBegin(index, *sd);
#else
	fprintf(index, "Y Thread Background Activity<br>\n");
#endif
	sd->IncrementalWrite();

	int base = (int)PerformanceTime();
	int printcount = 0;
	
	if (1) for (int i = base; ; i++) {

		double togo = double(i+1) - PerformanceTime();
		long mstogo = long(togo*1000.);
		if (mstogo > 0 && mstogo < 1000) Sleep(long(togo*1000.));
		if (printcount++ < 20 || (printcount & 7) == 0) {
			fprintf(index, "%f (%d) ", PerformanceTime() - (double)base, int(mstogo));
			sd->IncrementalWrite();
		}

		double t = 0.0;
		for (int i = 0; i < 50000000; i++)
			t+=i;

	}

#if 0 //FANCYPAGE != 0
	FancyPageEnd(index, *sd);
#else
	fprintf(index, "</body></html>\n");
#endif
	sd->CloseBufferFile();

	delete sd;
}

void GIFHeader(SOCKET theClient) {
	send(theClient, "HTTP/1.0 200\r\n", 14, 0);
	send(theClient, "Content-type: ", 14, 0);
	send(theClient, "image/gif", 9, 0);
	send(theClient, "\r\n", 2, 0);
	send(theClient, "\r\n", 2, 0);
}

void SuperSend(SOCKET theClient, const char *data1, int len1, const char *data2, int len2, const char *fn) {
	if (len1 > 0) send(theClient, (char *)data1, len1, 0);
	if (len2 > 0) send(theClient, (char *)data2, len2, 0);

	char fn2[200];
	sprintf(fn2, "archive/%s", fn);

	// see if the file exists
	FILE *f = fopen(fn2, "r");
	if (f != NULL) {
		fclose(f);
		return;
	}

	// doesn't exist, see if we can make it
	f = fopen(fn2, "wb+");
	if (f == NULL)
		return;

	// write the data
	fwrite(data1, len1, 1, f);
	fwrite(data2, len2, 1, f);
	fclose(f);
}

// helper to write a 1x1 pixel gif of a certain color
void SmallGIFHelper(SOCKET theClient, unsigned char *tail, const char *fn) {
	GIFHeader(theClient);
	unsigned char header[73] = { 71, 73, 70, 56, 55, 97, 1, 0, 1, 0, 179, 0, 0, 0, 0, 0, 128, 0, 0, 0, 128, 0, 128, 128, 0, 0, 0, 128, 128, 0, 128, 0, 128, 128, 192, 192, 192, 128, 128, 128, 255, 0, 0, 0, 255, 0, 255, 255, 0, 0, 0, 255, 255, 0, 255, 0, 255, 255, 255, 255, 255, 44, 0, 0, 0, 0, 1, 0, 1, 0, 0, 4, 2, };
	SuperSend(theClient, (char *)header, 73, (char *)tail, 4, fn);
	//send(theClient, (char *)header, 73, 0);
	//send(theClient, (char *)tail, 4, 0);
}

// helper to write a 1x1 pixel gif of a certain color
void BigGIFHelper(SOCKET theClient, unsigned char *data1, int len1, const char *fn) {
	GIFHeader(theClient);
	SuperSend(theClient, (char *)data1, len1, NULL, 0, fn);
}

        static unsigned char quantumsimulator[2736] = { 71, 73, 70, 56, 55, 97, 18, 1, 62, 0, 247, 0, 0, 0, 0, 0, 128, 0, 0, 0, 128, 0, 128, 128, 0, 0, 0, 128, 128, 0, 128, 0, 128, 128, 192, 192, 192, 192, 220, 192, 166, 202, 240, 0, 0, 0, 0, 0, 42, 0, 0, 85, 0, 0, 127, 0, 0, 170, 0, 0, 212, 0, 42, 0, 0, 42, 42, 0, 42, 85, 0, 42, 127, 0, 42, 170, 0, 42, 212, 0, 85, 0, 0, 85, 42, 0, 85, 85, 0, 85, 127, 0, 85, 170, 0, 85, 212, 0, 127, 0, 0, 127, 42, 0, 127, 85, 0, 127, 127, 0, 127, 170, 0, 127, 212, 0, 170, 0, 0, 170, 42, 0, 170, 85, 0, 170, 127, 0, 170, 170, 0, 170, 212, 0, 212, 0, 0, 212, 42, 0, 212, 85, 0, 212, 127, 0, 212, 170, 0, 212, 212, 42, 0, 0, 42, 0, 42, 42, 0, 85, 42, 0, 127, 42, 0, 170, 42, 0, 212, 42, 42, 0, 42, 42, 42, 42, 42, 85, 42, 42, 127, 42, 42, 170, 42, 42, 212, 42, 85, 0, 42, 85, 42, 42, 85, 85, 42, 85, 127, 42, 85, 170, 42, 85, 212, 42, 127, 0, 42, 127, 42, 42, 127, 85, 42, 127, 127, 42, 127, 170, 42, 127, 212, 42, 170, 0, 42, 170, 42, 42, 170, 85, 42, 170, 127, 42, 170, 170, 42, 170, 212, 42, 212, 0, 42, 212, 42, 42, 212, 85, 42, 212, 127, 42, 212, 170, 
                42, 212, 212, 85, 0, 0, 85, 0, 42, 85, 0, 85, 85, 0, 127, 85, 0, 170, 85, 0, 212, 85, 42, 0, 85, 42, 42, 85, 42, 85, 85, 42, 127, 85, 42, 170, 85, 42, 212, 85, 85, 0, 85, 85, 42, 85, 85, 85, 85, 85, 127, 85, 85, 170, 85, 85, 212, 85, 127, 0, 85, 127, 42, 85, 127, 85, 85, 127, 127, 85, 127, 170, 85, 127, 212, 85, 170, 0, 85, 170, 42, 85, 170, 85, 85, 170, 127, 85, 170, 170, 85, 170, 212, 85, 212, 0, 85, 212, 42, 85, 212, 85, 85, 212, 127, 85, 212, 170, 85, 212, 212, 127, 0, 0, 127, 0, 42, 127, 0, 85, 127, 0, 127, 127, 0, 170, 127, 0, 212, 127, 42, 0, 127, 42, 42, 127, 42, 85, 127, 42, 127, 127, 42, 170, 127, 42, 212, 127, 85, 0, 127, 85, 42, 127, 85, 85, 127, 85, 127, 127, 85, 170, 127, 85, 212, 127, 127, 0, 127, 127, 42, 127, 127, 85, 127, 127, 127, 127, 127, 170, 127, 127, 212, 127, 170, 0, 127, 170, 42, 127, 170, 85, 127, 170, 127, 127, 170, 170, 127, 170, 212, 127, 212, 0, 127, 212, 42, 127, 212, 85, 127, 212, 127, 127, 212, 170, 127, 212, 212, 170, 0, 0, 170, 0, 42, 170, 0, 85, 170, 0, 127, 170, 0, 170, 170, 0, 212, 170, 42, 0, 170, 42, 42, 170, 42, 85, 170, 42, 127, 170, 42, 170, 170, 42, 212, 170, 
                85, 0, 170, 85, 42, 170, 85, 85, 170, 85, 127, 170, 85, 170, 170, 85, 212, 170, 127, 0, 170, 127, 42, 170, 127, 85, 170, 127, 127, 170, 127, 170, 170, 127, 212, 170, 170, 0, 170, 170, 42, 170, 170, 85, 170, 170, 127, 170, 170, 170, 170, 170, 212, 170, 212, 0, 170, 212, 42, 170, 212, 85, 170, 212, 127, 170, 212, 170, 170, 212, 212, 212, 0, 0, 212, 0, 42, 212, 0, 85, 212, 0, 127, 212, 0, 170, 212, 0, 212, 212, 42, 0, 212, 42, 42, 212, 42, 85, 212, 42, 127, 212, 42, 170, 212, 42, 212, 212, 85, 0, 212, 85, 42, 212, 85, 85, 212, 85, 127, 212, 85, 170, 212, 85, 212, 212, 127, 0, 212, 127, 42, 212, 127, 85, 212, 127, 127, 212, 127, 170, 212, 127, 212, 212, 170, 0, 212, 170, 42, 212, 170, 85, 212, 170, 127, 212, 170, 170, 212, 170, 212, 212, 212, 0, 212, 212, 42, 212, 212, 85, 212, 212, 127, 212, 212, 170, 212, 212, 212, 0, 0, 0, 12, 12, 12, 25, 25, 25, 38, 38, 38, 51, 51, 51, 63, 63, 63, 76, 76, 76, 89, 89, 89, 102, 102, 102, 114, 114, 114, 127, 127, 127, 140, 140, 140, 153, 153, 153, 165, 165, 165, 178, 178, 178, 191, 191, 191, 204, 204, 204, 216, 216, 216, 229, 229, 229, 242, 242, 242, 255, 251, 240, 160, 160, 164, 128, 128, 128, 255, 0, 0, 0, 255, 0, 255, 255, 
                0, 0, 0, 255, 255, 0, 255, 0, 255, 255, 255, 255, 255, 44, 0, 0, 0, 0, 18, 1, 62, 0, 0, 8, 254, 0, 255, 9, 28, 72, 176, 160, 193, 131, 8, 19, 42, 92, 200, 176, 161, 195, 135, 16, 35, 74, 156, 72, 177, 162, 197, 139, 24, 51, 106, 220, 200, 177, 163, 199, 143, 32, 67, 138, 28, 73, 178, 164, 201, 147, 40, 83, 170, 92, 201, 178, 165, 203, 151, 48, 73, 214, 147, 32, 97, 2, 61, 133, 225, 104, 74, 136, 87, 47, 166, 207, 159, 5, 235, 245, 208, 89, 115, 94, 79, 160, 65, 135, 234, 156, 96, 212, 226, 45, 157, 140, 110, 34, 204, 73, 211, 157, 188, 163, 72, 179, 178, 124, 74, 84, 103, 60, 169, 89, 185, 118, 221, 9, 86, 34, 35, 157, 103, 224, 149, 37, 72, 85, 194, 34, 91, 243, 180, 202, 77, 217, 118, 108, 187, 175, 89, 235, 118, 189, 123, 243, 172, 132, 48, 77, 25, 182, 109, 247, 46, 238, 193, 182, 139, 10, 207, 93, 92, 50, 140, 206, 69, 238, 220, 189, 115, 76, 56, 92, 86, 199, 52, 33, 75, 166, 252, 206, 178, 223, 30, 182, 174, 58, 156, 7, 239, 157, 218, 169, 143, 21, 51, 94, 253, 81, 40, 205, 30, 145, 15, 204, 155, 23, 238, 64, 60, 195, 63, 93, 75, 128, 237, 78, 54, 109, 219, 113, 63, 247, 198, 186, 176, 222, 108, 122, 196, 9, 202, 75, 141, 155, 
                181, 243, 140, 186, 37, 180, 179, 37, 181, 158, 245, 122, 92, 39, 132, 59, 218, 150, 231, 63, 170, 140, 234, 254, 45, 215, 121, 203, 122, 65, 191, 52, 195, 32, 31, 8, 190, 30, 102, 73, 244, 148, 74, 167, 46, 240, 58, 122, 162, 55, 144, 143, 15, 255, 158, 222, 248, 9, 162, 213, 71, 148, 36, 7, 48, 55, 208, 125, 234, 97, 213, 94, 127, 201, 61, 199, 24, 102, 85, 173, 245, 143, 88, 195, 125, 167, 19, 95, 92, 133, 33, 159, 78, 97, 108, 39, 144, 94, 58, 185, 211, 20, 85, 97, 220, 240, 24, 60, 16, 74, 224, 206, 90, 41, 18, 21, 218, 120, 37, 158, 8, 207, 133, 7, 244, 36, 214, 88, 110, 41, 6, 98, 85, 35, 166, 103, 98, 102, 240, 120, 232, 32, 107, 32, 150, 71, 144, 88, 211, 245, 132, 88, 97, 55, 226, 216, 206, 85, 51, 225, 88, 213, 85, 59, 158, 225, 206, 61, 99, 25, 41, 208, 125, 68, 185, 3, 79, 129, 99, 89, 137, 101, 102, 212, 209, 35, 37, 115, 102, 158, 105, 85, 61, 85, 174, 57, 164, 115, 77, 210, 20, 32, 146, 182, 40, 201, 220, 141, 86, 78, 198, 33, 117, 232, 89, 9, 207, 152, 18, 156, 65, 221, 120, 52, 89, 41, 153, 45, 225, 216, 130, 99, 128, 245, 156, 241, 90, 100, 239, 188, 72, 148, 161, 145, 206, 152, 217, 59, 244, 160, 23, 134, 100, 248, 16, 149, 
                216, 60, 125, 122, 9, 168, 160, 254, 77, 10, 41, 162, 13, 190, 41, 215, 60, 56, 158, 70, 167, 157, 254, 151, 206, 35, 22, 100, 161, 197, 247, 232, 1, 74, 109, 10, 87, 61, 194, 29, 64, 232, 166, 239, 200, 182, 30, 171, 99, 157, 246, 143, 112, 240, 24, 37, 30, 135, 146, 9, 11, 102, 142, 244, 64, 72, 216, 1, 244, 196, 147, 90, 56, 185, 186, 179, 107, 175, 191, 54, 59, 207, 122, 170, 58, 87, 237, 143, 175, 125, 245, 170, 133, 177, 138, 133, 143, 155, 16, 122, 137, 150, 151, 70, 41, 186, 155, 59, 241, 60, 187, 110, 141, 5, 141, 75, 84, 15, 120, 245, 202, 157, 78, 247, 30, 69, 232, 167, 217, 158, 54, 240, 59, 132, 250, 25, 239, 163, 245, 2, 92, 97, 184, 67, 26, 7, 168, 116, 7, 200, 59, 31, 172, 57, 202, 10, 176, 106, 237, 190, 115, 102, 136, 95, 110, 220, 220, 64, 18, 19, 213, 78, 141, 254, 126, 40, 50, 123, 215, 190, 235, 221, 146, 132, 170, 25, 50, 77, 248, 168, 6, 241, 155, 188, 94, 104, 11, 160, 211, 221, 180, 164, 198, 52, 99, 42, 80, 199, 31, 239, 118, 79, 60, 132, 214, 44, 33, 201, 232, 245, 156, 50, 186, 18, 40, 205, 114, 172, 217, 226, 11, 115, 209, 61, 28, 157, 180, 208, 55, 171, 138, 94, 205, 146, 164, 230, 243, 157, 6, 70, 57, 47, 60, 217, 218, 226, 
                171, 60, 108, 179, 61, 207, 214, 75, 31, 40, 178, 112, 222, 65, 45, 181, 202, 84, 223, 106, 227, 254, 181, 105, 175, 221, 182, 60, 111, 111, 28, 119, 215, 185, 253, 165, 160, 78, 176, 189, 35, 86, 15, 76, 122, 218, 56, 90, 219, 65, 104, 101, 60, 16, 46, 242, 149, 117, 103, 137, 6, 183, 128, 97, 28, 254, 104, 97, 77, 215, 221, 214, 221, 80, 39, 150, 41, 90, 240, 228, 204, 92, 229, 151, 231, 172, 185, 224, 132, 47, 182, 227, 99, 218, 206, 179, 225, 88, 159, 198, 185, 151, 85, 49, 143, 245, 122, 208, 82, 205, 158, 153, 182, 244, 52, 121, 131, 117, 155, 227, 13, 109, 239, 184, 23, 198, 60, 81, 191, 71, 205, 117, 236, 90, 153, 61, 214, 58, 145, 241, 20, 103, 15, 74, 229, 254, 49, 173, 113, 233, 78, 211, 61, 242, 148, 10, 188, 128, 82, 98, 79, 175, 117, 93, 1, 24, 78, 242, 165, 11, 205, 229, 110, 221, 43, 38, 190, 4, 228, 155, 47, 253, 224, 212, 195, 20, 93, 122, 145, 129, 151, 64, 98, 6, 153, 250, 1, 45, 106, 235, 232, 82, 179, 142, 242, 191, 221, 188, 35, 82, 239, 187, 208, 244, 26, 184, 169, 200, 36, 75, 32, 105, 74, 207, 59, 26, 70, 19, 194, 128, 101, 48, 92, 123, 86, 212, 220, 97, 192, 250, 220, 142, 113, 16, 36, 148, 7, 251, 55, 23, 54, 149, 38, 128, 145, 186, 
                13, 86, 170, 245, 14, 72, 197, 35, 28, 241, 176, 69, 178, 212, 69, 175, 23, 70, 10, 95, 254, 36, 147, 135, 45, 106, 8, 169, 13, 222, 196, 56, 165, 49, 86, 125, 194, 241, 194, 83, 201, 144, 100, 21, 131, 84, 141, 144, 104, 154, 181, 144, 166, 138, 3, 161, 161, 100, 54, 40, 143, 28, 94, 176, 62, 66, 36, 226, 22, 241, 66, 69, 37, 178, 48, 43, 214, 121, 31, 219, 194, 17, 24, 130, 24, 199, 109, 215, 153, 13, 118, 68, 70, 143, 247, 177, 177, 65, 245, 168, 99, 219, 238, 72, 178, 227, 36, 39, 141, 123, 108, 99, 31, 215, 88, 29, 63, 6, 197, 144, 131, 4, 92, 28, 5, 153, 71, 53, 202, 131, 143, 245, 65, 228, 25, 91, 120, 29, 133, 152, 199, 32, 234, 82, 76, 37, 45, 185, 73, 136, 92, 39, 85, 36, 187, 164, 69, 68, 89, 156, 78, 78, 242, 148, 9, 201, 36, 255, 80, 201, 202, 86, 58, 4, 132, 171, 116, 165, 44, 103, 105, 144, 43, 154, 145, 150, 184, 204, 165, 27, 233, 241, 45, 80, 234, 242, 151, 192, 12, 166, 48, 135, 73, 204, 98, 26, 243, 152, 200, 76, 166, 50, 151, 201, 204, 102, 58, 243, 153, 208, 140, 166, 52, 167, 73, 205, 106, 98, 36, 74, 54, 65, 9, 85, 180, 227, 75, 107, 254, 242, 127, 76, 57, 138, 88, 162, 114, 146, 182, 184, 201, 155, 196, 20, 95, 191, 
                80, 23, 75, 141, 32, 134, 62, 232, 20, 166, 240, 248, 162, 66, 155, 141, 100, 73, 237, 254, 220, 82, 122, 4, 25, 207, 112, 85, 14, 82, 156, 241, 144, 45, 243, 137, 17, 124, 62, 228, 51, 161, 233, 102, 63, 25, 163, 27, 222, 248, 166, 54, 183, 137, 100, 47, 77, 98, 80, 135, 60, 109, 161, 56, 147, 79, 207, 234, 115, 157, 255, 136, 166, 61, 55, 226, 201, 178, 188, 130, 149, 236, 8, 169, 59, 24, 51, 157, 62, 241, 3, 174, 249, 73, 32, 63, 37, 237, 10, 26, 68, 185, 32, 154, 192, 71, 161, 24, 237, 72, 138, 86, 100, 144, 193, 212, 8, 70, 228, 2, 88, 139, 162, 214, 20, 10, 89, 237, 66, 95, 57, 152, 254, 198, 114, 15, 163, 12, 149, 38, 9, 181, 94, 87, 238, 177, 30, 18, 145, 107, 17, 65, 194, 105, 78, 221, 153, 37, 226, 188, 115, 169, 69, 115, 81, 248, 116, 150, 82, 231, 137, 237, 126, 18, 128, 139, 75, 85, 164, 150, 219, 77, 117, 59, 109, 10, 208, 86, 73, 162, 187, 0, 225, 51, 102, 134, 10, 42, 176, 228, 179, 136, 26, 157, 235, 103, 74, 93, 78, 24, 108, 97, 139, 122, 57, 170, 131, 83, 60, 44, 111, 34, 37, 15, 77, 73, 6, 109, 28, 82, 11, 94, 79, 37, 164, 185, 146, 132, 88, 93, 57, 205, 93, 153, 21, 26, 64, 233, 170, 139, 39, 154, 99, 7, 235, 20, 
                191, 192, 93, 170, 58, 119, 28, 169, 116, 82, 119, 172, 71, 37, 203, 58, 74, 73, 254, 28, 181, 108, 53, 47, 182, 49, 43, 88, 19, 181, 172, 73, 244, 133, 184, 164, 138, 45, 105, 107, 26, 216, 154, 50, 248, 22, 209, 94, 172, 180, 74, 29, 160, 148, 138, 219, 218, 121, 225, 203, 108, 126, 146, 10, 122, 232, 5, 92, 32, 234, 22, 37, 37, 163, 81, 96, 87, 246, 179, 127, 152, 173, 184, 127, 101, 142, 82, 51, 136, 35, 230, 166, 12, 186, 235, 91, 41, 91, 237, 101, 207, 235, 98, 183, 105, 133, 253, 45, 236, 12, 250, 221, 58, 209, 105, 108, 177, 82, 42, 132, 38, 240, 192, 118, 60, 134, 180, 231, 101, 22, 16, 225, 203, 94, 130, 186, 183, 35, 95, 51, 141, 124, 207, 71, 223, 255, 26, 87, 165, 128, 109, 217, 240, 180, 229, 14, 7, 55, 23, 54, 222, 105, 104, 133, 52, 60, 179, 253, 29, 120, 36, 51, 233, 220, 212, 230, 165, 184, 5, 123, 184, 193, 100, 98, 19, 226, 30, 151, 95, 9, 255, 133, 186, 158, 2, 48, 82, 143, 162, 41, 239, 72, 142, 94, 5, 254, 176, 72, 132, 231, 22, 109, 89, 235, 180, 155, 67, 177, 91, 234, 212, 192, 174, 124, 234, 96, 182, 251, 30, 105, 141, 135, 188, 143, 241, 198, 182, 231, 211, 49, 72, 164, 74, 20, 245, 33, 205, 196, 82, 19, 50, 115, 183, 103, 192, 237, 
                158, 137, 185, 82, 117, 159, 113, 219, 23, 153, 65, 193, 78, 202, 83, 190, 93, 156, 5, 225, 85, 79, 253, 173, 80, 203, 164, 253, 7, 1, 73, 216, 226, 14, 10, 141, 188, 18, 120, 71, 247, 226, 76, 222, 48, 108, 176, 39, 13, 92, 151, 182, 224, 42, 65, 3, 163, 57, 34, 46, 20, 99, 12, 3, 51, 80, 129, 52, 218, 209, 73, 4, 139, 45, 103, 24, 15, 34, 110, 16, 135, 58, 52, 204, 163, 233, 17, 197, 67, 29, 32, 137, 88, 169, 71, 167, 131, 197, 192, 48, 22, 241, 137, 144, 198, 226, 161, 167, 204, 166, 64, 18, 199, 56, 185, 133, 53, 184, 188, 43, 73, 89, 191, 250, 109, 138, 180, 142, 28, 37, 58, 235, 55, 62, 18, 57, 188, 156, 53, 173, 9, 185, 75, 53, 166, 214, 141, 146, 92, 53, 171, 73, 41, 19, 173, 26, 196, 148, 8, 129, 118, 40, 157, 173, 236, 106, 91, 251, 218, 216, 110, 72, 64, 0, 0, 59, };

	static unsigned char logo[199] = { 71, 73, 70, 56, 57, 97, 151, 0, 60, 0, 179, 0, 0, 192, 192, 192, 204, 255, 255, 204, 204, 255, 204, 204, 204, 153, 204, 255, 153, 204, 204, 153, 153, 153, 102, 204, 204, 102, 153, 204, 102, 102, 102, 51, 153, 204, 51, 51, 51, 0, 153, 204, 0, 0, 0, 0, 0, 0, 0, 0, 0, 33, 249, 4, 1, 0, 0, 0, 0, 44, 0, 0, 0, 0, 151, 0, 60, 0, 0, 4, 116, 16, 200, 73, 171, 189, 56, 235, 205, 187, 255, 96, 40, 142, 100, 105, 158, 104, 170, 174, 108, 235, 190, 112, 44, 207, 116, 109, 223, 120, 174, 239, 124, 239, 255, 192, 160, 112, 72, 44, 26, 143, 200, 164, 114, 201, 108, 58, 159, 208, 168, 116, 74, 173, 90, 175, 216, 172, 118, 203, 237, 122, 191, 224, 176, 120, 76, 46, 155, 207, 232, 180, 122, 205, 110, 187, 223, 240, 184, 124, 78, 175, 219, 239, 248, 188, 126, 207, 239, 251, 255, 128, 129, 130, 131, 132, 133, 134, 135, 136, 137, 138, 139, 140, 141, 142, 143, 144, 145, 146, 147, 148, 149, 150, 31, 17, 0, 0, 59, };

static unsigned char bluegif[4] =   { 144, 69, 0, 59,  };
static unsigned char clear[44] = { 71, 73, 70, 56, 57, 97, 1, 0, 1, 0, 128, 0, 0, 255, 255, 255, 0, 0, 0, 33, 249, 4, 1, 0, 0, 0, 0, 44, 0, 0, 0, 0, 1, 0, 1, 0, 64, 2, 2, 132, 81, 0, 59, 0, };
static unsigned char bkgrnd[83] = { 71, 73, 70, 56, 57, 97, 220, 5, 1, 0, 128, 255, 0, 255, 255, 255, 0, 51, 102, 44, 0, 0, 0, 0, 220, 5, 1, 0, 0, 2, 50, 140, 143, 169, 203, 237, 15, 163, 156, 148, 129, 139, 179, 222, 188, 251, 15, 134, 226, 72, 150, 230, 137, 166, 234, 202, 182, 238, 11, 199, 242, 76, 215, 246, 141, 231, 250, 206, 247, 254, 15, 12, 10, 135, 196, 162, 241, 136, 156, 21, 0, 0, 59, };

void GIFEncode(FILE *out, const char *code) {
	char fname[100];
	sprintf(fname, "%s.gif", code);
	FILE *test = fopen(fname, "rb");
	int len = 0;
	int c;
	while ((c = fgetc(test)) != -1)
		len++;

	fclose(test);
	test = fopen(fname, "rb");

	fprintf(out, "    else if (stricmp(File, \"%s\") == 0) {\n        static unsigned char %s[%d] = { ", fname, code, len);

	len = 0;
	while ((c = fgetc(test)) != -1) {
		fprintf(out, "%d, ", c);
		if (++len % 256 == 0)
			fprintf(out, "\n                ");
	}

	fprintf(out, "};\n        BigGIFHelper(theClient, %s, %d);\n    }\n", code, len);


	fclose(test);
}

int main(int argc, char* argv[]) {

    // helper code to generate USEGIF writing code
    // create a 1x1 pixel USEGIF in GIF87a uninterlaced format and convert to 77 integers with the code below
    if (0) {
	FILE *otest = fopen("images.cpp", "w");
	GIFEncode(otest, "networkplanner");
	fclose (otest);
    }

    printf("Superstrider beta 1.0. Usage: Access http://127.0.0.1 from a web browser.\n");

    // initialize web serving
#if __GNUC__ == 0

    WSADATA wsdata;
    int rc = WSAStartup(MAKEWORD(1,1), &wsdata);
	if (rc) {
        printf("WSAStartup failed: error code = %d\n", rc);
        return 1;
    }

    int Local_Address_Family = AF_INET;
    int Socket_Type = SOCK_STREAM;
    int Protocol = IPPROTO_TCP;
    u_int s = socket(Local_Address_Family, Socket_Type, Protocol);

    if (s == (u_int)INVALID_SOCKET) {
	    printf("Socket call failed\n");
	    return 1;
    }

    int yes = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (const char *)&yes, sizeof(int));

    SOCKADDR_IN Address;
    Address.sin_family = Local_Address_Family;
    Address.sin_addr.s_addr = INADDR_ANY;
    Address.sin_port = htons(80);

    rc = bind(s, (const struct sockaddr *) &Address, sizeof(SOCKADDR_IN));

    if (rc == SOCKET_ERROR) {
	printf("Error at bind()");
	return 0;
    }

    rc = listen(s, 10);             // 10 is the number of clients that can be queued
    if (rc == SOCKET_ERROR) {
	printf("Error at listen()");
	return 0;
    }

    Threads = new cThreadsSubclass;
    Threads->Initialize();

    // web serving loop
    for (int more = 1; more != 0; ) {
	//Threads->Print();

	// each time through the loop, free all the contents of ss
	// if the request is satisfied in a thread, the thread is responsible for freeing
	class ServeData ss;
	ss.Set(accept(s, NULL, NULL));
	if (ss.theClient == INVALID_SOCKET) {
	    printf("Error at accept()");
	    return 0;
	}
#else
    int pId, listenFd;
    socklen_t len;			//store size of the address
    bool loop = false;
    struct sockaddr_in svrAdd, clntAdd;
    
    //create socket
    listenFd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    
    if(listenFd < 0) {
        printf("Cannot open socket\n");
        return 0;
    }
    
    bzero((char*) &svrAdd, sizeof(svrAdd));
    
    svrAdd.sin_family = AF_INET;
    svrAdd.sin_addr.s_addr = INADDR_ANY;
    svrAdd.sin_port = htons(80);
    
    //bind socket
    if (bind(listenFd, (struct sockaddr *)&svrAdd, sizeof(svrAdd)) < 0) {
        printf("Cannot bind %d\n", errno);
	perror("bind");
        return 0;
    }
    
    listen(listenFd, 5);

    Threads = new cThreadsSubclass;
    Threads->Initialize();

    for (int more = 1; more != 0; ) {
        //printf("Listening\n");

		// each time through the loop, free all the contents of ss
		// if the request is satisfied in a thread, the thread is responsible for freeing
		class ServeData ss;

        //this is where client connects. svr will hang in this mode until client conn
	len = sizeof(clntAdd);
        ss.Set(accept(listenFd, (struct sockaddr *)&clntAdd, &len));

        if (ss.theClient < 0) {
            printf("Cannot accept connection\n");
            return 0;
        }
        else {
            //printf("Connection successful\n");
        }
	int rc;
#endif

	int total = 0;
	char buf[10000];
	do {
		rc = recv(ss.theClient, buf, sizeof buf, 0);
		if (rc < 0)
			break;
		ss.p = (char *)(total == 0 ? malloc(rc + 1) : realloc(ss.p, total + rc + 1));
		memcpy(ss.p + total, buf, rc);
		total += rc;
		ss.p[total] = 0;
	} while (rc == sizeof buf);

	//printf("%s", ss.p);

	int methodget = ss.p != NULL && ss.p[0] == 'G' && ss.p[1] == 'E' && ss.p[2] == 'T' && ss.p[3] == ' ';
	int methodpost = ss.p != NULL && ss.p[0] == 'P' && ss.p[1] == 'O' && ss.p[2] == 'S' && ss.p[3] == 'T' && ss.p[4] == ' ';

	// got something badly formed, neither GET nor POST
	if (!methodget && !methodpost) {
		if (ss.p != NULL)
			free(ss.p);
		continue;
	}

	char *q = ss.p + (methodget ? 4 : 5), *e = q;
	while (*e != 0 && *e != ' ' && *e != '?')
		e++;


	ss.URI = (char *)malloc(e - q + 1);
	strncpy(ss.URI, q, e - q + 1);
	ss.URI[e - q] = 0;
	//printf("file %s\n", ss.URI);

	// POST: skip to after blank line
	if (methodpost != 0)
		while (e[0] != 0 && (e[-4] != '\r' || e[-3] != '\n' || e[-2] != '\r' || e[-1] != '\n'))
			e++;

	// GET: skip ? delimiter
	if (methodget != 0 && *e == '?')
		e++;

	while (1) {
		q = e;
		while (*e != 0 && *e != '&' && *e != '=' && *e != ' ' && *e != '\r' && *e != '\n')
			e++;
		if (e != q) {
			ss.name = (char **)(ss.argc++ == 0 ? malloc(sizeof(char *) * ss.argc) : realloc(ss.name, sizeof(char *) * ss.argc));
			ss.name[ss.argc - 1] = (char *)malloc(e - q + 1);
			strncpy(ss.name[ss.argc - 1], q, e - q);

			ss.name[ss.argc - 1][e - q] = 0;
			//printf("%s", ss.name[ss.argc - 1]);
			if (*e == '=') {
				q = ++e;
				while (*e != 0 && *e != '&' && *e != ' ' && *e != '\r' && *e != '\n')
					e++;
			}
			else
				e = q;
			ss.value = (char **)(ss.argc == 1 ? malloc(sizeof(char *) * ss.argc) : realloc(ss.value, sizeof(char *) * ss.argc));
			ss.value[ss.argc - 1] = (char *)malloc(e - q + 1);
			strncpy(ss.value[ss.argc - 1], q, e - q);
			ss.value[ss.argc - 1][e - q] = 0;
			//printf("=%s\n", ss.value[ss.argc - 1]);
		}
		if (*e != '?' && *e != '&')
			break;
		else
			e++;
	}
//printf("foo\n");
	char *Path = ss.URI;
	char *File = Path;
	if (1) for (char *x = Path; *x != 0; x++)
		if (*x == '/')
			File = x + 1;

	ServeData *qx = new ServeData(&ss);

	int index;				// need it in an expression later

	void __stdcall MMAccThread(ServeData *qx);
	// main web server
	// if a thread IS NOT launched, call ss.CloseAndFreeUp() and delete qx
	// if a thred IS launched, the thread needs to call ss.CloseAndFreeUp and delete qx on a copy of ss
	if (strlen(Path) > 3 && (index = ImageStore.ImageStoreSearch(Path+1)) >= 0) {
		send(ss.theClient, "HTTP/1.0 200\r\n", 14, 0);
		send(ss.theClient, "Content-type: ", 14, 0);
		send(ss.theClient, ImageStore.Buffer[index].MIME, strlen(ImageStore.Buffer[index].MIME), 0);
		send(ss.theClient, "\r\n", 2, 0);
		send(ss.theClient, "\r\n", 2, 0);
		send(ss.theClient, ImageStore.Buffer[index].Dope, ImageStore.Buffer[index].Len, 0);
		ss.CloseAndFreeUp();
		delete qx;
	}
	else if (stricmp(Path, "/quantumsimulator.gif") == 0) { BigGIFHelper(ss.theClient, quantumsimulator, sizeof quantumsimulator, "quantumsimulator.gif"); ss.CloseAndFreeUp(); delete qx; }
	else if (stricmp(Path, "/logo.gif") == 0) { BigGIFHelper(ss.theClient, logo, sizeof logo, "logo.gif"); ss.CloseAndFreeUp(); delete qx; }
#if DIAGHDRMODE != 0
	else if (stricmp(Path, "/diag.gif") == 0) { BigGIFHelper(ss.theClient, diag, sizeof diag, "diag.gif"); ss.CloseAndFreeUp(); delete qx; }
#endif
	else if (stricmp(Path, "/blue.gif") == 0) { BigGIFHelper(ss.theClient, bluegif, sizeof bluegif, "blue.gif"); ss.CloseAndFreeUp(); delete qx; }
	else if (stricmp(Path, "/clear.gif") == 0) { BigGIFHelper(ss.theClient, clear, sizeof clear, "clear.gif"); ss.CloseAndFreeUp(); delete qx; }
	else if (stricmp(Path, "/bkgrnd.gif") == 0) { BigGIFHelper(ss.theClient, bkgrnd, sizeof bkgrnd, "bkgrnd.gif"); ss.CloseAndFreeUp(); delete qx; }
#if 0									// non multithreaded
	else if (stricmp(Path, "/") == 0 || stricmp(Path, "/index.htm") == 0) {
		ss.BasicServe("Index",
				"Fancy Page Stub No Sample Commands<br>\n"
				"<a href=/index.htm target=query>Index</a><br>"
				"<a href=/SuperStrider>SuperStrider</a><br>"
#if SAMPLECOMMANDS != 0
				"<a href=/test>Test</a><br>"
				"<a href=/status>Status</a><br>"
				"<a href=/y>Y</a><br>"
#endif
				"<a href=/help.htm>Help</a><br>"
				"<a href=/exit>Exit</a>");
		delete qx;
	}
#else
	else if (stricmp(Path, "/") == 0 || stricmp(Path, "/index.htm") == 0) Threads->Add(IndexPageThread, qx);
#endif
	else if (stricmp(Path, "/SuperStrider") == 0)
#if __GNUC__ == 0
	    Threads->Add(MMAccThread, qx);
#else
	    MMAccThread(qx);
#endif
#if SAMPLECOMMANDS != 0
	else if (stricmp(Path, "/test") == 0) Threads->Add(TestPageThread, qx);
	else if (stricmp(Path, "/status") == 0) Threads->Add(StatusPageThread, qx);
	else if (stricmp(Path, "/y") == 0) Threads->Add(YThread, qx);
#endif
	else if (strnicmp(Path, "/help", 5) == 0) { ss.BasicServe("Help", "help page"); delete qx; }
	else if (strnicmp(Path, "/exit", 5) == 0) { ss.BasicServe("Exit", "Exit"); delete qx; more = 0; }
	else { ss.Serve404(); delete qx; }
	_CrtCheckMemory();
    }
#if __GNUC__ == 0
    Threads->AllDone();

    ImageStore.ImageStoreClear();

    closesocket(s);
    WSACleanup();
    _CrtDumpMemoryLeaks();
    return 0;
#else
    //printf("Bye Erik. Calling getchar:\n");
    //getchar();
#endif
}

#define TESTALLOC 1

class Record {
public:
    int Index;
    double Value;
};

bool RecordSort1(Record i, Record j) { return (i.Index < j.Index); }
bool RecordSort2(Record i, Record j) { return (j.Index < i.Index); }

enum Opcodes { CLEAR=1, ADDVEC=2,
	MAXIMUM=3, RET_MAXIMUM2=103,
	MINIMUM=4, RET_MINIMUM2=104,
	NORMALIZE=5, RET_NORMALIZE2=105, RET_NORMALIZE3=106, };

enum LH { L = 0, H = 1, };

enum AB { A = 0, B = 1, };

const char *OpTxt(Opcodes Opcode) {
    const char *Otxt;
    switch (Opcode) {
	case MINIMUM: Otxt = "Minimum"; break;
	case RET_MINIMUM2: Otxt = "Minimum2"; break;
	case MAXIMUM: Otxt = "Maximum"; break;
	case RET_MAXIMUM2: Otxt = "Maximum2"; break;
	case NORMALIZE: Otxt = "Normalize"; break;
	case RET_NORMALIZE2: Otxt = "Normalize2"; break;
	case RET_NORMALIZE3: Otxt = "Normalize3"; break;
	case CLEAR: Otxt = "Clear"; break;
	case ADDVEC: Otxt = "Addvec"; break;
	default: Otxt = "XX"; break;
    }
    return Otxt;
}

class Superstrider;

class Row {
public:
    int RowA, NumA;			// RowA is left, lesser, or A branch -- or next item in freelist
    int RowB, NumB;			// RowB is right, greater-equal, or B branch
#if TESTALLOC
    int test;
#endif
    int Pivot;				// -1 value is flag that this row is unused
    int Goal;				// the goal of the particular function being executed at the moment
    char Step;				// number of steps completed in a row's multi-step function
    AB First;				// do A or B first (depends on function)
    int ReturnRow;			// this is a return address in the equivalent of the stack
    Opcodes ReturnOpcode;		// this is the function to execute on THIS ROW when the subroutine returns
    vector<Record> Records;

    void Init() {
	RowA = NumA = 0;
	RowB = NumB = 0;
	Pivot = -1;			// Not allocated
	Goal = 0;
	ReturnRow = -2;
	Records.clear();
    }
};

class Superstrider {
public:
    int PreviousRow, RowNumQ;
    int RowNum;
    int PendingPop;
    Opcodes Opcode;
    int ReturnRow;
    int FreeMeFlag;			// if set, indicates that the child node requests it be freed upon return
    Row *row;
    vector<Row> DRAM;			// this models the DRAM
    int FreeListRow;			// these variables control memory allocation in the DRAM
    int Arg;				// integer argument or return value
    unsigned CycleCount;
    vector<Record> Acc;			// This is the "accumulator," used for passing variables

    int RowAlloc() {
	int rval = FreeListRow;
	Row &row = DRAM[rval];
	FreeListRow = row.RowA;
	row.Init();
	return rval;
    }

    void RowFreeZeroIndex(int &RowNum) {
	Row &row = DRAM[RowNum];
	ASSERT(row.RowA == 0);
	ASSERT(row.RowB == 0);
	row.Init();
	row.RowA = FreeListRow;
	FreeListRow = RowNum;
	RowNum = 0;
    }

    int Verify(int RowNum) {
	Row &row = DRAM[RowNum];

	int lv = row.RowA != 0 ? Verify(row.RowA) : 0;
	int rv = row.RowB != 0 ? Verify(row.RowB) : 0;

	if (lv < 0 || rv < 0) return -1;

	if (lv != row.NumA) return -1;
	if (rv != row.NumB) return -1;

	return lv + row.Records.size() + rv;
    }
    void PrintAcc(const char *txt, vector<Record> *Acc, ServeData *qx, FILE *index);
    void PrintRowTree(int RowNum, int Indent, map<int, double> &GroundTruth, ServeData *qx, FILE *index);
    void Print(const char *txt, vector<Record> *Ap, int IndentFlag, int Trace, unsigned K, map<int, double> &GroundTruth, ServeData *qx, FILE *index);

    int Eff2(int RowNum) {
	int rval = 1;
	Row &row = DRAM[RowNum];

	if (row.RowA != 0) rval += Eff2(row.RowA);
	if (row.RowB != 0) rval += Eff2(row.RowB);
	return rval;
    }

    double Efficiency(unsigned K, map<int, double> &GroundTruth) {
	int Resources = 0;
	int gt = GroundTruth.size();
	Resources = Eff2(0);
	return 1.*
	    GroundTruth.size()/
	    (K*Resources);
    }

    void Compress(vector<Record> &vec) {
	if (vec.size() > 1) {
	    int k = 0;
	    for (vector<Record>::iterator it = vec.begin()+1 ; it != vec.end(); it++)
		if (vec[k].Index == it->Index)
		    vec[k].Value += it->Value;
		else
		    vec[++k] = *it;
	    vec.resize(k+1);
	}
    }

    void SendLow(int Validated, vector<Record> &a, vector<Record> &Acc);
    void SendHigh(int Validated, vector<Record> &a, vector<Record> &Acc);
    void CallNoData(AB ab, int a, Opcodes op, Opcodes op2);
    void CallData(AB ab, LH lh, int a, int n, Opcodes op, Opcodes op2);

    int StateMachine(Opcodes O, int R, unsigned K, FILE *Index);
} S;

// print accumulator only
void Superstrider::PrintAcc(const char *txt, vector<Record> *Acc, ServeData *qx, FILE *index) {
    const char *NOut1 = "<font face=arial size=-2>";
    const char *NOut2 = "</font>";
    fprintf(index, "<tr><td colspan=4>%s%s%s</td>", NOut1, txt, NOut2);
    for (vector<Record>::iterator it = Acc->begin(); it != Acc->end(); it++) {
	//fprintf(index, "<td bgcolor=%s>%s[%d]=%4.2f%s</td>", "\"#E6E6FA\"", NOut1, it->Index, it->Value, NOut2);
	fprintf(index, "<td bgcolor=%s>%s[%d]=%s%s</td>", "\"#E6E6FA\"", NOut1, it->Index, SISuffix(it->Value), NOut2);
    }
    fprintf(index, "</tr>\n");
}

// to suppress indenting, provide a large negative value for the parameter Intent
void Superstrider::PrintRowTree(int RowNum, int Indent, map<int, double> &GroundTruth, ServeData *qx, FILE *index) {
    const char *NOut1 = "<font face=arial size=-2>";
    const char *NOut2 = "</font>";
    Row &row = DRAM[RowNum];

#if TESTALLOC
    row.test++;
#endif

    if (row.RowA != 0) PrintRowTree(row.RowA, Indent+1, GroundTruth, qx, index);
    fprintf(index, "<tr>");
    fprintf(index, "<td>%s%d:(%d)%s</td><td>%s%d%s</td><td>%s%d(%d)%s</td><td>%s%d(%d)%s</td>",
	NOut1, RowNum, Verify(RowNum), NOut2, NOut1, row.Pivot, NOut2, NOut1, row.RowA, row.NumA, NOut2, NOut1, row.RowB, row.NumB, NOut2);
    for (int i = 0; i < Indent; i++)
	fprintf(index, "<td></td>");
    for (vector<Record>::iterator it = row.Records.begin(); it != row.Records.end(); it++) {
	const char *color;
	if (GroundTruth.find(it->Index) != GroundTruth.end()) {
	    double ratio = 1.;
	    double val1 = GroundTruth[it->Index];
	    double val2 = it->Value;
	    if (fabs(val1) > 1e-9 && fabs(val2) > 1e-9) {
		ratio = val1/val2;
		if (ratio < 1.) ratio = val2/val1;
	    }
	    if (ratio > 1.0000001) color = "yellow";
	    else if (it->Index < row.Pivot) color = "red";
	    else color = "green";
	}
	else
	    color = "grey";
	//fprintf(index, "<td bgcolor=%s>%s[%d]=%4.2f%s</td>", color, NOut1, it->Index, it->Value, NOut2);
	fprintf(index, "<td bgcolor=%s>%s[%d]=%s%s</td>", color, NOut1, it->Index, SISuffix(it->Value), NOut2);
    }
    fprintf(index, "</tr>\n");
    if (row.RowB != 0) PrintRowTree(row.RowB, Indent+1, GroundTruth, qx, index);
    qx->IncrementalWrite();
}

void Superstrider::Print(const char *txt, vector<Record> *Ap, int IndentFlag, int Trace, unsigned K, map<int, double> &GroundTruth, ServeData *qx, FILE *index) {
#if TESTALLOC
    for (unsigned i = 0; i < DRAM.size(); i++) DRAM[i].test = 0;
    for (unsigned i = FreeListRow; i != -1; i = DRAM[i].RowA)
	DRAM[i].test++;
#endif

    const char *NOut1 = "<font face=arial size=-2>";
    const char *NOut2 = "</font>";

    int gt = GroundTruth.size();
    int vf = Verify(0);
    const char *fmt = gt==vf ? "" : " bgcolor=orange";

    fprintf(index, "<table border><tr%s><td>%sAddr:(size)%s</td><td>%sPivot%s</td><td>%sAddr(size)%s</td><td>%sAddr(size)%s</td><td colspan=1000>%s%d %d %s%s</td></tr>\n",
	fmt, NOut1, NOut2, NOut1, NOut2, NOut1, NOut2, NOut1, NOut2, NOut1, gt, vf, txt, NOut2);
    if (Ap != 0) PrintAcc("Acc", Ap, qx, index);
    PrintRowTree(0, IndentFlag == 0 ? -10000: 0, GroundTruth, qx, index);

    fprintf(index, "</table>");

#if TESTALLOC
    int err = 0;
    for (unsigned i = 0; i < DRAM.size(); i++)
	if (DRAM[i].test != 1)
	    err++;

    if (err) {
	fprintf(index, "errors:");
        for (unsigned i = 0; i < DRAM.size(); i++)
	    if (DRAM[i].test != 1)
		fprintf(index, " %d=%d", i, DRAM[i].test);
	fprintf(index, "<p>\n");
    }
#endif
    qx->IncrementalWrite();
}

enum FROM { PARENT, CHILDA, CHILDB, UNKNOWN, };

void Superstrider::SendLow(int Validated, vector<Record> &a, vector<Record> &Acc) {
    int NumToVec = Validated;
    int Keep = Acc.size() - NumToVec;
    for (int i = 0; i < Keep; i++)
	a.push_back(Acc[i+NumToVec]);
    Acc.resize(NumToVec);
}

void Superstrider::SendHigh(int Validated, vector<Record> &a, vector<Record> &Acc) {
    int NumToVec = Validated;
    int Keep = Acc.size() - NumToVec;
    for (int i = 0; i < Keep; i++)
	a.push_back(Acc[i]);
    for (int i = 0; i < NumToVec; i++)
	Acc[i] = Acc[i+Keep];
    Acc.resize(NumToVec);
}

#define JMP(r) { RowNum = r; break; }

// a function executing on a row can call a function on the root of one of its subtrees
// this version takes an integer argument (a) but no records
void Superstrider::CallNoData(AB ab, int a, Opcodes op, Opcodes op2) {
    int rr = ab==0 ? row->RowA : row->RowB;
    ASSERT(row->ReturnRow == -2);

    row->Records.swap(Acc);

    row->ReturnRow = ReturnRow;
    ReturnRow = RowNum;
    RowNum = rr;
    Arg = a;
    Opcode = op;
    row->ReturnOpcode = op2;
}

// a function executing on a row can call a function on the root of one of its subtrees
// this version takes an integer argument (a) as well as a specified number of the least or greatest records
void Superstrider::CallData(AB ab, LH lh, int a, int n, Opcodes op, Opcodes op2) {
    int &rr = ab==0 ? row->RowA : row->RowB, &rn = ab==0 ? row->NumA : row->NumB;
    if (rr == 0) { rr = RowAlloc(); rn = 0; }
    rn += n;
    ASSERT(row->ReturnRow == -2);

    if (lh==L)
	SendLow(n, row->Records, Acc);
    else
	SendHigh(n, row->Records, Acc);

    row->ReturnRow = ReturnRow;
    ReturnRow = RowNum;
    RowNum = rr;
    Arg = a;
    Opcode = op;
    row->ReturnOpcode = op2;
}

#define RETURN(a, op) { RowNum = ReturnRow; PendingPop = 1; Arg = a; Opcode = op; break; }
#define RETURNFREE(a, op) { RowNum = ReturnRow; PendingPop = 1; Arg = a; Opcode = op; FreeMeFlag = Arg == 0; break; }

int Superstrider::StateMachine(Opcodes O, int R, unsigned K, FILE *Index) {
    PreviousRow = -1, RowNumQ = -1;
    RowNum = R;
    PendingPop = 0;
    Opcode = O;
    ReturnRow = -1;
    FreeMeFlag = 0;			// if set, indicates that the child node requests it be freed upon return
    while (RowNum != -1) {
	PreviousRow = RowNumQ;
	RowNumQ = RowNum;
	row = &DRAM[RowNum];
	CycleCount++;

	// previous state transition did a return using the variable ReturnRow above
	// we needed to pop the stack, but couldn't until the row with the right data had been accessed
	if (PendingPop) {
	    ASSERT(row->ReturnRow != -2);
	    ReturnRow = row->ReturnRow;
	    Opcode = row->ReturnOpcode;
	    row->ReturnRow = -2;
	    PendingPop = 0;
	}

	// memory is imagined to be organized as a tree, where each row can have descendant rows
	// a subroutine can be called on a descendant
	// the RETURN(op, c) transfers control to the parent, which will be the caller, but...
	// returns the memory to the freelist if c evaluates to TRUE, also zeroing the pointer in the parent

	// this feature creates a responsibility if called from outside the state machine:
	// if StateMachine returns true, the caller is expected to call RowFreeZeroIndex(row->RowB)
	FROM From = UNKNOWN;
	if (PreviousRow == row->RowA && Opcode >= 100) {
	    From = CHILDA;
	    row->NumA = Arg;
	    if (FreeMeFlag) RowFreeZeroIndex(row->RowA);
	}
	else if (PreviousRow == row->RowB && Opcode >= 100) {
	    From = CHILDB;
	    row->NumB = Arg;
	    if (FreeMeFlag) RowFreeZeroIndex(row->RowB);
	}
	FreeMeFlag = 0;

	const char *Otxt = OpTxt(Opcode);

	int InputAccSize = Acc.size();

	// merge memory row just accessed with accumulator and compress; then count records based on pivot
	for (vector<Record>::iterator it = row->Records.begin() ; it != row->Records.end(); it++)
	    Acc.push_back(*it);
	row->Records.clear();
	sort(Acc.begin(), Acc.end(), RecordSort1);
	Compress(Acc);
	unsigned LT = 0, GE = 0;
	for (vector<Record>::iterator it = Acc.begin() ; it != Acc.end(); it++)
	    if (it->Index < row->Pivot) LT++;
	    else GE++;

	switch(Opcode) {
	    case NORMALIZE:
	    case RET_NORMALIZE2:
	    case RET_NORMALIZE3:
		{
		    // code below needs to do the functions A B C D E F
		    // but they may need to be ordered C D A B E F to avoid an overflow condition
		    // therefore, the code is structured as if (OK) { A B } C D if (!OK) { A B } E F
		    // where OK is row->Goal+GE <= K and indicates no overflow

		    // code executes on function entry
		    if (Opcode == NORMALIZE) {	
			row->Step = 0;
			row->Goal = (row->NumA+LT) % K;
			if (row->Goal == 0 && row->NumA+LT >= (unsigned)K)
			    row->Goal += K;
			row->First = row->Goal+GE <= K ? A : B;
		    }
		    int N;

#define MATMARKET 1
		    // find maximum elements in A subtree, unless A subtree does not exist or if we don't need anything from it
		    // also, skip until later if Acc would overflow -- see documentation
		    if (row->Step <= 0 && row->First == A && row->RowA != 0 && row->Goal > 0)
			CallNoData(A, row->Goal, MAXIMUM, RET_NORMALIZE3), row->Step = 1;

		    // send excess records from max back to subtree with addvec
		    // also, skip until later if Acc would overflow -- see documentation
		    else if (row->Step <= 1 && row->First == A && (N = LT-row->Goal) > 0)
			CallData(A, L, -100, N, ADDVEC, RET_NORMALIZE2), row->Step = 2;

		    // find minimum elements in B subtree, unless B subtree does not exist or if we don't need anything from it
		    else if (row->Step <= 2 && row->RowB != 0 && K-row->Goal > 0)
			CallNoData(B, (K-row->Goal), MINIMUM, RET_NORMALIZE3), row->Step = 3;

		    // send excess records from max back to subtree with addvec
		    else if (row->Step <= 3 && (N = GE-(K-row->Goal)) > 0)
			CallData(B, H, -100, N, ADDVEC, RET_NORMALIZE2), row->Step = 4;

		    // find maximum elements in A subtree, unless A subtree does not exist or if we don't need anything from it
		    // also, this is the retry, given reordering to avoid overflow -- see documentation
		    else if (row->Step <= 4 && row->First == B && row->RowA != 0 && row->Goal > 0)
			CallNoData(A, row->Goal, MAXIMUM, RET_NORMALIZE3), row->Step = 5;

		    // send excess records from max back to subtree with addvec
		    // also, this is the retry, given reordering to avoid overflow -- see documentation
		    else if (row->Step <= 5 && row->First == B && (N = LT-row->Goal) > 0)
			CallData(A, L, -100, N, ADDVEC, RET_NORMALIZE2), row->Step = 6;

		    // recursively normalize A side, unless subtree A does not exist
		    else if (row->Step <= 6 && row->RowA != 0)
			CallNoData(A, -100, NORMALIZE, RET_NORMALIZE2), row->Step = 7;

		    // recursively normalize B side, unless subtree B does not exist
		    else if (row->Step <= 7 && row->RowB != 0)
			CallNoData(B, -100, NORMALIZE, RET_NORMALIZE2), row->Step = 8;

		    else {
			row->Records.swap(Acc);
			int t = row->NumA + row->Records.size() + row->NumB;
			RETURN(t, RET_NORMALIZE2);
		    }
		    break;
		}
	    case MAXIMUM:
	    case RET_MAXIMUM2:
		{
		    if (Opcode == MAXIMUM) row->Goal = Arg;
		    int N;

		    // pull from right side while
		    // the complete caller's request has not been validated yet and there is a right subtree
		    if (InputAccSize < row->Goal && row->RowB != 0)
			CallData(B, H, row->Goal, InputAccSize, MAXIMUM, RET_MAXIMUM2);

		    // if there is no left subtree, the LT part of the row will be minimal
		    // pull from left side while
		    // the complete caller's request has not been validated yet and there is a left subtree
		    else if ((N = max(unsigned(InputAccSize), min(GE, unsigned(row->Goal)))) < row->Goal && row->RowA != 0)
			CallData(A, H, row->Goal, N, MAXIMUM, RET_MAXIMUM2);

		    // when there are no subtrees, the entire row is minimal
		    else {
			SendHigh(min(LT+GE, unsigned(row->Goal)), row->Records, Acc);
			RETURNFREE(row->NumA + row->Records.size() + row->NumB, RET_MAXIMUM2);
		    }
		    break;
		}
	    case MINIMUM:
	    case RET_MINIMUM2:
		{
		    if (Opcode == MINIMUM) row->Goal = Arg;
		    int N;

		    // pull from left side while
		    // the complete caller's request has not been validated yet and there is a left subtree
		    if (InputAccSize < row->Goal && row->RowA != 0)
			CallData(A, L, row->Goal, InputAccSize, MINIMUM, RET_MINIMUM2);

		    // if there is no left subtree, the LT part of the row will be minimal
		    // pull from right side while
		    // the complete caller's request has not been validated yet and there is a right subtree
		    else if ((N = max(unsigned(InputAccSize), min(LT, unsigned(row->Goal)))) < row->Goal && row->RowB != 0)
			CallData(B, L, row->Goal, N, MINIMUM, RET_MINIMUM2);

		    // when there are no subtrees, the entire row is minimal
		    else {
			SendLow(min(LT+GE, unsigned(row->Goal)), row->Records, Acc);
			RETURNFREE(row->NumA + row->Records.size() + row->NumB, RET_MINIMUM2);
		    }
		    break;
		}
	    case CLEAR:
		row->Init();
		if (RowNum == 0) {
		    FreeListRow = 1;
		    JMP(RowNum+1);
		}
		else if (RowNum < int(DRAM.size())-1) {
		    row->RowA = RowNum+1;
		    JMP(RowNum+1);
		}
		else {
		    row->RowA = -1;
		    RETURN(-100, Opcodes(0));
		}
	    case ADDVEC:
		{
		    // this could be initial addition to the root node, or first use of a leaf node
		    // it may be assumed to have been initialized
		    if (row->Pivot == -1) {
			row->Pivot = Acc[Acc.size()/2].Index;
			row->Records.swap(Acc);
			RETURN(-100, Opcodes(0));
		    }

		    // even with additional records, still fits in a row
		    else if (LT + GE <= K) {
			row->Records.swap(Acc);
			RETURN(-100, Opcodes(0));
		    }

		    // there are more of the smaller values
		    // vec { passdown values | keep values }
		    else if (LT >= GE) {
			unsigned NumToVec = max(0, min(LT, K));
			SendLow(NumToVec, row->Records, Acc);
			if (row->RowA == 0) row->RowA = RowAlloc();
			row->NumA += NumToVec;
			JMP(row->RowA);
		    }

		    // there are more of the larger values
		    // vec { keep values | passdown values }
		    else {
			unsigned NumToVec = max(0, min(GE, K));
			SendHigh(NumToVec, row->Records, Acc);
			if (row->RowB == 0)  row->RowB = RowAlloc();
			row->NumB += NumToVec;
			JMP(row->RowB);
		    }
		    break;
		}
	}
	if (row->Records.size() > K)
	    double pi=3.14;
#undef CALL
#undef JMP
#undef RETURN
    }
    return FreeMeFlag;
}

vector<vector<double> > ReadMatrixMarket(const char *fn) {
    vector<vector<double> > data;
    FILE *f = fopen(fn, "r");
    if (f == 0) return data;
    {
	int ch;
	while ((ch=getc(f)) == '%')
	    while (getc(f) != '\n') ;
	ungetc(ch, f);
    }
    int rmax = 0, cmax = 0, nzs = 0, i, rval;
    {
	for (i = 0; i < 10000; i++)
	    if ((rval = fscanf(f, "%d %d %d \n", &rmax, &cmax, &nzs)) == 3)
		break;
	if (i == 10000)
	    ASSERT(i != 10000);
    }

    data.resize(rmax);
    for (int i = 0; i < rmax; i++)
	data[i].resize(cmax);

    int nz = 0, r, c;
    double vd;
    while (fscanf(f, "%d %d %lf\n", &r, &c, &vd) == 3) {
	r--;
	c--;
	nz++;
	if (r < 0 || r >= rmax || c < 0 || c >= cmax)
	    ASSERT(r >= 0 && r < rmax && c >= 0 && c < cmax);
	data[r][c] = vd;
    }
    if (nz != nzs)
	ASSERT(nz == nzs);
    return data;
}

void __stdcall MMAccThread(ServeData *qx) {
	//Query *qx = (Query *)sd;

// Part 1: define variables, default values, and HTML variable names
// d for double, i for integer
// variable name
// default, do not surround with quotes
// HTML variable names
#if 1
#define HTMLVARS \
			D(AFile, olm500.mtx, a)			\
			D(BFile, plskz362.mtx, b)		\
			I(MemRows, 200, fon)			\
			I(K, 5, foff)				\
			I(IndexRange, 29, ioff)			\
			I(Indent, 0, vhi)			\
			I(Trace, 0, t)
#endif

#define D(a, b, c) char *a = strdup(#b); double d##a;
#define I(a, b, c) int a = b;
HTMLVARS
#undef D
#undef I

// Part 2: Parse input (no specific programmer involvement here)
	int cmd = -1;
	// process input, if any
	if (1) for (int i = 0; i < qx->argc; i++) {
		char *n = qx->name[i];
		char *v = qx->value[i];
		if (0) ;
#define D(a, b, c) else if (strcmp(n, #c) == 0) qx->SS(a, v), qx->SD(d##a, v);
#define I(a, b, c) else if (strcmp(n, #c) == 0) qx->SI(a, v);
HTMLVARS
#undef D
#undef I
		else if (strcmp(n, "cmd") == 0)
			cmd = 0;
	}

	// print the header
	FILE *index = qx->TitleTextOpenBufferFile("Status Page", "test", "htm", 0, NULL);
#if FANCYPAGE != 0
	FancyPageBegin(index, *qx);
#else
	fprintf(Index, "Fancy Page Stub<br>\n");
#endif

// Part 3: Do entity encoding as needed (no specific programmer involvement here)
#define D(a, b, c) char *encoded_##a = EntityEncode(a);
#define I(a, b, c)
HTMLVARS
#undef D
#undef I

// Part 4: Output the HTML form
// For reals, hard code the HTML variable name and append encoded_ in front of the corresponding program variable and use %s
// For ints, hard code the HTML varibale name but just use the program variable and use %d
	const char *NOut1 = "<font face=arial size=-2>";
	const char *NOut2 = "</font>";
	fprintf(index, "%s<form method=get action=SuperStrider><br><table border>", NOut1);
	fprintf(index, "<tr><th colspan=4>Test Parameters</th></tr>"); 
	fprintf(index, "<tr><td>A Matrix</td><td><input type=text size=10 name=a Value=\"%s\">(filename)</td></tr>", AFile); 
	fprintf(index, "<tr><td>B Matrix</td><td><input type=text size=10 name=b Value=\"%s\">(filename)</td></tr>", BFile); 
	fprintf(index, "<tr><td>Maximum</td><td><input type=text size=10 name=fon Value=\"%d\">rows/cols</td></tr>", MemRows); 
	fprintf(index, "<tr><td>Memory width (K)</td><td><input type=text size=10 name=foff Value=\"%d\">records</td></tr>", K); 
	fprintf(index, "<tr><td>Index range</td><td><input type=text size=10 name=ioff Value=\"%d\"></td></tr>", IndexRange); 
	fprintf(index, "<tr><td>Indent</td><td><input type=text size=10 name=vhi Value=\"%d\">columns</td></tr>", Indent); 
	fprintf(index, "<tr><td>Trace Addvec</td><td><input type=text size=10 name=t Value=\"%d\">0=no 1=yes</td></tr>", Trace); 
	fprintf(index, "<tr><td colspan=2><input type=submit name=cmd Value=Run></td></tr>");
	fprintf(index, "</table></form>%s\n", NOut2); 

// Part 5: Free temporaries
#define D(a, b, c) free(a); free(encoded_##a);
#define I(a, b, c)
HTMLVARS
#undef D
#undef I
#undef HTMLVARS

//#define GRAPHPOINTS 400
//#define GRAPHHEIGHT 200
//extern char *GifImageStoreAdd(int x, int y, int ***image, int numimage = 1);
	if (cmd == 0) {
	    HTMLGraph::eDataMark DataPointMode = HTMLGraph::Square;
	    int Color[NUMPLOTS] = {
		    255<<16 | 0<<8 | 0,	// Air (red)
		    0<<16 | 255<<8 | 0,	// Water (green)
		    0<<16 | 0<<8 | 255,	// Fractal (blue)
		    0<<16 | 0<<8 | 0,	// Pulse (black)
		    255<<16 | 0<<8 | 255,	// DIMM (purple)
		    37<<16 | 40<<8 | 103,	// Sandia dark blue
		    165<<16 | 36<<8 | 46,	// Sandia dark red
		    0<<16 | 0<<8 | 255,	// Blue
		    200<<16 | 255<<8 | 200,	// Pale Green
	    };
	    mysrand(123);
	    {
		int capacity = 600;
		S.DRAM.clear();
		S.DRAM.resize(capacity);
		S.StateMachine(CLEAR, 0, K, index);
	    }
	    S.CycleCount = 0;
	    map<int, double> GroundTruth;
#if MATMARKET
	    vector<vector<double> > A, B, C;
	    A = ReadMatrixMarket("olm500.mtx");
	    B = ReadMatrixMarket("plskz362.mtx");
	    int m = A.size();
	    int acols = min(A[0].size(), B.size());
	    int bcols = B[0].size();

	    int limsz = MemRows;
	    acols = min(acols, limsz);
	    bcols = min(acols, limsz);
	    m = min(m, limsz);
	    C.resize(m);
	    for (int i = 0; i < m; i++)
		C[i].resize(bcols);

	    for (int i = 0; i < m; i++)
		for (int j = 0; j < acols; j++)
		    for (int k = 0; k < bcols; k++)
			if (A[i][j] != 0. && B[j][k] != 0.) {
			    Record r;
			    int rt = myrand();
			    r.Index = i*bcols + k;//i + k*m;
			    r.Index ^= 0x55aa;
			    r.Value = A[i][j] * B[j][k];
			    GroundTruth[r.Index] = GroundTruth.find(r.Index) == GroundTruth.end() ? r.Value : r.Value + GroundTruth[r.Index];
			    S.Acc.push_back(r);
			    if (S.Acc.size() == K) {
				sort(S.Acc.begin(), S.Acc.end(), RecordSort1);
				vector<Record> A = S.Acc;
				S.StateMachine(ADDVEC, 0, K, index);
				(*Plotfcn)(&PlotData[0], S.Efficiency(K, GroundTruth), S.CycleCount/*i*/, Real);	// vertical then horizontal
				if (Trace) S.Print("Addvec", &A, Indent, Trace, K, GroundTruth, qx, index);
				S.Acc.clear();
			    }
			    C[i][k] += A[i][j] * B[j][k];
			}

	    if (S.Acc.size() > 0) {
		sort(S.Acc.begin(), S.Acc.end(), RecordSort1);
		vector<Record> A = S.Acc;
		S.StateMachine(ADDVEC, 0, K, index);
		(*Plotfcn)(&PlotData[0], S.Efficiency(K, GroundTruth), S.CycleCount/*i*/, Real);	// vertical then horizontal
		if (Trace) S.Print("Addvec", &A, Indent, Trace, K, GroundTruth, qx, index);
		S.Acc.clear();
	    }
#else
	    for (int i = 0; i < MemRows; i++) {

		// create a new random vector that will be added to the tree
		int limit = i == 6 ? K-1 : K;
		for (int j = 0; j < limit; j++) {
		    Record r;
		    int rt = myrand();
		    r.Index = (rt&0x7fffffff) % IndexRange;
		    r.Value = 10.*myrand()/MYRANDMAX;
		    GroundTruth[r.Index] = GroundTruth.find(r.Index) == GroundTruth.end() ? r.Value : r.Value + GroundTruth[r.Index];
		    S.Acc.push_back(r);
		    sort(S.Acc.begin(), S.Acc.end(), RecordSort1);
		}
		vector<Record> A = S.Acc;
		S.StateMachine(ADDVEC, 0, K, index);
		(*Plotfcn)(&PlotData[0], S.Efficiency(K, GroundTruth), S.CycleCount/*i*/, Real);	// vertical then horizontal
		if (Trace) S.Print("Addvec", &A, Indent, Trace, K, GroundTruth, qx, index);
	    }
#endif
	    const char *NOut1 = "<font face=arial size=-2>";
	    const char *NOut2 = "</font>";

	    int repcount = 3;
	    for (int i = 0; i < repcount; i++) {
		S.Print("Results", 0, Indent, Trace, K, GroundTruth, qx, index);

		int treesize = 0;
		if (i < repcount-1) {
		    S.StateMachine(NORMALIZE, 0, K, index);
		    (*Plotfcn)(&PlotData[0], S.Efficiency(K, GroundTruth), S.CycleCount/*i+MemRows*/, Real);	// vertical then horizontal
		}
	    }

	    char buf[250];
	    sprintf(buf, "Memory efficiency");
	    int bgcolor = RGB(255, 255, 255);
	    // note: the following graph has caused difficulties in OS X when run in a thread
	    HTMLGraph_Print(index, buf, NUMPLOTS, 0, 1, 0, 0, &PlotData[0], Color, bgcolor,
			    DataPointMode,		// 1 for marks at the data points (NoMark or Square)
			    HTMLGraph::SpecMinX, 0.,
			    HTMLGraph::CalcMaxX, 1.,
			    HTMLGraph::SpecMinY, 0.,
			    HTMLGraph::CalcMaxY, 1.);	// Fix x axis to 0..any value
	    if (1) for (int i = 0; i < NUMPLOTS; i++)
		    PlotData[i].Clear();
	}
#undef GRAPHPOINTS
#undef GRAPHHEIGHT

#if FANCYPAGE != 0
    FancyPageEnd(index, *qx);
#else
    fprintf(Index, "</body></html>\n");
#endif
    qx->CloseBufferFile();

    delete qx;
}
#undef TESTALLOC
#undef TREENORMAL
#undef TREEREVERSE
