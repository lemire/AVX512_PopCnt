// AVX512_PopCnt.cpp : Defines the entry point for the console application.
//original ideas: https://arxiv.org/pdf/1611.07612.pdf
//AVX2 Harley-Seal: https://github.com/WojciechMula/sse-popcount/blob/master/popcnt-avx2-harley-seal.cpp
//AVX512 Harley-Seal: https://github.com/WojciechMula/sse-popcount/blob/master/popcnt-avx512-harley-seal.cpp

#include "stdafx.h"

#define ISA_RDTSC		0x001
#define ISA_RDTSCP		0x002
#define ISA_HWPOPCNT	0x004
#define ISA_RDRAND		0x008
#define ISA_AVX2		0x010
#define ISA_AVX512F		0x020
#define ISA_AVX512BW	0x040
#define ISA_AVX512VL	0x080
#define ISA_AVX512VBMI	0x100
#define ISA_VPOPCNT		0x200
#define ISA_BITALG		0x400

#define BUFSIZE			(256 * 1024 * 1024)
#define BUFSIZE_TEST	(8 * 1024 * 1024)
#define RETRY			10

#define NUM_METHODS		8
#define NUM_SIZES		120
#define NUM_RESULTS		4

using namespace std;
extern "C" UINT32 __fastcall CheckISA();
extern "C" UINT64 __fastcall PopCntNHM(UINT64 *, UINT64);
extern "C" UINT64 __fastcall PopCntNHM_Timed(UINT64 *, UINT64);
extern "C" UINT64 __fastcall VPopCntHSW(UINT64 *, UINT64);
extern "C" UINT64 __fastcall VPopCntHSW_Timed(UINT64 *, UINT64);
extern "C" UINT64 __fastcall VPopCntKNL(UINT64 *, UINT64);
extern "C" UINT64 __fastcall VPopCntKNL_Timed(UINT64 *, UINT64);
extern "C" UINT64 __fastcall VPopCntSKX(UINT64 *, UINT64);
extern "C" UINT64 __fastcall VPopCntSKX_Timed(UINT64 *, UINT64);
extern "C" UINT64 __fastcall VPopCntCNL(UINT64 *, UINT64);
extern "C" UINT64 __fastcall VPopCntCNL_Timed(UINT64 *, UINT64);
extern "C" UINT64 __fastcall VPopCntKNM(UINT64 *, UINT64);
extern "C" UINT64 __fastcall VPopCntKNM_Timed(UINT64 *, UINT64);
extern "C" UINT64 __fastcall VPopCntICL(UINT64 *, UINT64);
extern "C" UINT64 __fastcall VPopCntICL_Timed(UINT64 *, UINT64);

extern "C" UINT64 __fastcall VPopCntSKX_YMM(UINT64 *, UINT64);
extern "C" UINT64 __fastcall VPopCntSKX_YMM_Timed(UINT64 *, UINT64);

typedef UINT64 (__fastcall *TEST_PTR)(UINT64 *, UINT64);

typedef enum {
	AVX512_POPCNT_TEST_ZERO,
	AVX512_POPCNT_TEST_FULL,
	AVX512_POPCNT_TEST_RAND,
} testType;

typedef enum {
	AVX512_POPCNT_RES_TSC,
	AVX512_POPCNT_RES_BPC,
	AVX512_POPCNT_RES_BPCC,
	AVX512_POPCNT_RES_CPQ,
} resultType;

typedef struct {
	const char 	name[14];
	const char 	isaName[14];
	TEST_PTR	check;
	TEST_PTR	timed;
	UINT32		isa;
} methods;

methods m[] = {
	{"HWPopCnt     ",	"HWPopCnt     ", PopCntNHM,			PopCntNHM_Timed,		ISA_HWPOPCNT },
	{"VPopCntHSW   ",	"AVX2         ", VPopCntHSW,		VPopCntHSW_Timed,		ISA_AVX2 },
	{"VPopCntSKXYMM",	"AVX512VLBW   ", VPopCntSKX_YMM,	VPopCntSKX_YMM_Timed,	ISA_AVX512BW | ISA_AVX512VL },
	{"VPopCntKNL   ",	"AVX512F      ", VPopCntKNL,		VPopCntKNL_Timed,		ISA_AVX512F },
	{"VPopCntSKX   ",	"AVX512BW     ", VPopCntSKX,		VPopCntSKX_Timed,		ISA_AVX512BW },
	{"VPopCntCNL   ",	"AVX512VBMI   ", VPopCntCNL,		VPopCntCNL_Timed,		ISA_AVX512VBMI },
	{"VPopCntKNM   ",	"VPOPCNTDQ    ", VPopCntKNM,		VPopCntKNM_Timed,		ISA_VPOPCNT },
	{"VPopCntICL   ",	"BITALG       ", VPopCntICL,		VPopCntICL_Timed,		ISA_VPOPCNT | ISA_BITALG },
};

UINT64 test(UINT64 *buf, UINT64 s, TEST_PTR p) {
	UINT64 res, min1 = UINT64_MAX;
	for (int retry = 0; retry < RETRY; retry++) {
		res = (p)(buf, s);
		min1 = min(min1, res);
	}
	return min1;
}

void printRes(resultType type, UINT64 bestR, UINT64 size, UINT64 tsc, UINT64 corr, const char c[14], bool b) {
	if (b) {
		switch(type) {
			default:
			case AVX512_POPCNT_RES_TSC:
				cout << std::setw(11) << std::setprecision(3) << fixed << (double)(tsc) / 1000.0;									//in 1000 TSC clocks
				break;
			case AVX512_POPCNT_RES_BPC:
				cout << std::setw(11) << std::setprecision(3) << fixed << (double)(size) / (double)(max(tsc, 1));					//BytesPerClock
				break;
			case AVX512_POPCNT_RES_BPCC:
				cout << std::setw(11) << std::setprecision(3) << fixed << (double)(size) / (double)(max(tsc - corr, 1));			//BytesPerClock corrected
				break;
			case AVX512_POPCNT_RES_CPQ:
				cout << std::setw(11) << std::setprecision(3) << fixed << ((double)(tsc - corr) / (double)(size / sizeof(UINT64)));	//TSC per qword
				break;
		}
		cout << ((bestR == tsc) ? " *|" : "  |");
	} else {
		cout << " No_" << c << "|";
	}
}

void printSize(UINT64 s) {
	cout << std::setw(7) << right << ((s >= 4096) ? ((s >= 4096 * 1024) ? (s >> 20) : (s >> 10)) : s) << ((s >= 4096) ? ((s >= 4096 * 1024) ? " MiB" : " KiB") : " B  ") << "|";
}

void method(const char c[14], bool b) {
	if (b) {
		cout << c << "|";
	} else {
		cout << " No_" << c << "|";
	}
}

void test0print(UINT64 *buf, TEST_PTR p, const char c[14], UINT64 res, bool b) {
	if (b) {
		cout << std::setw(13) << res << "|";
	} else {
		cout << " No_" << c << "|";
	}
}

UINT64 test0wrap(UINT64 *buf, TEST_PTR p, const char c[14], bool b) {
	UINT64 res = UINT64_MAX;
	if (b) {
		res = test(buf, 0ULL, p);
		cout << std::setw(13) << res << "|";
	} else {
		cout << " No_" << c << "|";
	}
	return res;
}

UINT64 testwrap(UINT64 *buf, UINT64 s, TEST_PTR p, bool b) {
	UINT64 res = UINT64_MAX;
	if (b) {
		res = test(buf, s, p);
	}
	return res;
}

DWORD nextStep(UINT64 size) {
	DWORD step = 64;
	switch (size) {
		default:
			_BitScanReverse64(&step, size);
			step = 1ULL << (step - 1);
			break;
		case 1:
			step = 1; break;
		case 2:
			step = 2; break;
		case 4:
			step = 4; break;
	}
	return step;
}

void perf(UINT64 *buf, UINT32 ISA) {
	FillMemory(buf, BUFSIZE, 0xff);
	UINT64 results[NUM_METHODS][NUM_SIZES];
	UINT64 bestRes[NUM_SIZES];
	memset(results, 0, sizeof(UINT64) * NUM_METHODS * NUM_SIZES);
	memset(bestRes, 0xff, sizeof(UINT64) * NUM_SIZES);

	cout << endl;
	for (int t = 0; t < NUM_RESULTS; t++) {
		switch (t) {
			case AVX512_POPCNT_RES_TSC:
				cout << "  === Performance Test (total runtime in 1000 TSC clocks) === "; break;
			case AVX512_POPCNT_RES_BPC:
				cout << "  === Performance Test (in bytes per TSC clks) === "; break;
			case AVX512_POPCNT_RES_BPCC:
				cout << "  === Performance Test (in bytes per corrected TSC clks) === "; break;
			case AVX512_POPCNT_RES_CPQ:
				cout << "  === Performance Test (TSC clks per processed qwords) === "; break;
		}
		cout << endl << "   Method: |";
		for (int b = 0; b < NUM_METHODS; b++)
			method(m[b].name, (ISA & m[b].isa) == m[b].isa);

		cout << endl << "   RDTSCP  |";
		for (int b = 0; b < NUM_METHODS; b++)
			if (t == AVX512_POPCNT_RES_TSC)
				results[b][0] = test0wrap(buf, m[b].timed, m[b].isaName, (ISA & m[b].isa) == m[b].isa);
			else 
				test0print(buf, m[b].timed, m[b].isaName, results[b][0], (ISA & m[b].isa) == m[b].isa);

		cout << endl;

		for (SIZE_T s = 64, i = 1; (s <= BUFSIZE) && (i < NUM_SIZES); s += nextStep(s), i++) {
			printSize(s);
			if (t == AVX512_POPCNT_RES_TSC) {
				bestRes[i] = UINT64_MAX;
				for (int b = 0; b < NUM_METHODS; b++)
					results[b][i] = testwrap(buf, s, m[b].timed, (ISA & m[b].isa) == m[b].isa);
				for (int b = 0; b < NUM_METHODS; b++)
					bestRes[i] = min(bestRes[i], results[b][i]);
			}
			for (int b = 0; b < NUM_METHODS; b++)
				printRes((resultType)t, bestRes[i], s, results[b][i], results[b][0], m[b].isaName, (ISA & m[b].isa) == m[b].isa);
			cout << /*" " << bestRes[i] <<*/ endl;
		}
	}
}

void fillmem(UINT64 *buf, UINT64 s, testType test) {
	for (SIZE_T si = 0; si < s / sizeof(UINT64); si += 4) {
		switch (test) {
			default:
			case AVX512_POPCNT_TEST_ZERO:
				buf[si + 0] = 0ULL;
				buf[si + 1] = 0ULL;
				buf[si + 2] = 0ULL;
				buf[si + 3] = 0ULL;
				break;
			case AVX512_POPCNT_TEST_FULL:
				buf[si + 0] = ~0ULL;
				buf[si + 1] = ~0ULL;
				buf[si + 2] = ~0ULL;
				buf[si + 3] = ~0ULL;
				break;
			case AVX512_POPCNT_TEST_RAND:
				_rdrand64_step(&buf[si + 0]);
				_rdrand64_step(&buf[si + 1]);
				_rdrand64_step(&buf[si + 2]);
				_rdrand64_step(&buf[si + 3]);
				break;
		}
	}
}

void checkwrap(UINT64 *buf, UINT64 s, UINT64 target, TEST_PTR p, const char name[14], const char isa[14], bool b) {
	if (b) {
		cout << name << std::setw(6) << right << ((target == (p)(buf, s)) ? ":OK   |" : ":ERROR|");
	}
	else {
		cout << " No_" << isa << "|";
	}
}

void check(UINT64 *buf, UINT32 ISA, testType test) {
	switch (test) {
	default:
	case AVX512_POPCNT_TEST_ZERO:
		cout << "  === Zero Test === " << endl; break;
	case AVX512_POPCNT_TEST_FULL:
		cout << "  === Full Test === " << endl; break;
	case AVX512_POPCNT_TEST_RAND:
		cout << "  === Random Test === " << endl; break;
	}
	fillmem(buf, BUFSIZE_TEST, test);
	SIZE_T step = 1;
	for (SIZE_T s = 1; s <= BUFSIZE_TEST; s += step) {
		step *= 2;
		printSize(s);
		UINT64 target = ~0ULL;
		switch (test) {
			default:
			case AVX512_POPCNT_TEST_ZERO:
				target = 0ULL; break;
			case AVX512_POPCNT_TEST_FULL:
				target = 8 * s; break;
			case AVX512_POPCNT_TEST_RAND:
				target = PopCntNHM(buf, s); break;
		}
		for (int b = 0; b < NUM_METHODS; b++)
			checkwrap(buf, s, target, m[b].check, m[b].name, m[b].isaName, (ISA & m[b].isa) == m[b].isa);
		cout << endl;
	}
}

int main()
{
	SIZE_T minWork, maxWork;
	GetProcessWorkingSetSize(GetCurrentProcess(), &minWork, &maxWork);
	SetProcessWorkingSetSize(GetCurrentProcess(), minWork + BUFSIZE, maxWork + BUFSIZE);
	SetProcessAffinityMask(GetCurrentProcess(), 8);
	SetThreadAffinityMask(GetCurrentThread(), 8);
	Sleep(0);
	UINT64 *buf = (UINT64 *)VirtualAlloc(NULL, BUFSIZE, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
	if (buf != nullptr) {
		VirtualLock(buf, BUFSIZE);
		UINT32 ISA = CheckISA();
		if ((ISA & ISA_RDTSCP) != 0) {
			check(buf, ISA, AVX512_POPCNT_TEST_ZERO);
			check(buf, ISA, AVX512_POPCNT_TEST_FULL);
			if ((ISA & ISA_RDRAND) != 0)
				check(buf, ISA, AVX512_POPCNT_TEST_RAND);
			perf(buf, ISA);
		}
		VirtualUnlock(buf, BUFSIZE);
		VirtualFree(buf, 0, MEM_RELEASE);
	}
	return 0;
}

