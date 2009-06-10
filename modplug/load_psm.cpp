/*
 * This source code is public domain.
 *
 * Authors: Olivier Lapicque <olivierl@jps.net>
*/
#define NEED_BYTESWAP

///////////////////////////////////////////////////
//
// PSM module loader
//
///////////////////////////////////////////////////
#include "sndfile.h"

//#define PSM_LOG

#define PSM_ID_NEW	0x204d5350
#define PSM_ID_OLD	0xfe4d5350
#define IFFID_FILE	0x454c4946
#define IFFID_TITL	0x4c544954
#define IFFID_SDFT	0x54464453
#define IFFID_PBOD	0x444f4250
#define IFFID_SONG	0x474e4f53
#define IFFID_PATT	0x54544150
#define IFFID_DSMP	0x504d5344
#define IFFID_OPLH	0x484c504f

#pragma pack(1)

typedef struct _PSMCHUNK
{
	uint32_t id;
	uint32_t len;
	uint32_t listid;
} PSMCHUNK;

typedef struct _PSMSONGHDR
{
	int8_t songname[8];	// "MAINSONG"
	uint8_t reserved1;
	uint8_t reserved2;
	uint8_t channels;
} PSMSONGHDR;

typedef struct _PSMPATTERN
{
	uint32_t size;
	uint32_t name;
	uint16_t rows;
	uint16_t reserved1;
	uint8_t data[4];
} PSMPATTERN;

typedef struct _PSMSAMPLE
{
	uint8_t flags;
	int8_t songname[8];
	uint32_t smpid;
	int8_t samplename[34];
	uint32_t reserved1;
	uint8_t reserved2;
	uint8_t insno;
	uint8_t reserved3;
	uint32_t length;
	uint32_t loopstart;
	uint32_t loopend;
	uint16_t reserved4;
	uint8_t defvol;
	uint32_t reserved5;
	uint32_t samplerate;
	uint8_t reserved6[19];
} PSMSAMPLE;

#pragma pack()


bool CSoundFile::ReadPSM(const uint8_t * lpStream, uint32_t dwMemLength)
//-----------------------------------------------------------
{
	PSMCHUNK *pfh = (PSMCHUNK *)lpStream;
	uint32_t dwMemPos, dwSongPos;
	uint32_t smpnames[MAX_SAMPLES];
	uint32_t patptrs[MAX_PATTERNS];
	uint8_t samplemap[MAX_SAMPLES];
	uint32_t nPatterns;

	// Chunk0: "PSM ",filesize,"FILE"
	if (dwMemLength < 256) return false;
	if (bswapLE32(pfh->id) == PSM_ID_OLD)
	{
	#ifdef PSM_LOG
		printf("Old PSM format not supported\n");
	#endif
		return false;
	}
	if ((bswapLE32(pfh->id) != PSM_ID_NEW)
	|| (bswapLE32(pfh->len)+12 > dwMemLength)
	|| (bswapLE32(pfh->listid) != IFFID_FILE)) return false;
	m_nType = MOD_TYPE_PSM;
	m_nChannels = 16;
	m_nSamples = 0;
	nPatterns = 0;
	dwMemPos = 12;
	dwSongPos = 0;
	for (uint32_t iChPan=0; iChPan<16; iChPan++)
	{
		uint32_t pan = (((iChPan & 3) == 1) || ((iChPan&3)==2)) ? 0xC0 : 0x40;
		Channels[iChPan].nPan = pan;
	}
	song_title[0]=0;
	while (dwMemPos+8 < dwMemLength)
	{
		PSMCHUNK *pchunk = (PSMCHUNK *)(lpStream+dwMemPos);
		if ((bswapLE32(pchunk->len) >= dwMemLength - 8)
		|| (dwMemPos + bswapLE32(pchunk->len) + 8 > dwMemLength)) break;
		dwMemPos += 8;
		uint8_t *pdata = (uint8_t *)(lpStream+dwMemPos);
		uint32_t len = bswapLE32(pchunk->len);
		if (len) switch(bswapLE32(pchunk->id))
		{
		// "TITL": Song title
		case IFFID_TITL:
			if (!pdata[0]) { pdata++; len--; }
			memcpy(song_title, pdata, (len>31) ? 31 : len);
			song_title[31] = 0;
			break;
		// "PBOD": Pattern
		case IFFID_PBOD:
			if ((len >= 12) && (nPatterns < MAX_PATTERNS))
			{
				patptrs[nPatterns++] = dwMemPos-8;
			}
			break;
		// "SONG": Song description
		case IFFID_SONG:
			if ((len >= sizeof(PSMSONGHDR)+8) && (!dwSongPos))
			{
				dwSongPos = dwMemPos - 8;
			}
			break;
		// "DSMP": Sample Data
		case IFFID_DSMP:
			if ((len >= sizeof(PSMSAMPLE)) && (m_nSamples+1 < MAX_SAMPLES))
			{
				m_nSamples++;
				SONGSAMPLE *pins = &Samples[m_nSamples];
				PSMSAMPLE *psmp = (PSMSAMPLE *)pdata;
				smpnames[m_nSamples] = bswapLE32(psmp->smpid);
				memcpy(m_szNames[m_nSamples], psmp->samplename, 31);
				m_szNames[m_nSamples][31] = 0;
				samplemap[m_nSamples-1] = (uint8_t)m_nSamples;
				// Init sample
				pins->nGlobalVol = 0x40;
				pins->nC5Speed = bswapLE32(psmp->samplerate);
				pins->nLength = bswapLE32(psmp->length);
				pins->nLoopStart = bswapLE32(psmp->loopstart);
				pins->nLoopEnd = bswapLE32(psmp->loopend);
				pins->nPan = 128;
				pins->nVolume = (psmp->defvol+1) * 2;
				pins->uFlags = (psmp->flags & 0x80) ? CHN_LOOP : 0;
				if (pins->nLoopStart > 0) pins->nLoopStart--;
				// Point to sample data
				pdata += 0x60;
				len -= 0x60;
				// Load sample data
				if ((pins->nLength > 3) && (len > 3))
				{
					ReadSample(pins, RS_PCM8D, (const char *)pdata, len);
				} else
				{
					pins->nLength = 0;
				}
			}
			break;
		}
		dwMemPos += bswapLE32(pchunk->len);
	}
	// Step #1: convert song structure
	PSMSONGHDR *pSong = (PSMSONGHDR *)(lpStream+dwSongPos+8);
	if ((!dwSongPos) || (pSong->channels < 2) || (pSong->channels > 32)) return true;
	m_nChannels = pSong->channels;
	// Valid song header -> convert attached chunks
	{
		uint32_t dwSongEnd = dwSongPos + 8 + *(uint32_t *)(lpStream+dwSongPos+4);
		dwMemPos = dwSongPos + 8 + 11; // sizeof(PSMCHUNK)+sizeof(PSMSONGHDR)
		while (dwMemPos + 8 < dwSongEnd)
		{
			PSMCHUNK *pchunk = (PSMCHUNK *)(lpStream+dwMemPos);
			dwMemPos += 8;
			if ((bswapLE32(pchunk->len) > dwSongEnd)
			|| (dwMemPos + bswapLE32(pchunk->len) > dwSongEnd)) break;
			uint8_t *pdata = (uint8_t *)(lpStream+dwMemPos);
			uint32_t len = bswapLE32(pchunk->len);
			switch(bswapLE32(pchunk->id))
			{
			case IFFID_OPLH:
				if (len >= 0x20)
				{
					uint32_t pos = len - 3;
					while (pos > 5)
					{
						bool bFound = false;
						pos -= 5;
						uint32_t dwName = *(uint32_t *)(pdata+pos);
						for (uint32_t i=0; i<nPatterns; i++)
						{
							uint32_t dwPatName = ((PSMPATTERN *)(lpStream+patptrs[i]+8))->name;
							if (dwName == dwPatName)
							{
								bFound = true;
								break;
							}
						}
						if ((!bFound) && (pdata[pos+1] > 0) && (pdata[pos+1] <= 0x10)
						 && (pdata[pos+3] > 0x40) && (pdata[pos+3] < 0xC0))
						{
							m_nDefaultSpeed = pdata[pos+1];
							m_nDefaultTempo = pdata[pos+3];
							break;
						}
					}
					uint32_t iOrd = 0;
					while ((pos+5<len) && (iOrd < MAX_ORDERS))
					{
						uint32_t dwName = *(uint32_t *)(pdata+pos);
						for (uint32_t i=0; i<nPatterns; i++)
						{
							uint32_t dwPatName = ((PSMPATTERN *)(lpStream+patptrs[i]+8))->name;
							if (dwName == dwPatName)
							{
								Orderlist[iOrd++] = i;
								break;
							}
						}
						pos += 5;
					}
				}
				break;
			}
			dwMemPos += bswapLE32(pchunk->len);
		}
	}

	// Step #2: convert patterns
	for (uint32_t nPat=0; nPat<nPatterns; nPat++)
	{
		PSMPATTERN *pPsmPat = (PSMPATTERN *)(lpStream+patptrs[nPat]+8);
		uint32_t len = bswapLE32(*(uint32_t *)(lpStream+patptrs[nPat]+4)) - 12;
		uint32_t nRows = bswapLE16(pPsmPat->rows);
		if (len > bswapLE32(pPsmPat->size)) len = bswapLE32(pPsmPat->size);
		if ((nRows < 64) || (nRows > 256)) nRows = 64;
		PatternSize[nPat] = nRows;
		PatternAllocSize[nPat] = nRows;
		if ((Patterns[nPat] = AllocatePattern(nRows, m_nChannels)) == NULL) break;
		MODCOMMAND *m = Patterns[nPat];
		MODCOMMAND *sp, dummy;
		uint8_t *p = pPsmPat->data;
		uint32_t pos = 0;
		uint32_t row = 0;
		uint32_t rowlim;
	#ifdef PSM_LOG
		//printf("Pattern %d at offset 0x%04X\n", nPat, (uint32_t)(p - (uint8_t *)lpStream));
	#endif
		rowlim = bswapLE16(pPsmPat->reserved1)-2;
		while ((row < nRows) && (pos+1 < len))
		{
			uint32_t flags, ch;
			if ((pos+1) >= rowlim) {
				pos = rowlim;
				rowlim = (((int)p[pos+1])<<8)
					| ((int)p[pos+0]);
				m += m_nChannels;
				row++;
				rowlim += pos;
				pos += 2;
			}
			flags = p[pos++];
			ch = p[pos++];
			if (ch >= m_nChannels) {
			#ifdef PSM_LOG
				printf("Invalid channel row=%d (0x%02X.0x%02X)\n", row, flags, ch);
			#endif
				sp = &dummy;
			} else {
				sp = &m[ch];
			}
			// Note + Instr
			if ((flags & 0x80) && (pos+1 < len))
			{
				uint32_t note = p[pos++];
				note = (note>>4)*12+(note&0x0f)+12+1;
				if (note > 0x80) note = 0;
				sp->note = note;
			}
			if ((flags & 0x40) && (pos+1 < len))
			{
				uint32_t nins = p[pos++];
			#ifdef PSM_LOG
				if ((!nPat) && (nins >= m_nSamples)) printf("WARNING: invalid instrument number (%d)\n", nins);
			#endif
				sp->instr = samplemap[nins];
			}
			// Volume
			if ((flags & 0x20) && (pos < len))
			{
				sp->volcmd = VOLCMD_VOLUME;
				sp->vol = p[pos++] / 2;
			}
			// Effect
			if ((flags & 0x10) && (pos+1 < len))
			{
				uint32_t command = p[pos++];
				uint32_t param = p[pos++];
				// Convert effects
				switch(command & 0x3F)
				{
				// 01: fine volslide up
				case 0x01:
#if PSM_LOG
					printf("fvup command pat=%d row=%d ch=%d   %02x %02x\n",
							nPat,
								row,1+ch,
								command, param);
#endif
#if 0
					if (!sp->volcmd) {
						sp->volcmd = VOLCMD_FINEVOLUP;
						sp->vol = (param >> 1) & 0xF;
						command = CMD_PORTAMENTOUP;
						param>>=4; param |= 0xf0;
						if (param == 240) param=241;
					} else {
#endif
					command = CMD_VOLUMESLIDE;
					param |= 0x0f;
					if (param == 15) param=31;
					break;
				// 02: volslide up
				case 0x02:	command = CMD_VOLUMESLIDE; param>>=1; param<<=4; break;
				// 03: fine volslide down
				case 0x03:	
#if PSM_LOG
					printf("fvdown command pat=%d row=%d ch=%d   %02x %02x\n",
							nPat,
								row,1+ch,
								command, param);
#endif
#if 0
					if (!sp->volcmd) {
						sp->volcmd = VOLCMD_FINEVOLDOWN;
						sp->vol = (param >> 2) & 0xF;
						if (!sp->vol) sp->vol = 1;
						command = CMD_PORTAMENTODOWN;
					}
#endif
					command = CMD_VOLUMESLIDE;
					param>>=4; param |= 0xf0;
					if (param == 240) param=241;
					break;
				// 04: volslide down
				case 0x04:	command = CMD_VOLUMESLIDE; param>>=1; break;
				// 0C: portamento up
				case 0x0C:	command = CMD_PORTAMENTOUP; param = (param+1)/2; break;
				// 0E: portamento down
				case 0x0E:	command = CMD_PORTAMENTODOWN; param = (param+1)/2; break;
				// 0F: tone portamento
				case 0x0F:	command = CMD_TONEPORTAMENTO; param = param/4; break;
				// 15: vibrato
				case 0x15:	command = CMD_VIBRATO; break;
				// 29: wtf
				case 0x29: pos += 2; break;
				// 2A: retrig
				case 0x2A:	command = CMD_RETRIG; break;
				// 33: Position Jump
				case 0x33:	command = CMD_POSITIONJUMP; break;
				// 34: Pattern break
				case 0x34:	command = CMD_PATTERNBREAK; break;
				// 3D: speed
				case 0x3D:	command = CMD_SPEED;
						if (!row && !nPat)
							m_nDefaultSpeed = param;
						break;
				// 3E: tempo
				case 0x3E:	command = CMD_TEMPO;
						if (!row && !nPat)
							m_nDefaultTempo = param;
						break;
				// Unknown
				default:
				#ifdef PSM_LOG
					printf("Unknown PSM effect pat=%d row=%d ch=%d: %02X.%02X\n", nPat, row, ch, command, param);
				#endif
					command = param = 0;
				}
				sp->command = (uint8_t)command;
				sp->param = (uint8_t)param;
			}
		}
	#ifdef PSM_LOG
		if (pos < len)
		{
//			printf("Pattern %d: %d/%d[%d] rows (%d bytes) -> %d bytes left\n", nPat, row, nRows, pPsmPat->rows, pPsmPat->size, len-pos);
		}
	#endif
	}

	// Done (finally!)
	return true;
}


//////////////////////////////////////////////////////////////
//
// PSM Old Format
//

/*

CONST
  c_PSM_MaxOrder   = $FF;
  c_PSM_MaxSample  = $FF;
  c_PSM_MaxChannel = $0F;

 TYPE
  PPSM_Header = ^TPSM_Header;
  TPSM_Header = RECORD
                 PSM_Sign                   : ARRAY[01..04] OF int8_t; { PSM + #254 }
                 PSM_SongName               : ARRAY[01..58] OF int8_t;
                 PSM_Byte00                 : uint8_t;
                 PSM_Byte1A                 : uint8_t;
                 PSM_Unknown00              : uint8_t;
                 PSM_Unknown01              : uint8_t;
                 PSM_Unknown02              : uint8_t;
                 PSM_Speed                  : uint8_t;
                 PSM_Tempo                  : uint8_t;
                 PSM_Unknown03              : uint8_t;
                 PSM_Unknown04              : uint16_t;
                 PSM_OrderLength            : uint16_t;
                 PSM_PatternNumber          : uint16_t;
                 PSM_SampleNumber           : uint16_t;
                 PSM_ChannelNumber          : uint16_t;
                 PSM_ChannelUsed            : uint16_t;
                 PSM_OrderPosition          : LONGINT;
                 PSM_ChannelSettingPosition : LONGINT;
                 PSM_PatternPosition        : LONGINT;
                 PSM_SamplePosition         : LONGINT;
                { *** perhaps there are some more infos in a larger header,
                      but i have not decoded it and so it apears here NOT }
                END;

  PPSM_Sample = ^TPSM_Sample;
  TPSM_Sample = RECORD
                 PSM_SampleFileName  : ARRAY[01..12] OF int8_t;
                 PSM_SampleByte00    : uint8_t;
                 PSM_SampleName      : ARRAY[01..22] OF int8_t;
                 PSM_SampleUnknown00 : ARRAY[01..02] OF uint8_t;
                 PSM_SamplePosition  : LONGINT;
                 PSM_SampleUnknown01 : ARRAY[01..04] OF uint8_t;
                 PSM_SampleNumber    : uint8_t;
                 PSM_SampleFlags     : uint16_t;
                 PSM_SampleLength    : LONGINT;
                 PSM_SampleLoopBegin : LONGINT;
                 PSM_SampleLoopEnd   : LONGINT;
                 PSM_Unknown03       : uint8_t;
                 PSM_SampleVolume    : uint8_t;
                 PSM_SampleC5Speed   : uint16_t;
                END;

  PPSM_SampleList = ^TPSM_SampleList;
  TPSM_SampleList = ARRAY[01..c_PSM_MaxSample] OF TPSM_Sample;

  PPSM_Order = ^TPSM_Order;
  TPSM_Order = ARRAY[00..c_PSM_MaxOrder] OF uint8_t;

  PPSM_ChannelSettings = ^TPSM_ChannelSettings;
  TPSM_ChannelSettings = ARRAY[00..c_PSM_MaxChannel] OF uint8_t;

 CONST
  PSM_NotesInPattern   : uint8_t = $00;
  PSM_ChannelInPattern : uint8_t = $00;

 CONST
  c_PSM_SetSpeed = 60;

 FUNCTION PSM_Size(FileName : STRING;FilePosition : LONGINT) : LONGINT;
  BEGIN
  END;

 PROCEDURE PSM_UnpackPattern(VAR Source,Destination;PatternLength : uint16_t);
  VAR
   Witz : ARRAY[00..04] OF uint16_t;
   I1,I2        : uint16_t;
   I3,I4        : uint16_t;
   TopicalByte  : ^uint8_t;
   Pattern      : PUnpackedPattern;
   ChannelP     : uint8_t;
   NoteP        : uint8_t;
   InfoByte     : uint8_t;
   CodeByte     : uint8_t;
   InfoWord     : uint16_t;
   Effect       : uint8_t;
   Opperand     : uint8_t;
   Panning      : uint8_t;
   Volume       : uint8_t;
   PrevInfo     : uint8_t;
   InfoIndex    : uint8_t;
  BEGIN
   Pattern     := @Destination;
   TopicalByte := @Source;
  { *** Initialize patttern }
   FOR I2 := 0 TO c_Maximum_NoteIndex DO
    FOR I3 := 0 TO c_Maximum_ChannelIndex DO
     BEGIN
      Pattern^[I2,I3,c_Pattern_NoteIndex]     := $FF;
      Pattern^[I2,I3,c_Pattern_SampleIndex]   := $00;
      Pattern^[I2,I3,c_Pattern_VolumeIndex]   := $FF;
      Pattern^[I2,I3,c_Pattern_PanningIndex]  := $FF;
      Pattern^[I2,I3,c_Pattern_EffectIndex]   := $00;
      Pattern^[I2,I3,c_Pattern_OpperandIndex] := $00;
     END;
  { *** Byte-pointer on first pattern-entry }
   ChannelP    := $00;
   NoteP       := $00;
   InfoByte    := $00;
   PrevInfo    := $00;
   InfoIndex   := $02;
  { *** read notes in pattern }
   PSM_NotesInPattern   := TopicalByte^; INC(TopicalByte); DEC(PatternLength); INC(InfoIndex);
   PSM_ChannelInPattern := TopicalByte^; INC(TopicalByte); DEC(PatternLength); INC(InfoIndex);
  { *** unpack pattern }
   WHILE (INTEGER(PatternLength) > 0) AND (NoteP < c_Maximum_NoteIndex) DO
    BEGIN
    { *** Read info-byte }
     InfoByte := TopicalByte^; INC(TopicalByte); DEC(PatternLength); INC(InfoIndex);
     IF InfoByte <> $00 THEN
      BEGIN
       ChannelP := InfoByte AND $0F;
       IF InfoByte AND 128 = 128 THEN { note and sample }
        BEGIN
        { *** read note }
         CodeByte := TopicalByte^; INC(TopicalByte); DEC(PatternLength);
         DEC(CodeByte);
         CodeByte := CodeByte MOD 12 * 16 + CodeByte DIV 12 + 2;
         Pattern^[NoteP,ChannelP,c_Pattern_NoteIndex] := CodeByte;
        { *** read sample }
         CodeByte := TopicalByte^; INC(TopicalByte); DEC(PatternLength);
         Pattern^[NoteP,ChannelP,c_Pattern_SampleIndex] := CodeByte;
        END;
       IF InfoByte AND 64 = 64 THEN { Volume }
        BEGIN
         CodeByte := TopicalByte^; INC(TopicalByte); DEC(PatternLength);
         Pattern^[NoteP,ChannelP,c_Pattern_VolumeIndex] := CodeByte;
        END;
       IF InfoByte AND 32 = 32 THEN { effect AND opperand }
        BEGIN
         Effect   := TopicalByte^; INC(TopicalByte); DEC(PatternLength);
         Opperand := TopicalByte^; INC(TopicalByte); DEC(PatternLength);
         CASE Effect OF
          c_PSM_SetSpeed:
           BEGIN
            Effect := c_I_Set_Speed;
           END;
          ELSE
           BEGIN
            Effect   := c_I_NoEffect;
            Opperand := $00;
           END;
         END;
         Pattern^[NoteP,ChannelP,c_Pattern_EffectIndex]   := Effect;
         Pattern^[NoteP,ChannelP,c_Pattern_OpperandIndex] := Opperand;
        END;
      END ELSE INC(NoteP);
    END;
  END;

 PROCEDURE PSM_Load(FileName : STRING;FilePosition : LONGINT;VAR Module : PModule;VAR ErrorCode : uint16_t);
 { *** caution : Module has to be inited before!!!! }
  VAR
   Header             : PPSM_Header;
   Sample             : PPSM_SampleList;
   Orderlist              : PPSM_Order;
   ChannelSettings    : PPSM_ChannelSettings;
   MultiPurposeBuffer : PByteArray;
   PatternBuffer      : PUnpackedPattern;
   TopicalParaPointer : uint16_t;

   InFile : FILE;
   I1,I2  : uint16_t;
   I3,I4  : uint16_t;
   TempW  : uint16_t;
   TempB  : uint8_t;
   TempP  : PByteArray;
   TempI  : INTEGER;
  { *** copy-vars for loop-extension }
   CopySource      : LONGINT;
   CopyDestination : LONGINT;
   CopyLength      : LONGINT;
  BEGIN
  { *** try to open file }
   ASSIGN(InFile,FileName);
{$I-}
   RESET(InFile,1);
{$I+}
   IF IORESULT <> $00 THEN
    BEGIN
     EXIT;
    END;
{$I-}
  { *** seek start of module }
   IF FILESIZE(InFile) < FilePosition THEN
    BEGIN
     EXIT;
    END;
   SEEK(InFile,FilePosition);
  { *** look for enough memory for temporary variables }
   IF MEMAVAIL < SIZEOF(TPSM_Header)       + SIZEOF(TPSM_SampleList) +
                 SIZEOF(TPSM_Order)        + SIZEOF(TPSM_ChannelSettings) +
                 SIZEOF(TByteArray)        + SIZEOF(TUnpackedPattern)
   THEN
    BEGIN
     EXIT;
    END;
  { *** init dynamic variables }
   NEW(Header);
   NEW(Sample);
   NEW(Orderlist);
   NEW(ChannelSettings);
   NEW(MultiPurposeBuffer);
   NEW(PatternBuffer);
  { *** read header }
   BLOCKREAD(InFile,Header^,SIZEOF(TPSM_Header));
  { *** test if this is a DSM-file }
   IF NOT ((Header^.PSM_Sign[1] = 'P') AND (Header^.PSM_Sign[2] = 'S')   AND
           (Header^.PSM_Sign[3] = 'M') AND (Header^.PSM_Sign[4] = #254)) THEN
    BEGIN
     ErrorCode := c_NoValidFileFormat;
     CLOSE(InFile);
     EXIT;
    END;
  { *** read order }
   SEEK(InFile,FilePosition + Header^.PSM_OrderPosition);
   BLOCKREAD(InFile,Orderlist^,Header^.PSM_OrderLength);
  { *** read channelsettings }
   SEEK(InFile,FilePosition + Header^.PSM_ChannelSettingPosition);
   BLOCKREAD(InFile,ChannelSettings^,SIZEOF(TPSM_ChannelSettings));
  { *** read samplelist }
   SEEK(InFile,FilePosition + Header^.PSM_SamplePosition);
   BLOCKREAD(InFile,Sample^,Header^.PSM_SampleNumber * SIZEOF(TPSM_Sample));
  { *** copy header to intern NTMIK-structure }
   Module^.Module_Sign                 := 'MF';
   Module^.Module_FileFormatVersion    := $0100;
   Module^.Module_SampleNumber         := Header^.PSM_SampleNumber;
   Module^.Module_PatternNumber        := Header^.PSM_PatternNumber;
   Module^.Module_OrderLength          := Header^.PSM_OrderLength;
   Module^.Module_ChannelNumber        := Header^.PSM_ChannelNumber+1;
   Module^.Module_Initial_GlobalVolume := 64;
   Module^.Module_Initial_MasterVolume := $C0;
   Module^.Module_Initial_Speed        := Header^.PSM_Speed;
   Module^.Module_Initial_Tempo        := Header^.PSM_Tempo;
{ *** paragraph 01 start }
   Module^.Module_Flags                := c_Module_Flags_ZeroVolume        * uint8_t(1) +
                                          c_Module_Flags_Stereo            * uint8_t(1) +
                                          c_Module_Flags_ForceAmigaLimits  * uint8_t(0) +
                                          c_Module_Flags_Panning           * uint8_t(1) +
                                          c_Module_Flags_Surround          * uint8_t(1) +
                                          c_Module_Flags_QualityMixing     * uint8_t(1) +
                                          c_Module_Flags_FastVolumeSlides  * uint8_t(0) +
                                          c_Module_Flags_SpecialCustomData * uint8_t(0) +
                                          c_Module_Flags_SongName          * uint8_t(1);
   I1 := $01;
   WHILE (Header^.PSM_SongName[I1] > #00) AND (I1 < c_Module_SongNameLength) DO
    BEGIN
     Module^.Module_Name[I1] := Header^.PSM_SongName[I1];
     INC(I1);
    END;
   Module^.Module_Name[c_Module_SongNameLength] := #00;
  { *** Init channelsettings }
   FOR I1 := 0 TO c_Maximum_ChannelIndex DO
    BEGIN
     IF I1 < Header^.PSM_ChannelUsed THEN
      BEGIN
      { *** channel enabled }
       Module^.Module_ChannelSettingPointer^[I1].ChannelSettings_GlobalVolume := 64;
       Module^.Module_ChannelSettingPointer^[I1].ChannelSettings_Panning      := (ChannelSettings^[I1]) * $08;
       Module^.Module_ChannelSettingPointer^[I1].ChannelSettings_Code         := I1 + $10 * uint8_t(ChannelSettings^[I1] > $08) +
                                             c_ChannelSettings_Code_ChannelEnabled   * uint8_t(1) +
                                             c_ChannelSettings_Code_ChannelDigital   * uint8_t(1);
       Module^.Module_ChannelSettingPointer^[I1].ChannelSettings_Controls     :=
                                             c_ChannelSettings_Controls_EnhancedMode * uint8_t(1) +
                                             c_ChannelSettings_Controls_SurroundMode * uint8_t(0);
      END
     ELSE
      BEGIN
      { *** channel disabled }
       Module^.Module_ChannelSettingPointer^[I1].ChannelSettings_GlobalVolume := $00;
       Module^.Module_ChannelSettingPointer^[I1].ChannelSettings_Panning      := $00;
       Module^.Module_ChannelSettingPointer^[I1].ChannelSettings_Code         := $00;
       Module^.Module_ChannelSettingPointer^[I1].ChannelSettings_Controls     := $00;
      END;
    END;
  { *** init and copy order }
   FILLCHAR(Module^.Module_OrderPointer^,c_Maximum_OrderIndex+1,$FF);
   MOVE(Orderlist^,Module^.Module_OrderPointer^,Header^.PSM_OrderLength);
  { *** read pattern }
   SEEK(InFile,FilePosition + Header^.PSM_PatternPosition);
   NTMIK_LoaderPatternNumber := Header^.PSM_PatternNumber-1;
   FOR I1 := 0 TO Header^.PSM_PatternNumber-1 DO
    BEGIN
     NTMIK_LoadPatternProcedure;
    { *** read length }
     BLOCKREAD(InFile,TempW,2);
    { *** read pattern }
     BLOCKREAD(InFile,MultiPurposeBuffer^,TempW-2);
    { *** unpack pattern and set notes per channel to 64 }
     PSM_UnpackPattern(MultiPurposeBuffer^,PatternBuffer^,TempW);
     NTMIK_PackPattern(MultiPurposeBuffer^,PatternBuffer^,PSM_NotesInPattern);
     TempW := uint16_t(256) * MultiPurposeBuffer^[01] + MultiPurposeBuffer^[00];
     GETMEM(Module^.Module_PatternPointer^[I1],TempW);
     MOVE(MultiPurposeBuffer^,Module^.Module_PatternPointer^[I1]^,TempW);
    { *** next pattern }
    END;
  { *** read samples }
   NTMIK_LoaderSampleNumber := Header^.PSM_SampleNumber;
   FOR I1 := 1 TO Header^.PSM_SampleNumber DO
    BEGIN
     NTMIK_LoadSampleProcedure;
    { *** get index for sample }
     I3 := Sample^[I1].PSM_SampleNumber;
    { *** clip PSM-sample }
     IF Sample^[I1].PSM_SampleLoopEnd > Sample^[I1].PSM_SampleLength
     THEN Sample^[I1].PSM_SampleLoopEnd := Sample^[I1].PSM_SampleLength;
    { *** init intern sample }
     NEW(Module^.Module_SamplePointer^[I3]);
     FILLCHAR(Module^.Module_SamplePointer^[I3]^,SIZEOF(TSample),$00);
     FILLCHAR(Module^.Module_SamplePointer^[I3]^.Sample_SampleName,c_Sample_SampleNameLength,#32);
     FILLCHAR(Module^.Module_SamplePointer^[I3]^.Sample_FileName,c_Sample_FileNameLength,#32);
    { *** copy informations to intern sample }
     I2 := $01;
     WHILE (Sample^[I1].PSM_SampleName[I2] > #00) AND (I2 < c_Sample_SampleNameLength) DO
      BEGIN
       Module^.Module_SamplePointer^[I3]^.Sample_SampleName[I2] := Sample^[I1].PSM_SampleName[I2];
       INC(I2);
      END;
     Module^.Module_SamplePointer^[I3]^.Sample_Sign              := 'DF';
     Module^.Module_SamplePointer^[I3]^.Sample_FileFormatVersion := $00100;
     Module^.Module_SamplePointer^[I3]^.Sample_Position          := $00000000;
     Module^.Module_SamplePointer^[I3]^.Sample_Selector          := $0000;
     Module^.Module_SamplePointer^[I3]^.Sample_Volume            := Sample^[I1].PSM_SampleVolume;
     Module^.Module_SamplePointer^[I3]^.Sample_LoopCounter       := $00;
     Module^.Module_SamplePointer^[I3]^.Sample_C5Speed           := Sample^[I1].PSM_SampleC5Speed;
     Module^.Module_SamplePointer^[I3]^.Sample_Length            := Sample^[I1].PSM_SampleLength;
     Module^.Module_SamplePointer^[I3]^.Sample_LoopBegin         := Sample^[I1].PSM_SampleLoopBegin;
     Module^.Module_SamplePointer^[I3]^.Sample_LoopEnd           := Sample^[I1].PSM_SampleLoopEnd;
    { *** now it's time for the flags }
     Module^.Module_SamplePointer^[I3]^.Sample_Flags :=
                                 c_Sample_Flags_DigitalSample      * uint8_t(1) +
                                 c_Sample_Flags_8BitSample         * uint8_t(1) +
                                 c_Sample_Flags_UnsignedSampleData * uint8_t(1) +
                                 c_Sample_Flags_Packed             * uint8_t(0) +
                                 c_Sample_Flags_LoopCounter        * uint8_t(0) +
                                 c_Sample_Flags_SampleName         * uint8_t(1) +
                                 c_Sample_Flags_LoopActive         *
                             uint8_t(Sample^[I1].PSM_SampleFlags AND (LONGINT(1) SHL 15) = (LONGINT(1) SHL 15));
    { *** alloc memory for sample-data }
     E_Getmem(Module^.Module_SamplePointer^[I3]^.Sample_Selector,
              Module^.Module_SamplePointer^[I3]^.Sample_Position,
              Module^.Module_SamplePointer^[I3]^.Sample_Length + c_LoopExtensionSize);
    { *** read out data }
     EPT(TempP).p_Selector := Module^.Module_SamplePointer^[I3]^.Sample_Selector;
     EPT(TempP).p_Offset   := $0000;
     SEEK(InFile,Sample^[I1].PSM_SamplePosition);
     E_BLOCKREAD(InFile,TempP^,Module^.Module_SamplePointer^[I3]^.Sample_Length);
    { *** 'coz the samples are signed in a DSM-file -> PC-fy them }
     IF Module^.Module_SamplePointer^[I3]^.Sample_Length > 4 THEN
      BEGIN
       CopyLength := Module^.Module_SamplePointer^[I3]^.Sample_Length;
      { *** decode sample }
       ASM
        DB 066h; MOV CX,uint16_t PTR CopyLength
       { *** load sample selector }
                 MOV ES,uint16_t PTR TempP[00002h]
        DB 066h; XOR SI,SI
        DB 066h; XOR DI,DI
                 XOR AH,AH
       { *** conert all bytes }
                @@MainLoop:
        DB 026h; DB 067h; LODSB
                 ADD AL,AH
                 MOV AH,AL
        DB 067h; STOSB
        DB 066h; LOOP @@MainLoop
       END;
      { *** make samples unsigned }
       ASM
        DB 066h; MOV CX,uint16_t PTR CopyLength
       { *** load sample selector }
                 MOV ES,uint16_t PTR TempP[00002h]
        DB 066h; XOR SI,SI
        DB 066h; XOR DI,DI
       { *** conert all bytes }
                @@MainLoop:
        DB 026h; DB 067h; LODSB
                 SUB AL,080h
        DB 067h; STOSB
        DB 066h; LOOP @@MainLoop
       END;
      { *** Create Loop-Extension }
       IF Module^.Module_SamplePointer^[I3]^.Sample_Flags AND c_Sample_Flags_LoopActive = c_Sample_Flags_LoopActive THEN
        BEGIN
         CopySource      := Module^.Module_SamplePointer^[I3]^.Sample_LoopBegin;
         CopyDestination := Module^.Module_SamplePointer^[I3]^.Sample_LoopEnd;
         CopyLength      := CopyDestination - CopySource;
         ASM
         { *** load sample-selector }
                   MOV ES,uint16_t PTR TempP[00002h]
          DB 066h; MOV DI,uint16_t PTR CopyDestination
         { *** calculate number of full sample-loops to copy }
                   XOR DX,DX
                   MOV AX,c_LoopExtensionSize
                   MOV BX,uint16_t PTR CopyLength
                   DIV BX
                   OR AX,AX
                   JE @@NoFullLoop
         { *** copy some full-loops (size=bx) }
                   MOV CX,AX
                  @@InnerLoop:
                   PUSH CX
          DB 066h; MOV SI,uint16_t PTR CopySource
                   MOV CX,BX
          DB 0F3h; DB 026h,067h,0A4h { REP MOVS uint8_t PTR ES:[EDI],ES:[ESI] }
                   POP CX
                   LOOP @@InnerLoop
                  @@NoFullLoop:
         { *** calculate number of rest-bytes to copy }
          DB 066h; MOV SI,uint16_t PTR CopySource
                   MOV CX,DX
          DB 0F3h; DB 026h,067h,0A4h { REP MOVS uint8_t PTR ES:[EDI],ES:[ESI] }
         END;
        END
       ELSE
        BEGIN
         CopyDestination := Module^.Module_SamplePointer^[I3]^.Sample_Length;
         ASM
         { *** load sample-selector }
                   MOV ES,uint16_t PTR TempP[00002h]
          DB 066h; MOV DI,uint16_t PTR CopyDestination
         { *** clear extension }
                   MOV CX,c_LoopExtensionSize
                   MOV AL,080h
          DB 0F3h; DB 067h,0AAh       { REP STOS uint8_t PTR ES:[EDI] }
         END;
        END;
      END;
    { *** next sample }
    END;
  { *** init period-ranges }
   NTMIK_MaximumPeriod := $0000D600 SHR 1;
   NTMIK_MinimumPeriod := $0000D600 SHR 8;
  { *** close file }
   CLOSE(InFile);
  { *** dispose all dynamic variables }
   DISPOSE(Header);
   DISPOSE(Sample);
   DISPOSE(Orderlist);
   DISPOSE(ChannelSettings);
   DISPOSE(MultiPurposeBuffer);
   DISPOSE(PatternBuffer);
  { *** set errorcode to noerror }
   ErrorCode := c_NoError;
  END;

*/

