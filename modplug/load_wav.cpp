/*
 * This source code is public domain.
 *
 * Authors: Olivier Lapicque <olivierl@jps.net>
*/
#define NEED_BYTESWAP

#include "stdafx.h"
#include "sndfile.h"

#define WAVE_FORMAT_PCM 1
#ifndef WAVE_FORMAT_EXTENSIBLE
# define WAVE_FORMAT_EXTENSIBLE  0xFFFE
#endif


// WAV file support

bool CSoundFile::ReadWav(const unsigned char *data, unsigned int dwMemLength)
{
        WAVEFILEHEADER   phdr;
        WAVEFORMATHEADER pfmt;
        WAVEDATAHEADER   pdata;
        unsigned int dwMemPos = 0;

        if (!data ||
            dwMemLength < (unsigned int)(sizeof(WAVEFORMATHEADER) + sizeof(WAVEFILEHEADER)))
                return 0;

        memcpy(&phdr, data, sizeof(phdr));
        memcpy(&pfmt, data + sizeof(phdr), sizeof(pfmt));

        phdr.id_RIFF       = bswapLE32(phdr.id_RIFF);
        phdr.filesize      = bswapLE32(phdr.filesize);
        phdr.id_WAVE       = bswapLE32(phdr.id_WAVE);

        pfmt.id_fmt        = bswapLE32(pfmt.id_fmt);
        pfmt.hdrlen        = bswapLE32(pfmt.hdrlen);
        pfmt.format        = bswapLE16(pfmt.format);
        pfmt.channels      = bswapLE16(pfmt.channels);
        pfmt.freqHz        = bswapLE32(pfmt.freqHz);
        pfmt.bytessec      = bswapLE32(pfmt.bytessec);
        pfmt.samplesize    = bswapLE16(pfmt.samplesize);
        pfmt.bitspersample = bswapLE16(pfmt.bitspersample);

        if (phdr.id_RIFF != IFFID_RIFF ||
            phdr.id_WAVE != IFFID_WAVE || 
            pfmt.id_fmt != IFFID_fmt)
                return 0;

        dwMemPos = sizeof(WAVEFILEHEADER) + 8 + pfmt.hdrlen;

        if (dwMemPos + 8 >= dwMemLength ||
            (pfmt.format != WAVE_FORMAT_PCM && pfmt.format != WAVE_FORMAT_EXTENSIBLE) ||
            pfmt.channels > 4 ||
            !pfmt.channels ||
            !pfmt.freqHz ||
            pfmt.bitspersample & 7 ||
            pfmt.bitspersample < 8 ||
            pfmt.bitspersample > 32)
                return 0;

        for (;;) {
                memcpy(&pdata, data + dwMemPos, sizeof(pdata));
                pdata.id_data = bswapLE32(pdata.id_data);
                pdata.length  = bswapLE32(pdata.length);

                if (pdata.id_data == IFFID_data)
                    break;

                dwMemPos += pdata.length + 8;

                if (dwMemPos + 8 >= dwMemLength)
                    return 0;
        }

        m_nType         = MOD_TYPE_WAV;
        m_nSamples      = 0;
        m_nInstruments  = 0;
        m_nChannels     = 4;
        m_nDefaultSpeed = 8;
        m_nDefaultTempo = 125;
        m_dwSongFlags  |= SONG_LINEARSLIDES; // For no resampling

        Order[0] = 0;
        Order[1] = 0xFF;
        PatternSize[0] = PatternSize[1] = 64;
        PatternAllocSize[0] = PatternAllocSize[1] = 64;

        if ((Patterns[0] = AllocatePattern(64, 4)) == NULL)
                return 1;

        if ((Patterns[1] = AllocatePattern(64, 4)) == NULL)
                return 1;

        unsigned int samplesize = (pfmt.channels * pfmt.bitspersample) >> 3;
        unsigned int len = pdata.length, bytelen;

        if (dwMemPos + len > dwMemLength - 8)
                len = dwMemLength - dwMemPos - 8;

        len    /= samplesize;
        bytelen = len;

        if (pfmt.bitspersample >= 16)
                bytelen *= 2;

        if (len > MAX_SAMPLE_LENGTH)
                len = MAX_SAMPLE_LENGTH;

        if (!len)
                return 1;


        // Setting up module length
        unsigned int dwTime = ((len * 50) / pfmt.freqHz) + 1;
        unsigned int framesperrow = (dwTime + 63) / 63;

        if (framesperrow < 4)
            framesperrow = 4;

        unsigned int norders = 1;

        while (framesperrow >= 0x20) {
                Order[norders++] = 1;
                Order[norders]   = 0xFF;
                framesperrow     = (dwTime + (64 * norders - 1)) / (64 * norders);

                if (norders >= MAX_ORDERS - 1)
                    break;
        }

        m_nDefaultSpeed = framesperrow;

        for (unsigned int i = 0; i < 4; i++) {
                ChnSettings[i].nPan    = (i & 1) ? 256 : 0;
                ChnSettings[i].nVolume = 64;
                ChnSettings[i].dwFlags = 0;
        }

        // Setting up speed command
        MODCOMMAND *pcmd = Patterns[0];
        pcmd[0].command = CMD_SPEED;
        pcmd[0].param   = (unsigned char) m_nDefaultSpeed;
        pcmd[0].note    = 5*12+1;
        pcmd[0].instr   = 1;
        pcmd[1].note    = pcmd[0].note;
        pcmd[1].instr   = pcmd[0].instr;

        m_nSamples = pfmt.channels;

        // Support for Multichannel Wave
        for (unsigned int nChn = 0; nChn < m_nSamples; nChn++) {
                MODINSTRUMENT *pins = &Ins[nChn + 1];
                pcmd[nChn].note  = pcmd[0].note;
                pcmd[nChn].instr = (unsigned char)(nChn + 1);

                pins->nLength    = len;
                pins->nC5Speed   = pfmt.freqHz;
                pins->nVolume    = 256;
                pins->nPan       = 128;
                pins->nGlobalVol = 64;
                pins->uFlags     = (WORD)((pfmt.bitspersample >= 16) ? CHN_16BIT : 0);
                pins->uFlags    |= CHN_PANNING;

                if (m_nSamples > 1) {
                        switch (nChn)
                        {
                        case 0: 
                                pins->nPan = 0;
                                break;

                        case 1: 
                                pins->nPan = 256;
                                break;

                        case 2: 
                                pins->nPan = (WORD)((m_nSamples == 3) ? 128 : 64); 
                                pcmd[nChn].command = CMD_S3MCMDEX; 
                                pcmd[nChn].param = 0x91; 
                                break;

                        case 3: 
                                pins->nPan = 192; 
                                pcmd[nChn].command = CMD_S3MCMDEX; 
                                pcmd[nChn].param = 0x91;
                                break;

                        default:
                                pins->nPan = 128;
                                break;
                        }
                }

                if ((pins->pSample = AllocateSample(bytelen + 8)) == NULL)
                        return 1;

                if (pfmt.bitspersample >= 16) {
                        int slsize = pfmt.bitspersample >> 3;
                        signed short *p = (signed short *)pins->pSample;
                        signed char *psrc = (signed char *)(data + dwMemPos + 8 + nChn * slsize + slsize - 2);

                        for (unsigned int i = 0; i < len; i++) {
                                p[i] = bswapLE16(*((signed short *)psrc));
                                psrc += samplesize;
                        }

                        p[len + 1] = p[len] = p[len - 1];
                }
                else {
                        signed char *p = (signed char *)pins->pSample;
                        signed char *psrc = (signed char *)(data+dwMemPos+8+nChn);

                        for (unsigned int i = 0; i < len; i++) {
                                p[i] = (signed char)((*psrc) + 0x80);
                                psrc += samplesize;
                        }

                        p[len + 1] = p[len] = p[len - 1];
                }
        }

        return 1;
}


////////////////////////////////////////////////////////////////////////
// IMA ADPCM Support

#pragma pack(push, 1)

typedef struct IMAADPCMBLOCK
{
        WORD sample;
        unsigned char index;
        unsigned char Reserved;
} DVI_ADPCMBLOCKHEADER;

#pragma pack(pop)


static const int gIMAUnpackTable[90] = 
{
    7,     8,     9,     10,    11,    12,    13,    14,
    16,    17,    19,    21,    23,    25,    28,    31,
    34,    37,    41,    45,    50,    55,    60,    66,
    73,    80,    88,    97,    107,   118,   130,   143,
    157,   173,   190,   209,   230,   253,   279,   307,
    337,   371,   408,   449,   494,   544,   598,   658,
    724,   796,   876,   963,   1060,  1166,  1282,  1411,
    1552,  1707,  1878,  2066,  2272,  2499,  2749,  3024,
    3327,  3660,  4026,  4428,  4871,  5358,  5894,  6484,
    7132,  7845,  8630,  9493,  10442, 11487, 12635, 13899,
    15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794,
    32767, 0
};


int IMAADPCMUnpack16(signed short *pdest, unsigned int nLen, unsigned char *psrc, unsigned int dwBytes, unsigned int pkBlkAlign)
{
        static const int gIMAIndexTab[8] ={-1, -1, -1, -1, 2, 4, 6, 8};
        unsigned int nPos;
        int value;

        if (nLen < 4 || !pdest || !psrc ||
            pkBlkAlign < 5 || pkBlkAlign > dwBytes)
                return 0;

        nPos = 0;

        while (nPos < nLen && dwBytes > 4) {
                int nIndex;

                value    = bswapLE16(*((short int *) psrc));
                nIndex   = bswapLE16((short int) psrc[2]);
                psrc    += 4;
                dwBytes -= 4;

                pdest[nPos++] = (short int)value;

                for (unsigned int i = 0; i < (pkBlkAlign - 4) * 2 && nPos < nLen && dwBytes; i++) {
                        unsigned char delta;

                        if (i & 1) {
                                delta = (unsigned char)(((*(psrc++)) >> 4) & 0x0F);
                                dwBytes--;
                        } 
                        else {
                                delta = (unsigned char)((*psrc) & 0x0F);
                        }

                        int v = gIMAUnpackTable[nIndex % 90] >> 3;

                        if (delta & 1) v += gIMAUnpackTable[nIndex] >> 2;
                        if (delta & 2) v += gIMAUnpackTable[nIndex] >> 1;
                        if (delta & 4) v += gIMAUnpackTable[nIndex];
                        if (delta & 8) value -= v;
                        else value += v;

                        nIndex += gIMAIndexTab[delta & 7];

                             if (nIndex < 0)  nIndex = 0;
                        else if (nIndex > 88) nIndex = 88;

                             if (value > 32767)  value = 32767;
                        else if (value < -32768) value = -32768;

                        pdest[nPos++] = (short int)value;
                }
        }

        return 1;
}

