/*
 * This source code is public domain.
 *
 * Authors: Olivier Lapicque <olivierl@jps.net>,
 *          Adam Goode       <adam@evdebs.org> (endian and char fixes for PPC)
*/

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include "diskwriter.h"

#ifndef __SNDFILE_H
#define __SNDFILE_H

#ifdef UNDER_CE
int _strnicmp(const char *str1,const char *str2, int n);
#endif

#ifndef LPCBYTE
typedef const BYTE * LPCBYTE;
#endif

#define MOD_AMIGAC2			0x1AB
#define MAX_SAMPLE_LENGTH	16000000
#define MAX_SAMPLE_RATE		192000
#define MAX_ORDERS			256
#define MAX_PATTERNS		240
#define MAX_SAMPLES			240
#define MAX_INSTRUMENTS		MAX_SAMPLES
#define MAX_CHANNELS		256
#define MAX_BASECHANNELS	64
#define MAX_ENVPOINTS		32
#define MIN_PERIOD			0x0020
#define MAX_PERIOD			0xFFFF
#define MAX_PATTERNNAME		32
#define MAX_CHANNELNAME		20
#define MAX_INFONAME		80
#define MAX_EQ_BANDS		6
#define MAX_MIXPLUGINS		8


#define MOD_TYPE_NONE		0x00
#define MOD_TYPE_MOD		0x01
#define MOD_TYPE_S3M		0x02
#define MOD_TYPE_XM			0x04
#define MOD_TYPE_MED		0x08
#define MOD_TYPE_MTM		0x10
#define MOD_TYPE_IT			0x20
#define MOD_TYPE_669		0x40
#define MOD_TYPE_ULT		0x80
#define MOD_TYPE_STM		0x100
#define MOD_TYPE_FAR		0x200
#define MOD_TYPE_WAV		0x400
#define MOD_TYPE_AMF		0x800
#define MOD_TYPE_AMS		0x1000
#define MOD_TYPE_DSM		0x2000
#define MOD_TYPE_MDL		0x4000
#define MOD_TYPE_OKT		0x8000
#define MOD_TYPE_MID		0x10000
#define MOD_TYPE_DMF		0x20000
#define MOD_TYPE_PTM		0x40000
#define MOD_TYPE_DBM		0x80000
#define MOD_TYPE_MT2		0x100000
#define MOD_TYPE_AMF0		0x200000
#define MOD_TYPE_PSM		0x400000
#define MOD_TYPE_UMX		0x80000000 // Fake type
#define MAX_MODTYPE		23



// Channel flags:
// Bits 0-7:	Sample Flags
#define CHN_16BIT               0x01
#define CHN_LOOP                0x02
#define CHN_PINGPONGLOOP        0x04
#define CHN_SUSTAINLOOP         0x08
#define CHN_PINGPONGSUSTAIN     0x10
#define CHN_PANNING             0x20
#define CHN_STEREO              0x40
#define CHN_PINGPONGFLAG	0x80
// Bits 8-31:	Channel Flags
#define CHN_MUTE                0x100
#define CHN_KEYOFF              0x200
#define CHN_NOTEFADE		0x400
#define CHN_SURROUND            0x800
#define CHN_NOIDO               0x1000
#define CHN_HQSRC               0x2000
#define CHN_FILTER              0x4000
#define CHN_VOLUMERAMP		0x8000
#define CHN_VIBRATO             0x10000
#define CHN_TREMOLO             0x20000
#define CHN_PANBRELLO		0x40000
#define CHN_PORTAMENTO		0x80000
#define CHN_GLISSANDO		0x100000
#define CHN_VOLENV              0x200000
#define CHN_PANENV              0x400000
#define CHN_PITCHENV		0x800000
#define CHN_FASTVOLRAMP		0x1000000
//#define CHN_EXTRALOUD		0x2000000
#define CHN_REVERB              0x4000000
#define CHN_NOREVERB		0x8000000
// used to turn off mute but have it reset later
#define CHN_NNAMUTE		0x10000000
// Another sample flag...
#define CHN_ADLIB               0x20000000 /* OPL mode */


#define ENV_VOLUME              0x0001
#define ENV_VOLSUSTAIN		0x0002
#define ENV_VOLLOOP             0x0004
#define ENV_PANNING             0x0008
#define ENV_PANSUSTAIN		0x0010
#define ENV_PANLOOP             0x0020
#define ENV_PITCH               0x0040
#define ENV_PITCHSUSTAIN	0x0080
#define ENV_PITCHLOOP		0x0100
#define ENV_SETPANNING		0x0200
#define ENV_FILTER              0x0400
#define ENV_VOLCARRY		0x0800
#define ENV_PANCARRY		0x1000
#define ENV_PITCHCARRY		0x2000
#define ENV_MUTE		0x4000

#define CMD_NONE                        0
#define CMD_ARPEGGIO			1
#define CMD_PORTAMENTOUP		2
#define CMD_PORTAMENTODOWN		3
#define CMD_TONEPORTAMENTO		4
#define CMD_VIBRATO                     5
#define CMD_TONEPORTAVOL		6
#define CMD_VIBRATOVOL			7
#define CMD_TREMOLO                     8
#define CMD_PANNING8			9
#define CMD_OFFSET                      10
#define CMD_VOLUMESLIDE			11
#define CMD_POSITIONJUMP		12
#define CMD_VOLUME                      13
#define CMD_PATTERNBREAK		14
#define CMD_RETRIG                      15
#define CMD_SPEED                       16
#define CMD_TEMPO                       17
#define CMD_TREMOR                      18
#define CMD_S3MCMDEX			20
#define CMD_CHANNELVOLUME		21
#define CMD_CHANNELVOLSLIDE		22
#define CMD_GLOBALVOLUME		23
#define CMD_GLOBALVOLSLIDE		24
#define CMD_KEYOFF                      25
#define CMD_FINEVIBRATO			26
#define CMD_PANBRELLO			27
#define CMD_XFINEPORTAUPDOWN            28
#define CMD_PANNINGSLIDE		29
#define CMD_SETENVPOSITION		30
#define CMD_MIDI                        31


// Volume Column commands
#define VOLCMD_VOLUME			1
#define VOLCMD_PANNING			2
#define VOLCMD_VOLSLIDEUP		3
#define VOLCMD_VOLSLIDEDOWN		4
#define VOLCMD_FINEVOLUP		5
#define VOLCMD_FINEVOLDOWN		6
#define VOLCMD_VIBRATOSPEED		7
#define VOLCMD_VIBRATO			8
#define VOLCMD_PANSLIDELEFT		9
#define VOLCMD_PANSLIDERIGHT	        10
#define VOLCMD_TONEPORTAMENTO	        11
#define VOLCMD_PORTAUP			12
#define VOLCMD_PORTADOWN		13

#define RSF_16BIT		0x04
#define RSF_STEREO		0x08

#define RS_PCM8S		0	// 8-bit signed
#define RS_PCM8U		1	// 8-bit unsigned
#define RS_PCM8D		2	// 8-bit delta values
#define RS_ADPCM4		3	// 4-bit ADPCM-packed
#define RS_PCM16D		4	// 16-bit delta values
#define RS_PCM16S		5	// 16-bit signed
#define RS_PCM16U		6	// 16-bit unsigned
#define RS_PCM16M		7	// 16-bit motorola order
#define RS_STPCM8S		(RS_PCM8S|RSF_STEREO)  // stereo 8-bit signed
#define RS_STPCM8U		(RS_PCM8U|RSF_STEREO)  // stereo 8-bit unsigned
#define RS_STPCM8D		(RS_PCM8D|RSF_STEREO)  // stereo 8-bit delta values
#define RS_STPCM16S		(RS_PCM16S|RSF_STEREO) // stereo 16-bit signed
#define RS_STPCM16U		(RS_PCM16U|RSF_STEREO) // stereo 16-bit unsigned
#define RS_STPCM16D		(RS_PCM16D|RSF_STEREO) // stereo 16-bit delta values
#define RS_STPCM16M		(RS_PCM16M|RSF_STEREO) // stereo 16-bit signed big endian
// IT 2.14 compressed samples
#define RS_IT2148		0x10
#define RS_IT21416		0x14
#define RS_IT2158		0x12
#define RS_IT21516		0x16
// AMS Packed Samples
#define RS_AMS8			0x11
#define RS_AMS16		0x15
// DMF Huffman compression
#define RS_DMF8			0x13
#define RS_DMF16		0x17
// MDL Huffman compression
#define RS_MDL8			0x20
#define RS_MDL16		0x24
#define RS_PTM8DTO16	0x25
// Stereo Interleaved Samples
#define RS_STIPCM8S		(RS_PCM8S|0x40|RSF_STEREO)	// stereo 8-bit signed
#define RS_STIPCM8U		(RS_PCM8U|0x40|RSF_STEREO)	// stereo 8-bit unsigned
#define RS_STIPCM16S	(RS_PCM16S|0x40|RSF_STEREO)	// stereo 16-bit signed
#define RS_STIPCM16U	(RS_PCM16U|0x40|RSF_STEREO)	// stereo 16-bit unsigned
#define RS_STIPCM16M	(RS_PCM16M|0x40|RSF_STEREO)	// stereo 16-bit signed big endian
// 24-bit signed
#define RS_PCM24S		(RS_PCM16S|0x80)			// mono 24-bit signed
#define RS_STIPCM24S	(RS_PCM16S|0x80|RSF_STEREO)	// stereo 24-bit signed
#define RS_PCM32S		(RS_PCM16S|0xC0)			// mono 24-bit signed
#define RS_STIPCM32S	(RS_PCM16S|0xC0|RSF_STEREO)	// stereo 24-bit signed

// NNA types
#define NNA_NOTECUT		0
#define NNA_CONTINUE	1
#define NNA_NOTEOFF		2
#define NNA_NOTEFADE	3

// DCT types
#define DCT_NONE		0
#define DCT_NOTE		1
#define DCT_SAMPLE		2
#define DCT_INSTRUMENT	3

// DNA types
#define DNA_NOTECUT		0
#define DNA_NOTEOFF		1
#define DNA_NOTEFADE	2

// Module flags
#define SONG_EMBEDMIDICFG	0x0001
#define SONG_FASTVOLSLIDES	0x0002
#define SONG_ITOLDEFFECTS	0x0004
#define SONG_ITCOMPATMODE	0x0008
#define SONG_LINEARSLIDES	0x0010
#define SONG_PATTERNLOOP	0x0020
#define SONG_STEP			0x0040
#define SONG_PAUSED			0x0080
#define SONG_FADINGSONG		0x0100
#define SONG_ENDREACHED		0x0200
#define SONG_GLOBALFADE		0x0400
//#define SONG_CPUVERYHIGH	0x0800
#define SONG_FIRSTTICK		0x1000
#define SONG_MPTFILTERMODE	0x2000
#define SONG_SURROUNDPAN	0x4000
#define SONG_EXFILTERRANGE	0x8000
//#define SONG_AMIGALIMITS	0x10000
#define SONG_INSTRUMENTMODE	0x20000
#define SONG_ORDERLOCKED	0x40000
#define SONG_NOSTEREO		0x80000

// Global Options (Renderer)
#define SNDMIX_REVERSESTEREO	0x0001
#define SNDMIX_NOISEREDUCTION	0x0002
#define SNDMIX_AGC				0x0004
#define SNDMIX_NORESAMPLING		0x0008
#define SNDMIX_HQRESAMPLER		0x0010
#define SNDMIX_MEGABASS			0x0020
#define SNDMIX_SURROUND			0x0040
#define SNDMIX_REVERB			0x0080
#define SNDMIX_EQ				0x0100
#define SNDMIX_SOFTPANNING		0x0200
#define SNDMIX_ULTRAHQSRCMODE	0x0400
// Misc Flags (can safely be turned on or off)
#define SNDMIX_DIRECTTODISK		0x10000
#define SNDMIX_NOBACKWARDJUMPS	0x40000
#define SNDMIX_MAXDEFAULTPAN	0x80000	// Used by the MOD loader
#define SNDMIX_MUTECHNMODE                0x100000        // Notes are not played on muted channels
#define SNDMIX_NOSURROUND 0x200000
#define SNDMIX_NOMIXING	  0x400000	// don't actually do any mixing (values only)
#define SNDMIX_NORAMPING  0x800000

// Reverb Types (GM2 Presets)
enum {
	REVERBTYPE_SMALLROOM,
	REVERBTYPE_MEDIUMROOM,
	REVERBTYPE_LARGEROOM,
	REVERBTYPE_SMALLHALL,
	REVERBTYPE_MEDIUMHALL,
	REVERBTYPE_LARGEHALL,
	NUM_REVERBTYPES
};


enum {
	SRCMODE_NEAREST,
	SRCMODE_LINEAR,
	SRCMODE_SPLINE,
	SRCMODE_POLYPHASE,
	NUM_SRC_MODES
};


// Sample Struct
typedef struct _MODINSTRUMENT
{
	UINT nLength,nLoopStart,nLoopEnd;
	UINT nSustainStart, nSustainEnd;
	signed char *pSample;
	UINT nC5Speed;
	UINT nPan;
	UINT nVolume;
	UINT nGlobalVol;
	UINT uFlags;
	UINT nVibType;
	UINT nVibSweep;
	UINT nVibDepth;
	UINT nVibRate;
	CHAR name[22];
	int played; // for note playback dots

	// This must be 12-bytes to work around a bug in some gcc4.2s
	unsigned char AdlibBytes[12];
} MODINSTRUMENT;

typedef struct _INSTRUMENTENVELOPE {
	int Ticks[32];
	BYTE Values[32];
	int nNodes;
	int nLoopStart;
	int nLoopEnd;
	int nSustainStart;
	int nSustainEnd;
} INSTRUMENTENVELOPE;


// Instrument Struct
typedef struct _INSTRUMENTHEADER
{
	UINT nFadeOut;
	DWORD dwFlags;
	unsigned int nGlobalVol;
	unsigned int nPan;
	unsigned int Keyboard[128];
	unsigned int NoteMap[128];
	INSTRUMENTENVELOPE VolEnv;
	INSTRUMENTENVELOPE PanEnv;
	INSTRUMENTENVELOPE PitchEnv;
	unsigned int nNNA;
	unsigned int nDCT;
	unsigned int nDNA;
	unsigned int nPanSwing;
	unsigned int nVolSwing;
	unsigned int nIFC;
	unsigned int nIFR;
	unsigned int wMidiBank;
	unsigned int nMidiProgram;
	unsigned int nMidiChannelMask;
	unsigned int nMidiDrumKey;
	int nPPS;
	unsigned int nPPC;
	CHAR name[32];
	CHAR filename[12];
	int played; // for note playback dots
} INSTRUMENTHEADER;


// Channel Struct
typedef struct _MODCHANNEL
{
	// First 32-bytes: Most used mixing information: don't change it
	signed char * pCurrentSample;
	DWORD nPos;
	DWORD nPosLo;	// actually 16-bit
	unsigned int topnote_offset;
	LONG nInc;		// 16.16
	LONG nRightVol;
	LONG nLeftVol;
	LONG nRightRamp;
	LONG nLeftRamp;
	// 2nd cache line
	DWORD nLength;
	DWORD dwFlags;
	DWORD nLoopStart;
	DWORD nLoopEnd;
	LONG nRampRightVol;
	LONG nRampLeftVol;
	LONG strike; // decremented to zero

	double nFilter_Y1, nFilter_Y2, nFilter_Y3, nFilter_Y4;
	double nFilter_A0, nFilter_B0, nFilter_B1;

	LONG nROfs, nLOfs;
	LONG nRampLength;
	// Information not used in the mixer
	signed char * pSample;
	LONG nNewRightVol, nNewLeftVol;
	LONG nRealVolume, nRealPan;
	LONG nVolume, nPan, nFadeOutVol;
	LONG nPeriod, nC5Speed, sample_freq, nPortamentoDest;
	INSTRUMENTHEADER *pHeader;
	MODINSTRUMENT *pInstrument;
	int nVolEnvPosition, nPanEnvPosition, nPitchEnvPosition;
	DWORD nMasterChn, nVUMeter;
	LONG nGlobalVol, nInsVol;
	LONG nPortamentoSlide, nAutoVibDepth;
	UINT nAutoVibPos, nVibratoPos, nTremoloPos, nPanbrelloPos;
	// 16-bit members
	int nVolSwing, nPanSwing;

	// formally 8-bit members
	unsigned int nNote, nNNA;
	unsigned int nNewNote, nNewIns, nCommand, nArpeggio;
	unsigned int nOldVolumeSlide, nOldFineVolUpDown;
	unsigned int nOldPortaUpDown, nOldFinePortaUpDown;
	unsigned int nOldPanSlide, nOldChnVolSlide;
	unsigned int nVibratoType, nVibratoSpeed, nVibratoDepth;
	unsigned int nTremoloType, nTremoloSpeed, nTremoloDepth;
	unsigned int nPanbrelloType, nPanbrelloSpeed, nPanbrelloDepth;
	unsigned int nOldCmdEx, nOldVolParam, nOldTempo;
	unsigned int nOldOffset, nOldHiOffset;
	unsigned int nCutOff, nResonance;
	unsigned int nRetrigCount, nRetrigParam;
	unsigned int nTremorCount, nTremorParam;
	unsigned int nPatternLoop, nPatternLoopCount;
	unsigned int nRowNote, nRowInstr;
	unsigned int nRowVolCmd, nRowVolume;
	unsigned int nRowCommand, nRowParam;
	unsigned int nLeftVU, nRightVU;
	unsigned int nActiveMacro, nLastInstr;
	unsigned int nTickStart;
	unsigned int nRealtime;
	BYTE stupid_gcc_workaround;

} MODCHANNEL;


typedef struct _MODCHANNELSETTINGS
{
	UINT nPan;
	UINT nVolume;
	DWORD dwFlags;
	UINT nMixPlugin;
        char szName[MAX_CHANNELNAME];        // changed from CHAR
} MODCHANNELSETTINGS;


typedef struct _MODCOMMAND
{
	BYTE note;
	BYTE instr;
	BYTE volcmd;
	BYTE command;
	BYTE vol;
	BYTE param;
} MODCOMMAND, *LPMODCOMMAND;

////////////////////////////////////////////////////////////////////
// Mix Plugins
#define MIXPLUG_MIXREADY			0x01	// Set when cleared

class IMixPlugin
{
public:
	virtual ~IMixPlugin() = 0;
	virtual int AddRef() = 0;
	virtual int Release() = 0;
	virtual void SaveAllParameters() = 0;
	virtual void RestoreAllParameters() = 0;
	virtual void Process(float *pOutL, float *pOutR, unsigned int nSamples) = 0;
	virtual void Init(unsigned int nFreq, int bReset) = 0;
	virtual void MidiSend(DWORD dwMidiCode) = 0;
	virtual void MidiCommand(UINT nMidiCh, UINT nMidiProg, UINT note, UINT vol) = 0;
};


#define MIXPLUG_INPUTF_MASTEREFFECT		0x01	// Apply to master mix
#define MIXPLUG_INPUTF_BYPASS			0x02	// Bypass effect
#define MIXPLUG_INPUTF_WETMIX			0x04	// Wet Mix (dry added)

typedef struct _SNDMIXPLUGINSTATE
{
	DWORD dwFlags;					// MIXPLUG_XXXX
	LONG nVolDecayL, nVolDecayR;	// Buffer click removal
	int *pMixBuffer;				// Stereo effect send buffer
	float *pOutBufferL;				// Temp storage for int -> float conversion
	float *pOutBufferR;
} SNDMIXPLUGINSTATE, *PSNDMIXPLUGINSTATE;

typedef struct _SNDMIXPLUGININFO
{
	DWORD dwPluginId1;
	DWORD dwPluginId2;
	DWORD dwInputRouting;	// MIXPLUG_INPUTF_XXXX
	DWORD dwOutputRouting;	// 0=mix 0x80+=fx
	DWORD dwReserved[4];	// Reserved for routing info
	CHAR szName[32];
	CHAR szLibraryName[64];	// original DLL name
} SNDMIXPLUGININFO, *PSNDMIXPLUGININFO; // Size should be 128

typedef struct _SNDMIXPLUGIN
{
	IMixPlugin *pMixPlugin;
	PSNDMIXPLUGINSTATE pMixState;
	ULONG nPluginDataSize;
	PVOID pPluginData;
	SNDMIXPLUGININFO Info;
} SNDMIXPLUGIN, *PSNDMIXPLUGIN;

typedef	BOOL (*PMIXPLUGINCREATEPROC)(PSNDMIXPLUGIN);

////////////////////////////////////////////////////////////////////

enum {
	MIDIOUT_START=0,
	MIDIOUT_STOP,
	MIDIOUT_TICK,
	MIDIOUT_NOTEON,
	MIDIOUT_NOTEOFF,
	MIDIOUT_VOLUME,
	MIDIOUT_PAN,
	MIDIOUT_BANKSEL,
	MIDIOUT_PROGRAM,
};


typedef struct MODMIDICFG
{
        char szMidiGlb[9*32];      // changed from CHAR
        char szMidiSFXExt[16*32];  // changed from CHAR
        char szMidiZXXExt[128*32]; // changed from CHAR
} MODMIDICFG, *LPMODMIDICFG;


typedef VOID (* LPSNDMIXHOOKPROC)(int *, unsigned int, unsigned int); // buffer, samples, channels



//==============
class CSoundFile
//==============
{
public:	// Static Members
	static UINT m_nXBassDepth, m_nXBassRange;
	static UINT m_nReverbDepth, m_nReverbDelay, gnReverbType;
	static UINT m_nProLogicDepth, m_nProLogicDelay;
	static UINT m_nMaxMixChannels;
	static LONG m_nStreamVolume;
	static DWORD gdwSysInfo, gdwSoundSetup, gdwMixingFreq, gnBitsPerSample, gnChannels;
	static UINT gnAGC, gnVolumeRampSamples;
	static UINT gnVULeft, gnVURight;
	static LPSNDMIXHOOKPROC gpSndMixHook;
	static PMIXPLUGINCREATEPROC gpMixPluginCreateProc;

public:	// for Editing
	MODCHANNEL Chn[MAX_CHANNELS];					// Channels
	UINT ChnMix[MAX_CHANNELS];						// Channels to be mixed
	MODINSTRUMENT Ins[MAX_SAMPLES];					// Instruments
	INSTRUMENTHEADER *Headers[MAX_INSTRUMENTS];		// Instrument Headers
	MODCHANNELSETTINGS ChnSettings[MAX_BASECHANNELS]; // Channels settings
	MODCOMMAND *Patterns[MAX_PATTERNS];				// Patterns
	WORD PatternSize[MAX_PATTERNS];					// Patterns Lengths
	WORD PatternAllocSize[MAX_PATTERNS];				// Allocated pattern lengths (for async. resizing/playback)
	BYTE Order[MAX_ORDERS];							// Pattern Orders
	MODMIDICFG m_MidiCfg;							// Midi macro config table
	SNDMIXPLUGIN m_MixPlugins[MAX_MIXPLUGINS];		// Mix plugins
	UINT m_nDefaultSpeed, m_nDefaultTempo, m_nDefaultGlobalVolume;
	DWORD m_dwSongFlags;							// Song flags SONG_XXXX
	UINT m_nStereoSeparation;
	UINT m_nChannels, m_nMixChannels, m_nMixStat, m_nBufferCount;
	UINT m_nType, m_nSamples, m_nInstruments;
	UINT m_nTickCount, m_nTotalCount, m_nPatternDelay, m_nFrameDelay;
	UINT m_nMusicSpeed, m_nMusicTempo;
	UINT m_nNextRow, m_nRow;
	UINT m_nPattern,m_nCurrentPattern,m_nNextPattern,m_nLockedPattern,m_nRestartPos;
	UINT m_nMasterVolume, m_nGlobalVolume, m_nSongPreAmp;
	UINT m_nFreqFactor, m_nTempoFactor, m_nOldGlbVolSlide;
	LONG m_nMinPeriod, m_nMaxPeriod, m_nRepeatCount, m_nInitialRepeatCount;
	DWORD m_nGlobalFadeSamples, m_nGlobalFadeMaxSamples;
	BYTE m_rowHighlightMajor, m_rowHighlightMinor;
	UINT m_nPatternNames;
	LPSTR m_lpszSongComments, m_lpszPatternNames;
	char m_szNames[MAX_INSTRUMENTS][32];    // changed from CHAR
	CHAR CompressionTable[16];

	// chaseback
	int stop_at_order;
	int stop_at_row;
	unsigned int stop_at_time;

	static MODMIDICFG m_MidiCfgDefault;							// Midi macro config table
public:
	CSoundFile();
	~CSoundFile();

public:
	BOOL Create(LPCBYTE lpStream, DWORD dwMemLength=0);
	BOOL Destroy();
	UINT GetHighestUsedChannel();
	UINT GetType() const { return m_nType; }
	UINT GetNumChannels() const;
	UINT GetLogicalChannels() const { return m_nChannels; }
	BOOL SetMasterVolume(UINT vol, BOOL bAdjustAGC=FALSE);
	UINT GetMasterVolume() const { return m_nMasterVolume; }
	UINT GetNumPatterns() const;
	UINT GetNumInstruments() const;
	UINT GetNumSamples() const { return m_nSamples; }
	UINT GetCurrentPos() const;
	UINT GetCurrentPattern() const { return m_nPattern; }
	UINT GetCurrentOrder() const { return m_nCurrentPattern; }
	UINT GetSongComments(LPSTR s, UINT cbsize, UINT linesize=32);
	UINT GetRawSongComments(LPSTR s, UINT cbsize, UINT linesize=32);
	UINT GetMaxPosition() const;
	void SetCurrentPos(UINT nPos);
	void SetCurrentOrder(UINT nOrder);
	void GetTitle(LPSTR s) const { lstrcpyn(s,m_szNames[0],32); }
	LPCSTR GetTitle() const { return m_szNames[0]; }
	UINT GetSampleName(UINT nSample,LPSTR s=NULL) const;
	UINT GetInstrumentName(UINT nInstr,LPSTR s=NULL) const;
	UINT GetMusicSpeed() const { return m_nMusicSpeed; }
	UINT GetMusicTempo() const { return m_nMusicTempo; }
	unsigned int GetLength(BOOL bAdjust, BOOL bTotal=FALSE);
	unsigned int GetSongTime() { return GetLength(FALSE, TRUE); }
	void SetRepeatCount(int n) { m_nRepeatCount = n; m_nInitialRepeatCount = n; }
	int GetRepeatCount() const { return m_nRepeatCount; }
	BOOL IsPaused() const {	return (m_dwSongFlags & SONG_PAUSED) ? TRUE : FALSE; }
	void LoopPattern(int nPat, int nRow=0);
	BOOL SetPatternName(UINT nPat, LPCSTR lpszName);
	BOOL GetPatternName(UINT nPat, LPSTR lpszName, UINT cbSize=MAX_PATTERNNAME) const;
	// Module Loaders
	BOOL ReadXM(LPCBYTE lpStream, DWORD dwMemLength);
	BOOL ReadS3M(LPCBYTE lpStream, DWORD dwMemLength);
	BOOL ReadMod(LPCBYTE lpStream, DWORD dwMemLength);
	BOOL ReadMed(LPCBYTE lpStream, DWORD dwMemLength);
	BOOL ReadMTM(LPCBYTE lpStream, DWORD dwMemLength);
	BOOL ReadSTM(LPCBYTE lpStream, DWORD dwMemLength);
	BOOL ReadIT(LPCBYTE lpStream, DWORD dwMemLength);
	BOOL Read669(LPCBYTE lpStream, DWORD dwMemLength);
	BOOL ReadUlt(LPCBYTE lpStream, DWORD dwMemLength);
	BOOL ReadWav(LPCBYTE lpStream, DWORD dwMemLength);
	BOOL ReadDSM(LPCBYTE lpStream, DWORD dwMemLength);
	BOOL ReadFAR(LPCBYTE lpStream, DWORD dwMemLength);
	BOOL ReadAMS(LPCBYTE lpStream, DWORD dwMemLength);
	BOOL ReadAMS2(LPCBYTE lpStream, DWORD dwMemLength);
	BOOL ReadMDL(LPCBYTE lpStream, DWORD dwMemLength);
	BOOL ReadOKT(LPCBYTE lpStream, DWORD dwMemLength);
	BOOL ReadDMF(LPCBYTE lpStream, DWORD dwMemLength);
	BOOL ReadPTM(LPCBYTE lpStream, DWORD dwMemLength);
	BOOL ReadDBM(LPCBYTE lpStream, DWORD dwMemLength);
	BOOL ReadAMF(LPCBYTE lpStream, DWORD dwMemLength);
	BOOL ReadMT2(LPCBYTE lpStream, DWORD dwMemLength);
	BOOL ReadPSM(LPCBYTE lpStream, DWORD dwMemLength);
	BOOL ReadUMX(LPCBYTE lpStream, DWORD dwMemLength);
	BOOL ReadMID(LPCBYTE lpStream, DWORD dwMemLength);
	// Save Functions
	UINT WriteSample(diskwriter_driver_t *f, MODINSTRUMENT *pins, UINT nFlags, UINT nMaxLen=0);
	BOOL SaveXM(diskwriter_driver_t *f, UINT);
	BOOL SaveS3M(diskwriter_driver_t *f, UINT);
	BOOL SaveMod(diskwriter_driver_t *f, UINT);
	// MOD Convert function
	UINT GetBestSaveFormat() const;
	UINT GetSaveFormats() const;
	void ConvertModCommand(MODCOMMAND *) const;
	void S3MConvert(MODCOMMAND *m, BOOL bIT) const;
	void S3MSaveConvert(UINT *pcmd, UINT *pprm, BOOL bIT) const;
	WORD ModSaveCommand(const MODCOMMAND *m, BOOL bXM) const;
public:
	// backhooks :)
	static void (*_midi_out_note)(int chan, const MODCOMMAND *m);
	static void (*_midi_out_raw)(const unsigned char *,unsigned int, unsigned int);
	static void (*_multi_out_raw)(int chan, int *buf, int len);

public:
	// Real-time sound functions
	VOID ResetChannels();

	UINT Read(LPVOID lpBuffer, UINT cbBuffer);
	UINT CreateStereoMix(int count);
	BOOL FadeSong(UINT msec);
	BOOL GlobalFadeSong(UINT msec);
	UINT GetTotalTickCount() const { return m_nTotalCount; }
	VOID ResetTotalTickCount() { m_nTotalCount = 0; }

public:
	// Mixer Config
	static BOOL InitPlayer(BOOL bReset=FALSE);
	static BOOL SetWaveConfig(UINT nRate,UINT nBits,UINT nChannels);
	static BOOL SetResamplingMode(UINT nMode); // SRCMODE_XXXX
	static BOOL IsStereo() { return (gnChannels > 1) ? TRUE : FALSE; }
	static DWORD GetSampleRate() { return gdwMixingFreq; }
	static DWORD GetBitsPerSample() { return gnBitsPerSample; }
	static DWORD InitSysInfo();
	static DWORD GetSysInfo() { return gdwSysInfo; }
	// AGC
	static BOOL GetAGC() { return (gdwSoundSetup & SNDMIX_AGC) ? TRUE : FALSE; }
	static void SetAGC(BOOL b);
	static void ResetAGC();
	static void ProcessAGC(int count);

	// Floats
	static VOID StereoMixToFloat(const int *pSrc, float *pOut1, float *pOut2, UINT nCount);
	static VOID FloatToStereoMix(const float *pIn1, const float *pIn2, int *pOut, UINT nCount);
	static VOID MonoMixToFloat(const int *pSrc, float *pOut, UINT nCount);
	static VOID FloatToMonoMix(const float *pIn, int *pOut, UINT nCount);
	
	
	
	

	// wee...
        static void InitializeEQ(BOOL bReset=TRUE);
        static void SetEQGains(const UINT *pGains, UINT nBands, const UINT *pFreqs=NULL, BOOL bReset=FALSE);    // 0=-12dB, 32=+12dB
        /*static*/ void EQStereo(int *pbuffer, UINT nCount);
        /*static*/ void EQMono(int *pbuffer, UINT nCount);


	//GCCFIX -- added these functions back in!
	static BOOL SetWaveConfigEx(BOOL bSurround,BOOL bNoOverSampling,BOOL bReverb,BOOL hqido,BOOL bMegaBass,BOOL bNR,BOOL bEQ);
	// DSP Effects
	static void InitializeDSP(BOOL bReset);
	static void ProcessStereoDSP(int count);
	static void ProcessMonoDSP(int count);
	// [Reverb level 0(quiet)-100(loud)], [delay in ms, usually 40-200ms]
	static BOOL SetReverbParameters(UINT nDepth, UINT nDelay);
	// [XBass level 0(quiet)-100(loud)], [cutoff in Hz 10-100]
	static BOOL SetXBassParameters(UINT nDepth, UINT nRange);
	// [Surround level 0(quiet)-100(heavy)] [delay in ms, usually 5-40ms]
	static BOOL SetSurroundParameters(UINT nDepth, UINT nDelay);
public:
	BOOL ReadNote();
	BOOL ProcessRow();
	BOOL ProcessEffects();
	UINT GetNNAChannel(UINT nChn);
	void CheckNNA(UINT nChn, UINT instr, int note, BOOL bForceCut);
	void NoteChange(UINT nChn, int note, BOOL bPorta=FALSE, BOOL bResetEnv=TRUE, BOOL bManual=FALSE);
	void InstrumentChange(MODCHANNEL *pChn, UINT instr, BOOL bPorta=FALSE,BOOL bUpdVol=TRUE,BOOL bResetEnv=TRUE);
	void TranslateKeyboard(INSTRUMENTHEADER* penv, UINT note, MODINSTRUMENT*& psmp);
	// Channel Effects
	void PortamentoUp(MODCHANNEL *pChn, UINT param);
	void PortamentoDown(MODCHANNEL *pChn, UINT param);
	void FinePortamentoUp(MODCHANNEL *pChn, UINT param);
	void FinePortamentoDown(MODCHANNEL *pChn, UINT param);
	void ExtraFinePortamentoUp(MODCHANNEL *pChn, UINT param);
	void ExtraFinePortamentoDown(MODCHANNEL *pChn, UINT param);
	void TonePortamento(MODCHANNEL *pChn, UINT param);
	void Vibrato(MODCHANNEL *pChn, UINT param);
	void FineVibrato(MODCHANNEL *pChn, UINT param);
	void VolumeSlide(MODCHANNEL *pChn, UINT param);
	void PanningSlide(MODCHANNEL *pChn, UINT param);
	void ChannelVolSlide(MODCHANNEL *pChn, UINT param);
	void FineVolumeUp(MODCHANNEL *pChn, UINT param);
	void FineVolumeDown(MODCHANNEL *pChn, UINT param);
	void Tremolo(MODCHANNEL *pChn, UINT param);
	void Panbrello(MODCHANNEL *pChn, UINT param);
	void RetrigNote(UINT nChn, UINT param);
	void NoteCut(UINT nChn, UINT nTick);
	void KeyOff(UINT nChn);
	int PatternLoop(MODCHANNEL *, UINT param);
	void ExtendedMODCommands(UINT nChn, UINT param);
	void ExtendedS3MCommands(UINT nChn, UINT param);
	void ExtendedChannelEffect(MODCHANNEL *, UINT param);
	void MidiSend(const unsigned char *data, unsigned int len, UINT nChn=0, int fake = 0);
	void ProcessMidiMacro(UINT nChn, LPCSTR pszMidiMacro, UINT param=0,
			UINT note=0, UINT velocity=0, UINT use_instr=0);
	void SetupChannelFilter(MODCHANNEL *pChn, BOOL bReset, int flt_modifier=256,int freq=0) const;
	// Low-Level effect processing
	void DoFreqSlide(MODCHANNEL *pChn, LONG nFreqSlide);
	// Global Effects
	void SetTempo(UINT param);
	void SetSpeed(UINT param);
	void GlobalVolSlide(UINT param);
	DWORD IsSongFinished(UINT nOrder, UINT nRow) const;
	BOOL IsValidBackwardJump(UINT nStartOrder, UINT nStartRow, UINT nJumpOrder, UINT nJumpRow) const;
	// Read/Write sample functions
	signed char GetDeltaValue(signed char prev, UINT n) const { return (signed char)(prev + CompressionTable[n & 0x0F]); }
	UINT ReadSample(MODINSTRUMENT *pIns, UINT nFlags, LPCSTR pMemFile, DWORD dwMemLength);
	BOOL DestroySample(UINT nSample);
	BOOL DestroyInstrument(UINT nInstr);
	BOOL IsSampleUsed(UINT nSample);
	BOOL IsInstrumentUsed(UINT nInstr);
	BOOL RemoveInstrumentSamples(UINT nInstr);
	UINT DetectUnusedSamples(BOOL *);
	BOOL RemoveSelectedSamples(BOOL *);
	void AdjustSampleLoop(MODINSTRUMENT *pIns);
	// I/O from another sound file
	BOOL ReadInstrumentFromSong(UINT nInstr, CSoundFile *, UINT nSrcInstrument);
	BOOL ReadSampleFromSong(UINT nSample, CSoundFile *, UINT nSrcSample);
	// Period/Note functions
	UINT GetNoteFromPeriod(UINT period) const;
	UINT GetPeriodFromNote(UINT note, int nFineTune, UINT nC5Speed) const;
	UINT GetFreqFromPeriod(UINT period, UINT nC5Speed, int nPeriodFrac=0) const;
	// Misc functions
	MODINSTRUMENT *GetSample(UINT n) { return Ins+n; }
	void ResetMidiCfg();
	UINT MapMidiInstrument(DWORD dwProgram, UINT nChannel, UINT nNote);
	BOOL ITInstrToMPT(const void *p, INSTRUMENTHEADER *penv, UINT trkvers);
	UINT SaveMixPlugins(FILE *f=NULL, BOOL bUpdate=TRUE);
	UINT LoadMixPlugins(const void *pData, UINT nLen);
	void ResetTimestamps(); // for note playback dots

	// Static helper functions
public:
	static DWORD TransposeToFrequency(int transp, int ftune=0);
	static int FrequencyToTranspose(DWORD freq);

	// System-Dependant functions
public:
	static MODCOMMAND *AllocatePattern(UINT rows, UINT nchns);
	static signed char* AllocateSample(UINT nbytes);
	static void FreePattern(LPVOID pat);
	static void FreeSample(LPVOID p);
	static UINT Normalize24BitBuffer(LPBYTE pbuffer, UINT cbsizebytes, DWORD lmax24, DWORD dwByteInc);

private:
    /* CSoundFile is a sentinel, prevent copying to avoid memory leaks */
    CSoundFile(const CSoundFile&);
    void operator=(const CSoundFile&);
};


// inline DWORD BigEndian(DWORD x) { return ((x & 0xFF) << 24) | ((x & 0xFF00) << 8) | ((x & 0xFF0000) >> 8) | ((x & 0xFF000000) >> 24); }
// inline WORD BigEndianW(WORD x) { return (WORD)(((x >> 8) & 0xFF) | ((x << 8) & 0xFF00)); }


//////////////////////////////////////////////////////////
// Tables defined in tables.cpp

extern BYTE ImpulseTrackerPortaVolCmd[16];
extern WORD S3MFineTuneTable[16];
extern WORD ProTrackerPeriodTable[6*12];
extern WORD ProTrackerTunedPeriods[16*12];
extern WORD FreqS3MTable[];
extern WORD XMPeriodTable[96+8];
extern UINT XMLinearTable[768];
extern DWORD FineLinearSlideUpTable[16];
extern DWORD FineLinearSlideDownTable[16];
extern DWORD LinearSlideUpTable[256];
extern DWORD LinearSlideDownTable[256];
extern signed char retrigTable1[16];
extern signed char retrigTable2[16];
extern short int ModSinusTable[64];
extern short int ModRampDownTable[64];
extern short int ModSquareTable[64];
extern short int ModRandomTable[64];
extern signed char ft2VibratoTable[256];	// -64 .. +64

//////////////////////////////////////////////////////////
// WAVE format information

#pragma pack(1)

// Standard IFF chunks IDs
#define IFFID_FORM		0x4d524f46
#define IFFID_RIFF		0x46464952
#define IFFID_WAVE		0x45564157
#define IFFID_LIST		0x5453494C
#define IFFID_INFO		0x4F464E49

// IFF Info fields
#define IFFID_ICOP		0x504F4349
#define IFFID_IART		0x54524149
#define IFFID_IPRD		0x44525049
#define IFFID_INAM		0x4D414E49
#define IFFID_ICMT		0x544D4349
#define IFFID_IENG		0x474E4549
#define IFFID_ISFT		0x54465349
#define IFFID_ISBJ		0x4A425349
#define IFFID_IGNR		0x524E4749
#define IFFID_ICRD		0x44524349

// Wave IFF chunks IDs
#define IFFID_wave		0x65766177
#define IFFID_fmt		0x20746D66
#define IFFID_wsmp		0x706D7377
#define IFFID_pcm		0x206d6370
#define IFFID_data		0x61746164
#define IFFID_smpl		0x6C706D73
#define IFFID_xtra		0x61727478

typedef struct WAVEFILEHEADER
{
	DWORD id_RIFF;		// "RIFF"
	DWORD filesize;		// file length-8
	DWORD id_WAVE;
} WAVEFILEHEADER;


typedef struct WAVEFORMATHEADER
{
	DWORD id_fmt;		// "fmt "
	DWORD hdrlen;		// 16
	WORD format;		// 1
	WORD channels;		// 1:mono, 2:stereo
	DWORD freqHz;		// sampling freq
	DWORD bytessec;		// bytes/sec=freqHz*samplesize
	WORD samplesize;	// sizeof(sample)
	WORD bitspersample;	// bits per sample (8/16)
} WAVEFORMATHEADER;


typedef struct WAVEDATAHEADER
{
	DWORD id_data;		// "data"
	DWORD length;		// length of data
} WAVEDATAHEADER;


typedef struct WAVESMPLHEADER
{
	// SMPL
	DWORD smpl_id;		// "smpl"	-> 0x6C706D73
	DWORD smpl_len;		// length of smpl: 3Ch	(54h with sustain loop)
	DWORD dwManufacturer;
	DWORD dwProduct;
	DWORD dwSamplePeriod;	// 1000000000/freqHz
	DWORD dwBaseNote;	// 3Ch = C-4 -> 60 + RelativeTone
	DWORD dwPitchFraction;
	DWORD dwSMPTEFormat;
	DWORD dwSMPTEOffset;
	DWORD dwSampleLoops;	// number of loops
	DWORD cbSamplerData;
} WAVESMPLHEADER;


typedef struct SAMPLELOOPSTRUCT
{
	DWORD dwIdentifier;
	DWORD dwLoopType;		// 0=normal, 1=bidi
	DWORD dwLoopStart;
	DWORD dwLoopEnd;		// Byte offset ?
	DWORD dwFraction;
	DWORD dwPlayCount;		// Loop Count, 0=infinite
} SAMPLELOOPSTRUCT;


typedef struct WAVESAMPLERINFO
{
	WAVESMPLHEADER wsiHdr;
	SAMPLELOOPSTRUCT wsiLoops[2];
} WAVESAMPLERINFO;


typedef struct WAVELISTHEADER
{
	DWORD list_id;	// "LIST" -> 0x5453494C
	DWORD list_len;
	DWORD info;		// "INFO"
} WAVELISTHEADER;


typedef struct WAVEEXTRAHEADER
{
	DWORD xtra_id;	// "xtra"	-> 0x61727478
	DWORD xtra_len;
	DWORD dwFlags;
	WORD  wPan;
	WORD  wVolume;
	WORD  wGlobalVol;
	WORD  wReserved;
	BYTE nVibType;
	BYTE nVibSweep;
	BYTE nVibDepth;
	BYTE nVibRate;
} WAVEEXTRAHEADER;

#pragma pack()

///////////////////////////////////////////////////////////
// Low-level Mixing functions

#define MIXBUFFERSIZE		512
#define MIXING_ATTENUATION	5
#define MIXING_CLIPMIN		(-0x04000000)
#define MIXING_CLIPMAX		(0x03FFFFFF)
#define VOLUMERAMPPRECISION	12
#define FADESONGDELAY		100
#define EQ_BUFFERSIZE		(MIXBUFFERSIZE)
#define AGC_PRECISION		9
#define AGC_UNITY			(1 << AGC_PRECISION)

// Calling conventions
#ifdef MSC_VER
#define MPPASMCALL	__cdecl
#define MPPFASTCALL	__fastcall
#else
#define MPPASMCALL
#define MPPFASTCALL
#endif

#define MOD2XMFineTune(k)	((int)( (signed char)((k)<<4) ))
#define XM2MODFineTune(k)	((int)( (k>>4)&0x0f ))

// Return (a*b)/c - no divide error
static inline int _muldiv(int a, int b, int c)
{
	return ((unsigned long long) a * (unsigned long long) b ) / c;
}


// Return (a*b+c/2)/c - no divide error
static inline int _muldivr(int a, int b, int c)
{
	return ((unsigned long long) a * (unsigned long long) b + (c >> 1)) / c;
}


#define NEED_BYTESWAP
#include "headers.h"

#endif
