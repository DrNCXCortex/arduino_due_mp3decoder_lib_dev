#include <Arduino.h>
#include <assert.h>

/********************************************************************************/
/*                                                                              */
/*  GLOBAL SETUP VAR                                                            */
/*                                                                              */
/********************************************************************************/


#define LITTLE_ENDIAN 1  //very importatnt for arduino DUE set 1!!!!!!!!

typedef unsigned int UINT32;
typedef int INT32;
typedef int SAMPLEINT;

static unsigned char look_u[8192];

/********************************************************************************/
/*                                                                              */
/*  small_header.h                                                              */
/*                                                                              */
/********************************************************************************/
typedef union
{
   int s;
   float x;
}
_SAMPLE;

typedef struct
{
   int in_bytes;
   int out_bytes;
}
IN_OUT;

//AUDIO_DECODE_ROUTINE IN_OUT;
/********************************************************************************/
/*                                                                              */
/*  mp3struc.h                                                                  */
/*                                                                              */
/********************************************************************************/

typedef void (*SBT_FUNCTION) (float *sample, short *pcm, int n);
typedef void (*XFORM_FUNCTION) (void *pcm, int igr);
typedef IN_OUT(*DECODE_FUNCTION) (unsigned char *bs, unsigned char *pcm);

typedef struct
{
  union
  {
    struct
    {
      SBT_FUNCTION sbt; 

      float cs_factor[3][64]; // 768 bytes

      int   nbat[4];
      int   bat[4][16];
      int   max_sb;   
      int   stereo_sb;
      int   bit_skip;

      float*  cs_factorL1;
      int   nbatL1;

    };//L1_2;

    struct 
    {
      SBT_FUNCTION sbt_L3;
      XFORM_FUNCTION Xform;
      DECODE_FUNCTION decode_function;

      _SAMPLE  __sample[2][2][576];    // if this isn't kept per stream then the decode breaks up

    // the 4k version of these 2 seems to work for everything, but I'm reverting to original 8k for safety jic.
    //
    #define NBUF (8*1024)
    #define BUF_TRIGGER (NBUF-1500)
//    #define NBUF (4096) // 2048 works for all except 133+ kbps VBR files, 4096 copes with these
//    #define BUF_TRIGGER ((NBUF/4)*3)

      unsigned char buf[NBUF];
      int   buf_ptr0;
      int   buf_ptr1;
      int   main_pos_bit;


      int   band_limit_nsb;
      int     nBand[2][22];   /* [long/short][cb] */
      int     sfBandIndex[2][22];   /* [long/short][cb] */
      int     half_outbytes;
      int     crcbytes;
      int     nchan;
      int     ms_mode;
      int     is_mode;
      unsigned int zero_level_pcm;
      int     mpeg25_flag;
      int     band_limit;
      int     band_limit21;
      int     band_limit12;
      int     gain_adjust;
      int     ncbl_mixed;
    };//L3;
  };
  // from csbt.c...
  //
  // if this isn't kept per stream then the decode breaks up
  signed int  vb_ptr;     // 
  signed int  vb2_ptr;    // 
  float   vbuf[512];    //
  float   vbuf2[512];   // this can be lost if we stick to mono samples

  // L3 only...
  //
  int     sr_index; // L3 only (99%)
  int     id;

  // any type...
  //
  int     outvalues;
  int     outbytes;
  int     framebytes;
  int     pad;
  int     nsb_limit;  

  // stuff added now that the game uses streaming MP3s...
  //
  byte    *pbSourceData;      // a useful dup ptr only, this whole struct will be owned by an sfx_t struct that has the actual data ptr field
  int     iSourceBytesRemaining;
  int     iSourceReadIndex;
  int     iSourceFrameBytes;
#ifdef _DEBUG
  int     iSourceFrameCounter;  // not really important
#endif
  int     iBytesDecodedTotal;
  int     iBytesDecodedThisPacket;// not sure how useful this will be, it's only per-frame, so will always be full frame size (eg 2304 or below for mono) except possibly for the last frame?

  int     iRewind_FinalReductionCode;
  int     iRewind_FinalConvertCode;
  int     iRewind_SourceBytesRemaining;
  int     iRewind_SourceReadIndex;
  byte    bDecodeBuffer[2304*2];  // *2 to allow for stereo now
  int     iCopyOffset;      // used for painting to DMA-feeder, since 2304 won't match the size it wants

  // some new stuff added for dynamic music, to allow "how many seconds left to play" queries...
  //
  // ( m_lengthInSeconds = ((iUnpackedDataLength / iRate) / iChannels) / iWidth; )
  //
  // Note that these fields are only valid/initialised if MP3Stream_InitPlayingTimeFields() was called.
  //  If not, this->iTimeQuery_UnpackedLength will be zero.
  //
  int     iTimeQuery_UnpackedLength;  
  int     iTimeQuery_SampleRate;
  int     iTimeQuery_Channels;
  int     iTimeQuery_Width;

} MP3STREAM, *LP_MP3STREAM;

#define MP3STUFF_KNOWN

/********************************************************************************/
/*                                                                              */
/*  mhead.h                                                                     */
/*                                                                              */
/********************************************************************************/

/* portable copy of eco\mhead.h */
/* mpeg audio header   */
typedef struct
{
   int sync;      /* 1 if valid sync */
   int id;
   int option;
   int prot;
   int br_index;
   int sr_index;
   int pad;
   int private_bit;
   int mode;
   int mode_ext;
   int cr;
   int original;
   int emphasis;
}
MPEG_HEAD;

/* portable mpeg audio decoder, decoder functions */

typedef struct
{
   int channels;
   int outvalues;
   long samprate;
   int _bits;
   int framebytes;
   int type;
}
DEC_INFO;


/*
   int head_info(unsigned char *buf, unsigned int n, MPEG_HEAD * h);
   int head_info2(unsigned char *buf,
     unsigned int n, MPEG_HEAD * h, int *br);
  int head_info3(unsigned char *buf, unsigned int n, MPEG_HEAD *h, int*br, unsigned int *searchForward);
// head_info returns framebytes > 0 for success 
// audio_decode_init returns 1 for success, 0 for fail 
// audio_decode returns in_bytes = 0 on sync loss 

   int audio_decode_init(MPEG_HEAD * h, int framebytes_arg,
       int reduction_code, int transform_code, int convert_code,
       int freq_limit);
   void audio_decode_info(DEC_INFO * info);
   IN_OUT audio_decode(unsigned char *bs, short *pcm, unsigned char *pNextByteAfterData);

   int audio_decode8_init(MPEG_HEAD * h, int framebytes_arg,
       int reduction_code, int transform_code, int convert_code,
        int freq_limit);
   void audio_decode8_info(DEC_INFO * info);
   IN_OUT audio_decode8(unsigned char *bs, short *pcmbuf);
   */

/********************************************************************************/
/*                                                                              */
/*  mhead.c                                                                     */
/*                                                                              */
/********************************************************************************/


static const int mp_br_table[2][16] =
{{0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160, 0},
 {0, 32, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 384, 0}};
static const int mp_sr20_table[2][4] =
{{441, 480, 320, -999}, {882, 960, 640, -999}};

static const int mp_br_tableL1[2][16] =
{{0, 32, 48, 56, 64, 80, 96, 112, 128, 144, 160, 176, 192, 224, 256, 0},// mpeg2 
 {0, 32, 64, 96, 128, 160, 192, 224, 256, 288, 320, 352, 384, 416, 448, 0}};

static const int mp_br_tableL3[2][16] =
{{0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160, 0},      // mpeg 2 
 {0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 0}};



static int find_sync(unsigned char *buf, int n);
static int sync_scan(unsigned char *buf, int n, int i0);
static int sync_test(unsigned char *buf, int n, int isync, int padbytes);


/*--------------------------------------------------------------*/
int head_info(unsigned char *buf, unsigned int n, MPEG_HEAD * h)
{
   int framebytes;
   int mpeg25_flag;
  
   if (n > 10000)
      n = 10000;    /* limit scan for free format */



   h->sync = 0;
   //if ((buf[0] == 0xFF) && ((buf[1] & 0xF0) == 0xF0))
   if ((buf[0] == 0xFF) && ((buf[0+1] & 0xF0) == 0xF0))
   {
      mpeg25_flag = 0;    // mpeg 1 & 2

   }
   else if ((buf[0] == 0xFF) && ((buf[0+1] & 0xF0) == 0xE0))
   {
      mpeg25_flag = 1;    // mpeg 2.5

   }
   else
      return 0;     // sync fail

   h->sync = 1;
   if (mpeg25_flag)
      h->sync = 2;    //low bit clear signals mpeg25 (as in 0xFFE)

   h->id = (buf[0+1] & 0x08) >> 3;
   h->option = (buf[0+1] & 0x06) >> 1;
   h->prot = (buf[0+1] & 0x01);

   h->br_index = (buf[0+2] & 0xf0) >> 4;
   h->sr_index = (buf[0+2] & 0x0c) >> 2;
   h->pad = (buf[0+2] & 0x02) >> 1;
   h->private_bit = (buf[0+2] & 0x01);
   h->mode = (buf[0+3] & 0xc0) >> 6;
   h->mode_ext = (buf[0+3] & 0x30) >> 4;
   h->cr = (buf[0+3] & 0x08) >> 3;
   h->original = (buf[0+3] & 0x04) >> 2;
   h->emphasis = (buf[0+3] & 0x03);


// if( mpeg25_flag ) {
 //    if( h->sr_index == 2 ) return 0;   // fail 8khz
 //}


/* compute framebytes for Layer I, II, III */
   if (h->option < 1)
      return 0;
   if (h->option > 3)
      return 0;

   framebytes = 0;

   if (h->br_index > 0)
   {
      if (h->option == 3)
      {       /* layer I */
   framebytes =
      240 * mp_br_tableL1[h->id][h->br_index]
      / mp_sr20_table[h->id][h->sr_index];
   framebytes = 4 * framebytes;
      }
      else if (h->option == 2)
      {       /* layer II */
   framebytes =
      2880 * mp_br_table[h->id][h->br_index]
      / mp_sr20_table[h->id][h->sr_index];
      }
      else if (h->option == 1)
      {       /* layer III */
   if (h->id)
   {      // mpeg1

      framebytes =
         2880 * mp_br_tableL3[h->id][h->br_index]
         / mp_sr20_table[h->id][h->sr_index];
   }
   else
   {      // mpeg2

      if (mpeg25_flag)
      {     // mpeg2.2

         framebytes =
      2880 * mp_br_tableL3[h->id][h->br_index]
      / mp_sr20_table[h->id][h->sr_index];
      }
      else
      {
         framebytes =
      1440 * mp_br_tableL3[h->id][h->br_index]
      / mp_sr20_table[h->id][h->sr_index];
      }
   }
      }
   }
   else
      framebytes = find_sync(buf, n); /* free format */

  return framebytes;
}

/*--------------------------------------------------------------*/
int head_info2(unsigned char *buf, unsigned int n, MPEG_HEAD * h, int *br)
{
  int framebytes;
  
  /*---  return br (in bits/sec) in addition to frame bytes ---*/
  
  *br = 0;
  /*-- assume fail --*/
  framebytes = head_info(buf, n, h);
  
  if (framebytes == 0)
    return 0; 
  
  switch (h->option)
  {
    case 1: /* layer III */
    {
      if (h->br_index > 0)
        *br = 1000 * mp_br_tableL3[h->id][h->br_index];
      else
      {
        if (h->id)    // mpeg1
          
          *br = 1000 * framebytes * mp_sr20_table[h->id][h->sr_index] / (144 * 20);
        else
        {     // mpeg2
          
          if ((h->sync & 1) == 0) //  flags mpeg25
            
            *br = 500 * framebytes * mp_sr20_table[h->id][h->sr_index] / (72 * 20);
          else
            *br = 1000 * framebytes * mp_sr20_table[h->id][h->sr_index] / (72 * 20);
        }
      }
    }
    break;

    case 2: /* layer II */
    {
      if (h->br_index > 0)
        *br = 1000 * mp_br_table[h->id][h->br_index];
      else
        *br = 1000 * framebytes * mp_sr20_table[h->id][h->sr_index] / (144 * 20);
    }
    break;

    case 3: /* layer I */
    {
      if (h->br_index > 0)
        *br = 1000 * mp_br_tableL1[h->id][h->br_index];
      else
        *br = 1000 * framebytes * mp_sr20_table[h->id][h->sr_index] / (48 * 20);
    }
    break;

    default:

      return 0; // fuck knows what this is, but it ain't one of ours...
  }
    
    
  return framebytes;
}
/*--------------------------------------------------------------*/
int head_info3(unsigned char *buf, unsigned int n, MPEG_HEAD *h, int *br, unsigned int *searchForward) {
  unsigned int pBuf = 0;

  // jdw insertion...
   while ((pBuf < n) && !((buf[pBuf] == 0xFF) && 
          ((buf[pBuf+1] & 0xF0) == 0xF0 || (buf[pBuf+1] & 0xF0) == 0xE0))) 
   {
    pBuf++;
   }

   if (pBuf == n) return 0;

   *searchForward = pBuf;
   return head_info2(&(buf[pBuf]),n,h,br);
}

/*--------------------------------------------------------------*/

static int compare(unsigned char *buf, unsigned char *buf2)
{
   if (buf[0] != buf2[0])
      return 0;
   if (buf[1] != buf2[1])
      return 0;
   return 1;
}
/*----------------------------------------------------------*/
/*-- does not scan for initial sync, initial sync assumed --*/
static int find_sync(unsigned char *buf, int n)
{
   int i0, isync, nmatch, pad;
   int padbytes, option;

/* mod 4/12/95 i0 change from 72, allows as low as 8kbits for mpeg1 */
   i0 = 24;
   padbytes = 1;
   option = (buf[1] & 0x06) >> 1;
   if (option == 3)
   {
      padbytes = 4;
      i0 = 24;      /* for shorter layer I frames */
   }

   pad = (buf[2] & 0x02) >> 1;

   n -= 3;      /*  need 3 bytes of header  */

   while (i0 < 2000)
   {
      isync = sync_scan(buf, n, i0);
      i0 = isync + 1;
      isync -= pad;
      if (isync <= 0)
   return 0;
      nmatch = sync_test(buf, n, isync, padbytes);
      if (nmatch > 0)
   return isync;
   }

   return 0;
}
/*------------------------------------------------------*/
/*---- scan for next sync, assume start is valid -------*/
/*---- return number bytes to next sync ----------------*/
static int sync_scan(unsigned char *buf, int n, int i0)
{
   int i;

   for (i = i0; i < n; i++)
      if (compare(buf, buf + i))
   return i;

   return 0;
}
/*------------------------------------------------------*/
/*- test consecutative syncs, input isync without pad --*/
static int sync_test(unsigned char *buf, int n, int isync, int padbytes)
{
   int i, nmatch, pad;

   nmatch = 0;
   for (i = 0;;)
   {
      pad = padbytes * ((buf[i + 2] & 0x02) >> 1);
      i += (pad + isync);
      if (i > n)
   break;
      if (!compare(buf, buf + i))
   return -nmatch;
      nmatch++;
   }
   return nmatch;
}   

/********************************************************************************/
/*                                                                              */
/*  L3.h                                                                        */
/*                                                                              */
/********************************************************************************/


#define GLOBAL_GAIN_SCALE (4*15)
/* #define GLOBAL_GAIN_SCALE 0 */




/*-----------------------------------------------------------*/
/*---- huffman lookup tables ---*/
/* endian dependent !!! */
#if LITTLE_ENDIAN
typedef union
{
   int ptr;
   struct
   {
      unsigned char signbits;
      unsigned char x;
      unsigned char y;
      unsigned char purgebits;  // 0 = esc

   }
   b;
}
HUFF_ELEMENT;

#else /* big endian machines */
typedef union
{
   int ptr;     /* int must be 32 bits or more */
   struct
   {
      unsigned char purgebits;  // 0 = esc

      unsigned char y;
      unsigned char x;
      unsigned char signbits;
   }
   b;
}
HUFF_ELEMENT;

#endif
/*--------------------------------------------------------------*/
typedef struct
{
   unsigned int bitbuf;
   int _bits;
   unsigned char *bs_ptr;
   unsigned char *bs_ptr0;
   unsigned char *bs_ptr_end; // optional for overrun test

}
BITDAT;

/*-- side info ---*/
typedef struct
{
   int part2_3_length;
   int big_values;
   int global_gain;
   int scalefac_compress;
   int window_switching_flag;
   int block_type;
   int mixed_block_flag;
   int table_select[3];
   int subblock_gain[3];
   int region0_count;
   int region1_count;
   int preflag;
   int scalefac_scale;
   int count1table_select;
}
GR;
typedef struct
{
   int mode;
   int mode_ext;
/*---------------*/
   int main_data_begin;   /* beginning, not end, my spec wrong */
   int private_bits;
/*---------------*/
   int scfsi[2];    /* 4 bit flags [ch] */
   GR gr[2][2];     /* [gran][ch] */
}
SIDE_INFO;

/*-----------------------------------------------------------*/
/*-- scale factors ---*/
// check dimensions - need 21 long, 3*12 short
// plus extra for implicit sf=0 above highest cb
typedef struct
{
   int l[23];     /* [cb] */
   int s[3][13];    /* [window][cb] */
}
SCALEFACT;

/*-----------------------------------------------------------*/
typedef struct
{
   int cbtype;      /* long=0 short=1 */
   int cbmax;     /* max crit band */
//   int lb_type;     /* long block type 0 1 3 */
   int cbs0;      /* short band start index 0 3 12 (12=no shorts */
   int ncbl;      /* number long cb's 0 8 21 */
   int cbmax_s[3];    /* cbmax by individual short blocks */
}
CB_INFO;

/*-----------------------------------------------------------*/
/* scale factor infor for MPEG2 intensity stereo  */
typedef struct
{
   int nr[3];
   int slen[3];
   int intensity_scale;
}
IS_SF_INFO;


/********************************************************************************/
/*                                                                              */
/*  ........FIX                                                                 */
/*                                                                              */
/********************************************************************************/


// Do NOT change these, ever!!!!!!!!!!!!!!!!!!
//
const int reduction_code  = 0;    // unpack at full sample rate output
const int convert_code_mono = 1;    
const int convert_code_stereo = 0;
const int freq_limit    = 24000;  // no idea what this is about, but it's always this value so...

// the entire decode mechanism uses this now...
//
MP3STREAM _MP3Stream;
LP_MP3STREAM pMP3Stream = &_MP3Stream;
int bFastEstimateOnly = 0;  // MUST DEFAULT TO THIS VALUE!!!!!!!!!

/********************************************************************************/
/*                                                                              */
/*  cup.c                                                                    */
/*                                                                              */
/********************************************************************************/

/******************************************************************

       MPEG audio software decoder portable ANSI c.
       Decodes all Layer I/II to 16 bit linear pcm.
       Optional stereo to mono conversion.  Optional
       output sample rate conversion to half or quarter of
       native mpeg rate. dec8.c adds oupuut conversion features.

-------------------------------------
int audio_decode_init(MPEG_HEAD *h, int framebytes_arg,
         int reduction_code, int transform_code, int convert_code,
         int freq_limit)

initilize decoder:
       return 0 = fail, not 0 = success

MPEG_HEAD *h    input, mpeg header info (returned by call to head_info)
pMP3Stream->framebytes      input, mpeg frame size (returned by call to head_info)
reduction_code  input, sample rate reduction code
                    0 = full rate
                    1 = half rate
                    2 = quarter rate

transform_code  input, ignored
convert_code    input, channel conversion
                  convert_code:  0 = two chan output
                                 1 = convert two chan to mono
                                 2 = convert two chan to left chan
                                 3 = convert two chan to right chan
freq_limit      input, limits bandwidth of pcm output to specified
                frequency.  Special use. Set to 24000 for normal use.


---------------------------------
void audio_decode_info( DEC_INFO *info)

information return:
          Call after audio_decode_init.  See mhead.h for
          information returned in DEC_INFO structure.


---------------------------------
IN_OUT audio_decode(unsigned char *bs, void *pcmbuf)

decode one mpeg audio frame:
bs        input, mpeg bitstream, must start with
          sync word.  Caution: may read up to 3 bytes
          beyond end of frame.
pcmbuf    output, pcm samples.

IN_OUT structure returns:
          Number bytes conceptually removed from mpeg bitstream.
          Returns 0 if sync loss.
          Number bytes of pcm output.

*******************************************************************/
/*-------------------------------------------------------
NOTE:  Decoder may read up to three bytes beyond end of
frame.  Calling application must ensure that this does
not cause a memory access violation (protection fault)
---------------------------------------------------------*/

/*====================================================================*/
/*----------------*/
//@@@@ This next one (decinfo) is ok:
DEC_INFO decinfo;   /* global for Layer III */  // only written into by decode init funcs, then copied to stack struct higher up

/*----------------*/
static float look_c_value[18];  /* built by init */ // effectively constant

/*----------------*/
////@@@@static int pMP3Stream->outbytes;    // !!!!!!!!!!!!!!?
////@@@@static int pMP3Stream->framebytes;    // !!!!!!!!!!!!!!!!
////@@@@static int pMP3Stream->outvalues;   // !!!!!!!!!!!!?
////@@@@static int pad;
static const int look_joint[16] =
{       /* lookup stereo sb's by mode+ext */
   64, 64, 64, 64,    /* stereo */
   2 * 4, 2 * 8, 2 * 12, 2 * 16,  /* joint */
   64, 64, 64, 64,    /* dual */
   32, 32, 32, 32,    /* mono */
};

/*----------------*/
////@@@@static int max_sb;    // !!!!!!!!! L1, 2 3
////@@@@static int stereo_sb;

/*----------------*/
////@@@@static int pMP3Stream->nsb_limit = 6;
////@@@@static int bit_skip;
static const int bat_bit_master[] =
{
   0, 5, 7, 9, 10, 12, 15, 18, 21, 24, 27, 30, 33, 36, 39, 42, 45, 48};

/*----------------*/
////@@@@static int nbat[4] = {3, 8, 12, 7}; // !!!!!!!!!!!!! not constant!!!!
////@@@@static int bat[4][16];  // built as constant, but built according to header type (sigh)
static int ballo[64];   /* set by unpack_ba */          // scratchpad
static unsigned int samp_dispatch[66];  /* set by unpack_ba */    // scratchpad?
static float c_value[64]; /* set by unpack_ba */          // scratchpad

/*----------------*/
static unsigned int sf_dispatch[66];  /* set by unpack_ba */    // scratchpad?
static float sf_table[64];    // effectively constant
////@@@@ static float cs_factor[3][64];

/*----------------*/
////@@@@FINDME - groan....  (I shoved a *2 in just in case it needed it for stereo. This whole thing is crap now
float ___sample[2304*2];   /* global for use by Later 3 */ // !!!!!!!!!!!!!!!!!!!!!! // scratchpad?
static signed char group3_table[32][3];   // effectively constant
static signed char group5_table[128][3];  // effectively constant
static signed short group9_table[1024][3];  // effectively constant

/*----------------*/

////@@@@typedef void (*SBT_FUNCTION) (float *sample, short *pcm, int n);
void sbt_mono(float *sample, short *pcm, int n);
void sbt_dual(float *sample, short *pcm, int n);
////@@@@static SBT_FUNCTION sbt = sbt_mono;


typedef IN_OUT(*AUDIO_DECODE_ROUTINE) (unsigned char *bs, signed short *pcm);
IN_OUT L2audio_decode(unsigned char *bs, signed short *pcm);
static AUDIO_DECODE_ROUTINE audio_decode_routine = L2audio_decode;

/*======================================================================*/
/*======================================================================*/
/* get bits from bitstream in endian independent way */
////@@@@ FINDME - this stuff doesn't appear to be used by any of our samples (phew)
static unsigned char *bs_ptr;
static unsigned long bitbuf;
static int _bits;
static long bitval;

/*------------- initialize bit getter -------------*/
static void load_init(unsigned char *buf)
{
   bs_ptr = buf;
   _bits = 0;
   bitbuf = 0;
}
/*------------- get n bits from bitstream -------------*/
static long load(int n)
{
   unsigned long x;

   if (_bits < n)
   {        /* refill bit buf if necessary */
      while (_bits <= 24)
      {
   bitbuf = (bitbuf << 8) | *bs_ptr++;
   _bits += 8;
      }
   }
   _bits -= n;
   x = bitbuf >> _bits;
   bitbuf -= x << _bits;
   return x;
}
/*------------- skip over n bits in bitstream -------------*/
static void skip(int n)
{
   int k;

   if (_bits < n)
   {
      n -= _bits;
      k = n >> 3;
/*--- bytes = n/8 --*/
      bs_ptr += k;
      n -= k << 3;
      bitbuf = *bs_ptr++;
      _bits = 8;
   }
   _bits -= n;
   bitbuf -= (bitbuf >> _bits) << _bits;
}
/*--------------------------------------------------------------*/
#define mac_load_check(n) if( _bits < (n) ) {                           \
          while( _bits <= 24 ) {               \
             bitbuf = (bitbuf << 8) | *bs_ptr++;  \
             _bits += 8;                       \
          }                                   \
   }
/*--------------------------------------------------------------*/
#define mac_load(n) ( _bits -= n,                    \
         bitval = bitbuf >> _bits,      \
         bitbuf -= bitval << _bits,     \
         bitval )
/*======================================================================*/
static void unpack_ba()
{
   int i, j, k;
   static int nbit[4] =
   {4, 4, 3, 2};
   int nstereo;

   pMP3Stream->bit_skip = 0;
   nstereo = pMP3Stream->stereo_sb;
   k = 0;
   for (i = 0; i < 4; i++)
   {
      for (j = 0; j < pMP3Stream->nbat[i]; j++, k++)
      {
   mac_load_check(4);
   ballo[k] = samp_dispatch[k] = pMP3Stream->bat[i][mac_load(nbit[i])];
   if (k >= pMP3Stream->nsb_limit)
      pMP3Stream->bit_skip += bat_bit_master[samp_dispatch[k]];
   c_value[k] = look_c_value[samp_dispatch[k]];
   if (--nstereo < 0)
   {
      ballo[k + 1] = ballo[k];
      samp_dispatch[k] += 18; /* flag as joint */
      samp_dispatch[k + 1] = samp_dispatch[k];  /* flag for sf */
      c_value[k + 1] = c_value[k];
      k++;
      j++;
   }
      }
   }
   samp_dispatch[pMP3Stream->nsb_limit] = 37; /* terminate the dispatcher with skip */
   samp_dispatch[k] = 36; /* terminate the dispatcher */

}
/*-------------------------------------------------------------------------*/
static void unpack_sfs()  /* unpack scale factor selectors */
{
   int i;

   for (i = 0; i < pMP3Stream->max_sb; i++)
   {
      mac_load_check(2);
      if (ballo[i])
   sf_dispatch[i] = mac_load(2);
      else
   sf_dispatch[i] = 4;  /* no allo */
   }
   sf_dispatch[i] = 5;    /* terminate dispatcher */
}
/*-------------------------------------------------------------------------*/
static void unpack_sf()   /* unpack scale factor */
{       /* combine dequant and scale factors */
   int i;

   i = -1;
 dispatch:switch (sf_dispatch[++i])
   {
      case 0:     /* 3 factors 012 */
   mac_load_check(18);
   pMP3Stream->cs_factor[0][i] = c_value[i] * sf_table[mac_load(6)];
   pMP3Stream->cs_factor[1][i] = c_value[i] * sf_table[mac_load(6)];
   pMP3Stream->cs_factor[2][i] = c_value[i] * sf_table[mac_load(6)];
   goto dispatch;
      case 1:     /* 2 factors 002 */
   mac_load_check(12);
   pMP3Stream->cs_factor[1][i] = pMP3Stream->cs_factor[0][i] = c_value[i] * sf_table[mac_load(6)];
   pMP3Stream->cs_factor[2][i] = c_value[i] * sf_table[mac_load(6)];
   goto dispatch;
      case 2:     /* 1 factor 000 */
   mac_load_check(6);
   pMP3Stream->cs_factor[2][i] = pMP3Stream->cs_factor[1][i] = pMP3Stream->cs_factor[0][i] =
      c_value[i] * sf_table[mac_load(6)];
   goto dispatch;
      case 3:     /* 2 factors 022 */
   mac_load_check(12);
   pMP3Stream->cs_factor[0][i] = c_value[i] * sf_table[mac_load(6)];
   pMP3Stream->cs_factor[2][i] = pMP3Stream->cs_factor[1][i] = c_value[i] * sf_table[mac_load(6)];
   goto dispatch;
      case 4:     /* no allo */
/*-- pMP3Stream->cs_factor[2][i] = pMP3Stream->cs_factor[1][i] = pMP3Stream->cs_factor[0][i] = 0.0;  --*/
   goto dispatch;
      case 5:     /* all done */
   ;
   }        /* end switch */
}
/*-------------------------------------------------------------------------*/
#define UNPACK_N(n) s[k]     =  pMP3Stream->cs_factor[i][k]*(load(n)-((1 << (n-1)) -1));   \
    s[k+64]  =  pMP3Stream->cs_factor[i][k]*(load(n)-((1 << (n-1)) -1));   \
    s[k+128] =  pMP3Stream->cs_factor[i][k]*(load(n)-((1 << (n-1)) -1));   \
    goto dispatch;
#define UNPACK_N2(n) mac_load_check(3*n);                                         \
    s[k]     =  pMP3Stream->cs_factor[i][k]*(mac_load(n)-((1 << (n-1)) -1));   \
    s[k+64]  =  pMP3Stream->cs_factor[i][k]*(mac_load(n)-((1 << (n-1)) -1));   \
    s[k+128] =  pMP3Stream->cs_factor[i][k]*(mac_load(n)-((1 << (n-1)) -1));   \
    goto dispatch;
#define UNPACK_N3(n) mac_load_check(2*n);                                         \
    s[k]     =  pMP3Stream->cs_factor[i][k]*(mac_load(n)-((1 << (n-1)) -1));   \
    s[k+64]  =  pMP3Stream->cs_factor[i][k]*(mac_load(n)-((1 << (n-1)) -1));   \
    mac_load_check(n);                                           \
    s[k+128] =  pMP3Stream->cs_factor[i][k]*(mac_load(n)-((1 << (n-1)) -1));   \
    goto dispatch;
#define UNPACKJ_N(n) tmp        =  (load(n)-((1 << (n-1)) -1));                \
    s[k]       =  pMP3Stream->cs_factor[i][k]*tmp;                       \
    s[k+1]     =  pMP3Stream->cs_factor[i][k+1]*tmp;                     \
    tmp        =  (load(n)-((1 << (n-1)) -1));                 \
    s[k+64]    =  pMP3Stream->cs_factor[i][k]*tmp;                       \
    s[k+64+1]  =  pMP3Stream->cs_factor[i][k+1]*tmp;                     \
    tmp        =  (load(n)-((1 << (n-1)) -1));                 \
    s[k+128]   =  pMP3Stream->cs_factor[i][k]*tmp;                       \
    s[k+128+1] =  pMP3Stream->cs_factor[i][k+1]*tmp;                     \
    k++;       /* skip right chan dispatch */                \
    goto dispatch;
/*-------------------------------------------------------------------------*/
static void unpack_samp() /* unpack samples */
{
   int i, j, k;
   float *s;
   int n;
   long tmp;

   s = ___sample;
   for (i = 0; i < 3; i++)
   {        /* 3 groups of scale factors */
      for (j = 0; j < 4; j++)
      {
   k = -1;
       dispatch:switch (samp_dispatch[++k])
   {
      case 0:
         s[k + 128] = s[k + 64] = s[k] = 0.0F;
         goto dispatch;
      case 1:   /* 3 levels grouped 5 bits */
         mac_load_check(5);
         n = mac_load(5);
         s[k] = pMP3Stream->cs_factor[i][k] * group3_table[n][0];
         s[k + 64] = pMP3Stream->cs_factor[i][k] * group3_table[n][1];
         s[k + 128] = pMP3Stream->cs_factor[i][k] * group3_table[n][2];
         goto dispatch;
      case 2:   /* 5 levels grouped 7 bits */
         mac_load_check(7);
         n = mac_load(7);
         s[k] = pMP3Stream->cs_factor[i][k] * group5_table[n][0];
         s[k + 64] = pMP3Stream->cs_factor[i][k] * group5_table[n][1];
         s[k + 128] = pMP3Stream->cs_factor[i][k] * group5_table[n][2];
         goto dispatch;
      case 3:
         UNPACK_N2(3) /* 7 levels */
      case 4:   /* 9 levels grouped 10 bits */
         mac_load_check(10);
         n = mac_load(10);
         s[k] = pMP3Stream->cs_factor[i][k] * group9_table[n][0];
         s[k + 64] = pMP3Stream->cs_factor[i][k] * group9_table[n][1];
         s[k + 128] = pMP3Stream->cs_factor[i][k] * group9_table[n][2];
         goto dispatch;
      case 5:
         UNPACK_N2(4) /* 15 levels */
      case 6:
         UNPACK_N2(5) /* 31 levels */
      case 7:
         UNPACK_N2(6) /* 63 levels */
      case 8:
         UNPACK_N2(7) /* 127 levels */
      case 9:
         UNPACK_N2(8) /* 255 levels */
      case 10:
         UNPACK_N3(9) /* 511 levels */
      case 11:
         UNPACK_N3(10)  /* 1023 levels */
      case 12:
         UNPACK_N3(11)  /* 2047 levels */
      case 13:
         UNPACK_N3(12)  /* 4095 levels */
      case 14:
         UNPACK_N(13) /* 8191 levels */
      case 15:
         UNPACK_N(14) /* 16383 levels */
      case 16:
         UNPACK_N(15) /* 32767 levels */
      case 17:
         UNPACK_N(16) /* 65535 levels */
/* -- joint ---- */
      case 18 + 0:
         s[k + 128 + 1] = s[k + 128] = s[k + 64 + 1] = s[k + 64] = s[k + 1] = s[k] = 0.0F;
         k++;   /* skip right chan dispatch */
         goto dispatch;
      case 18 + 1:  /* 3 levels grouped 5 bits */
         n = load(5);
         s[k] = pMP3Stream->cs_factor[i][k] * group3_table[n][0];
         s[k + 1] = pMP3Stream->cs_factor[i][k + 1] * group3_table[n][0];
         s[k + 64] = pMP3Stream->cs_factor[i][k] * group3_table[n][1];
         s[k + 64 + 1] = pMP3Stream->cs_factor[i][k + 1] * group3_table[n][1];
         s[k + 128] = pMP3Stream->cs_factor[i][k] * group3_table[n][2];
         s[k + 128 + 1] = pMP3Stream->cs_factor[i][k + 1] * group3_table[n][2];
         k++;   /* skip right chan dispatch */
         goto dispatch;
      case 18 + 2:  /* 5 levels grouped 7 bits */
         n = load(7);
         s[k] = pMP3Stream->cs_factor[i][k] * group5_table[n][0];
         s[k + 1] = pMP3Stream->cs_factor[i][k + 1] * group5_table[n][0];
         s[k + 64] = pMP3Stream->cs_factor[i][k] * group5_table[n][1];
         s[k + 64 + 1] = pMP3Stream->cs_factor[i][k + 1] * group5_table[n][1];
         s[k + 128] = pMP3Stream->cs_factor[i][k] * group5_table[n][2];
         s[k + 128 + 1] = pMP3Stream->cs_factor[i][k + 1] * group5_table[n][2];
         k++;   /* skip right chan dispatch */
         goto dispatch;
      case 18 + 3:
         UNPACKJ_N(3) /* 7 levels */
      case 18 + 4:  /* 9 levels grouped 10 bits */
         n = load(10);
         s[k] = pMP3Stream->cs_factor[i][k] * group9_table[n][0];
         s[k + 1] = pMP3Stream->cs_factor[i][k + 1] * group9_table[n][0];
         s[k + 64] = pMP3Stream->cs_factor[i][k] * group9_table[n][1];
         s[k + 64 + 1] = pMP3Stream->cs_factor[i][k + 1] * group9_table[n][1];
         s[k + 128] = pMP3Stream->cs_factor[i][k] * group9_table[n][2];
         s[k + 128 + 1] = pMP3Stream->cs_factor[i][k + 1] * group9_table[n][2];
         k++;   /* skip right chan dispatch */
         goto dispatch;
      case 18 + 5:
         UNPACKJ_N(4) /* 15 levels */
      case 18 + 6:
         UNPACKJ_N(5) /* 31 levels */
      case 18 + 7:
         UNPACKJ_N(6) /* 63 levels */
      case 18 + 8:
         UNPACKJ_N(7) /* 127 levels */
      case 18 + 9:
         UNPACKJ_N(8) /* 255 levels */
      case 18 + 10:
         UNPACKJ_N(9) /* 511 levels */
      case 18 + 11:
         UNPACKJ_N(10)  /* 1023 levels */
      case 18 + 12:
         UNPACKJ_N(11)  /* 2047 levels */
      case 18 + 13:
         UNPACKJ_N(12)  /* 4095 levels */
      case 18 + 14:
         UNPACKJ_N(13)  /* 8191 levels */
      case 18 + 15:
         UNPACKJ_N(14)  /* 16383 levels */
      case 18 + 16:
         UNPACKJ_N(15)  /* 32767 levels */
      case 18 + 17:
         UNPACKJ_N(16)  /* 65535 levels */
/* -- end of dispatch -- */
      case 37:
         skip(pMP3Stream->bit_skip);
      case 36:
         s += 3 * 64;
   }      /* end switch */
      }       /* end j loop */
   }        /* end i loop */


}
/*-------------------------------------------------------------------------*/
unsigned char *gpNextByteAfterData = NULL;
IN_OUT audio_decode(unsigned char *bs, signed short *pcm, unsigned char *pNextByteAfterData)
{
  gpNextByteAfterData = pNextByteAfterData; // sigh....
   return audio_decode_routine(bs, pcm);
}
/*-------------------------------------------------------------------------*/
IN_OUT L2audio_decode(unsigned char *bs, signed short *pcm)
{
   int sync, prot;
   IN_OUT in_out;

   load_init(bs);   /* initialize bit getter */
/* test sync */
   in_out.in_bytes = 0;   /* assume fail */
   in_out.out_bytes = 0;
   sync = load(12);
   if (sync != 0xFFF)
      return in_out;    /* sync fail */

   load(3);     /* skip id and option (checked by init) */
   prot = load(1);    /* load prot bit */
   load(6);     /* skip to pad */
   pMP3Stream->pad = load(1);
   load(1);     /* skip to mode */
   pMP3Stream->stereo_sb = look_joint[load(4)];
   if (prot)
      load(4);      /* skip to data */
   else
      load(20);     /* skip crc */

   unpack_ba();     /* unpack bit allocation */
   unpack_sfs();    /* unpack scale factor selectors */
   unpack_sf();     /* unpack scale factor */
   unpack_samp();   /* unpack samples */

   pMP3Stream->sbt(___sample, pcm, 36);
/*-----------*/
   in_out.in_bytes = pMP3Stream->framebytes + pMP3Stream->pad;
   in_out.out_bytes = pMP3Stream->outbytes;

   return in_out;
}
/*-------------------------------------------------------------------------*/
#define COMPILE_ME

//#include "cupini.c"   /* initialization */
//#include "cupL1.c"    /* Layer I */
/*-------------------------------------------------------------------------*/



/********************************************************************************/
/*                                                                              */
/*  csbt.c                                                                      */
/*                                                                              */
/********************************************************************************/

void fdct32(float *, float *);
void fdct32_dual(float *, float *);
void fdct32_dual_mono(float *, float *);
void fdct16(float *, float *);
void fdct16_dual(float *, float *);
void fdct16_dual_mono(float *, float *);
void fdct8(float *, float *);
void fdct8_dual(float *, float *);
void fdct8_dual_mono(float *, float *);

void window(float *vbuf, int vb_ptr, short *pcm);
void window_dual(float *vbuf, int vb_ptr, short *pcm);
void window16(float *vbuf, int vb_ptr, short *pcm);
void window16_dual(float *vbuf, int vb_ptr, short *pcm);
void window8(float *vbuf, int vb_ptr, short *pcm);
void window8_dual(float *vbuf, int vb_ptr, short *pcm);

/*-------------------------------------------------------------------------*/
/* circular window buffers */

////static signed int vb_ptr; // !!!!!!!!!!!!!
////static signed int vb2_ptr;  // !!!!!!!!!!!!!
////static float pMP3Stream->vbuf[512];   // !!!!!!!!!!!!!
////static float vbuf2[512];  // !!!!!!!!!!!!!

float *dct_coef_addr();

/*======================================================================*/
static void gencoef()   /* gen coef for N=32 (31 coefs) */
{
  static int iOnceOnly = 0;
   int p, n, i, k;
   double t, pi;
   float *coef32;

   if (!iOnceOnly++)
   {
     coef32 = dct_coef_addr();

     pi = 4.0 * atan(1.0);
     n = 16;
     k = 0;
     for (i = 0; i < 5; i++, n = n / 2)
     {

      for (p = 0; p < n; p++, k++)
      {
     t = (pi / (4 * n)) * (2 * p + 1);
     coef32[k] = (float) (0.50 / cos(t));
      }
     }
   }
}
/*------------------------------------------------------------*/
void sbt_init()
{
   int i;
   static int first_pass = 1;

   if (first_pass)
   {
      gencoef();
      first_pass = 0;
   }

/* clear window pMP3Stream->vbuf */
   for (i = 0; i < 512; i++)
   {
      pMP3Stream->vbuf[i] = 0.0F;
      pMP3Stream->vbuf2[i] = 0.0F;
   }
   pMP3Stream->vb2_ptr = pMP3Stream->vb_ptr = 0;

}
/*============================================================*/
/*============================================================*/
/*============================================================*/
void sbt_mono(float *sample, short *pcm, int n)
{
   int i;

   for (i = 0; i < n; i++)
   {
      fdct32(sample, pMP3Stream->vbuf + pMP3Stream->vb_ptr);
      window(pMP3Stream->vbuf, pMP3Stream->vb_ptr, pcm);
      sample += 64;
      pMP3Stream->vb_ptr = (pMP3Stream->vb_ptr - 32) & 511;
      pcm += 32;
   }

}
/*------------------------------------------------------------*/
void sbt_dual(float *sample, short *pcm, int n)
{
   int i;

   for (i = 0; i < n; i++)
   {
      fdct32_dual(sample, pMP3Stream->vbuf + pMP3Stream->vb_ptr);
      fdct32_dual(sample + 1, pMP3Stream->vbuf2 + pMP3Stream->vb_ptr);
      window_dual(pMP3Stream->vbuf, pMP3Stream->vb_ptr, pcm);
      window_dual(pMP3Stream->vbuf2, pMP3Stream->vb_ptr, pcm + 1);
      sample += 64;
      pMP3Stream->vb_ptr = (pMP3Stream->vb_ptr - 32) & 511;
      pcm += 64;
   }


}
/*------------------------------------------------------------*/
/* convert dual to mono */
void sbt_dual_mono(float *sample, short *pcm, int n)
{
   int i;

   for (i = 0; i < n; i++)
   {
      fdct32_dual_mono(sample, pMP3Stream->vbuf + pMP3Stream->vb_ptr);
      window(pMP3Stream->vbuf, pMP3Stream->vb_ptr, pcm);
      sample += 64;
      pMP3Stream->vb_ptr = (pMP3Stream->vb_ptr - 32) & 511;
      pcm += 32;
   }

}
/*------------------------------------------------------------*/
/* convert dual to left */
void sbt_dual_left(float *sample, short *pcm, int n)
{
   int i;

   for (i = 0; i < n; i++)
   {
      fdct32_dual(sample, pMP3Stream->vbuf + pMP3Stream->vb_ptr);
      window(pMP3Stream->vbuf, pMP3Stream->vb_ptr, pcm);
      sample += 64;
      pMP3Stream->vb_ptr = (pMP3Stream->vb_ptr - 32) & 511;
      pcm += 32;
   }
}
/*------------------------------------------------------------*/
/* convert dual to right */
void sbt_dual_right(float *sample, short *pcm, int n)
{
   int i;

   sample++;      /* point to right chan */
   for (i = 0; i < n; i++)
   {
      fdct32_dual(sample, pMP3Stream->vbuf + pMP3Stream->vb_ptr);
      window(pMP3Stream->vbuf, pMP3Stream->vb_ptr, pcm);
      sample += 64;
      pMP3Stream->vb_ptr = (pMP3Stream->vb_ptr - 32) & 511;
      pcm += 32;
   }
}
/*------------------------------------------------------------*/
/*---------------- 16 pt sbt's  -------------------------------*/
/*------------------------------------------------------------*/
void sbt16_mono(float *sample, short *pcm, int n)
{
   int i;

   for (i = 0; i < n; i++)
   {
      fdct16(sample, pMP3Stream->vbuf + pMP3Stream->vb_ptr);
      window16(pMP3Stream->vbuf, pMP3Stream->vb_ptr, pcm);
      sample += 64;
      pMP3Stream->vb_ptr = (pMP3Stream->vb_ptr - 16) & 255;
      pcm += 16;
   }


}
/*------------------------------------------------------------*/
void sbt16_dual(float *sample, short *pcm, int n)
{
   int i;

   for (i = 0; i < n; i++)
   {
      fdct16_dual(sample, pMP3Stream->vbuf + pMP3Stream->vb_ptr);
      fdct16_dual(sample + 1, pMP3Stream->vbuf2 + pMP3Stream->vb_ptr);
      window16_dual(pMP3Stream->vbuf, pMP3Stream->vb_ptr, pcm);
      window16_dual(pMP3Stream->vbuf2, pMP3Stream->vb_ptr, pcm + 1);
      sample += 64;
      pMP3Stream->vb_ptr = (pMP3Stream->vb_ptr - 16) & 255;
      pcm += 32;
   }
}
/*------------------------------------------------------------*/
void sbt16_dual_mono(float *sample, short *pcm, int n)
{
   int i;

   for (i = 0; i < n; i++)
   {
      fdct16_dual_mono(sample, pMP3Stream->vbuf + pMP3Stream->vb_ptr);
      window16(pMP3Stream->vbuf, pMP3Stream->vb_ptr, pcm);
      sample += 64;
      pMP3Stream->vb_ptr = (pMP3Stream->vb_ptr - 16) & 255;
      pcm += 16;
   }
}
/*------------------------------------------------------------*/
void sbt16_dual_left(float *sample, short *pcm, int n)
{
   int i;

   for (i = 0; i < n; i++)
   {
      fdct16_dual(sample, pMP3Stream->vbuf + pMP3Stream->vb_ptr);
      window16(pMP3Stream->vbuf, pMP3Stream->vb_ptr, pcm);
      sample += 64;
      pMP3Stream->vb_ptr = (pMP3Stream->vb_ptr - 16) & 255;
      pcm += 16;
   }
}
/*------------------------------------------------------------*/
void sbt16_dual_right(float *sample, short *pcm, int n)
{
   int i;

   sample++;
   for (i = 0; i < n; i++)
   {
      fdct16_dual(sample, pMP3Stream->vbuf + pMP3Stream->vb_ptr);
      window16(pMP3Stream->vbuf, pMP3Stream->vb_ptr, pcm);
      sample += 64;
      pMP3Stream->vb_ptr = (pMP3Stream->vb_ptr - 16) & 255;
      pcm += 16;
   }
}
/*------------------------------------------------------------*/
/*---------------- 8 pt sbt's  -------------------------------*/
/*------------------------------------------------------------*/
void sbt8_mono(float *sample, short *pcm, int n)
{
   int i;

   for (i = 0; i < n; i++)
   {
      fdct8(sample, pMP3Stream->vbuf + pMP3Stream->vb_ptr);
      window8(pMP3Stream->vbuf, pMP3Stream->vb_ptr, pcm);
      sample += 64;
      pMP3Stream->vb_ptr = (pMP3Stream->vb_ptr - 8) & 127;
      pcm += 8;
   }

}
/*------------------------------------------------------------*/
void sbt8_dual(float *sample, short *pcm, int n)
{
   int i;

   for (i = 0; i < n; i++)
   {
      fdct8_dual(sample, pMP3Stream->vbuf + pMP3Stream->vb_ptr);
      fdct8_dual(sample + 1, pMP3Stream->vbuf2 + pMP3Stream->vb_ptr);
      window8_dual(pMP3Stream->vbuf, pMP3Stream->vb_ptr, pcm);
      window8_dual(pMP3Stream->vbuf2, pMP3Stream->vb_ptr, pcm + 1);
      sample += 64;
      pMP3Stream->vb_ptr = (pMP3Stream->vb_ptr - 8) & 127;
      pcm += 16;
   }
}
/*------------------------------------------------------------*/
void sbt8_dual_mono(float *sample, short *pcm, int n)
{
   int i;

   for (i = 0; i < n; i++)
   {
      fdct8_dual_mono(sample, pMP3Stream->vbuf + pMP3Stream->vb_ptr);
      window8(pMP3Stream->vbuf, pMP3Stream->vb_ptr, pcm);
      sample += 64;
      pMP3Stream->vb_ptr = (pMP3Stream->vb_ptr - 8) & 127;
      pcm += 8;
   }
}
/*------------------------------------------------------------*/
void sbt8_dual_left(float *sample, short *pcm, int n)
{
   int i;

   for (i = 0; i < n; i++)
   {
      fdct8_dual(sample, pMP3Stream->vbuf + pMP3Stream->vb_ptr);
      window8(pMP3Stream->vbuf, pMP3Stream->vb_ptr, pcm);
      sample += 64;
      pMP3Stream->vb_ptr = (pMP3Stream->vb_ptr - 8) & 127;
      pcm += 8;
   }
}
/*------------------------------------------------------------*/
void sbt8_dual_right(float *sample, short *pcm, int n)
{
   int i;

   sample++;
   for (i = 0; i < n; i++)
   {
      fdct8_dual(sample, pMP3Stream->vbuf + pMP3Stream->vb_ptr);
      window8(pMP3Stream->vbuf, pMP3Stream->vb_ptr, pcm);
      sample += 64;
      pMP3Stream->vb_ptr = (pMP3Stream->vb_ptr - 8) & 127;
      pcm += 8;
   }
}
/*------------------------------------------------------------*/
/*------------------------------------------------------------*/

//#include "csbtb.c"    /* 8 bit output */
//#include "csbtL3.c"   /* Layer III */
/*------------------------------------------------------------*/

/********************************************************************************/
/*                                                                              */
/*  cwinm.c                                                                     */
/*                                                                              */
/********************************************************************************/

const float wincoef[264] =
{        /* window coefs */
/* decoder analysis window gen by dinit.c (asm version table gen) */
0.000000000f, 0.000442505f, -0.003250122f, 0.007003784f,
-0.031082151f, 0.078628540f, -0.100311279f, 0.572036743f,
-1.144989014f, -0.572036743f, -0.100311279f, -0.078628540f,
-0.031082151f, -0.007003784f, -0.003250122f, -0.000442505f,
0.000015259f, 0.000473022f, -0.003326416f, 0.007919312f,
-0.030517576f, 0.084182739f, -0.090927124f, 0.600219727f,
-1.144287109f, -0.543823242f, -0.108856201f, -0.073059082f,
-0.031478882f, -0.006118774f, -0.003173828f, -0.000396729f,
0.000015259f, 0.000534058f, -0.003387451f, 0.008865356f,
-0.029785154f, 0.089706421f, -0.080688477f, 0.628295898f,
-1.142211914f, -0.515609741f, -0.116577141f, -0.067520142f,
-0.031738281f, -0.005294800f, -0.003082275f, -0.000366211f,
0.000015259f, 0.000579834f, -0.003433228f, 0.009841919f,
-0.028884888f, 0.095169067f, -0.069595337f, 0.656219482f,
-1.138763428f, -0.487472534f, -0.123474121f, -0.061996460f,
-0.031845093f, -0.004486084f, -0.002990723f, -0.000320435f,
0.000015259f, 0.000625610f, -0.003463745f, 0.010848999f,
-0.027801514f, 0.100540161f, -0.057617184f, 0.683914185f,
-1.133926392f, -0.459472656f, -0.129577637f, -0.056533810f,
-0.031814575f, -0.003723145f, -0.002899170f, -0.000289917f,
0.000015259f, 0.000686646f, -0.003479004f, 0.011886597f,
-0.026535034f, 0.105819702f, -0.044784546f, 0.711318970f,
-1.127746582f, -0.431655884f, -0.134887695f, -0.051132202f,
-0.031661987f, -0.003005981f, -0.002792358f, -0.000259399f,
0.000015259f, 0.000747681f, -0.003479004f, 0.012939452f,
-0.025085449f, 0.110946655f, -0.031082151f, 0.738372803f,
-1.120223999f, -0.404083252f, -0.139450073f, -0.045837402f,
-0.031387329f, -0.002334595f, -0.002685547f, -0.000244141f,
0.000030518f, 0.000808716f, -0.003463745f, 0.014022826f,
-0.023422241f, 0.115921021f, -0.016510010f, 0.765029907f,
-1.111373901f, -0.376800537f, -0.143264771f, -0.040634155f,
-0.031005858f, -0.001693726f, -0.002578735f, -0.000213623f,
0.000030518f, 0.000885010f, -0.003417969f, 0.015121460f,
-0.021575928f, 0.120697014f, -0.001068115f, 0.791213989f,
-1.101211548f, -0.349868774f, -0.146362305f, -0.035552979f,
-0.030532837f, -0.001098633f, -0.002456665f, -0.000198364f,
0.000030518f, 0.000961304f, -0.003372192f, 0.016235352f,
-0.019531250f, 0.125259399f, 0.015228271f, 0.816864014f,
-1.089782715f, -0.323318481f, -0.148773193f, -0.030609131f,
-0.029937742f, -0.000549316f, -0.002349854f, -0.000167847f,
0.000030518f, 0.001037598f, -0.003280640f, 0.017349243f,
-0.017257690f, 0.129562378f, 0.032379150f, 0.841949463f,
-1.077117920f, -0.297210693f, -0.150497437f, -0.025817871f,
-0.029281614f, -0.000030518f, -0.002243042f, -0.000152588f,
0.000045776f, 0.001113892f, -0.003173828f, 0.018463135f,
-0.014801024f, 0.133590698f, 0.050354004f, 0.866363525f,
-1.063217163f, -0.271591187f, -0.151596069f, -0.021179199f,
-0.028533936f, 0.000442505f, -0.002120972f, -0.000137329f,
0.000045776f, 0.001205444f, -0.003051758f, 0.019577026f,
-0.012115479f, 0.137298584f, 0.069168091f, 0.890090942f,
-1.048156738f, -0.246505737f, -0.152069092f, -0.016708374f,
-0.027725220f, 0.000869751f, -0.002014160f, -0.000122070f,
0.000061035f, 0.001296997f, -0.002883911f, 0.020690918f,
-0.009231566f, 0.140670776f, 0.088775635f, 0.913055420f,
-1.031936646f, -0.221984863f, -0.151962280f, -0.012420653f,
-0.026840210f, 0.001266479f, -0.001907349f, -0.000106812f,
0.000061035f, 0.001388550f, -0.002700806f, 0.021789551f,
-0.006134033f, 0.143676758f, 0.109161377f, 0.935195923f,
-1.014617920f, -0.198059082f, -0.151306152f, -0.008316040f,
-0.025909424f, 0.001617432f, -0.001785278f, -0.000106812f,
0.000076294f, 0.001480103f, -0.002487183f, 0.022857666f,
-0.002822876f, 0.146255493f, 0.130310059f, 0.956481934f,
-0.996246338f, -0.174789429f, -0.150115967f, -0.004394531f,
-0.024932859f, 0.001937866f, -0.001693726f, -0.000091553f,
-0.001586914f, -0.023910521f, -0.148422241f, -0.976852417f,
0.152206421f, 0.000686646f, -0.002227783f, 0.000076294f,
};

/********************************************************************************/
/*                                                                              */
/*  cwinb.c                                                                     */
/*                                                                              */
/********************************************************************************/
/****  cwin.c  ***************************************************

include to cwinm.c

MPEG audio decoder, float window routines - 8 bit output
portable C

******************************************************************/
/*-------------------------------------------------------------------------*/

void windowB(float *vbuf, int vb_ptr, unsigned char *pcm)
{
   int i, j;
   int si, bx;
   const float *coef;
   float sum;
   long tmp;

   si = vb_ptr + 16;
   bx = (si + 32) & 511;
   coef = wincoef;

/*-- first 16 --*/
   for (i = 0; i < 16; i++)
   {
      sum = 0.0F;
      for (j = 0; j < 8; j++)
      {
   sum += (*coef++) * vbuf[si];
   si = (si + 64) & 511;
   sum -= (*coef++) * vbuf[bx];
   bx = (bx + 64) & 511;
      }
      si++;
      bx--;
      tmp = (long) sum;
      if (tmp > 32767)
   tmp = 32767;
      else if (tmp < -32768)
   tmp = -32768;
      *pcm++ = ((unsigned char) (tmp >> 8)) ^ 0x80;
   }
/*--  special case --*/
   sum = 0.0F;
   for (j = 0; j < 8; j++)
   {
      sum += (*coef++) * vbuf[bx];
      bx = (bx + 64) & 511;
   }
   tmp = (long) sum;
   if (tmp > 32767)
      tmp = 32767;
   else if (tmp < -32768)
      tmp = -32768;
   *pcm++ = ((unsigned char) (tmp >> 8)) ^ 0x80;
/*-- last 15 --*/
   coef = wincoef + 255;  /* back pass through coefs */
   for (i = 0; i < 15; i++)
   {
      si--;
      bx++;
      sum = 0.0F;
      for (j = 0; j < 8; j++)
      {
   sum += (*coef--) * vbuf[si];
   si = (si + 64) & 511;
   sum += (*coef--) * vbuf[bx];
   bx = (bx + 64) & 511;
      }
      tmp = (long) sum;
      if (tmp > 32767)
   tmp = 32767;
      else if (tmp < -32768)
   tmp = -32768;
      *pcm++ = ((unsigned char) (tmp >> 8)) ^ 0x80;
   }
}
/*------------------------------------------------------------*/
void windowB_dual(float *vbuf, int vb_ptr, unsigned char *pcm)
{
   int i, j;      /* dual window interleaves output */
   int si, bx;
   const float *coef;
   float sum;
   long tmp;

   si = vb_ptr + 16;
   bx = (si + 32) & 511;
   coef = wincoef;

/*-- first 16 --*/
   for (i = 0; i < 16; i++)
   {
      sum = 0.0F;
      for (j = 0; j < 8; j++)
      {
   sum += (*coef++) * vbuf[si];
   si = (si + 64) & 511;
   sum -= (*coef++) * vbuf[bx];
   bx = (bx + 64) & 511;
      }
      si++;
      bx--;
      tmp = (long) sum;
      if (tmp > 32767)
   tmp = 32767;
      else if (tmp < -32768)
   tmp = -32768;
      *pcm = ((unsigned char) (tmp >> 8)) ^ 0x80;
      pcm += 2;
   }
/*--  special case --*/
   sum = 0.0F;
   for (j = 0; j < 8; j++)
   {
      sum += (*coef++) * vbuf[bx];
      bx = (bx + 64) & 511;
   }
   tmp = (long) sum;
   if (tmp > 32767)
      tmp = 32767;
   else if (tmp < -32768)
      tmp = -32768;
   *pcm = ((unsigned char) (tmp >> 8)) ^ 0x80;
   pcm += 2;
/*-- last 15 --*/
   coef = wincoef + 255;  /* back pass through coefs */
   for (i = 0; i < 15; i++)
   {
      si--;
      bx++;
      sum = 0.0F;
      for (j = 0; j < 8; j++)
      {
   sum += (*coef--) * vbuf[si];
   si = (si + 64) & 511;
   sum += (*coef--) * vbuf[bx];
   bx = (bx + 64) & 511;
      }
      tmp = (long) sum;
      if (tmp > 32767)
   tmp = 32767;
      else if (tmp < -32768)
   tmp = -32768;
      *pcm = ((unsigned char) (tmp >> 8)) ^ 0x80;
      pcm += 2;
   }
}
/*------------------------------------------------------------*/
/*------------------- 16 pt window ------------------------------*/
void windowB16(float *vbuf, int vb_ptr, unsigned char *pcm)
{
   int i, j;
   unsigned char si, bx;
   const float *coef;
   float sum;
   long tmp;

   si = vb_ptr + 8;
   bx = si + 16;
   coef = wincoef;

/*-- first 8 --*/
   for (i = 0; i < 8; i++)
   {
      sum = 0.0F;
      for (j = 0; j < 8; j++)
      {
   sum += (*coef++) * vbuf[si];
   si += 32;
   sum -= (*coef++) * vbuf[bx];
   bx += 32;
      }
      si++;
      bx--;
      coef += 16;
      tmp = (long) sum;
      if (tmp > 32767)
   tmp = 32767;
      else if (tmp < -32768)
   tmp = -32768;
      *pcm++ = ((unsigned char) (tmp >> 8)) ^ 0x80;
   }
/*--  special case --*/
   sum = 0.0F;
   for (j = 0; j < 8; j++)
   {
      sum += (*coef++) * vbuf[bx];
      bx += 32;
   }
   tmp = (long) sum;
   if (tmp > 32767)
      tmp = 32767;
   else if (tmp < -32768)
      tmp = -32768;
   *pcm++ = ((unsigned char) (tmp >> 8)) ^ 0x80;
/*-- last 7 --*/
   coef = wincoef + 255;  /* back pass through coefs */
   for (i = 0; i < 7; i++)
   {
      coef -= 16;
      si--;
      bx++;
      sum = 0.0F;
      for (j = 0; j < 8; j++)
      {
   sum += (*coef--) * vbuf[si];
   si += 32;
   sum += (*coef--) * vbuf[bx];
   bx += 32;
      }
      tmp = (long) sum;
      if (tmp > 32767)
   tmp = 32767;
      else if (tmp < -32768)
   tmp = -32768;
      *pcm++ = ((unsigned char) (tmp >> 8)) ^ 0x80;
   }
}
/*--------------- 16 pt dual window (interleaved output) -----------------*/
void windowB16_dual(float *vbuf, int vb_ptr, unsigned char *pcm)
{
   int i, j;
   unsigned char si, bx;
   const float *coef;
   float sum;
   long tmp;

   si = vb_ptr + 8;
   bx = si + 16;
   coef = wincoef;

/*-- first 8 --*/
   for (i = 0; i < 8; i++)
   {
      sum = 0.0F;
      for (j = 0; j < 8; j++)
      {
   sum += (*coef++) * vbuf[si];
   si += 32;
   sum -= (*coef++) * vbuf[bx];
   bx += 32;
      }
      si++;
      bx--;
      coef += 16;
      tmp = (long) sum;
      if (tmp > 32767)
   tmp = 32767;
      else if (tmp < -32768)
   tmp = -32768;
      *pcm = ((unsigned char) (tmp >> 8)) ^ 0x80;
      pcm += 2;
   }
/*--  special case --*/
   sum = 0.0F;
   for (j = 0; j < 8; j++)
   {
      sum += (*coef++) * vbuf[bx];
      bx += 32;
   }
   tmp = (long) sum;
   if (tmp > 32767)
      tmp = 32767;
   else if (tmp < -32768)
      tmp = -32768;
   *pcm = ((unsigned char) (tmp >> 8)) ^ 0x80;
   pcm += 2;
/*-- last 7 --*/
   coef = wincoef + 255;  /* back pass through coefs */
   for (i = 0; i < 7; i++)
   {
      coef -= 16;
      si--;
      bx++;
      sum = 0.0F;
      for (j = 0; j < 8; j++)
      {
   sum += (*coef--) * vbuf[si];
   si += 32;
   sum += (*coef--) * vbuf[bx];
   bx += 32;
      }
      tmp = (long) sum;
      if (tmp > 32767)
   tmp = 32767;
      else if (tmp < -32768)
   tmp = -32768;
      *pcm = ((unsigned char) (tmp >> 8)) ^ 0x80;
      pcm += 2;
   }
}
/*------------------- 8 pt window ------------------------------*/
void windowB8(float *vbuf, int vb_ptr, unsigned char *pcm)
{
   int i, j;
   int si, bx;
   const float *coef;
   float sum;
   long tmp;

   si = vb_ptr + 4;
   bx = (si + 8) & 127;
   coef = wincoef;

/*-- first 4 --*/
   for (i = 0; i < 4; i++)
   {
      sum = 0.0F;
      for (j = 0; j < 8; j++)
      {
   sum += (*coef++) * vbuf[si];
   si = (si + 16) & 127;
   sum -= (*coef++) * vbuf[bx];
   bx = (bx + 16) & 127;
      }
      si++;
      bx--;
      coef += 48;
      tmp = (long) sum;
      if (tmp > 32767)
   tmp = 32767;
      else if (tmp < -32768)
   tmp = -32768;
      *pcm++ = ((unsigned char) (tmp >> 8)) ^ 0x80;
   }
/*--  special case --*/
   sum = 0.0F;
   for (j = 0; j < 8; j++)
   {
      sum += (*coef++) * vbuf[bx];
      bx = (bx + 16) & 127;
   }
   tmp = (long) sum;
   if (tmp > 32767)
      tmp = 32767;
   else if (tmp < -32768)
      tmp = -32768;
   *pcm++ = ((unsigned char) (tmp >> 8)) ^ 0x80;
/*-- last 3 --*/
   coef = wincoef + 255;  /* back pass through coefs */
   for (i = 0; i < 3; i++)
   {
      coef -= 48;
      si--;
      bx++;
      sum = 0.0F;
      for (j = 0; j < 8; j++)
      {
   sum += (*coef--) * vbuf[si];
   si = (si + 16) & 127;
   sum += (*coef--) * vbuf[bx];
   bx = (bx + 16) & 127;
      }
      tmp = (long) sum;
      if (tmp > 32767)
   tmp = 32767;
      else if (tmp < -32768)
   tmp = -32768;
      *pcm++ = ((unsigned char) (tmp >> 8)) ^ 0x80;
   }
}
/*--------------- 8 pt dual window (interleaved output) -----------------*/
void windowB8_dual(float *vbuf, int vb_ptr, unsigned char *pcm)
{
   int i, j;
   int si, bx;
   const float *coef;
   float sum;
   long tmp;

   si = vb_ptr + 4;
   bx = (si + 8) & 127;
   coef = wincoef;

/*-- first 4 --*/
   for (i = 0; i < 4; i++)
   {
      sum = 0.0F;
      for (j = 0; j < 8; j++)
      {
   sum += (*coef++) * vbuf[si];
   si = (si + 16) & 127;
   sum -= (*coef++) * vbuf[bx];
   bx = (bx + 16) & 127;
      }
      si++;
      bx--;
      coef += 48;
      tmp = (long) sum;
      if (tmp > 32767)
   tmp = 32767;
      else if (tmp < -32768)
   tmp = -32768;
      *pcm = ((unsigned char) (tmp >> 8)) ^ 0x80;
      pcm += 2;
   }
/*--  special case --*/
   sum = 0.0F;
   for (j = 0; j < 8; j++)
   {
      sum += (*coef++) * vbuf[bx];
      bx = (bx + 16) & 127;
   }
   tmp = (long) sum;
   if (tmp > 32767)
      tmp = 32767;
   else if (tmp < -32768)
      tmp = -32768;
   *pcm = ((unsigned char) (tmp >> 8)) ^ 0x80;
   pcm += 2;
/*-- last 3 --*/
   coef = wincoef + 255;  /* back pass through coefs */
   for (i = 0; i < 3; i++)
   {
      coef -= 48;
      si--;
      bx++;
      sum = 0.0F;
      for (j = 0; j < 8; j++)
      {
   sum += (*coef--) * vbuf[si];
   si = (si + 16) & 127;
   sum += (*coef--) * vbuf[bx];
   bx = (bx + 16) & 127;
      }
      tmp = (long) sum;
      if (tmp > 32767)
   tmp = 32767;
      else if (tmp < -32768)
   tmp = -32768;
      *pcm = ((unsigned char) (tmp >> 8)) ^ 0x80;
      pcm += 2;
   }
}
/*------------------------------------------------------------*/


/********************************************************************************/
/*                                                                              */
/*  csbtb.c                                                                     */
/*                                                                              */
/********************************************************************************/

/*============================================================*/
/*============================================================*/
/*void windowB(float *vbuf, int vb_ptr, unsigned char *pcm);
void windowB_dual(float *vbuf, int vb_ptr, unsigned char *pcm);
void windowB16(float *vbuf, int vb_ptr, unsigned char *pcm);
void windowB16_dual(float *vbuf, int vb_ptr, unsigned char *pcm);
void windowB8(float *vbuf, int vb_ptr, unsigned char *pcm);
void windowB8_dual(float *vbuf, int vb_ptr, unsigned char *pcm);*/

/*============================================================*/
void sbtB_mono(float *sample, unsigned char *pcm, int n)
{
   int i;

   for (i = 0; i < n; i++)
   {
      fdct32(sample, pMP3Stream->vbuf + pMP3Stream->vb_ptr);
      windowB(pMP3Stream->vbuf, pMP3Stream->vb_ptr, pcm);
      sample += 64;
      pMP3Stream->vb_ptr = (pMP3Stream->vb_ptr - 32) & 511;
      pcm += 32;
   }

}
/*------------------------------------------------------------*/
void sbtB_dual(float *sample, unsigned char *pcm, int n)
{
   int i;

   for (i = 0; i < n; i++)
   {
      fdct32_dual(sample, pMP3Stream->vbuf + pMP3Stream->vb_ptr);
      fdct32_dual(sample + 1, pMP3Stream->vbuf2 + pMP3Stream->vb_ptr);
      windowB_dual(pMP3Stream->vbuf, pMP3Stream->vb_ptr, pcm);
      windowB_dual(pMP3Stream->vbuf2, pMP3Stream->vb_ptr, pcm + 1);
      sample += 64;
      pMP3Stream->vb_ptr = (pMP3Stream->vb_ptr - 32) & 511;
      pcm += 64;
   }


}
/*------------------------------------------------------------*/
/* convert dual to mono */
void sbtB_dual_mono(float *sample, unsigned char *pcm, int n)
{
   int i;

   for (i = 0; i < n; i++)
   {
      fdct32_dual_mono(sample, pMP3Stream->vbuf + pMP3Stream->vb_ptr);
      windowB(pMP3Stream->vbuf, pMP3Stream->vb_ptr, pcm);
      sample += 64;
      pMP3Stream->vb_ptr = (pMP3Stream->vb_ptr - 32) & 511;
      pcm += 32;
   }

}
/*------------------------------------------------------------*/
/* convert dual to left */
void sbtB_dual_left(float *sample, unsigned char *pcm, int n)
{
   int i;

   for (i = 0; i < n; i++)
   {
      fdct32_dual(sample, pMP3Stream->vbuf + pMP3Stream->vb_ptr);
      windowB(pMP3Stream->vbuf, pMP3Stream->vb_ptr, pcm);
      sample += 64;
      pMP3Stream->vb_ptr = (pMP3Stream->vb_ptr - 32) & 511;
      pcm += 32;
   }
}
/*------------------------------------------------------------*/
/* convert dual to right */
void sbtB_dual_right(float *sample, unsigned char *pcm, int n)
{
   int i;

   sample++;      /* point to right chan */
   for (i = 0; i < n; i++)
   {
      fdct32_dual(sample, pMP3Stream->vbuf + pMP3Stream->vb_ptr);
      windowB(pMP3Stream->vbuf, pMP3Stream->vb_ptr, pcm);
      sample += 64;
      pMP3Stream->vb_ptr = (pMP3Stream->vb_ptr - 32) & 511;
      pcm += 32;
   }
}
/*------------------------------------------------------------*/
/*---------------- 16 pt sbt's  -------------------------------*/
/*------------------------------------------------------------*/
void sbtB16_mono(float *sample, unsigned char *pcm, int n)
{
   int i;

   for (i = 0; i < n; i++)
   {
      fdct16(sample, pMP3Stream->vbuf + pMP3Stream->vb_ptr);
      windowB16(pMP3Stream->vbuf, pMP3Stream->vb_ptr, pcm);
      sample += 64;
      pMP3Stream->vb_ptr = (pMP3Stream->vb_ptr - 16) & 255;
      pcm += 16;
   }


}
/*------------------------------------------------------------*/
void sbtB16_dual(float *sample, unsigned char *pcm, int n)
{
   int i;

   for (i = 0; i < n; i++)
   {
      fdct16_dual(sample, pMP3Stream->vbuf + pMP3Stream->vb_ptr);
      fdct16_dual(sample + 1, pMP3Stream->vbuf2 + pMP3Stream->vb_ptr);
      windowB16_dual(pMP3Stream->vbuf, pMP3Stream->vb_ptr, pcm);
      windowB16_dual(pMP3Stream->vbuf2, pMP3Stream->vb_ptr, pcm + 1);
      sample += 64;
      pMP3Stream->vb_ptr = (pMP3Stream->vb_ptr - 16) & 255;
      pcm += 32;
   }
}
/*------------------------------------------------------------*/
void sbtB16_dual_mono(float *sample, unsigned char *pcm, int n)
{
   int i;

   for (i = 0; i < n; i++)
   {
      fdct16_dual_mono(sample, pMP3Stream->vbuf + pMP3Stream->vb_ptr);
      windowB16(pMP3Stream->vbuf, pMP3Stream->vb_ptr, pcm);
      sample += 64;
      pMP3Stream->vb_ptr = (pMP3Stream->vb_ptr - 16) & 255;
      pcm += 16;
   }
}
/*------------------------------------------------------------*/
void sbtB16_dual_left(float *sample, unsigned char *pcm, int n)
{
   int i;

   for (i = 0; i < n; i++)
   {
      fdct16_dual(sample, pMP3Stream->vbuf + pMP3Stream->vb_ptr);
      windowB16(pMP3Stream->vbuf, pMP3Stream->vb_ptr, pcm);
      sample += 64;
      pMP3Stream->vb_ptr = (pMP3Stream->vb_ptr - 16) & 255;
      pcm += 16;
   }
}
/*------------------------------------------------------------*/
void sbtB16_dual_right(float *sample, unsigned char *pcm, int n)
{
   int i;

   sample++;
   for (i = 0; i < n; i++)
   {
      fdct16_dual(sample, pMP3Stream->vbuf + pMP3Stream->vb_ptr);
      windowB16(pMP3Stream->vbuf, pMP3Stream->vb_ptr, pcm);
      sample += 64;
      pMP3Stream->vb_ptr = (pMP3Stream->vb_ptr - 16) & 255;
      pcm += 16;
   }
}
/*------------------------------------------------------------*/
/*---------------- 8 pt sbt's  -------------------------------*/
/*------------------------------------------------------------*/
void sbtB8_mono(float *sample, unsigned char *pcm, int n)
{
   int i;

   for (i = 0; i < n; i++)
   {
      fdct8(sample, pMP3Stream->vbuf + pMP3Stream->vb_ptr);
      windowB8(pMP3Stream->vbuf, pMP3Stream->vb_ptr, pcm);
      sample += 64;
      pMP3Stream->vb_ptr = (pMP3Stream->vb_ptr - 8) & 127;
      pcm += 8;
   }

}
/*------------------------------------------------------------*/
void sbtB8_dual(float *sample, unsigned char *pcm, int n)
{
   int i;

   for (i = 0; i < n; i++)
   {
      fdct8_dual(sample, pMP3Stream->vbuf + pMP3Stream->vb_ptr);
      fdct8_dual(sample + 1, pMP3Stream->vbuf2 + pMP3Stream->vb_ptr);
      windowB8_dual(pMP3Stream->vbuf, pMP3Stream->vb_ptr, pcm);
      windowB8_dual(pMP3Stream->vbuf2, pMP3Stream->vb_ptr, pcm + 1);
      sample += 64;
      pMP3Stream->vb_ptr = (pMP3Stream->vb_ptr - 8) & 127;
      pcm += 16;
   }
}
/*------------------------------------------------------------*/
void sbtB8_dual_mono(float *sample, unsigned char *pcm, int n)
{
   int i;

   for (i = 0; i < n; i++)
   {
      fdct8_dual_mono(sample, pMP3Stream->vbuf + pMP3Stream->vb_ptr);
      windowB8(pMP3Stream->vbuf, pMP3Stream->vb_ptr, pcm);
      sample += 64;
      pMP3Stream->vb_ptr = (pMP3Stream->vb_ptr - 8) & 127;
      pcm += 8;
   }
}
/*------------------------------------------------------------*/
void sbtB8_dual_left(float *sample, unsigned char *pcm, int n)
{
   int i;

   for (i = 0; i < n; i++)
   {
      fdct8_dual(sample, pMP3Stream->vbuf + pMP3Stream->vb_ptr);
      windowB8(pMP3Stream->vbuf, pMP3Stream->vb_ptr, pcm);
      sample += 64;
      pMP3Stream->vb_ptr = (pMP3Stream->vb_ptr - 8) & 127;
      pcm += 8;
   }
}
/*------------------------------------------------------------*/
void sbtB8_dual_right(float *sample, unsigned char *pcm, int n)
{
   int i;

   sample++;
   for (i = 0; i < n; i++)
   {
      fdct8_dual(sample, pMP3Stream->vbuf + pMP3Stream->vb_ptr);
      windowB8(pMP3Stream->vbuf, pMP3Stream->vb_ptr, pcm);
      sample += 64;
      pMP3Stream->vb_ptr = (pMP3Stream->vb_ptr - 8) & 127;
      pcm += 8;
   }
}
/*------------------------------------------------------------*/


/********************************************************************************/
/*                                                                              */
/*  csbtL3.c                                                                    */
/*                                                                              */
/********************************************************************************/

/*============================================================*/
/*============ Layer III =====================================*/
/*============================================================*/
void sbt_mono_L3(float *sample, short *pcm, int ch)
{
  int i;
  
  ch = 0;
  for (i = 0; i < 18; i++)
  {
    fdct32(sample, pMP3Stream->vbuf + pMP3Stream->vb_ptr);
    window(pMP3Stream->vbuf, pMP3Stream->vb_ptr, pcm);
    sample += 32;
    pMP3Stream->vb_ptr = (pMP3Stream->vb_ptr - 32) & 511;
    pcm += 32;
  } 
}
/*------------------------------------------------------------*/
void sbt_dual_L3(float *sample, short *pcm, int ch)
{
  int i;
  
  if (ch == 0)
  {
    for (i = 0; i < 18; i++)
    {
      fdct32(sample, pMP3Stream->vbuf + pMP3Stream->vb_ptr);
      window_dual(pMP3Stream->vbuf, pMP3Stream->vb_ptr, pcm);
      sample += 32;
      pMP3Stream->vb_ptr = (pMP3Stream->vb_ptr - 32) & 511;
      pcm += 64;
    }
  }
  else
  {
    for (i = 0; i < 18; i++)
    {
      fdct32(sample, pMP3Stream->vbuf2 + pMP3Stream->vb2_ptr);
      window_dual(pMP3Stream->vbuf2, pMP3Stream->vb2_ptr, pcm + 1);
      sample += 32;
      pMP3Stream->vb2_ptr = (pMP3Stream->vb2_ptr - 32) & 511;
      pcm += 64;
    }
  }
}
/*------------------------------------------------------------*/
/*------------------------------------------------------------*/
/*---------------- 16 pt sbt's  -------------------------------*/
/*------------------------------------------------------------*/
void sbt16_mono_L3(float *sample, short *pcm, int ch)
{
  int i;
  
  ch = 0;
  for (i = 0; i < 18; i++)
  {
    fdct16(sample, pMP3Stream->vbuf + pMP3Stream->vb_ptr);
    window16(pMP3Stream->vbuf, pMP3Stream->vb_ptr, pcm);
    sample += 32;
    pMP3Stream->vb_ptr = (pMP3Stream->vb_ptr - 16) & 255;
    pcm += 16;
  }
}
/*------------------------------------------------------------*/
void sbt16_dual_L3(float *sample, short *pcm, int ch)
{
   int i;

   if (ch == 0)
   {
     for (i = 0; i < 18; i++)
     {
       fdct16(sample, pMP3Stream->vbuf + pMP3Stream->vb_ptr);
       window16_dual(pMP3Stream->vbuf, pMP3Stream->vb_ptr, pcm);
       sample += 32;
       pMP3Stream->vb_ptr = (pMP3Stream->vb_ptr - 16) & 255;
       pcm += 32;
     }
   }
   else
   {
     for (i = 0; i < 18; i++)
     {
       fdct16(sample, pMP3Stream->vbuf2 + pMP3Stream->vb2_ptr);
       window16_dual(pMP3Stream->vbuf2, pMP3Stream->vb2_ptr, pcm + 1);
       sample += 32;
       pMP3Stream->vb2_ptr = (pMP3Stream->vb2_ptr - 16) & 255;
       pcm += 32;
     }
   }   
}
/*------------------------------------------------------------*/
/*---------------- 8 pt sbt's  -------------------------------*/
/*------------------------------------------------------------*/
void sbt8_mono_L3(float *sample, short *pcm, int ch)
{
  int i;
  
  ch = 0;
  for (i = 0; i < 18; i++)
  {
    fdct8(sample, pMP3Stream->vbuf + pMP3Stream->vb_ptr);
    window8(pMP3Stream->vbuf, pMP3Stream->vb_ptr, pcm);
    sample += 32;
    pMP3Stream->vb_ptr = (pMP3Stream->vb_ptr - 8) & 127;
    pcm += 8;
  }
}
/*------------------------------------------------------------*/
void sbt8_dual_L3(float *sample, short *pcm, int ch)
{
  int i;
  
  if (ch == 0)
  {
    for (i = 0; i < 18; i++)
    {
      fdct8(sample, pMP3Stream->vbuf + pMP3Stream->vb_ptr);
      window8_dual(pMP3Stream->vbuf, pMP3Stream->vb_ptr, pcm);
      sample += 32;
      pMP3Stream->vb_ptr = (pMP3Stream->vb_ptr - 8) & 127;
      pcm += 16;
    }
  }
  else
  {
    for (i = 0; i < 18; i++)
    {
      fdct8(sample, pMP3Stream->vbuf2 + pMP3Stream->vb2_ptr);
      window8_dual(pMP3Stream->vbuf2, pMP3Stream->vb2_ptr, pcm + 1);
      sample += 32;
      pMP3Stream->vb2_ptr = (pMP3Stream->vb2_ptr - 8) & 127;
      pcm += 16;
    }
  }
}
/*------------------------------------------------------------*/
/*------- 8 bit output ---------------------------------------*/
/*------------------------------------------------------------*/
void sbtB_mono_L3(float *sample, unsigned char *pcm, int ch)
{
  int i;
  
  ch = 0;
  for (i = 0; i < 18; i++)
  {
    fdct32(sample, pMP3Stream->vbuf + pMP3Stream->vb_ptr);
    windowB(pMP3Stream->vbuf, pMP3Stream->vb_ptr, pcm);
    sample += 32;
    pMP3Stream->vb_ptr = (pMP3Stream->vb_ptr - 32) & 511;
    pcm += 32;
  }
}
/*------------------------------------------------------------*/
void sbtB_dual_L3(float *sample, unsigned char *pcm, int ch)
{
  int i;
  
  if (ch == 0)
  {
    for (i = 0; i < 18; i++)
    {
      fdct32(sample, pMP3Stream->vbuf + pMP3Stream->vb_ptr);
      windowB_dual(pMP3Stream->vbuf, pMP3Stream->vb_ptr, pcm);
      sample += 32;
      pMP3Stream->vb_ptr = (pMP3Stream->vb_ptr - 32) & 511;
      pcm += 64;
    }
  }
  else
  {
    for (i = 0; i < 18; i++)
    {
      fdct32(sample, pMP3Stream->vbuf2 + pMP3Stream->vb2_ptr);
      windowB_dual(pMP3Stream->vbuf2, pMP3Stream->vb2_ptr, pcm + 1);
      sample += 32;
      pMP3Stream->vb2_ptr = (pMP3Stream->vb2_ptr - 32) & 511;
      pcm += 64;
    }
  }
}
/*------------------------------------------------------------*/
/*------------------------------------------------------------*/
/*---------------- 16 pt sbtB's  -------------------------------*/
/*------------------------------------------------------------*/
void sbtB16_mono_L3(float *sample, unsigned char *pcm, int ch)
{
  int i;
  
  ch = 0;
  for (i = 0; i < 18; i++)
  {
    fdct16(sample, pMP3Stream->vbuf + pMP3Stream->vb_ptr);
    windowB16(pMP3Stream->vbuf, pMP3Stream->vb_ptr, pcm);
    sample += 32;
    pMP3Stream->vb_ptr = (pMP3Stream->vb_ptr - 16) & 255;
    pcm += 16;
  }
}
/*------------------------------------------------------------*/
void sbtB16_dual_L3(float *sample, unsigned char *pcm, int ch)
{
  int i;
  
  if (ch == 0)
  {
    for (i = 0; i < 18; i++)
    {
      fdct16(sample, pMP3Stream->vbuf + pMP3Stream->vb_ptr);
      windowB16_dual(pMP3Stream->vbuf, pMP3Stream->vb_ptr, pcm);
      sample += 32;
      pMP3Stream->vb_ptr = (pMP3Stream->vb_ptr - 16) & 255;
      pcm += 32;
    }
  }
  else
  {
    for (i = 0; i < 18; i++)
    {
      fdct16(sample, pMP3Stream->vbuf2 + pMP3Stream->vb2_ptr);
      windowB16_dual(pMP3Stream->vbuf2, pMP3Stream->vb2_ptr, pcm + 1);
      sample += 32;
      pMP3Stream->vb2_ptr = (pMP3Stream->vb2_ptr - 16) & 255;
      pcm += 32;
    }
  }
}
/*------------------------------------------------------------*/
/*---------------- 8 pt sbtB's  -------------------------------*/
/*------------------------------------------------------------*/
void sbtB8_mono_L3(float *sample, unsigned char *pcm, int ch)
{
  int i;
  
  ch = 0;
  for (i = 0; i < 18; i++)
  {
    fdct8(sample, pMP3Stream->vbuf + pMP3Stream->vb_ptr);
    windowB8(pMP3Stream->vbuf, pMP3Stream->vb_ptr, pcm);
    sample += 32;
    pMP3Stream->vb_ptr = (pMP3Stream->vb_ptr - 8) & 127;
    pcm += 8;
  }
}
/*------------------------------------------------------------*/
void sbtB8_dual_L3(float *sample, unsigned char *pcm, int ch)
{
  int i;
  
  if (ch == 0)
  {
    for (i = 0; i < 18; i++)
    {
      fdct8(sample, pMP3Stream->vbuf + pMP3Stream->vb_ptr);
      windowB8_dual(pMP3Stream->vbuf, pMP3Stream->vb_ptr, pcm);
      sample += 32;
      pMP3Stream->vb_ptr = (pMP3Stream->vb_ptr - 8) & 127;
      pcm += 16;
    }
  }
  else
  {
    for (i = 0; i < 18; i++)
    {
      fdct8(sample, pMP3Stream->vbuf2 + pMP3Stream->vb2_ptr);
      windowB8_dual(pMP3Stream->vbuf2, pMP3Stream->vb2_ptr, pcm + 1);
      sample += 32;
      pMP3Stream->vb2_ptr = (pMP3Stream->vb2_ptr - 8) & 127;
      pcm += 16;
    }
  }
}
/*------------------------------------------------------------*/

/********************************************************************************/
/*                                                                              */
/*  cupini.c                                                                    */
/*                                                                              */
/********************************************************************************/

static const long steps[18] =
{
   0, 3, 5, 7, 9, 15, 31, 63, 127,
   255, 511, 1023, 2047, 4095, 8191, 16383, 32767, 65535};


/* ABCD_INDEX = lookqt[mode][sr_index][br_index]  */
/* -1 = invalid  */
static const signed char lookqt[4][3][16] =
{
 {{1, -1, -1, -1, 2, -1, 2, 0, 0, 0, 1, 1, 1, 1, 1, -1},  /*  44ks stereo */
  {0, -1, -1, -1, 2, -1, 2, 0, 0, 0, 0, 0, 0, 0, 0, -1},  /*  48ks */
  {1, -1, -1, -1, 3, -1, 3, 0, 0, 0, 1, 1, 1, 1, 1, -1}}, /*  32ks */
 {{1, -1, -1, -1, 2, -1, 2, 0, 0, 0, 1, 1, 1, 1, 1, -1},  /*  44ks joint stereo */
  {0, -1, -1, -1, 2, -1, 2, 0, 0, 0, 0, 0, 0, 0, 0, -1},  /*  48ks */
  {1, -1, -1, -1, 3, -1, 3, 0, 0, 0, 1, 1, 1, 1, 1, -1}}, /*  32ks */
 {{1, -1, -1, -1, 2, -1, 2, 0, 0, 0, 1, 1, 1, 1, 1, -1},  /*  44ks dual chan */
  {0, -1, -1, -1, 2, -1, 2, 0, 0, 0, 0, 0, 0, 0, 0, -1},  /*  48ks */
  {1, -1, -1, -1, 3, -1, 3, 0, 0, 0, 1, 1, 1, 1, 1, -1}}, /*  32ks */
// mono extended beyond legal br index
//  1,2,2,0,0,0,1,1,1,1,1,1,1,1,1,-1,          /*  44ks single chan */
//  0,2,2,0,0,0,0,0,0,0,0,0,0,0,0,-1,          /*  48ks */
//  1,3,3,0,0,0,1,1,1,1,1,1,1,1,1,-1,          /*  32ks */
// legal mono
 {{1, 2, 2, 0, 0, 0, 1, 1, 1, 1, 1, -1, -1, -1, -1, -1},  /*  44ks single chan */
  {0, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, -1, -1, -1, -1, -1},  /*  48ks */
  {1, 3, 3, 0, 0, 0, 1, 1, 1, 1, 1, -1, -1, -1, -1, -1}}, /*  32ks */
};

static const long sr_table[8] =
{22050L, 24000L, 16000L, 1L,
 44100L, 48000L, 32000L, 1L};

/* bit allocation table look up */
/* table per mpeg spec tables 3b2a/b/c/d  /e is mpeg2 */
/* look_bat[abcd_index][4][16]  */
static const unsigned char look_bat[5][4][16] =
{
/* LOOK_BATA */
 {{0, 1, 3, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17},
  {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 17},
  {0, 1, 2, 3, 4, 5, 6, 17, 0, 0, 0, 0, 0, 0, 0, 0},
  {0, 1, 2, 17, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
/* LOOK_BATB */
 {{0, 1, 3, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17},
  {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 17},
  {0, 1, 2, 3, 4, 5, 6, 17, 0, 0, 0, 0, 0, 0, 0, 0},
  {0, 1, 2, 17, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
/* LOOK_BATC */
 {{0, 1, 2, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16},
  {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
  {0, 1, 2, 4, 5, 6, 7, 8, 0, 0, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
/* LOOK_BATD */
 {{0, 1, 2, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16},
  {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
  {0, 1, 2, 4, 5, 6, 7, 8, 0, 0, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
/* LOOK_BATE */
 {{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15},
  {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
  {0, 1, 2, 4, 5, 6, 7, 8, 0, 0, 0, 0, 0, 0, 0, 0},
  {0, 1, 2, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
};

/* look_nbat[abcd_index]][4] */
static const unsigned char look_nbat[5][4] =
{
  {3, 8, 12, 4},
  {3, 8, 12, 7},
  {2, 0, 6, 0},
  {2, 0, 10, 0},
  {4, 0, 7, 19},
};

/*

void sbt_mono(float *sample, short *pcm, int n);
void sbt_dual(float *sample, short *pcm, int n);
void sbt_dual_mono(float *sample, short *pcm, int n);
void sbt_dual_left(float *sample, short *pcm, int n);
void sbt_dual_right(float *sample, short *pcm, int n);
void sbt16_mono(float *sample, short *pcm, int n);
void sbt16_dual(float *sample, short *pcm, int n);
void sbt16_dual_mono(float *sample, short *pcm, int n);
void sbt16_dual_left(float *sample, short *pcm, int n);
void sbt16_dual_right(float *sample, short *pcm, int n);
void sbt8_mono(float *sample, short *pcm, int n);
void sbt8_dual(float *sample, short *pcm, int n);
void sbt8_dual_mono(float *sample, short *pcm, int n);
void sbt8_dual_left(float *sample, short *pcm, int n);
void sbt8_dual_right(float *sample, short *pcm, int n);

/*--- 8 bit output ---*//*
void sbtB_mono(float *sample, unsigned char *pcm, int n);
void sbtB_dual(float *sample, unsigned char *pcm, int n);
void sbtB_dual_mono(float *sample, unsigned char *pcm, int n);
void sbtB_dual_left(float *sample, unsigned char *pcm, int n);
void sbtB_dual_right(float *sample, unsigned char *pcm, int n);
void sbtB16_mono(float *sample, unsigned char *pcm, int n);
void sbtB16_dual(float *sample, unsigned char *pcm, int n);
void sbtB16_dual_mono(float *sample, unsigned char *pcm, int n);
void sbtB16_dual_left(float *sample, unsigned char *pcm, int n);
void sbtB16_dual_right(float *sample, unsigned char *pcm, int n);
void sbtB8_mono(float *sample, unsigned char *pcm, int n);
void sbtB8_dual(float *sample, unsigned char *pcm, int n);
void sbtB8_dual_mono(float *sample, unsigned char *pcm, int n);
void sbtB8_dual_left(float *sample, unsigned char *pcm, int n);
void sbtB8_dual_right(float *sample, unsigned char *pcm, int n);*/


static const SBT_FUNCTION sbt_table[2][3][5] =
{
 {{sbt_mono, sbt_dual, sbt_dual_mono, sbt_dual_left, sbt_dual_right},
  {sbt16_mono, sbt16_dual, sbt16_dual_mono, sbt16_dual_left, sbt16_dual_right},
  {sbt8_mono, sbt8_dual, sbt8_dual_mono, sbt8_dual_left, sbt8_dual_right}},
 {{(SBT_FUNCTION) sbtB_mono,
   (SBT_FUNCTION) sbtB_dual,
   (SBT_FUNCTION) sbtB_dual_mono,
   (SBT_FUNCTION) sbtB_dual_left,
   (SBT_FUNCTION) sbtB_dual_right},
  {(SBT_FUNCTION) sbtB16_mono,
   (SBT_FUNCTION) sbtB16_dual,
   (SBT_FUNCTION) sbtB16_dual_mono,
   (SBT_FUNCTION) sbtB16_dual_left,
   (SBT_FUNCTION) sbtB16_dual_right},
  {(SBT_FUNCTION) sbtB8_mono,
   (SBT_FUNCTION) sbtB8_dual,
   (SBT_FUNCTION) sbtB8_dual_mono,
   (SBT_FUNCTION) sbtB8_dual_left,
   (SBT_FUNCTION) sbtB8_dual_right}},
};

static const int out_chans[5] =
{1, 2, 1, 1, 1};


int audio_decode_initL1(MPEG_HEAD * h, int framebytes_arg,
       int reduction_code, int transform_code, int convert_code,
      int freq_limit);
void sbt_init();


IN_OUT L1audio_decode(unsigned char *bs, signed short *pcm);
IN_OUT L2audio_decode(unsigned char *bs, signed short *pcm);
IN_OUT L3audio_decode(unsigned char *bs, unsigned char *pcm);
static const AUDIO_DECODE_ROUTINE decode_routine_table[4] =
{
   L2audio_decode,
   (AUDIO_DECODE_ROUTINE)L3audio_decode,
   L2audio_decode,
   L1audio_decode,};

/*---------------------------------------------------------*/
static void table_init()
{
   int i, j;
   int code;
   static int iOnceOnly=0;

   if (!iOnceOnly++)
   {
  /*--  c_values (dequant) --*/
     for (i = 1; i < 18; i++)
      look_c_value[i] = 2.0F / steps[i];

  /*--  scale factor table, scale by 32768 for 16 pcm output  --*/
     for (i = 0; i < 64; i++)
      sf_table[i] = (float) (32768.0 * 2.0 * pow(2.0, -i / 3.0));   

  /*--  grouped 3 level lookup table 5 bit token --*/
     for (i = 0; i < 32; i++)
     {
      code = i;
      for (j = 0; j < 3; j++)
      {
     group3_table[i][j] = (char) ((code % 3) - 1);
     code /= 3;
      }
     }
   
  /*--  grouped 5 level lookup table 7 bit token --*/
     for (i = 0; i < 128; i++)
     {
      code = i;
      for (j = 0; j < 3; j++)
      {
     group5_table[i][j] = (char) ((code % 5) - 2);
     code /= 5;
      }
     }

  /*--  grouped 9 level lookup table 10 bit token --*/
     for (i = 0; i < 1024; i++)
     {
      code = i;
      for (j = 0; j < 3; j++)
      {
     group9_table[i][j] = (short) ((code % 9) - 4);
     code /= 9;
      }
     }
   }
}
/*---------------------------------------------------------*/
int L1audio_decode_init(MPEG_HEAD * h, int framebytes_arg,
       int reduction_code, int transform_code, int convert_code,
      int freq_limit);
int L3audio_decode_init(MPEG_HEAD * h, int framebytes_arg,
       int reduction_code, int transform_code, int convert_code,
      int freq_limit);

/*---------------------------------------------------------*/
/* mpeg_head defined in mhead.h  frame bytes is without pad */
int audio_decode_init(MPEG_HEAD * h, int framebytes_arg,
       int reduction_code, int transform_code, int convert_code,
          int freq_limit)
{
   int i, j, k;
   static int first_pass = 1;
   int abcd_index;
   long samprate;
   int limit;
   int bit_code;

   if (first_pass)
   {
      table_init();
      first_pass = 0;
   }

/* select decoder routine Layer I,II,III */
   audio_decode_routine = decode_routine_table[h->option & 3];


   if (h->option == 3)    /* layer I */
      return L1audio_decode_init(h, framebytes_arg,
      reduction_code, transform_code, convert_code, freq_limit);

   if (h->option == 1)    /* layer III */
      return L3audio_decode_init(h, framebytes_arg,
      reduction_code, transform_code, convert_code, freq_limit);



   transform_code = transform_code; /* not used, asm compatability */
   bit_code = 0;
   if (convert_code & 8)
      bit_code = 1;
   convert_code = convert_code & 3; /* higher bits used by dec8 freq cvt */
   if (reduction_code < 0)
      reduction_code = 0;
   if (reduction_code > 2)
      reduction_code = 2;
   if (freq_limit < 1000)
      freq_limit = 1000;


   pMP3Stream->framebytes = framebytes_arg;
/* check if code handles */
   if (h->option != 2)
      return 0;     /* layer II only */
   if (h->sr_index == 3)
      return 0;     /* reserved */

/* compute abcd index for bit allo table selection */
   if (h->id)     /* mpeg 1 */
      abcd_index = lookqt[h->mode][h->sr_index][h->br_index];
   else
      abcd_index = 4;   /* mpeg 2 */

   if (abcd_index < 0)
      return 0;     // fail invalid Layer II bit rate index

   for (i = 0; i < 4; i++)
      for (j = 0; j < 16; j++)
    pMP3Stream->bat[i][j] = look_bat[abcd_index][i][j];
   for (i = 0; i < 4; i++)
      pMP3Stream->nbat[i] = look_nbat[abcd_index][i];
   pMP3Stream->max_sb = pMP3Stream->nbat[0] + pMP3Stream->nbat[1] + pMP3Stream->nbat[2] + pMP3Stream->nbat[3];
/*----- compute pMP3Stream->nsb_limit --------*/
   samprate = sr_table[4 * h->id + h->sr_index];
   pMP3Stream->nsb_limit = (freq_limit * 64L + samprate / 2) / samprate;
/*- caller limit -*/
/*---- limit = 0.94*(32>>reduction_code);  ----*/
   limit = (32 >> reduction_code);
   if (limit > 8)
      limit--;
   if (pMP3Stream->nsb_limit > limit)
      pMP3Stream->nsb_limit = limit;
   if (pMP3Stream->nsb_limit > pMP3Stream->max_sb)
      pMP3Stream->nsb_limit = pMP3Stream->max_sb;

   pMP3Stream->outvalues = 1152 >> reduction_code;
   if (h->mode != 3)
   {        /* adjust for 2 channel modes */
      for (i = 0; i < 4; i++)
    pMP3Stream->nbat[i] *= 2;
      pMP3Stream->max_sb *= 2;
      pMP3Stream->nsb_limit *= 2;
   }

/* set sbt function */
   k = 1 + convert_code;
   if (h->mode == 3)
   {
      k = 0;
   }
   pMP3Stream->sbt = sbt_table[bit_code][reduction_code][k];
   pMP3Stream->outvalues *= out_chans[k];
   if (bit_code)
      pMP3Stream->outbytes = pMP3Stream->outvalues;
   else
      pMP3Stream->outbytes = sizeof(short) * pMP3Stream->outvalues;

   decinfo.channels = out_chans[k];
   decinfo.outvalues = pMP3Stream->outvalues;
   decinfo.samprate = samprate >> reduction_code;
   if (bit_code)
      decinfo._bits = 8;
   else
      decinfo._bits = sizeof(short) * 8;

   decinfo.framebytes = pMP3Stream->framebytes;
   decinfo.type = 0;



/* clear sample buffer, unused sub bands must be 0 */
   for (i = 0; i < 2304*2; i++) // the *2 here was inserted by me just in case, since the array is now *2, because of stereo files unpacking at 4608 bytes per frame (which may or may not be relevant, but in any case I don't think we use the L1 versions of MP3 now anyway
      ___sample[i] = 0.0F;


/* init sub-band transform */
   sbt_init();

   return 1;
}
/*---------------------------------------------------------*/
void audio_decode_info(DEC_INFO * info)
{
   *info = decinfo;   /* info return, call after init */
}
/*---------------------------------------------------------*/
void decode_table_init()
{
/* dummy for asm version compatability */
}
/*---------------------------------------------------------*/

/********************************************************************************/
/*                                                                              */
/*  mdct.c                                                                      */
/*                                                                              */
/********************************************************************************/

/****  mdct.c  ***************************************************

Layer III 

  cos transform for n=18, n=6

computes  c[k] =  Sum( cos((pi/4*n)*(2*k+1)*(2*p+1))*f[p] )
                k = 0, ...n-1,  p = 0...n-1


inplace ok.

******************************************************************/

/*------ 18 point xform -------*/
float mdct18w[18];    // effectively constant
float mdct18w2[9];    //  "  "
float coef[9][4];   //  "  "

float mdct6_3v[6];    //  "  "
float mdct6_3v2[3];   //  "  "
float coef87;     //  "  "

typedef struct
{
   float *w;
   float *w2;
   void *coef;
}
IMDCT_INIT_BLOCK;

static const IMDCT_INIT_BLOCK imdct_info_18 =
{mdct18w, mdct18w2, coef};
static const IMDCT_INIT_BLOCK imdct_info_6 =
{mdct6_3v, mdct6_3v2, &coef87};



/*====================================================================*/
const IMDCT_INIT_BLOCK *imdct_init_addr_18()
{
   return &imdct_info_18;
}
const IMDCT_INIT_BLOCK *imdct_init_addr_6()
{
   return &imdct_info_6;
}
/*--------------------------------------------------------------------*/
void imdct18(float f[18]) /* 18 point */
{
   int p;
   float a[9], b[9];
   float ap, bp, a8p, b8p;
   float g1, g2;


   for (p = 0; p < 4; p++)
   {
      g1 = mdct18w[p] * f[p];
      g2 = mdct18w[17 - p] * f[17 - p];
      ap = g1 + g2;   // a[p]

      bp = mdct18w2[p] * (g1 - g2); // b[p]

      g1 = mdct18w[8 - p] * f[8 - p];
      g2 = mdct18w[9 + p] * f[9 + p];
      a8p = g1 + g2;    // a[8-p]

      b8p = mdct18w2[8 - p] * (g1 - g2);  // b[8-p]

      a[p] = ap + a8p;
      a[5 + p] = ap - a8p;
      b[p] = bp + b8p;
      b[5 + p] = bp - b8p;
   }
   g1 = mdct18w[p] * f[p];
   g2 = mdct18w[17 - p] * f[17 - p];
   a[p] = g1 + g2;
   b[p] = mdct18w2[p] * (g1 - g2);


   f[0] = 0.5f * (a[0] + a[1] + a[2] + a[3] + a[4]);
   f[1] = 0.5f * (b[0] + b[1] + b[2] + b[3] + b[4]);

   f[2] = coef[1][0] * a[5] + coef[1][1] * a[6] + coef[1][2] * a[7]
      + coef[1][3] * a[8];
   f[3] = coef[1][0] * b[5] + coef[1][1] * b[6] + coef[1][2] * b[7]
      + coef[1][3] * b[8] - f[1];
   f[1] = f[1] - f[0];
   f[2] = f[2] - f[1];

   f[4] = coef[2][0] * a[0] + coef[2][1] * a[1] + coef[2][2] * a[2]
      + coef[2][3] * a[3] - a[4];
   f[5] = coef[2][0] * b[0] + coef[2][1] * b[1] + coef[2][2] * b[2]
      + coef[2][3] * b[3] - b[4] - f[3];
   f[3] = f[3] - f[2];
   f[4] = f[4] - f[3];

   f[6] = coef[3][0] * (a[5] - a[7] - a[8]);
   f[7] = coef[3][0] * (b[5] - b[7] - b[8]) - f[5];
   f[5] = f[5] - f[4];
   f[6] = f[6] - f[5];

   f[8] = coef[4][0] * a[0] + coef[4][1] * a[1] + coef[4][2] * a[2]
      + coef[4][3] * a[3] + a[4];
   f[9] = coef[4][0] * b[0] + coef[4][1] * b[1] + coef[4][2] * b[2]
      + coef[4][3] * b[3] + b[4] - f[7];
   f[7] = f[7] - f[6];
   f[8] = f[8] - f[7];

   f[10] = coef[5][0] * a[5] + coef[5][1] * a[6] + coef[5][2] * a[7]
      + coef[5][3] * a[8];
   f[11] = coef[5][0] * b[5] + coef[5][1] * b[6] + coef[5][2] * b[7]
      + coef[5][3] * b[8] - f[9];
   f[9] = f[9] - f[8];
   f[10] = f[10] - f[9];

   f[12] = 0.5f * (a[0] + a[2] + a[3]) - a[1] - a[4];
   f[13] = 0.5f * (b[0] + b[2] + b[3]) - b[1] - b[4] - f[11];
   f[11] = f[11] - f[10];
   f[12] = f[12] - f[11];

   f[14] = coef[7][0] * a[5] + coef[7][1] * a[6] + coef[7][2] * a[7]
      + coef[7][3] * a[8];
   f[15] = coef[7][0] * b[5] + coef[7][1] * b[6] + coef[7][2] * b[7]
      + coef[7][3] * b[8] - f[13];
   f[13] = f[13] - f[12];
   f[14] = f[14] - f[13];

   f[16] = coef[8][0] * a[0] + coef[8][1] * a[1] + coef[8][2] * a[2]
      + coef[8][3] * a[3] + a[4];
   f[17] = coef[8][0] * b[0] + coef[8][1] * b[1] + coef[8][2] * b[2]
      + coef[8][3] * b[3] + b[4] - f[15];
   f[15] = f[15] - f[14];
   f[16] = f[16] - f[15];
   f[17] = f[17] - f[16];


   return;
}
/*--------------------------------------------------------------------*/
/* does 3, 6 pt dct.  changes order from f[i][window] c[window][i] */
void imdct6_3(float f[])  /* 6 point */
{
   int w;
   float buf[18];
   float *a, *c;    // b[i] = a[3+i]

   float g1, g2;
   float a02, b02;

   c = f;
   a = buf;
   for (w = 0; w < 3; w++)
   {
      g1 = mdct6_3v[0] * f[3 * 0];
      g2 = mdct6_3v[5] * f[3 * 5];
      a[0] = g1 + g2;
      a[3 + 0] = mdct6_3v2[0] * (g1 - g2);

      g1 = mdct6_3v[1] * f[3 * 1];
      g2 = mdct6_3v[4] * f[3 * 4];
      a[1] = g1 + g2;
      a[3 + 1] = mdct6_3v2[1] * (g1 - g2);

      g1 = mdct6_3v[2] * f[3 * 2];
      g2 = mdct6_3v[3] * f[3 * 3];
      a[2] = g1 + g2;
      a[3 + 2] = mdct6_3v2[2] * (g1 - g2);

      a += 6;
      f++;
   }

   a = buf;
   for (w = 0; w < 3; w++)
   {
      a02 = (a[0] + a[2]);
      b02 = (a[3 + 0] + a[3 + 2]);
      c[0] = a02 + a[1];
      c[1] = b02 + a[3 + 1];
      c[2] = coef87 * (a[0] - a[2]);
      c[3] = coef87 * (a[3 + 0] - a[3 + 2]) - c[1];
      c[1] = c[1] - c[0];
      c[2] = c[2] - c[1];
      c[4] = a02 - a[1] - a[1];
      c[5] = b02 - a[3 + 1] - a[3 + 1] - c[3];
      c[3] = c[3] - c[2];
      c[4] = c[4] - c[3];
      c[5] = c[5] - c[4];
      a += 6;
      c += 6;
   }

   return;
}
/*--------------------------------------------------------------------*/

/********************************************************************************/
/*                                                                              */
/*  hwin.c                                                                     */
/*                                                                              */
/********************************************************************************/

/*____________________________________________________________________________
  
  FreeAmp - The Free MP3 Player

        MP3 Decoder originally Copyright (C) 1995-1997 Xing Technology
        Corp.  http://www.xingtech.com

  Portions Copyright (C) 1998-1999 EMusic.com

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
  
  $Id: hwin.c,v 1.5 1999/10/19 07:13:08 elrod Exp $
____________________________________________________________________________*/

/****  hwin.c  ***************************************************

Layer III 

hybrid window/filter 

******************************************************************/

////@@@@extern int band_limit_nsb;

typedef float ARRAY36[36];

/*-- windows by block type --*/
static float win[4][36];  // effectively a constant

/*====================================================================*/
/*void imdct18(float f[]);  /* 18 point */
/*void imdct6_3(float f[]); /* 6 point */

/*====================================================================*/
ARRAY36 *hwin_init_addr()
{
   return win;
}


/*====================================================================*/
int hybrid(float xin[], float xprev[], float y[18][32],
     int btype, int nlong, int ntot, int nprev)
{
   int i, j;
   float *x, *x0;
   float xa, xb;
   int n;
   int nout;



   if (btype == 2)
      btype = 0;
   x = xin;
   x0 = xprev;

/*-- do long blocks (if any) --*/
   n = (nlong + 17) / 18; /* number of dct's to do */
   for (i = 0; i < n; i++)
   {
      imdct18(x);
      for (j = 0; j < 9; j++)
      {
   y[j][i] = x0[j] + win[btype][j] * x[9 + j];
   y[9 + j][i] = x0[9 + j] + win[btype][9 + j] * x[17 - j];
      }
    /* window x for next time x0 */
      for (j = 0; j < 4; j++)
      {
   xa = x[j];
   xb = x[8 - j];
   x[j] = win[btype][18 + j] * xb;
   x[8 - j] = win[btype][(18 + 8) - j] * xa;
   x[9 + j] = win[btype][(18 + 9) + j] * xa;
   x[17 - j] = win[btype][(18 + 17) - j] * xb;
      }
      xa = x[j];
      x[j] = win[btype][18 + j] * xa;
      x[9 + j] = win[btype][(18 + 9) + j] * xa;

      x += 18;
      x0 += 18;
   }

/*-- do short blocks (if any) --*/
   n = (ntot + 17) / 18;  /* number of 6 pt dct's triples to do */
   for (; i < n; i++)
   {
      imdct6_3(x);
      for (j = 0; j < 3; j++)
      {
   y[j][i] = x0[j];
   y[3 + j][i] = x0[3 + j];

   y[6 + j][i] = x0[6 + j] + win[2][j] * x[3 + j];
   y[9 + j][i] = x0[9 + j] + win[2][3 + j] * x[5 - j];

   y[12 + j][i] = x0[12 + j] + win[2][6 + j] * x[2 - j] + win[2][j] * x[(6 + 3) + j];
   y[15 + j][i] = x0[15 + j] + win[2][9 + j] * x[j] + win[2][3 + j] * x[(6 + 5) - j];
      }
    /* window x for next time x0 */
      for (j = 0; j < 3; j++)
      {
   x[j] = win[2][6 + j] * x[(6 + 2) - j] + win[2][j] * x[(12 + 3) + j];
   x[3 + j] = win[2][9 + j] * x[6 + j] + win[2][3 + j] * x[(12 + 5) - j];
      }
      for (j = 0; j < 3; j++)
      {
   x[6 + j] = win[2][6 + j] * x[(12 + 2) - j];
   x[9 + j] = win[2][9 + j] * x[12 + j];
      }
      for (j = 0; j < 3; j++)
      {
   x[12 + j] = 0.0f;
   x[15 + j] = 0.0f;
      }
      x += 18;
      x0 += 18;
   }

/*--- overlap prev if prev longer that current --*/
   n = (nprev + 17) / 18;
   for (; i < n; i++)
   {
      for (j = 0; j < 18; j++)
   y[j][i] = x0[j];
      x0 += 18;
   }
   nout = 18 * i;

/*--- clear remaining only to band limit --*/
   for (; i < pMP3Stream->band_limit_nsb; i++)
   {
      for (j = 0; j < 18; j++)
   y[j][i] = 0.0f;
   }

   return nout;
}


/*--------------------------------------------------------------------*/
/*--------------------------------------------------------------------*/
/*-- convert to mono, add curr result to y,
    window and add next time to current left */
int hybrid_sum(float xin[], float xin_left[], float y[18][32],
         int btype, int nlong, int ntot)
{
   int i, j;
   float *x, *x0;
   float xa, xb;
   int n;
   int nout;



   if (btype == 2)
      btype = 0;
   x = xin;
   x0 = xin_left;

/*-- do long blocks (if any) --*/
   n = (nlong + 17) / 18; /* number of dct's to do */
   for (i = 0; i < n; i++)
   {
      imdct18(x);
      for (j = 0; j < 9; j++)
      {
   y[j][i] += win[btype][j] * x[9 + j];
   y[9 + j][i] += win[btype][9 + j] * x[17 - j];
      }
    /* window x for next time x0 */
      for (j = 0; j < 4; j++)
      {
   xa = x[j];
   xb = x[8 - j];
   x0[j] += win[btype][18 + j] * xb;
   x0[8 - j] += win[btype][(18 + 8) - j] * xa;
   x0[9 + j] += win[btype][(18 + 9) + j] * xa;
   x0[17 - j] += win[btype][(18 + 17) - j] * xb;
      }
      xa = x[j];
      x0[j] += win[btype][18 + j] * xa;
      x0[9 + j] += win[btype][(18 + 9) + j] * xa;

      x += 18;
      x0 += 18;
   }

/*-- do short blocks (if any) --*/
   n = (ntot + 17) / 18;  /* number of 6 pt dct's triples to do */
   for (; i < n; i++)
   {
      imdct6_3(x);
      for (j = 0; j < 3; j++)
      {
   y[6 + j][i] += win[2][j] * x[3 + j];
   y[9 + j][i] += win[2][3 + j] * x[5 - j];

   y[12 + j][i] += win[2][6 + j] * x[2 - j] + win[2][j] * x[(6 + 3) + j];
   y[15 + j][i] += win[2][9 + j] * x[j] + win[2][3 + j] * x[(6 + 5) - j];
      }
    /* window x for next time */
      for (j = 0; j < 3; j++)
      {
   x0[j] += win[2][6 + j] * x[(6 + 2) - j] + win[2][j] * x[(12 + 3) + j];
   x0[3 + j] += win[2][9 + j] * x[6 + j] + win[2][3 + j] * x[(12 + 5) - j];
      }
      for (j = 0; j < 3; j++)
      {
   x0[6 + j] += win[2][6 + j] * x[(12 + 2) - j];
   x0[9 + j] += win[2][9 + j] * x[12 + j];
      }
      x += 18;
      x0 += 18;
   }

   nout = 18 * i;

   return nout;
}
/*--------------------------------------------------------------------*/
void sum_f_bands(float a[], float b[], int n)
{
   int i;

   for (i = 0; i < n; i++)
      a[i] += b[i];
}
/*--------------------------------------------------------------------*/
/*--------------------------------------------------------------------*/
void FreqInvert(float y[18][32], int n)
{
   int i, j;

   n = (n + 17) / 18;
   for (j = 0; j < 18; j += 2)
   {
      for (i = 0; i < n; i += 2)
      {
   y[1 + j][1 + i] = -y[1 + j][1 + i];
      }
   }
}
/*--------------------------------------------------------------------*/

/********************************************************************************/
/*                                                                              */
/*  htable.h                                                                    */
/*                                                                              */
/********************************************************************************/

/*____________________________________________________________________________
  
  FreeAmp - The Free MP3 Player

        MP3 Decoder originally Copyright (C) 1995-1997 Xing Technology
        Corp.  http://www.xingtech.com

  Portions Copyright (C) 1998-1999 EMusic.com

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License}, {or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not}, {write to the Free Software
  Foundation}, {Inc.}, {675 Mass Ave}, {Cambridge}, {MA 02139}, {USA.
  
  $Id: htable.h,v 1.2 1999/10/19 07:13:08 elrod Exp $
____________________________________________________________________________*/

/* TABLE  1    4 entries  maxbits  3  linbits  0 */
static const HUFF_ELEMENT huff_table_1[] =
{
  {0xFF000003}, {0x03010102}, {0x03010001}, {0x02000101}, {0x02000101}, /*  4 */
  {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000},
};

/* max table bits  3 */

/* TABLE  2    9 entries  maxbits  6  linbits  0 */
static const HUFF_ELEMENT huff_table_2[] =
{
  {0xFF000006}, {0x06020202}, {0x06020001}, {0x05020102}, {0x05020102}, /*  4 */
  {0x05010202}, {0x05010202}, {0x05000201}, {0x05000201}, {0x03010102}, /*  9 */
  {0x03010102}, {0x03010102}, {0x03010102}, {0x03010102}, {0x03010102}, /* 14 */
  {0x03010102}, {0x03010102}, {0x03010001}, {0x03010001}, {0x03010001}, /* 19 */
  {0x03010001}, {0x03010001}, {0x03010001}, {0x03010001}, {0x03010001}, /* 24 */
  {0x03000101}, {0x03000101}, {0x03000101}, {0x03000101}, {0x03000101}, /* 29 */
  {0x03000101}, {0x03000101}, {0x03000101}, {0x01000000}, {0x01000000}, /* 34 */
  {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, /* 39 */
  {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, /* 44 */
  {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, /* 49 */
  {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, /* 54 */
  {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, /* 59 */
  {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, /* 64 */
};

/* max table bits  6 */

/* TABLE  3    9 entries  maxbits  6  linbits  0 */
static const HUFF_ELEMENT huff_table_3[] =
{
  {0xFF000006}, {0x06020202}, {0x06020001}, {0x05020102}, {0x05020102}, /*  4 */
  {0x05010202}, {0x05010202}, {0x05000201}, {0x05000201}, {0x03000101}, /*  9 */
  {0x03000101}, {0x03000101}, {0x03000101}, {0x03000101}, {0x03000101}, /* 14 */
  {0x03000101}, {0x03000101}, {0x02010102}, {0x02010102}, {0x02010102}, /* 19 */
  {0x02010102}, {0x02010102}, {0x02010102}, {0x02010102}, {0x02010102}, /* 24 */
  {0x02010102}, {0x02010102}, {0x02010102}, {0x02010102}, {0x02010102}, /* 29 */
  {0x02010102}, {0x02010102}, {0x02010102}, {0x02010001}, {0x02010001}, /* 34 */
  {0x02010001}, {0x02010001}, {0x02010001}, {0x02010001}, {0x02010001}, /* 39 */
  {0x02010001}, {0x02010001}, {0x02010001}, {0x02010001}, {0x02010001}, /* 44 */
  {0x02010001}, {0x02010001}, {0x02010001}, {0x02010001}, {0x02000000}, /* 49 */
  {0x02000000}, {0x02000000}, {0x02000000}, {0x02000000}, {0x02000000}, /* 54 */
  {0x02000000}, {0x02000000}, {0x02000000}, {0x02000000}, {0x02000000}, /* 59 */
  {0x02000000}, {0x02000000}, {0x02000000}, {0x02000000}, {0x02000000}, /* 64 */
};

/* max table bits  6 */
/* NO XING TABLE 4 */

/* TABLE  5   16 entries  maxbits  8  linbits  0 */
static const HUFF_ELEMENT huff_table_5[] =
{
  {0xFF000008}, {0x08030302}, {0x08030202}, {0x07020302}, {0x07020302}, /*  4 */
  {0x06010302}, {0x06010302}, {0x06010302}, {0x06010302}, {0x07030102}, /*  9 */
  {0x07030102}, {0x07030001}, {0x07030001}, {0x07000301}, {0x07000301}, /* 14 */
  {0x07020202}, {0x07020202}, {0x06020102}, {0x06020102}, {0x06020102}, /* 19 */
  {0x06020102}, {0x06010202}, {0x06010202}, {0x06010202}, {0x06010202}, /* 24 */
  {0x06020001}, {0x06020001}, {0x06020001}, {0x06020001}, {0x06000201}, /* 29 */
  {0x06000201}, {0x06000201}, {0x06000201}, {0x03010102}, {0x03010102}, /* 34 */
  {0x03010102}, {0x03010102}, {0x03010102}, {0x03010102}, {0x03010102}, /* 39 */
  {0x03010102}, {0x03010102}, {0x03010102}, {0x03010102}, {0x03010102}, /* 44 */
  {0x03010102}, {0x03010102}, {0x03010102}, {0x03010102}, {0x03010102}, /* 49 */
  {0x03010102}, {0x03010102}, {0x03010102}, {0x03010102}, {0x03010102}, /* 54 */
  {0x03010102}, {0x03010102}, {0x03010102}, {0x03010102}, {0x03010102}, /* 59 */
  {0x03010102}, {0x03010102}, {0x03010102}, {0x03010102}, {0x03010102}, /* 64 */
  {0x03010001}, {0x03010001}, {0x03010001}, {0x03010001}, {0x03010001}, /* 69 */
  {0x03010001}, {0x03010001}, {0x03010001}, {0x03010001}, {0x03010001}, /* 74 */
  {0x03010001}, {0x03010001}, {0x03010001}, {0x03010001}, {0x03010001}, /* 79 */
  {0x03010001}, {0x03010001}, {0x03010001}, {0x03010001}, {0x03010001}, /* 84 */
  {0x03010001}, {0x03010001}, {0x03010001}, {0x03010001}, {0x03010001}, /* 89 */
  {0x03010001}, {0x03010001}, {0x03010001}, {0x03010001}, {0x03010001}, /* 94 */
  {0x03010001}, {0x03010001}, {0x03000101}, {0x03000101}, {0x03000101}, /* 99 */
  {0x03000101}, {0x03000101}, {0x03000101}, {0x03000101}, {0x03000101}, /* 104 */
  {0x03000101}, {0x03000101}, {0x03000101}, {0x03000101}, {0x03000101}, /* 109 */
  {0x03000101}, {0x03000101}, {0x03000101}, {0x03000101}, {0x03000101}, /* 114 */
  {0x03000101}, {0x03000101}, {0x03000101}, {0x03000101}, {0x03000101}, /* 119 */
  {0x03000101}, {0x03000101}, {0x03000101}, {0x03000101}, {0x03000101}, /* 124 */
  {0x03000101}, {0x03000101}, {0x03000101}, {0x03000101}, {0x01000000}, /* 129 */
  {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, /* 134 */
  {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, /* 139 */
  {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, /* 144 */
  {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, /* 149 */
  {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, /* 154 */
  {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, /* 159 */
  {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, /* 164 */
  {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, /* 169 */
  {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, /* 174 */
  {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, /* 179 */
  {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, /* 184 */
  {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, /* 189 */
  {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, /* 194 */
  {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, /* 199 */
  {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, /* 204 */
  {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, /* 209 */
  {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, /* 214 */
  {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, /* 219 */
  {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, /* 224 */
  {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, /* 229 */
  {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, /* 234 */
  {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, /* 239 */
  {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, /* 244 */
  {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, /* 249 */
  {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, /* 254 */
  {0x01000000}, {0x01000000},
};

/* max table bits  8 */

/* TABLE  6   16 entries  maxbits  7  linbits  0 */
static const HUFF_ELEMENT huff_table_6[] =
{
  {0xFF000007}, {0x07030302}, {0x07030001}, {0x06030202}, {0x06030202}, /*  4 */
  {0x06020302}, {0x06020302}, {0x06000301}, {0x06000301}, {0x05030102}, /*  9 */
  {0x05030102}, {0x05030102}, {0x05030102}, {0x05010302}, {0x05010302}, /* 14 */
  {0x05010302}, {0x05010302}, {0x05020202}, {0x05020202}, {0x05020202}, /* 19 */
  {0x05020202}, {0x05020001}, {0x05020001}, {0x05020001}, {0x05020001}, /* 24 */
  {0x04020102}, {0x04020102}, {0x04020102}, {0x04020102}, {0x04020102}, /* 29 */
  {0x04020102}, {0x04020102}, {0x04020102}, {0x04010202}, {0x04010202}, /* 34 */
  {0x04010202}, {0x04010202}, {0x04010202}, {0x04010202}, {0x04010202}, /* 39 */
  {0x04010202}, {0x04000201}, {0x04000201}, {0x04000201}, {0x04000201}, /* 44 */
  {0x04000201}, {0x04000201}, {0x04000201}, {0x04000201}, {0x03010001}, /* 49 */
  {0x03010001}, {0x03010001}, {0x03010001}, {0x03010001}, {0x03010001}, /* 54 */
  {0x03010001}, {0x03010001}, {0x03010001}, {0x03010001}, {0x03010001}, /* 59 */
  {0x03010001}, {0x03010001}, {0x03010001}, {0x03010001}, {0x03010001}, /* 64 */
  {0x02010102}, {0x02010102}, {0x02010102}, {0x02010102}, {0x02010102}, /* 69 */
  {0x02010102}, {0x02010102}, {0x02010102}, {0x02010102}, {0x02010102}, /* 74 */
  {0x02010102}, {0x02010102}, {0x02010102}, {0x02010102}, {0x02010102}, /* 79 */
  {0x02010102}, {0x02010102}, {0x02010102}, {0x02010102}, {0x02010102}, /* 84 */
  {0x02010102}, {0x02010102}, {0x02010102}, {0x02010102}, {0x02010102}, /* 89 */
  {0x02010102}, {0x02010102}, {0x02010102}, {0x02010102}, {0x02010102}, /* 94 */
  {0x02010102}, {0x02010102}, {0x03000101}, {0x03000101}, {0x03000101}, /* 99 */
  {0x03000101}, {0x03000101}, {0x03000101}, {0x03000101}, {0x03000101}, /* 104 */
  {0x03000101}, {0x03000101}, {0x03000101}, {0x03000101}, {0x03000101}, /* 109 */
  {0x03000101}, {0x03000101}, {0x03000101}, {0x03000000}, {0x03000000}, /* 114 */
  {0x03000000}, {0x03000000}, {0x03000000}, {0x03000000}, {0x03000000}, /* 119 */
  {0x03000000}, {0x03000000}, {0x03000000}, {0x03000000}, {0x03000000}, /* 124 */
  {0x03000000}, {0x03000000}, {0x03000000}, {0x03000000},
};

/* max table bits  7 */

/* TABLE  7   36 entries  maxbits 10  linbits  0 */
static const HUFF_ELEMENT huff_table_7[] =
{
  {0xFF000006}, {0x00000041}, {0x00000052}, {0x0000005B}, {0x00000060}, /*  4 */
  {0x00000063}, {0x00000068}, {0x0000006B}, {0x06020102}, {0x05010202}, /*  9 */
  {0x05010202}, {0x06020001}, {0x06000201}, {0x04010102}, {0x04010102}, /* 14 */
  {0x04010102}, {0x04010102}, {0x03010001}, {0x03010001}, {0x03010001}, /* 19 */
  {0x03010001}, {0x03010001}, {0x03010001}, {0x03010001}, {0x03010001}, /* 24 */
  {0x03000101}, {0x03000101}, {0x03000101}, {0x03000101}, {0x03000101}, /* 29 */
  {0x03000101}, {0x03000101}, {0x03000101}, {0x01000000}, {0x01000000}, /* 34 */
  {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, /* 39 */
  {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, /* 44 */
  {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, /* 49 */
  {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, /* 54 */
  {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, /* 59 */
  {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, /* 64 */
  {0xFF000004}, {0x04050502}, {0x04050402}, {0x04040502}, {0x04030502}, /* 69 */
  {0x03050302}, {0x03050302}, {0x03040402}, {0x03040402}, {0x03050202}, /* 74 */
  {0x03050202}, {0x03020502}, {0x03020502}, {0x02050102}, {0x02050102}, /* 79 */
  {0x02050102}, {0x02050102}, {0xFF000003}, {0x02010502}, {0x02010502}, /* 84 */
  {0x03050001}, {0x03040302}, {0x02000501}, {0x02000501}, {0x03030402}, /* 89 */
  {0x03030302}, {0xFF000002}, {0x02040202}, {0x02020402}, {0x01040102}, /* 94 */
  {0x01040102}, {0xFF000001}, {0x01010402}, {0x01000401}, {0xFF000002}, /* 99 */
  {0x02040001}, {0x02030202}, {0x02020302}, {0x02030001}, {0xFF000001}, /* 104 */
  {0x01030102}, {0x01010302}, {0xFF000001}, {0x01000301}, {0x01020202}, /* 109 */
};

/* max table bits  6 */

/* TABLE  8   36 entries  maxbits 11  linbits  0 */
static const HUFF_ELEMENT huff_table_8[] =
{
  {0xFF000008}, {0x00000101}, {0x0000010A}, {0x0000010F}, {0x08050102}, /*  4 */
  {0x08010502}, {0x00000112}, {0x00000115}, {0x08040202}, {0x08020402}, /*  9 */
  {0x08040102}, {0x07010402}, {0x07010402}, {0x08040001}, {0x08000401}, /* 14 */
  {0x08030202}, {0x08020302}, {0x08030102}, {0x08010302}, {0x08030001}, /* 19 */
  {0x08000301}, {0x06020202}, {0x06020202}, {0x06020202}, {0x06020202}, /* 24 */
  {0x06020001}, {0x06020001}, {0x06020001}, {0x06020001}, {0x06000201}, /* 29 */
  {0x06000201}, {0x06000201}, {0x06000201}, {0x04020102}, {0x04020102}, /* 34 */
  {0x04020102}, {0x04020102}, {0x04020102}, {0x04020102}, {0x04020102}, /* 39 */
  {0x04020102}, {0x04020102}, {0x04020102}, {0x04020102}, {0x04020102}, /* 44 */
  {0x04020102}, {0x04020102}, {0x04020102}, {0x04020102}, {0x04010202}, /* 49 */
  {0x04010202}, {0x04010202}, {0x04010202}, {0x04010202}, {0x04010202}, /* 54 */
  {0x04010202}, {0x04010202}, {0x04010202}, {0x04010202}, {0x04010202}, /* 59 */
  {0x04010202}, {0x04010202}, {0x04010202}, {0x04010202}, {0x04010202}, /* 64 */
  {0x02010102}, {0x02010102}, {0x02010102}, {0x02010102}, {0x02010102}, /* 69 */
  {0x02010102}, {0x02010102}, {0x02010102}, {0x02010102}, {0x02010102}, /* 74 */
  {0x02010102}, {0x02010102}, {0x02010102}, {0x02010102}, {0x02010102}, /* 79 */
  {0x02010102}, {0x02010102}, {0x02010102}, {0x02010102}, {0x02010102}, /* 84 */
  {0x02010102}, {0x02010102}, {0x02010102}, {0x02010102}, {0x02010102}, /* 89 */
  {0x02010102}, {0x02010102}, {0x02010102}, {0x02010102}, {0x02010102}, /* 94 */
  {0x02010102}, {0x02010102}, {0x02010102}, {0x02010102}, {0x02010102}, /* 99 */
  {0x02010102}, {0x02010102}, {0x02010102}, {0x02010102}, {0x02010102}, /* 104 */
  {0x02010102}, {0x02010102}, {0x02010102}, {0x02010102}, {0x02010102}, /* 109 */
  {0x02010102}, {0x02010102}, {0x02010102}, {0x02010102}, {0x02010102}, /* 114 */
  {0x02010102}, {0x02010102}, {0x02010102}, {0x02010102}, {0x02010102}, /* 119 */
  {0x02010102}, {0x02010102}, {0x02010102}, {0x02010102}, {0x02010102}, /* 124 */
  {0x02010102}, {0x02010102}, {0x02010102}, {0x02010102}, {0x03010001}, /* 129 */
  {0x03010001}, {0x03010001}, {0x03010001}, {0x03010001}, {0x03010001}, /* 134 */
  {0x03010001}, {0x03010001}, {0x03010001}, {0x03010001}, {0x03010001}, /* 139 */
  {0x03010001}, {0x03010001}, {0x03010001}, {0x03010001}, {0x03010001}, /* 144 */
  {0x03010001}, {0x03010001}, {0x03010001}, {0x03010001}, {0x03010001}, /* 149 */
  {0x03010001}, {0x03010001}, {0x03010001}, {0x03010001}, {0x03010001}, /* 154 */
  {0x03010001}, {0x03010001}, {0x03010001}, {0x03010001}, {0x03010001}, /* 159 */
  {0x03010001}, {0x03000101}, {0x03000101}, {0x03000101}, {0x03000101}, /* 164 */
  {0x03000101}, {0x03000101}, {0x03000101}, {0x03000101}, {0x03000101}, /* 169 */
  {0x03000101}, {0x03000101}, {0x03000101}, {0x03000101}, {0x03000101}, /* 174 */
  {0x03000101}, {0x03000101}, {0x03000101}, {0x03000101}, {0x03000101}, /* 179 */
  {0x03000101}, {0x03000101}, {0x03000101}, {0x03000101}, {0x03000101}, /* 184 */
  {0x03000101}, {0x03000101}, {0x03000101}, {0x03000101}, {0x03000101}, /* 189 */
  {0x03000101}, {0x03000101}, {0x03000101}, {0x02000000}, {0x02000000}, /* 194 */
  {0x02000000}, {0x02000000}, {0x02000000}, {0x02000000}, {0x02000000}, /* 199 */
  {0x02000000}, {0x02000000}, {0x02000000}, {0x02000000}, {0x02000000}, /* 204 */
  {0x02000000}, {0x02000000}, {0x02000000}, {0x02000000}, {0x02000000}, /* 209 */
  {0x02000000}, {0x02000000}, {0x02000000}, {0x02000000}, {0x02000000}, /* 214 */
  {0x02000000}, {0x02000000}, {0x02000000}, {0x02000000}, {0x02000000}, /* 219 */
  {0x02000000}, {0x02000000}, {0x02000000}, {0x02000000}, {0x02000000}, /* 224 */
  {0x02000000}, {0x02000000}, {0x02000000}, {0x02000000}, {0x02000000}, /* 229 */
  {0x02000000}, {0x02000000}, {0x02000000}, {0x02000000}, {0x02000000}, /* 234 */
  {0x02000000}, {0x02000000}, {0x02000000}, {0x02000000}, {0x02000000}, /* 239 */
  {0x02000000}, {0x02000000}, {0x02000000}, {0x02000000}, {0x02000000}, /* 244 */
  {0x02000000}, {0x02000000}, {0x02000000}, {0x02000000}, {0x02000000}, /* 249 */
  {0x02000000}, {0x02000000}, {0x02000000}, {0x02000000}, {0x02000000}, /* 254 */
  {0x02000000}, {0x02000000}, {0xFF000003}, {0x03050502}, {0x03040502}, /* 259 */
  {0x02050402}, {0x02050402}, {0x01030502}, {0x01030502}, {0x01030502}, /* 264 */
  {0x01030502}, {0xFF000002}, {0x02050302}, {0x02040402}, {0x01050202}, /* 269 */
  {0x01050202}, {0xFF000001}, {0x01020502}, {0x01050001}, {0xFF000001}, /* 274 */
  {0x01040302}, {0x01030402}, {0xFF000001}, {0x01000501}, {0x01030302}, /* 279 */
};

/* max table bits  8 */

/* TABLE  9   36 entries  maxbits  9  linbits  0 */
static const HUFF_ELEMENT huff_table_9[] =
{
  {0xFF000006}, {0x00000041}, {0x0000004A}, {0x0000004F}, {0x00000052}, /*  4 */
  {0x00000057}, {0x0000005A}, {0x06040102}, {0x06010402}, {0x06030202}, /*  9 */
  {0x06020302}, {0x05030102}, {0x05030102}, {0x05010302}, {0x05010302}, /* 14 */
  {0x06030001}, {0x06000301}, {0x05020202}, {0x05020202}, {0x05020001}, /* 19 */
  {0x05020001}, {0x04020102}, {0x04020102}, {0x04020102}, {0x04020102}, /* 24 */
  {0x04010202}, {0x04010202}, {0x04010202}, {0x04010202}, {0x04000201}, /* 29 */
  {0x04000201}, {0x04000201}, {0x04000201}, {0x03010102}, {0x03010102}, /* 34 */
  {0x03010102}, {0x03010102}, {0x03010102}, {0x03010102}, {0x03010102}, /* 39 */
  {0x03010102}, {0x03010001}, {0x03010001}, {0x03010001}, {0x03010001}, /* 44 */
  {0x03010001}, {0x03010001}, {0x03010001}, {0x03010001}, {0x03000101}, /* 49 */
  {0x03000101}, {0x03000101}, {0x03000101}, {0x03000101}, {0x03000101}, /* 54 */
  {0x03000101}, {0x03000101}, {0x03000000}, {0x03000000}, {0x03000000}, /* 59 */
  {0x03000000}, {0x03000000}, {0x03000000}, {0x03000000}, {0x03000000}, /* 64 */
  {0xFF000003}, {0x03050502}, {0x03050402}, {0x02050302}, {0x02050302}, /* 69 */
  {0x02030502}, {0x02030502}, {0x03040502}, {0x03050001}, {0xFF000002}, /* 74 */
  {0x02040402}, {0x02050202}, {0x02020502}, {0x02050102}, {0xFF000001}, /* 79 */
  {0x01010502}, {0x01040302}, {0xFF000002}, {0x01030402}, {0x01030402}, /* 84 */
  {0x02000501}, {0x02040001}, {0xFF000001}, {0x01040202}, {0x01020402}, /* 89 */
  {0xFF000001}, {0x01030302}, {0x01000401},
};

/* max table bits  6 */

/* TABLE 10   64 entries  maxbits 11  linbits  0 */
static const HUFF_ELEMENT huff_table_10[] =
{
  {0xFF000008}, {0x00000101}, {0x0000010A}, {0x0000010F}, {0x00000118}, /*  4 */
  {0x0000011B}, {0x00000120}, {0x00000125}, {0x08070102}, {0x08010702}, /*  9 */
  {0x0000012A}, {0x0000012D}, {0x00000132}, {0x08060102}, {0x08010602}, /* 14 */
  {0x08000601}, {0x00000137}, {0x0000013A}, {0x0000013D}, {0x08040102}, /* 19 */
  {0x08010402}, {0x08000401}, {0x08030202}, {0x08020302}, {0x08030001}, /* 24 */
  {0x07030102}, {0x07030102}, {0x07010302}, {0x07010302}, {0x07000301}, /* 29 */
  {0x07000301}, {0x07020202}, {0x07020202}, {0x06020102}, {0x06020102}, /* 34 */
  {0x06020102}, {0x06020102}, {0x06010202}, {0x06010202}, {0x06010202}, /* 39 */
  {0x06010202}, {0x06020001}, {0x06020001}, {0x06020001}, {0x06020001}, /* 44 */
  {0x06000201}, {0x06000201}, {0x06000201}, {0x06000201}, {0x04010102}, /* 49 */
  {0x04010102}, {0x04010102}, {0x04010102}, {0x04010102}, {0x04010102}, /* 54 */
  {0x04010102}, {0x04010102}, {0x04010102}, {0x04010102}, {0x04010102}, /* 59 */
  {0x04010102}, {0x04010102}, {0x04010102}, {0x04010102}, {0x04010102}, /* 64 */
  {0x03010001}, {0x03010001}, {0x03010001}, {0x03010001}, {0x03010001}, /* 69 */
  {0x03010001}, {0x03010001}, {0x03010001}, {0x03010001}, {0x03010001}, /* 74 */
  {0x03010001}, {0x03010001}, {0x03010001}, {0x03010001}, {0x03010001}, /* 79 */
  {0x03010001}, {0x03010001}, {0x03010001}, {0x03010001}, {0x03010001}, /* 84 */
  {0x03010001}, {0x03010001}, {0x03010001}, {0x03010001}, {0x03010001}, /* 89 */
  {0x03010001}, {0x03010001}, {0x03010001}, {0x03010001}, {0x03010001}, /* 94 */
  {0x03010001}, {0x03010001}, {0x03000101}, {0x03000101}, {0x03000101}, /* 99 */
  {0x03000101}, {0x03000101}, {0x03000101}, {0x03000101}, {0x03000101}, /* 104 */
  {0x03000101}, {0x03000101}, {0x03000101}, {0x03000101}, {0x03000101}, /* 109 */
  {0x03000101}, {0x03000101}, {0x03000101}, {0x03000101}, {0x03000101}, /* 114 */
  {0x03000101}, {0x03000101}, {0x03000101}, {0x03000101}, {0x03000101}, /* 119 */
  {0x03000101}, {0x03000101}, {0x03000101}, {0x03000101}, {0x03000101}, /* 124 */
  {0x03000101}, {0x03000101}, {0x03000101}, {0x03000101}, {0x01000000}, /* 129 */
  {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, /* 134 */
  {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, /* 139 */
  {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, /* 144 */
  {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, /* 149 */
  {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, /* 154 */
  {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, /* 159 */
  {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, /* 164 */
  {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, /* 169 */
  {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, /* 174 */
  {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, /* 179 */
  {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, /* 184 */
  {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, /* 189 */
  {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, /* 194 */
  {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, /* 199 */
  {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, /* 204 */
  {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, /* 209 */
  {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, /* 214 */
  {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, /* 219 */
  {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, /* 224 */
  {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, /* 229 */
  {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, /* 234 */
  {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, /* 239 */
  {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, /* 244 */
  {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, /* 249 */
  {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, /* 254 */
  {0x01000000}, {0x01000000}, {0xFF000003}, {0x03070702}, {0x03070602}, /* 259 */
  {0x03060702}, {0x03070502}, {0x03050702}, {0x03060602}, {0x02070402}, /* 264 */
  {0x02070402}, {0xFF000002}, {0x02040702}, {0x02060502}, {0x02050602}, /* 269 */
  {0x02070302}, {0xFF000003}, {0x02030702}, {0x02030702}, {0x02060402}, /* 274 */
  {0x02060402}, {0x03050502}, {0x03040502}, {0x02030602}, {0x02030602}, /* 279 */
  {0xFF000001}, {0x01070202}, {0x01020702}, {0xFF000002}, {0x02040602}, /* 284 */
  {0x02070001}, {0x01000701}, {0x01000701}, {0xFF000002}, {0x01020602}, /* 289 */
  {0x01020602}, {0x02050402}, {0x02050302}, {0xFF000002}, {0x01060001}, /* 294 */
  {0x01060001}, {0x02030502}, {0x02040402}, {0xFF000001}, {0x01060302}, /* 299 */
  {0x01060202}, {0xFF000002}, {0x02050202}, {0x02020502}, {0x01050102}, /* 304 */
  {0x01050102}, {0xFF000002}, {0x01010502}, {0x01010502}, {0x02040302}, /* 309 */
  {0x02030402}, {0xFF000001}, {0x01050001}, {0x01000501}, {0xFF000001}, /* 314 */
  {0x01040202}, {0x01020402}, {0xFF000001}, {0x01030302}, {0x01040001}, /* 319 */
};

/* max table bits  8 */

/* TABLE 11   64 entries  maxbits 11  linbits  0 */
static const HUFF_ELEMENT huff_table_11[] =
{
  {0xFF000008}, {0x00000101}, {0x00000106}, {0x0000010F}, {0x00000114}, /*  4 */
  {0x00000117}, {0x08070202}, {0x08020702}, {0x0000011C}, {0x07010702}, /*  9 */
  {0x07010702}, {0x08070102}, {0x08000701}, {0x08060302}, {0x08030602}, /* 14 */
  {0x08000601}, {0x0000011F}, {0x00000122}, {0x08050102}, {0x07020602}, /* 19 */
  {0x07020602}, {0x08060202}, {0x08060001}, {0x07060102}, {0x07060102}, /* 24 */
  {0x07010602}, {0x07010602}, {0x08010502}, {0x08040302}, {0x08000501}, /* 29 */
  {0x00000125}, {0x08040202}, {0x08020402}, {0x08040102}, {0x08010402}, /* 34 */
  {0x08040001}, {0x08000401}, {0x07030202}, {0x07030202}, {0x07020302}, /* 39 */
  {0x07020302}, {0x06030102}, {0x06030102}, {0x06030102}, {0x06030102}, /* 44 */
  {0x06010302}, {0x06010302}, {0x06010302}, {0x06010302}, {0x07030001}, /* 49 */
  {0x07030001}, {0x07000301}, {0x07000301}, {0x06020202}, {0x06020202}, /* 54 */
  {0x06020202}, {0x06020202}, {0x05010202}, {0x05010202}, {0x05010202}, /* 59 */
  {0x05010202}, {0x05010202}, {0x05010202}, {0x05010202}, {0x05010202}, /* 64 */
  {0x04020102}, {0x04020102}, {0x04020102}, {0x04020102}, {0x04020102}, /* 69 */
  {0x04020102}, {0x04020102}, {0x04020102}, {0x04020102}, {0x04020102}, /* 74 */
  {0x04020102}, {0x04020102}, {0x04020102}, {0x04020102}, {0x04020102}, /* 79 */
  {0x04020102}, {0x05020001}, {0x05020001}, {0x05020001}, {0x05020001}, /* 84 */
  {0x05020001}, {0x05020001}, {0x05020001}, {0x05020001}, {0x05000201}, /* 89 */
  {0x05000201}, {0x05000201}, {0x05000201}, {0x05000201}, {0x05000201}, /* 94 */
  {0x05000201}, {0x05000201}, {0x03010102}, {0x03010102}, {0x03010102}, /* 99 */
  {0x03010102}, {0x03010102}, {0x03010102}, {0x03010102}, {0x03010102}, /* 104 */
  {0x03010102}, {0x03010102}, {0x03010102}, {0x03010102}, {0x03010102}, /* 109 */
  {0x03010102}, {0x03010102}, {0x03010102}, {0x03010102}, {0x03010102}, /* 114 */
  {0x03010102}, {0x03010102}, {0x03010102}, {0x03010102}, {0x03010102}, /* 119 */
  {0x03010102}, {0x03010102}, {0x03010102}, {0x03010102}, {0x03010102}, /* 124 */
  {0x03010102}, {0x03010102}, {0x03010102}, {0x03010102}, {0x03010001}, /* 129 */
  {0x03010001}, {0x03010001}, {0x03010001}, {0x03010001}, {0x03010001}, /* 134 */
  {0x03010001}, {0x03010001}, {0x03010001}, {0x03010001}, {0x03010001}, /* 139 */
  {0x03010001}, {0x03010001}, {0x03010001}, {0x03010001}, {0x03010001}, /* 144 */
  {0x03010001}, {0x03010001}, {0x03010001}, {0x03010001}, {0x03010001}, /* 149 */
  {0x03010001}, {0x03010001}, {0x03010001}, {0x03010001}, {0x03010001}, /* 154 */
  {0x03010001}, {0x03010001}, {0x03010001}, {0x03010001}, {0x03010001}, /* 159 */
  {0x03010001}, {0x03000101}, {0x03000101}, {0x03000101}, {0x03000101}, /* 164 */
  {0x03000101}, {0x03000101}, {0x03000101}, {0x03000101}, {0x03000101}, /* 169 */
  {0x03000101}, {0x03000101}, {0x03000101}, {0x03000101}, {0x03000101}, /* 174 */
  {0x03000101}, {0x03000101}, {0x03000101}, {0x03000101}, {0x03000101}, /* 179 */
  {0x03000101}, {0x03000101}, {0x03000101}, {0x03000101}, {0x03000101}, /* 184 */
  {0x03000101}, {0x03000101}, {0x03000101}, {0x03000101}, {0x03000101}, /* 189 */
  {0x03000101}, {0x03000101}, {0x03000101}, {0x02000000}, {0x02000000}, /* 194 */
  {0x02000000}, {0x02000000}, {0x02000000}, {0x02000000}, {0x02000000}, /* 199 */
  {0x02000000}, {0x02000000}, {0x02000000}, {0x02000000}, {0x02000000}, /* 204 */
  {0x02000000}, {0x02000000}, {0x02000000}, {0x02000000}, {0x02000000}, /* 209 */
  {0x02000000}, {0x02000000}, {0x02000000}, {0x02000000}, {0x02000000}, /* 214 */
  {0x02000000}, {0x02000000}, {0x02000000}, {0x02000000}, {0x02000000}, /* 219 */
  {0x02000000}, {0x02000000}, {0x02000000}, {0x02000000}, {0x02000000}, /* 224 */
  {0x02000000}, {0x02000000}, {0x02000000}, {0x02000000}, {0x02000000}, /* 229 */
  {0x02000000}, {0x02000000}, {0x02000000}, {0x02000000}, {0x02000000}, /* 234 */
  {0x02000000}, {0x02000000}, {0x02000000}, {0x02000000}, {0x02000000}, /* 239 */
  {0x02000000}, {0x02000000}, {0x02000000}, {0x02000000}, {0x02000000}, /* 244 */
  {0x02000000}, {0x02000000}, {0x02000000}, {0x02000000}, {0x02000000}, /* 249 */
  {0x02000000}, {0x02000000}, {0x02000000}, {0x02000000}, {0x02000000}, /* 254 */
  {0x02000000}, {0x02000000}, {0xFF000002}, {0x02070702}, {0x02070602}, /* 259 */
  {0x02060702}, {0x02050702}, {0xFF000003}, {0x02060602}, {0x02060602}, /* 264 */
  {0x02070402}, {0x02070402}, {0x02040702}, {0x02040702}, {0x03070502}, /* 269 */
  {0x03050502}, {0xFF000002}, {0x02060502}, {0x02050602}, {0x01070302}, /* 274 */
  {0x01070302}, {0xFF000001}, {0x01030702}, {0x01060402}, {0xFF000002}, /* 279 */
  {0x02050402}, {0x02040502}, {0x02050302}, {0x02030502}, {0xFF000001}, /* 284 */
  {0x01040602}, {0x01070001}, {0xFF000001}, {0x01040402}, {0x01050202}, /* 289 */
  {0xFF000001}, {0x01020502}, {0x01050001}, {0xFF000001}, {0x01030402}, /* 294 */
  {0x01030302},
};

/* max table bits  8 */

/* TABLE 12   64 entries  maxbits 10  linbits  0 */
static const HUFF_ELEMENT huff_table_12[] =
{
  {0xFF000007}, {0x00000081}, {0x0000008A}, {0x0000008F}, {0x00000092}, /*  4 */
  {0x00000097}, {0x0000009A}, {0x0000009D}, {0x000000A2}, {0x000000A5}, /*  9 */
  {0x000000A8}, {0x07060202}, {0x07020602}, {0x07010602}, {0x000000AD}, /* 14 */
  {0x000000B0}, {0x000000B3}, {0x07050102}, {0x07010502}, {0x07040302}, /* 19 */
  {0x07030402}, {0x000000B6}, {0x07040202}, {0x07020402}, {0x07040102}, /* 24 */
  {0x06030302}, {0x06030302}, {0x06010402}, {0x06010402}, {0x06030202}, /* 29 */
  {0x06030202}, {0x06020302}, {0x06020302}, {0x07000401}, {0x07030001}, /* 34 */
  {0x06000301}, {0x06000301}, {0x05030102}, {0x05030102}, {0x05030102}, /* 39 */
  {0x05030102}, {0x05010302}, {0x05010302}, {0x05010302}, {0x05010302}, /* 44 */
  {0x05020202}, {0x05020202}, {0x05020202}, {0x05020202}, {0x04020102}, /* 49 */
  {0x04020102}, {0x04020102}, {0x04020102}, {0x04020102}, {0x04020102}, /* 54 */
  {0x04020102}, {0x04020102}, {0x04010202}, {0x04010202}, {0x04010202}, /* 59 */
  {0x04010202}, {0x04010202}, {0x04010202}, {0x04010202}, {0x04010202}, /* 64 */
  {0x05020001}, {0x05020001}, {0x05020001}, {0x05020001}, {0x05000201}, /* 69 */
  {0x05000201}, {0x05000201}, {0x05000201}, {0x04000000}, {0x04000000}, /* 74 */
  {0x04000000}, {0x04000000}, {0x04000000}, {0x04000000}, {0x04000000}, /* 79 */
  {0x04000000}, {0x03010102}, {0x03010102}, {0x03010102}, {0x03010102}, /* 84 */
  {0x03010102}, {0x03010102}, {0x03010102}, {0x03010102}, {0x03010102}, /* 89 */
  {0x03010102}, {0x03010102}, {0x03010102}, {0x03010102}, {0x03010102}, /* 94 */
  {0x03010102}, {0x03010102}, {0x03010001}, {0x03010001}, {0x03010001}, /* 99 */
  {0x03010001}, {0x03010001}, {0x03010001}, {0x03010001}, {0x03010001}, /* 104 */
  {0x03010001}, {0x03010001}, {0x03010001}, {0x03010001}, {0x03010001}, /* 109 */
  {0x03010001}, {0x03010001}, {0x03010001}, {0x03000101}, {0x03000101}, /* 114 */
  {0x03000101}, {0x03000101}, {0x03000101}, {0x03000101}, {0x03000101}, /* 119 */
  {0x03000101}, {0x03000101}, {0x03000101}, {0x03000101}, {0x03000101}, /* 124 */
  {0x03000101}, {0x03000101}, {0x03000101}, {0x03000101}, {0xFF000003}, /* 129 */
  {0x03070702}, {0x03070602}, {0x02060702}, {0x02060702}, {0x02070502}, /* 134 */
  {0x02070502}, {0x02050702}, {0x02050702}, {0xFF000002}, {0x02060602}, /* 139 */
  {0x02070402}, {0x02040702}, {0x02050602}, {0xFF000001}, {0x01060502}, /* 144 */
  {0x01070302}, {0xFF000002}, {0x02030702}, {0x02050502}, {0x01070202}, /* 149 */
  {0x01070202}, {0xFF000001}, {0x01020702}, {0x01060402}, {0xFF000001}, /* 154 */
  {0x01040602}, {0x01070102}, {0xFF000002}, {0x01010702}, {0x01010702}, /* 159 */
  {0x02070001}, {0x02000701}, {0xFF000001}, {0x01060302}, {0x01030602}, /* 164 */
  {0xFF000001}, {0x01050402}, {0x01040502}, {0xFF000002}, {0x01040402}, /* 169 */
  {0x01040402}, {0x02060001}, {0x02050001}, {0xFF000001}, {0x01060102}, /* 174 */
  {0x01000601}, {0xFF000001}, {0x01050302}, {0x01030502}, {0xFF000001}, /* 179 */
  {0x01050202}, {0x01020502}, {0xFF000001}, {0x01000501}, {0x01040001}, /* 184 */
};

/* max table bits  7 */

/* TABLE 13  256 entries  maxbits 19  linbits  0 */
static const HUFF_ELEMENT huff_table_13[] =
{
  {0xFF000006}, {0x00000041}, {0x00000082}, {0x000000C3}, {0x000000E4}, /*  4 */
  {0x00000105}, {0x00000116}, {0x0000011F}, {0x00000130}, {0x00000139}, /*  9 */
  {0x0000013E}, {0x00000143}, {0x00000146}, {0x06020102}, {0x06010202}, /* 14 */
  {0x06020001}, {0x06000201}, {0x04010102}, {0x04010102}, {0x04010102}, /* 19 */
  {0x04010102}, {0x04010001}, {0x04010001}, {0x04010001}, {0x04010001}, /* 24 */
  {0x03000101}, {0x03000101}, {0x03000101}, {0x03000101}, {0x03000101}, /* 29 */
  {0x03000101}, {0x03000101}, {0x03000101}, {0x01000000}, {0x01000000}, /* 34 */
  {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, /* 39 */
  {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, /* 44 */
  {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, /* 49 */
  {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, /* 54 */
  {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, /* 59 */
  {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, /* 64 */
  {0xFF000006}, {0x00000108}, {0x00000111}, {0x0000011A}, {0x00000123}, /* 69 */
  {0x0000012C}, {0x00000131}, {0x00000136}, {0x0000013F}, {0x00000144}, /* 74 */
  {0x00000147}, {0x0000014C}, {0x00000151}, {0x00000156}, {0x0000015B}, /* 79 */
  {0x060F0102}, {0x06010F02}, {0x06000F01}, {0x00000160}, {0x00000163}, /* 84 */
  {0x00000166}, {0x06020E02}, {0x00000169}, {0x060E0102}, {0x06010E02}, /* 89 */
  {0x0000016C}, {0x0000016F}, {0x00000172}, {0x00000175}, {0x00000178}, /* 94 */
  {0x0000017B}, {0x06060C02}, {0x060D0302}, {0x0000017E}, {0x060D0202}, /* 99 */
  {0x06020D02}, {0x060D0102}, {0x06070B02}, {0x00000181}, {0x00000184}, /* 104 */
  {0x06030C02}, {0x00000187}, {0x060B0402}, {0x05010D02}, {0x05010D02}, /* 109 */
  {0x060D0001}, {0x06000D01}, {0x060A0802}, {0x06080A02}, {0x060C0402}, /* 114 */
  {0x06040C02}, {0x060B0602}, {0x06060B02}, {0x050C0302}, {0x050C0302}, /* 119 */
  {0x050C0202}, {0x050C0202}, {0x05020C02}, {0x05020C02}, {0x050B0502}, /* 124 */
  {0x050B0502}, {0x06050B02}, {0x06090802}, {0x050C0102}, {0x050C0102}, /* 129 */
  {0xFF000006}, {0x05010C02}, {0x05010C02}, {0x06080902}, {0x060C0001}, /* 134 */
  {0x05000C01}, {0x05000C01}, {0x06040B02}, {0x060A0602}, {0x06060A02}, /* 139 */
  {0x06090702}, {0x050B0302}, {0x050B0302}, {0x05030B02}, {0x05030B02}, /* 144 */
  {0x06080802}, {0x060A0502}, {0x050B0202}, {0x050B0202}, {0x06050A02}, /* 149 */
  {0x06090602}, {0x05040A02}, {0x05040A02}, {0x06080702}, {0x06070802}, /* 154 */
  {0x05040902}, {0x05040902}, {0x06070702}, {0x06060702}, {0x04020B02}, /* 159 */
  {0x04020B02}, {0x04020B02}, {0x04020B02}, {0x040B0102}, {0x040B0102}, /* 164 */
  {0x040B0102}, {0x040B0102}, {0x04010B02}, {0x04010B02}, {0x04010B02}, /* 169 */
  {0x04010B02}, {0x050B0001}, {0x050B0001}, {0x05000B01}, {0x05000B01}, /* 174 */
  {0x05060902}, {0x05060902}, {0x050A0402}, {0x050A0402}, {0x050A0302}, /* 179 */
  {0x050A0302}, {0x05030A02}, {0x05030A02}, {0x05090502}, {0x05090502}, /* 184 */
  {0x05050902}, {0x05050902}, {0x040A0202}, {0x040A0202}, {0x040A0202}, /* 189 */
  {0x040A0202}, {0x04020A02}, {0x04020A02}, {0x04020A02}, {0x04020A02}, /* 194 */
  {0xFF000005}, {0x040A0102}, {0x040A0102}, {0x04010A02}, {0x04010A02}, /* 199 */
  {0x050A0001}, {0x05080602}, {0x04000A01}, {0x04000A01}, {0x05060802}, /* 204 */
  {0x05090402}, {0x04030902}, {0x04030902}, {0x05090302}, {0x05080502}, /* 209 */
  {0x05050802}, {0x05070602}, {0x04090202}, {0x04090202}, {0x04020902}, /* 214 */
  {0x04020902}, {0x05070502}, {0x05050702}, {0x04080302}, {0x04080302}, /* 219 */
  {0x04030802}, {0x04030802}, {0x05060602}, {0x05070402}, {0x05040702}, /* 224 */
  {0x05060502}, {0x05050602}, {0x05030702}, {0xFF000005}, {0x03090102}, /* 229 */
  {0x03090102}, {0x03090102}, {0x03090102}, {0x03010902}, {0x03010902}, /* 234 */
  {0x03010902}, {0x03010902}, {0x04090001}, {0x04090001}, {0x04000901}, /* 239 */
  {0x04000901}, {0x04080402}, {0x04080402}, {0x04040802}, {0x04040802}, /* 244 */
  {0x04020702}, {0x04020702}, {0x05060402}, {0x05040602}, {0x03080202}, /* 249 */
  {0x03080202}, {0x03080202}, {0x03080202}, {0x03020802}, {0x03020802}, /* 254 */
  {0x03020802}, {0x03020802}, {0x03080102}, {0x03080102}, {0x03080102}, /* 259 */
  {0x03080102}, {0xFF000004}, {0x04070302}, {0x04070202}, {0x03070102}, /* 264 */
  {0x03070102}, {0x03010702}, {0x03010702}, {0x04050502}, {0x04070001}, /* 269 */
  {0x04000701}, {0x04060302}, {0x04030602}, {0x04050402}, {0x04040502}, /* 274 */
  {0x04060202}, {0x04020602}, {0x04050302}, {0xFF000003}, {0x02010802}, /* 279 */
  {0x02010802}, {0x03080001}, {0x03000801}, {0x03060102}, {0x03010602}, /* 284 */
  {0x03060001}, {0x03000601}, {0xFF000004}, {0x04030502}, {0x04040402}, /* 289 */
  {0x03050202}, {0x03050202}, {0x03020502}, {0x03020502}, {0x03050001}, /* 294 */
  {0x03050001}, {0x02050102}, {0x02050102}, {0x02050102}, {0x02050102}, /* 299 */
  {0x02010502}, {0x02010502}, {0x02010502}, {0x02010502}, {0xFF000003}, /* 304 */
  {0x03040302}, {0x03030402}, {0x03000501}, {0x03040202}, {0x03020402}, /* 309 */
  {0x03030302}, {0x02040102}, {0x02040102}, {0xFF000002}, {0x01010402}, /* 314 */
  {0x01010402}, {0x02040001}, {0x02000401}, {0xFF000002}, {0x02030202}, /* 319 */
  {0x02020302}, {0x01030102}, {0x01030102}, {0xFF000001}, {0x01010302}, /* 324 */
  {0x01030001}, {0xFF000001}, {0x01000301}, {0x01020202}, {0xFF000003}, /* 329 */
  {0x00000082}, {0x0000008B}, {0x0000008E}, {0x00000091}, {0x00000094}, /* 334 */
  {0x00000097}, {0x030C0E02}, {0x030D0D02}, {0xFF000003}, {0x00000093}, /* 339 */
  {0x030E0B02}, {0x030B0E02}, {0x030F0902}, {0x03090F02}, {0x030A0E02}, /* 344 */
  {0x030D0B02}, {0x030B0D02}, {0xFF000003}, {0x030F0802}, {0x03080F02}, /* 349 */
  {0x030C0C02}, {0x0000008D}, {0x030E0802}, {0x00000090}, {0x02070F02}, /* 354 */
  {0x02070F02}, {0xFF000003}, {0x020A0D02}, {0x020A0D02}, {0x030D0A02}, /* 359 */
  {0x030C0B02}, {0x030B0C02}, {0x03060F02}, {0x020F0602}, {0x020F0602}, /* 364 */
  {0xFF000002}, {0x02080E02}, {0x020F0502}, {0x020D0902}, {0x02090D02}, /* 369 */
  {0xFF000002}, {0x02050F02}, {0x02070E02}, {0x020C0A02}, {0x020B0B02}, /* 374 */
  {0xFF000003}, {0x020F0402}, {0x020F0402}, {0x02040F02}, {0x02040F02}, /* 379 */
  {0x030A0C02}, {0x03060E02}, {0x02030F02}, {0x02030F02}, {0xFF000002}, /* 384 */
  {0x010F0302}, {0x010F0302}, {0x020D0802}, {0x02080D02}, {0xFF000001}, /* 389 */
  {0x010F0202}, {0x01020F02}, {0xFF000002}, {0x020E0602}, {0x020C0902}, /* 394 */
  {0x010F0001}, {0x010F0001}, {0xFF000002}, {0x02090C02}, {0x020E0502}, /* 399 */
  {0x010B0A02}, {0x010B0A02}, {0xFF000002}, {0x020D0702}, {0x02070D02}, /* 404 */
  {0x010E0402}, {0x010E0402}, {0xFF000002}, {0x02080C02}, {0x02060D02}, /* 409 */
  {0x010E0302}, {0x010E0302}, {0xFF000002}, {0x01090B02}, {0x01090B02}, /* 414 */
  {0x020B0902}, {0x020A0A02}, {0xFF000001}, {0x010A0B02}, {0x01050E02}, /* 419 */
  {0xFF000001}, {0x01040E02}, {0x010C0802}, {0xFF000001}, {0x010D0602}, /* 424 */
  {0x01030E02}, {0xFF000001}, {0x010E0202}, {0x010E0001}, {0xFF000001}, /* 429 */
  {0x01000E01}, {0x010D0502}, {0xFF000001}, {0x01050D02}, {0x010C0702}, /* 434 */
  {0xFF000001}, {0x01070C02}, {0x010D0402}, {0xFF000001}, {0x010B0802}, /* 439 */
  {0x01080B02}, {0xFF000001}, {0x01040D02}, {0x010A0902}, {0xFF000001}, /* 444 */
  {0x01090A02}, {0x010C0602}, {0xFF000001}, {0x01030D02}, {0x010B0702}, /* 449 */
  {0xFF000001}, {0x010C0502}, {0x01050C02}, {0xFF000001}, {0x01090902}, /* 454 */
  {0x010A0702}, {0xFF000001}, {0x01070A02}, {0x01070902}, {0xFF000003}, /* 459 */
  {0x00000023}, {0x030D0F02}, {0x020D0E02}, {0x020D0E02}, {0x010F0F02}, /* 464 */
  {0x010F0F02}, {0x010F0F02}, {0x010F0F02}, {0xFF000001}, {0x010F0E02}, /* 469 */
  {0x010F0D02}, {0xFF000001}, {0x010E0E02}, {0x010F0C02}, {0xFF000001}, /* 474 */
  {0x010E0D02}, {0x010F0B02}, {0xFF000001}, {0x010B0F02}, {0x010E0C02}, /* 479 */
  {0xFF000002}, {0x010C0D02}, {0x010C0D02}, {0x020F0A02}, {0x02090E02}, /* 484 */
  {0xFF000001}, {0x010A0F02}, {0x010D0C02}, {0xFF000001}, {0x010E0A02}, /* 489 */
  {0x010E0902}, {0xFF000001}, {0x010F0702}, {0x010E0702}, {0xFF000001}, /* 494 */
  {0x010E0F02}, {0x010C0F02},
};

/* max table bits  6 */
/* NO XING TABLE 14 */

/* TABLE 15  256 entries  maxbits 13  linbits  0 */
static const HUFF_ELEMENT huff_table_15[] =
{
  {0xFF000008}, {0x00000101}, {0x00000122}, {0x00000143}, {0x00000154}, /*  4 */
  {0x00000165}, {0x00000176}, {0x0000017F}, {0x00000188}, {0x00000199}, /*  9 */
  {0x000001A2}, {0x000001AB}, {0x000001B4}, {0x000001BD}, {0x000001C2}, /* 14 */
  {0x000001CB}, {0x000001D4}, {0x000001D9}, {0x000001DE}, {0x000001E3}, /* 19 */
  {0x000001E8}, {0x000001ED}, {0x000001F2}, {0x000001F7}, {0x000001FC}, /* 24 */
  {0x00000201}, {0x00000204}, {0x00000207}, {0x0000020A}, {0x0000020F}, /* 29 */
  {0x00000212}, {0x00000215}, {0x0000021A}, {0x0000021D}, {0x00000220}, /* 34 */
  {0x08010902}, {0x00000223}, {0x00000226}, {0x00000229}, {0x0000022C}, /* 39 */
  {0x0000022F}, {0x08080202}, {0x08020802}, {0x08080102}, {0x08010802}, /* 44 */
  {0x00000232}, {0x00000235}, {0x00000238}, {0x0000023B}, {0x08070202}, /* 49 */
  {0x08020702}, {0x08040602}, {0x08070102}, {0x08050502}, {0x08010702}, /* 54 */
  {0x0000023E}, {0x08060302}, {0x08030602}, {0x08050402}, {0x08040502}, /* 59 */
  {0x08060202}, {0x08020602}, {0x08060102}, {0x00000241}, {0x08050302}, /* 64 */
  {0x07010602}, {0x07010602}, {0x08030502}, {0x08040402}, {0x07050202}, /* 69 */
  {0x07050202}, {0x07020502}, {0x07020502}, {0x07050102}, {0x07050102}, /* 74 */
  {0x07010502}, {0x07010502}, {0x08050001}, {0x08000501}, {0x07040302}, /* 79 */
  {0x07040302}, {0x07030402}, {0x07030402}, {0x07040202}, {0x07040202}, /* 84 */
  {0x07020402}, {0x07020402}, {0x07030302}, {0x07030302}, {0x06010402}, /* 89 */
  {0x06010402}, {0x06010402}, {0x06010402}, {0x07040102}, {0x07040102}, /* 94 */
  {0x07040001}, {0x07040001}, {0x06030202}, {0x06030202}, {0x06030202}, /* 99 */
  {0x06030202}, {0x06020302}, {0x06020302}, {0x06020302}, {0x06020302}, /* 104 */
  {0x07000401}, {0x07000401}, {0x07030001}, {0x07030001}, {0x06030102}, /* 109 */
  {0x06030102}, {0x06030102}, {0x06030102}, {0x06010302}, {0x06010302}, /* 114 */
  {0x06010302}, {0x06010302}, {0x06000301}, {0x06000301}, {0x06000301}, /* 119 */
  {0x06000301}, {0x05020202}, {0x05020202}, {0x05020202}, {0x05020202}, /* 124 */
  {0x05020202}, {0x05020202}, {0x05020202}, {0x05020202}, {0x05020102}, /* 129 */
  {0x05020102}, {0x05020102}, {0x05020102}, {0x05020102}, {0x05020102}, /* 134 */
  {0x05020102}, {0x05020102}, {0x05010202}, {0x05010202}, {0x05010202}, /* 139 */
  {0x05010202}, {0x05010202}, {0x05010202}, {0x05010202}, {0x05010202}, /* 144 */
  {0x05020001}, {0x05020001}, {0x05020001}, {0x05020001}, {0x05020001}, /* 149 */
  {0x05020001}, {0x05020001}, {0x05020001}, {0x05000201}, {0x05000201}, /* 154 */
  {0x05000201}, {0x05000201}, {0x05000201}, {0x05000201}, {0x05000201}, /* 159 */
  {0x05000201}, {0x03010102}, {0x03010102}, {0x03010102}, {0x03010102}, /* 164 */
  {0x03010102}, {0x03010102}, {0x03010102}, {0x03010102}, {0x03010102}, /* 169 */
  {0x03010102}, {0x03010102}, {0x03010102}, {0x03010102}, {0x03010102}, /* 174 */
  {0x03010102}, {0x03010102}, {0x03010102}, {0x03010102}, {0x03010102}, /* 179 */
  {0x03010102}, {0x03010102}, {0x03010102}, {0x03010102}, {0x03010102}, /* 184 */
  {0x03010102}, {0x03010102}, {0x03010102}, {0x03010102}, {0x03010102}, /* 189 */
  {0x03010102}, {0x03010102}, {0x03010102}, {0x04010001}, {0x04010001}, /* 194 */
  {0x04010001}, {0x04010001}, {0x04010001}, {0x04010001}, {0x04010001}, /* 199 */
  {0x04010001}, {0x04010001}, {0x04010001}, {0x04010001}, {0x04010001}, /* 204 */
  {0x04010001}, {0x04010001}, {0x04010001}, {0x04010001}, {0x04000101}, /* 209 */
  {0x04000101}, {0x04000101}, {0x04000101}, {0x04000101}, {0x04000101}, /* 214 */
  {0x04000101}, {0x04000101}, {0x04000101}, {0x04000101}, {0x04000101}, /* 219 */
  {0x04000101}, {0x04000101}, {0x04000101}, {0x04000101}, {0x04000101}, /* 224 */
  {0x03000000}, {0x03000000}, {0x03000000}, {0x03000000}, {0x03000000}, /* 229 */
  {0x03000000}, {0x03000000}, {0x03000000}, {0x03000000}, {0x03000000}, /* 234 */
  {0x03000000}, {0x03000000}, {0x03000000}, {0x03000000}, {0x03000000}, /* 239 */
  {0x03000000}, {0x03000000}, {0x03000000}, {0x03000000}, {0x03000000}, /* 244 */
  {0x03000000}, {0x03000000}, {0x03000000}, {0x03000000}, {0x03000000}, /* 249 */
  {0x03000000}, {0x03000000}, {0x03000000}, {0x03000000}, {0x03000000}, /* 254 */
  {0x03000000}, {0x03000000}, {0xFF000005}, {0x050F0F02}, {0x050F0E02}, /* 259 */
  {0x050E0F02}, {0x050F0D02}, {0x040E0E02}, {0x040E0E02}, {0x050D0F02}, /* 264 */
  {0x050F0C02}, {0x050C0F02}, {0x050E0D02}, {0x050D0E02}, {0x050F0B02}, /* 269 */
  {0x040B0F02}, {0x040B0F02}, {0x050E0C02}, {0x050C0E02}, {0x040D0D02}, /* 274 */
  {0x040D0D02}, {0x040F0A02}, {0x040F0A02}, {0x040A0F02}, {0x040A0F02}, /* 279 */
  {0x040E0B02}, {0x040E0B02}, {0x040B0E02}, {0x040B0E02}, {0x040D0C02}, /* 284 */
  {0x040D0C02}, {0x040C0D02}, {0x040C0D02}, {0x040F0902}, {0x040F0902}, /* 289 */
  {0xFF000005}, {0x04090F02}, {0x04090F02}, {0x040A0E02}, {0x040A0E02}, /* 294 */
  {0x040D0B02}, {0x040D0B02}, {0x040B0D02}, {0x040B0D02}, {0x040F0802}, /* 299 */
  {0x040F0802}, {0x04080F02}, {0x04080F02}, {0x040C0C02}, {0x040C0C02}, /* 304 */
  {0x040E0902}, {0x040E0902}, {0x04090E02}, {0x04090E02}, {0x040F0702}, /* 309 */
  {0x040F0702}, {0x04070F02}, {0x04070F02}, {0x040D0A02}, {0x040D0A02}, /* 314 */
  {0x040A0D02}, {0x040A0D02}, {0x040C0B02}, {0x040C0B02}, {0x040F0602}, /* 319 */
  {0x040F0602}, {0x050E0A02}, {0x050F0001}, {0xFF000004}, {0x030B0C02}, /* 324 */
  {0x030B0C02}, {0x03060F02}, {0x03060F02}, {0x040E0802}, {0x04080E02}, /* 329 */
  {0x040F0502}, {0x040D0902}, {0x03050F02}, {0x03050F02}, {0x030E0702}, /* 334 */
  {0x030E0702}, {0x03070E02}, {0x03070E02}, {0x030C0A02}, {0x030C0A02}, /* 339 */
  {0xFF000004}, {0x030A0C02}, {0x030A0C02}, {0x030B0B02}, {0x030B0B02}, /* 344 */
  {0x04090D02}, {0x040D0802}, {0x030F0402}, {0x030F0402}, {0x03040F02}, /* 349 */
  {0x03040F02}, {0x030F0302}, {0x030F0302}, {0x03030F02}, {0x03030F02}, /* 354 */
  {0x03080D02}, {0x03080D02}, {0xFF000004}, {0x03060E02}, {0x03060E02}, /* 359 */
  {0x030F0202}, {0x030F0202}, {0x03020F02}, {0x03020F02}, {0x040E0602}, /* 364 */
  {0x04000F01}, {0x030F0102}, {0x030F0102}, {0x03010F02}, {0x03010F02}, /* 369 */
  {0x030C0902}, {0x030C0902}, {0x03090C02}, {0x03090C02}, {0xFF000003}, /* 374 */
  {0x030E0502}, {0x030B0A02}, {0x030A0B02}, {0x03050E02}, {0x030D0702}, /* 379 */
  {0x03070D02}, {0x030E0402}, {0x03040E02}, {0xFF000003}, {0x030C0802}, /* 384 */
  {0x03080C02}, {0x030E0302}, {0x030D0602}, {0x03060D02}, {0x03030E02}, /* 389 */
  {0x030B0902}, {0x03090B02}, {0xFF000004}, {0x030E0202}, {0x030E0202}, /* 394 */
  {0x030A0A02}, {0x030A0A02}, {0x03020E02}, {0x03020E02}, {0x030E0102}, /* 399 */
  {0x030E0102}, {0x03010E02}, {0x03010E02}, {0x040E0001}, {0x04000E01}, /* 404 */
  {0x030D0502}, {0x030D0502}, {0x03050D02}, {0x03050D02}, {0xFF000003}, /* 409 */
  {0x030C0702}, {0x03070C02}, {0x030D0402}, {0x030B0802}, {0x02040D02}, /* 414 */
  {0x02040D02}, {0x03080B02}, {0x030A0902}, {0xFF000003}, {0x03090A02}, /* 419 */
  {0x030C0602}, {0x03060C02}, {0x030D0302}, {0x02030D02}, {0x02030D02}, /* 424 */
  {0x02020D02}, {0x02020D02}, {0xFF000003}, {0x030D0202}, {0x030D0001}, /* 429 */
  {0x020D0102}, {0x020D0102}, {0x020B0702}, {0x020B0702}, {0x02070B02}, /* 434 */
  {0x02070B02}, {0xFF000003}, {0x02010D02}, {0x02010D02}, {0x030C0502}, /* 439 */
  {0x03000D01}, {0x02050C02}, {0x02050C02}, {0x020A0802}, {0x020A0802}, /* 444 */
  {0xFF000002}, {0x02080A02}, {0x020C0402}, {0x02040C02}, {0x020B0602}, /* 449 */
  {0xFF000003}, {0x02060B02}, {0x02060B02}, {0x03090902}, {0x030C0001}, /* 454 */
  {0x020C0302}, {0x020C0302}, {0x02030C02}, {0x02030C02}, {0xFF000003}, /* 459 */
  {0x020A0702}, {0x020A0702}, {0x02070A02}, {0x02070A02}, {0x02060A02}, /* 464 */
  {0x02060A02}, {0x03000C01}, {0x030B0001}, {0xFF000002}, {0x01020C02}, /* 469 */
  {0x01020C02}, {0x020C0202}, {0x020B0502}, {0xFF000002}, {0x02050B02}, /* 474 */
  {0x020C0102}, {0x02090802}, {0x02080902}, {0xFF000002}, {0x02010C02}, /* 479 */
  {0x020B0402}, {0x02040B02}, {0x020A0602}, {0xFF000002}, {0x020B0302}, /* 484 */
  {0x02090702}, {0x01030B02}, {0x01030B02}, {0xFF000002}, {0x02070902}, /* 489 */
  {0x02080802}, {0x020B0202}, {0x020A0502}, {0xFF000002}, {0x01020B02}, /* 494 */
  {0x01020B02}, {0x02050A02}, {0x020B0102}, {0xFF000002}, {0x01010B02}, /* 499 */
  {0x01010B02}, {0x02000B01}, {0x02090602}, {0xFF000002}, {0x02060902}, /* 504 */
  {0x020A0402}, {0x02040A02}, {0x02080702}, {0xFF000002}, {0x02070802}, /* 509 */
  {0x020A0302}, {0x01030A02}, {0x01030A02}, {0xFF000001}, {0x01090502}, /* 514 */
  {0x01050902}, {0xFF000001}, {0x010A0202}, {0x01020A02}, {0xFF000001}, /* 519 */
  {0x010A0102}, {0x01010A02}, {0xFF000002}, {0x020A0001}, {0x02000A01}, /* 524 */
  {0x01080602}, {0x01080602}, {0xFF000001}, {0x01060802}, {0x01090402}, /* 529 */
  {0xFF000001}, {0x01040902}, {0x01090302}, {0xFF000002}, {0x01030902}, /* 534 */
  {0x01030902}, {0x02070702}, {0x02090001}, {0xFF000001}, {0x01080502}, /* 539 */
  {0x01050802}, {0xFF000001}, {0x01090202}, {0x01070602}, {0xFF000001}, /* 544 */
  {0x01060702}, {0x01020902}, {0xFF000001}, {0x01090102}, {0x01000901}, /* 549 */
  {0xFF000001}, {0x01080402}, {0x01040802}, {0xFF000001}, {0x01070502}, /* 554 */
  {0x01050702}, {0xFF000001}, {0x01080302}, {0x01030802}, {0xFF000001}, /* 559 */
  {0x01060602}, {0x01070402}, {0xFF000001}, {0x01040702}, {0x01080001}, /* 564 */
  {0xFF000001}, {0x01000801}, {0x01060502}, {0xFF000001}, {0x01050602}, /* 569 */
  {0x01070302}, {0xFF000001}, {0x01030702}, {0x01060402}, {0xFF000001}, /* 574 */
  {0x01070001}, {0x01000701}, {0xFF000001}, {0x01060001}, {0x01000601}, /* 579 */
};

/* max table bits  8 */

/* TABLE 16  256 entries  maxbits 17  linbits  0 */
static const HUFF_ELEMENT huff_table_16[] =
{
  {0xFF000008}, {0x00000101}, {0x0000010A}, {0x00000113}, {0x080F0F02}, /*  4 */
  {0x00000118}, {0x0000011D}, {0x00000120}, {0x08020F02}, {0x00000131}, /*  9 */
  {0x080F0102}, {0x08010F02}, {0x00000134}, {0x00000145}, {0x00000156}, /* 14 */
  {0x00000167}, {0x00000178}, {0x00000189}, {0x0000019A}, {0x000001A3}, /* 19 */
  {0x000001AC}, {0x000001B5}, {0x000001BE}, {0x000001C7}, {0x000001D0}, /* 24 */
  {0x000001D9}, {0x000001DE}, {0x000001E3}, {0x000001E6}, {0x000001EB}, /* 29 */
  {0x000001F0}, {0x08010502}, {0x000001F3}, {0x000001F6}, {0x000001F9}, /* 34 */
  {0x000001FC}, {0x08040102}, {0x08010402}, {0x000001FF}, {0x08030202}, /* 39 */
  {0x08020302}, {0x07030102}, {0x07030102}, {0x07010302}, {0x07010302}, /* 44 */
  {0x08030001}, {0x08000301}, {0x07020202}, {0x07020202}, {0x06020102}, /* 49 */
  {0x06020102}, {0x06020102}, {0x06020102}, {0x06010202}, {0x06010202}, /* 54 */
  {0x06010202}, {0x06010202}, {0x06020001}, {0x06020001}, {0x06020001}, /* 59 */
  {0x06020001}, {0x06000201}, {0x06000201}, {0x06000201}, {0x06000201}, /* 64 */
  {0x04010102}, {0x04010102}, {0x04010102}, {0x04010102}, {0x04010102}, /* 69 */
  {0x04010102}, {0x04010102}, {0x04010102}, {0x04010102}, {0x04010102}, /* 74 */
  {0x04010102}, {0x04010102}, {0x04010102}, {0x04010102}, {0x04010102}, /* 79 */
  {0x04010102}, {0x04010001}, {0x04010001}, {0x04010001}, {0x04010001}, /* 84 */
  {0x04010001}, {0x04010001}, {0x04010001}, {0x04010001}, {0x04010001}, /* 89 */
  {0x04010001}, {0x04010001}, {0x04010001}, {0x04010001}, {0x04010001}, /* 94 */
  {0x04010001}, {0x04010001}, {0x03000101}, {0x03000101}, {0x03000101}, /* 99 */
  {0x03000101}, {0x03000101}, {0x03000101}, {0x03000101}, {0x03000101}, /* 104 */
  {0x03000101}, {0x03000101}, {0x03000101}, {0x03000101}, {0x03000101}, /* 109 */
  {0x03000101}, {0x03000101}, {0x03000101}, {0x03000101}, {0x03000101}, /* 114 */
  {0x03000101}, {0x03000101}, {0x03000101}, {0x03000101}, {0x03000101}, /* 119 */
  {0x03000101}, {0x03000101}, {0x03000101}, {0x03000101}, {0x03000101}, /* 124 */
  {0x03000101}, {0x03000101}, {0x03000101}, {0x03000101}, {0x01000000}, /* 129 */
  {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, /* 134 */
  {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, /* 139 */
  {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, /* 144 */
  {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, /* 149 */
  {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, /* 154 */
  {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, /* 159 */
  {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, /* 164 */
  {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, /* 169 */
  {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, /* 174 */
  {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, /* 179 */
  {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, /* 184 */
  {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, /* 189 */
  {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, /* 194 */
  {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, /* 199 */
  {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, /* 204 */
  {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, /* 209 */
  {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, /* 214 */
  {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, /* 219 */
  {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, /* 224 */
  {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, /* 229 */
  {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, /* 234 */
  {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, /* 239 */
  {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, /* 244 */
  {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, /* 249 */
  {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, {0x01000000}, /* 254 */
  {0x01000000}, {0x01000000}, {0xFF000003}, {0x030F0E02}, {0x030E0F02}, /* 259 */
  {0x030F0D02}, {0x030D0F02}, {0x030F0C02}, {0x030C0F02}, {0x030F0B02}, /* 264 */
  {0x030B0F02}, {0xFF000003}, {0x020F0A02}, {0x020F0A02}, {0x030A0F02}, /* 269 */
  {0x030F0902}, {0x03090F02}, {0x03080F02}, {0x020F0802}, {0x020F0802}, /* 274 */
  {0xFF000002}, {0x020F0702}, {0x02070F02}, {0x020F0602}, {0x02060F02}, /* 279 */
  {0xFF000002}, {0x020F0502}, {0x02050F02}, {0x010F0402}, {0x010F0402}, /* 284 */
  {0xFF000001}, {0x01040F02}, {0x01030F02}, {0xFF000004}, {0x01000F01}, /* 289 */
  {0x01000F01}, {0x01000F01}, {0x01000F01}, {0x01000F01}, {0x01000F01}, /* 294 */
  {0x01000F01}, {0x01000F01}, {0x020F0302}, {0x020F0302}, {0x020F0302}, /* 299 */
  {0x020F0302}, {0x000000E2}, {0x000000F3}, {0x000000FC}, {0x00000105}, /* 304 */
  {0xFF000001}, {0x010F0202}, {0x010F0001}, {0xFF000004}, {0x000000FA}, /* 309 */
  {0x000000FF}, {0x00000104}, {0x00000109}, {0x0000010C}, {0x00000111}, /* 314 */
  {0x00000116}, {0x00000119}, {0x0000011E}, {0x00000123}, {0x00000128}, /* 319 */
  {0x04030E02}, {0x0000012D}, {0x00000130}, {0x00000133}, {0x00000136}, /* 324 */
  {0xFF000004}, {0x00000128}, {0x0000012B}, {0x0000012E}, {0x040D0001}, /* 329 */
  {0x00000131}, {0x00000134}, {0x00000137}, {0x040C0302}, {0x0000013A}, /* 334 */
  {0x040C0102}, {0x04000C01}, {0x0000013D}, {0x03020E02}, {0x03020E02}, /* 339 */
  {0x040E0202}, {0x040E0102}, {0xFF000004}, {0x04030D02}, {0x040D0202}, /* 344 */
  {0x04020D02}, {0x04010D02}, {0x040B0302}, {0x0000012F}, {0x030D0102}, /* 349 */
  {0x030D0102}, {0x04040C02}, {0x040B0602}, {0x04030C02}, {0x04070A02}, /* 354 */
  {0x030C0202}, {0x030C0202}, {0x04020C02}, {0x04050B02}, {0xFF000004}, /* 359 */
  {0x04010C02}, {0x040C0001}, {0x040B0402}, {0x04040B02}, {0x040A0602}, /* 364 */
  {0x04060A02}, {0x03030B02}, {0x03030B02}, {0x040A0502}, {0x04050A02}, /* 369 */
  {0x030B0202}, {0x030B0202}, {0x03020B02}, {0x03020B02}, {0x030B0102}, /* 374 */
  {0x030B0102}, {0xFF000004}, {0x03010B02}, {0x03010B02}, {0x040B0001}, /* 379 */
  {0x04000B01}, {0x04090602}, {0x04060902}, {0x040A0402}, {0x04040A02}, /* 384 */
  {0x04080702}, {0x04070802}, {0x03030A02}, {0x03030A02}, {0x040A0302}, /* 389 */
  {0x04090502}, {0x030A0202}, {0x030A0202}, {0xFF000004}, {0x04050902}, /* 394 */
  {0x04080602}, {0x03010A02}, {0x03010A02}, {0x04060802}, {0x04070702}, /* 399 */
  {0x03040902}, {0x03040902}, {0x04090402}, {0x04070502}, {0x03070602}, /* 404 */
  {0x03070602}, {0x02020A02}, {0x02020A02}, {0x02020A02}, {0x02020A02}, /* 409 */
  {0xFF000003}, {0x020A0102}, {0x020A0102}, {0x030A0001}, {0x03000A01}, /* 414 */
  {0x03090302}, {0x03030902}, {0x03080502}, {0x03050802}, {0xFF000003}, /* 419 */
  {0x02090202}, {0x02090202}, {0x02020902}, {0x02020902}, {0x03060702}, /* 424 */
  {0x03090001}, {0x02090102}, {0x02090102}, {0xFF000003}, {0x02010902}, /* 429 */
  {0x02010902}, {0x03000901}, {0x03080402}, {0x03040802}, {0x03050702}, /* 434 */
  {0x03080302}, {0x03030802}, {0xFF000003}, {0x03060602}, {0x03080202}, /* 439 */
  {0x02020802}, {0x02020802}, {0x03070402}, {0x03040702}, {0x02080102}, /* 444 */
  {0x02080102}, {0xFF000003}, {0x02010802}, {0x02010802}, {0x02000801}, /* 449 */
  {0x02000801}, {0x03080001}, {0x03060502}, {0x02070302}, {0x02070302}, /* 454 */
  {0xFF000003}, {0x02030702}, {0x02030702}, {0x03050602}, {0x03060402}, /* 459 */
  {0x02070202}, {0x02070202}, {0x02020702}, {0x02020702}, {0xFF000003}, /* 464 */
  {0x03040602}, {0x03050502}, {0x02070001}, {0x02070001}, {0x01070102}, /* 469 */
  {0x01070102}, {0x01070102}, {0x01070102}, {0xFF000002}, {0x01010702}, /* 474 */
  {0x01010702}, {0x02000701}, {0x02060302}, {0xFF000002}, {0x02030602}, /* 479 */
  {0x02050402}, {0x02040502}, {0x02060202}, {0xFF000001}, {0x01020602}, /* 484 */
  {0x01060102}, {0xFF000002}, {0x01010602}, {0x01010602}, {0x02060001}, /* 489 */
  {0x02000601}, {0xFF000002}, {0x01030502}, {0x01030502}, {0x02050302}, /* 494 */
  {0x02040402}, {0xFF000001}, {0x01050202}, {0x01020502}, {0xFF000001}, /* 499 */
  {0x01050102}, {0x01050001}, {0xFF000001}, {0x01040302}, {0x01030402}, /* 504 */
  {0xFF000001}, {0x01000501}, {0x01040202}, {0xFF000001}, {0x01020402}, /* 509 */
  {0x01030302}, {0xFF000001}, {0x01040001}, {0x01000401}, {0xFF000004}, /* 514 */
  {0x040E0C02}, {0x00000086}, {0x030E0D02}, {0x030E0D02}, {0x03090E02}, /* 519 */
  {0x03090E02}, {0x040A0E02}, {0x04090D02}, {0x020E0E02}, {0x020E0E02}, /* 524 */
  {0x020E0E02}, {0x020E0E02}, {0x030D0E02}, {0x030D0E02}, {0x030B0E02}, /* 529 */
  {0x030B0E02}, {0xFF000003}, {0x020E0B02}, {0x020E0B02}, {0x020D0C02}, /* 534 */
  {0x020D0C02}, {0x030C0D02}, {0x030B0D02}, {0x020E0A02}, {0x020E0A02}, /* 539 */
  {0xFF000003}, {0x020C0C02}, {0x020C0C02}, {0x030D0A02}, {0x030A0D02}, /* 544 */
  {0x030E0702}, {0x030C0A02}, {0x020A0C02}, {0x020A0C02}, {0xFF000003}, /* 549 */
  {0x03090C02}, {0x030D0702}, {0x020E0502}, {0x020E0502}, {0x010D0B02}, /* 554 */
  {0x010D0B02}, {0x010D0B02}, {0x010D0B02}, {0xFF000002}, {0x010E0902}, /* 559 */
  {0x010E0902}, {0x020C0B02}, {0x020B0C02}, {0xFF000002}, {0x020E0802}, /* 564 */
  {0x02080E02}, {0x020D0902}, {0x02070E02}, {0xFF000002}, {0x020B0B02}, /* 569 */
  {0x020D0802}, {0x02080D02}, {0x020E0602}, {0xFF000001}, {0x01060E02}, /* 574 */
  {0x010C0902}, {0xFF000002}, {0x020B0A02}, {0x020A0B02}, {0x02050E02}, /* 579 */
  {0x02070D02}, {0xFF000002}, {0x010E0402}, {0x010E0402}, {0x02040E02}, /* 584 */
  {0x020C0802}, {0xFF000001}, {0x01080C02}, {0x010E0302}, {0xFF000002}, /* 589 */
  {0x010D0602}, {0x010D0602}, {0x02060D02}, {0x020B0902}, {0xFF000002}, /* 594 */
  {0x02090B02}, {0x020A0A02}, {0x01010E02}, {0x01010E02}, {0xFF000002}, /* 599 */
  {0x01040D02}, {0x01040D02}, {0x02080B02}, {0x02090A02}, {0xFF000002}, /* 604 */
  {0x010B0702}, {0x010B0702}, {0x02070B02}, {0x02000D01}, {0xFF000001}, /* 609 */
  {0x010E0001}, {0x01000E01}, {0xFF000001}, {0x010D0502}, {0x01050D02}, /* 614 */
  {0xFF000001}, {0x010C0702}, {0x01070C02}, {0xFF000001}, {0x010D0402}, /* 619 */
  {0x010B0802}, {0xFF000001}, {0x010A0902}, {0x010C0602}, {0xFF000001}, /* 624 */
  {0x01060C02}, {0x010D0302}, {0xFF000001}, {0x010C0502}, {0x01050C02}, /* 629 */
  {0xFF000001}, {0x010A0802}, {0x01080A02}, {0xFF000001}, {0x01090902}, /* 634 */
  {0x010C0402}, {0xFF000001}, {0x01060B02}, {0x010A0702}, {0xFF000001}, /* 639 */
  {0x010B0502}, {0x01090802}, {0xFF000001}, {0x01080902}, {0x01090702}, /* 644 */
  {0xFF000001}, {0x01070902}, {0x01080802}, {0xFF000001}, {0x010C0E02}, /* 649 */
  {0x010D0D02},
};

/* max table bits  8 */
/* NO XING TABLE 17 */
/* NO XING TABLE 18 */
/* NO XING TABLE 19 */
/* NO XING TABLE 20 */
/* NO XING TABLE 21 */
/* NO XING TABLE 22 */
/* NO XING TABLE 23 */

/* TABLE 24  256 entries  maxbits 12  linbits  0 */
static const HUFF_ELEMENT huff_table_24[] =
{
  {0xFF000009}, {0x080F0E02}, {0x080F0E02}, {0x080E0F02}, {0x080E0F02}, /*  4 */
  {0x080F0D02}, {0x080F0D02}, {0x080D0F02}, {0x080D0F02}, {0x080F0C02}, /*  9 */
  {0x080F0C02}, {0x080C0F02}, {0x080C0F02}, {0x080F0B02}, {0x080F0B02}, /* 14 */
  {0x080B0F02}, {0x080B0F02}, {0x070A0F02}, {0x070A0F02}, {0x070A0F02}, /* 19 */
  {0x070A0F02}, {0x080F0A02}, {0x080F0A02}, {0x080F0902}, {0x080F0902}, /* 24 */
  {0x07090F02}, {0x07090F02}, {0x07090F02}, {0x07090F02}, {0x07080F02}, /* 29 */
  {0x07080F02}, {0x07080F02}, {0x07080F02}, {0x080F0802}, {0x080F0802}, /* 34 */
  {0x080F0702}, {0x080F0702}, {0x07070F02}, {0x07070F02}, {0x07070F02}, /* 39 */
  {0x07070F02}, {0x070F0602}, {0x070F0602}, {0x070F0602}, {0x070F0602}, /* 44 */
  {0x07060F02}, {0x07060F02}, {0x07060F02}, {0x07060F02}, {0x070F0502}, /* 49 */
  {0x070F0502}, {0x070F0502}, {0x070F0502}, {0x07050F02}, {0x07050F02}, /* 54 */
  {0x07050F02}, {0x07050F02}, {0x070F0402}, {0x070F0402}, {0x070F0402}, /* 59 */
  {0x070F0402}, {0x07040F02}, {0x07040F02}, {0x07040F02}, {0x07040F02}, /* 64 */
  {0x070F0302}, {0x070F0302}, {0x070F0302}, {0x070F0302}, {0x07030F02}, /* 69 */
  {0x07030F02}, {0x07030F02}, {0x07030F02}, {0x070F0202}, {0x070F0202}, /* 74 */
  {0x070F0202}, {0x070F0202}, {0x07020F02}, {0x07020F02}, {0x07020F02}, /* 79 */
  {0x07020F02}, {0x07010F02}, {0x07010F02}, {0x07010F02}, {0x07010F02}, /* 84 */
  {0x080F0102}, {0x080F0102}, {0x08000F01}, {0x08000F01}, {0x090F0001}, /* 89 */
  {0x00000201}, {0x00000206}, {0x0000020B}, {0x00000210}, {0x00000215}, /* 94 */
  {0x0000021A}, {0x0000021F}, {0x040F0F02}, {0x040F0F02}, {0x040F0F02}, /* 99 */
  {0x040F0F02}, {0x040F0F02}, {0x040F0F02}, {0x040F0F02}, {0x040F0F02}, /* 104 */
  {0x040F0F02}, {0x040F0F02}, {0x040F0F02}, {0x040F0F02}, {0x040F0F02}, /* 109 */
  {0x040F0F02}, {0x040F0F02}, {0x040F0F02}, {0x040F0F02}, {0x040F0F02}, /* 114 */
  {0x040F0F02}, {0x040F0F02}, {0x040F0F02}, {0x040F0F02}, {0x040F0F02}, /* 119 */
  {0x040F0F02}, {0x040F0F02}, {0x040F0F02}, {0x040F0F02}, {0x040F0F02}, /* 124 */
  {0x040F0F02}, {0x040F0F02}, {0x040F0F02}, {0x040F0F02}, {0x00000224}, /* 129 */
  {0x00000229}, {0x00000232}, {0x00000237}, {0x0000023A}, {0x0000023F}, /* 134 */
  {0x00000242}, {0x00000245}, {0x0000024A}, {0x0000024D}, {0x00000250}, /* 139 */
  {0x00000253}, {0x00000256}, {0x00000259}, {0x0000025C}, {0x0000025F}, /* 144 */
  {0x00000262}, {0x00000265}, {0x00000268}, {0x0000026B}, {0x0000026E}, /* 149 */
  {0x00000271}, {0x00000274}, {0x00000277}, {0x0000027A}, {0x0000027D}, /* 154 */
  {0x00000280}, {0x00000283}, {0x00000288}, {0x0000028B}, {0x0000028E}, /* 159 */
  {0x00000291}, {0x00000294}, {0x00000297}, {0x0000029A}, {0x0000029F}, /* 164 */
  {0x09040B02}, {0x000002A4}, {0x000002A7}, {0x000002AA}, {0x09030B02}, /* 169 */
  {0x09080802}, {0x000002AF}, {0x09020B02}, {0x000002B2}, {0x000002B5}, /* 174 */
  {0x09060902}, {0x09040A02}, {0x000002B8}, {0x09070802}, {0x090A0302}, /* 179 */
  {0x09030A02}, {0x09090502}, {0x09050902}, {0x090A0202}, {0x09020A02}, /* 184 */
  {0x09010A02}, {0x09080602}, {0x09060802}, {0x09070702}, {0x09090402}, /* 189 */
  {0x09040902}, {0x09090302}, {0x09030902}, {0x09080502}, {0x09050802}, /* 194 */
  {0x09090202}, {0x09070602}, {0x09060702}, {0x09020902}, {0x09090102}, /* 199 */
  {0x09010902}, {0x09080402}, {0x09040802}, {0x09070502}, {0x09050702}, /* 204 */
  {0x09080302}, {0x09030802}, {0x09060602}, {0x09080202}, {0x09020802}, /* 209 */
  {0x09080102}, {0x09070402}, {0x09040702}, {0x09010802}, {0x000002BB}, /* 214 */
  {0x09060502}, {0x09050602}, {0x09070102}, {0x000002BE}, {0x08030702}, /* 219 */
  {0x08030702}, {0x09070302}, {0x09070202}, {0x08020702}, {0x08020702}, /* 224 */
  {0x08060402}, {0x08060402}, {0x08040602}, {0x08040602}, {0x08050502}, /* 229 */
  {0x08050502}, {0x08010702}, {0x08010702}, {0x08060302}, {0x08060302}, /* 234 */
  {0x08030602}, {0x08030602}, {0x08050402}, {0x08050402}, {0x08040502}, /* 239 */
  {0x08040502}, {0x08060202}, {0x08060202}, {0x08020602}, {0x08020602}, /* 244 */
  {0x08060102}, {0x08060102}, {0x08010602}, {0x08010602}, {0x09060001}, /* 249 */
  {0x09000601}, {0x08050302}, {0x08050302}, {0x08030502}, {0x08030502}, /* 254 */
  {0x08040402}, {0x08040402}, {0x08050202}, {0x08050202}, {0x08020502}, /* 259 */
  {0x08020502}, {0x08050102}, {0x08050102}, {0x09050001}, {0x09000501}, /* 264 */
  {0x07010502}, {0x07010502}, {0x07010502}, {0x07010502}, {0x08040302}, /* 269 */
  {0x08040302}, {0x08030402}, {0x08030402}, {0x07040202}, {0x07040202}, /* 274 */
  {0x07040202}, {0x07040202}, {0x07020402}, {0x07020402}, {0x07020402}, /* 279 */
  {0x07020402}, {0x07030302}, {0x07030302}, {0x07030302}, {0x07030302}, /* 284 */
  {0x07040102}, {0x07040102}, {0x07040102}, {0x07040102}, {0x07010402}, /* 289 */
  {0x07010402}, {0x07010402}, {0x07010402}, {0x08040001}, {0x08040001}, /* 294 */
  {0x08000401}, {0x08000401}, {0x07030202}, {0x07030202}, {0x07030202}, /* 299 */
  {0x07030202}, {0x07020302}, {0x07020302}, {0x07020302}, {0x07020302}, /* 304 */
  {0x06030102}, {0x06030102}, {0x06030102}, {0x06030102}, {0x06030102}, /* 309 */
  {0x06030102}, {0x06030102}, {0x06030102}, {0x06010302}, {0x06010302}, /* 314 */
  {0x06010302}, {0x06010302}, {0x06010302}, {0x06010302}, {0x06010302}, /* 319 */
  {0x06010302}, {0x07030001}, {0x07030001}, {0x07030001}, {0x07030001}, /* 324 */
  {0x07000301}, {0x07000301}, {0x07000301}, {0x07000301}, {0x06020202}, /* 329 */
  {0x06020202}, {0x06020202}, {0x06020202}, {0x06020202}, {0x06020202}, /* 334 */
  {0x06020202}, {0x06020202}, {0x05020102}, {0x05020102}, {0x05020102}, /* 339 */
  {0x05020102}, {0x05020102}, {0x05020102}, {0x05020102}, {0x05020102}, /* 344 */
  {0x05020102}, {0x05020102}, {0x05020102}, {0x05020102}, {0x05020102}, /* 349 */
  {0x05020102}, {0x05020102}, {0x05020102}, {0x05010202}, {0x05010202}, /* 354 */
  {0x05010202}, {0x05010202}, {0x05010202}, {0x05010202}, {0x05010202}, /* 359 */
  {0x05010202}, {0x05010202}, {0x05010202}, {0x05010202}, {0x05010202}, /* 364 */
  {0x05010202}, {0x05010202}, {0x05010202}, {0x05010202}, {0x06020001}, /* 369 */
  {0x06020001}, {0x06020001}, {0x06020001}, {0x06020001}, {0x06020001}, /* 374 */
  {0x06020001}, {0x06020001}, {0x06000201}, {0x06000201}, {0x06000201}, /* 379 */
  {0x06000201}, {0x06000201}, {0x06000201}, {0x06000201}, {0x06000201}, /* 384 */
  {0x04010102}, {0x04010102}, {0x04010102}, {0x04010102}, {0x04010102}, /* 389 */
  {0x04010102}, {0x04010102}, {0x04010102}, {0x04010102}, {0x04010102}, /* 394 */
  {0x04010102}, {0x04010102}, {0x04010102}, {0x04010102}, {0x04010102}, /* 399 */
  {0x04010102}, {0x04010102}, {0x04010102}, {0x04010102}, {0x04010102}, /* 404 */
  {0x04010102}, {0x04010102}, {0x04010102}, {0x04010102}, {0x04010102}, /* 409 */
  {0x04010102}, {0x04010102}, {0x04010102}, {0x04010102}, {0x04010102}, /* 414 */
  {0x04010102}, {0x04010102}, {0x04010001}, {0x04010001}, {0x04010001}, /* 419 */
  {0x04010001}, {0x04010001}, {0x04010001}, {0x04010001}, {0x04010001}, /* 424 */
  {0x04010001}, {0x04010001}, {0x04010001}, {0x04010001}, {0x04010001}, /* 429 */
  {0x04010001}, {0x04010001}, {0x04010001}, {0x04010001}, {0x04010001}, /* 434 */
  {0x04010001}, {0x04010001}, {0x04010001}, {0x04010001}, {0x04010001}, /* 439 */
  {0x04010001}, {0x04010001}, {0x04010001}, {0x04010001}, {0x04010001}, /* 444 */
  {0x04010001}, {0x04010001}, {0x04010001}, {0x04010001}, {0x04000101}, /* 449 */
  {0x04000101}, {0x04000101}, {0x04000101}, {0x04000101}, {0x04000101}, /* 454 */
  {0x04000101}, {0x04000101}, {0x04000101}, {0x04000101}, {0x04000101}, /* 459 */
  {0x04000101}, {0x04000101}, {0x04000101}, {0x04000101}, {0x04000101}, /* 464 */
  {0x04000101}, {0x04000101}, {0x04000101}, {0x04000101}, {0x04000101}, /* 469 */
  {0x04000101}, {0x04000101}, {0x04000101}, {0x04000101}, {0x04000101}, /* 474 */
  {0x04000101}, {0x04000101}, {0x04000101}, {0x04000101}, {0x04000101}, /* 479 */
  {0x04000101}, {0x04000000}, {0x04000000}, {0x04000000}, {0x04000000}, /* 484 */
  {0x04000000}, {0x04000000}, {0x04000000}, {0x04000000}, {0x04000000}, /* 489 */
  {0x04000000}, {0x04000000}, {0x04000000}, {0x04000000}, {0x04000000}, /* 494 */
  {0x04000000}, {0x04000000}, {0x04000000}, {0x04000000}, {0x04000000}, /* 499 */
  {0x04000000}, {0x04000000}, {0x04000000}, {0x04000000}, {0x04000000}, /* 504 */
  {0x04000000}, {0x04000000}, {0x04000000}, {0x04000000}, {0x04000000}, /* 509 */
  {0x04000000}, {0x04000000}, {0x04000000}, {0xFF000002}, {0x020E0E02}, /* 514 */
  {0x020E0D02}, {0x020D0E02}, {0x020E0C02}, {0xFF000002}, {0x020C0E02}, /* 519 */
  {0x020D0D02}, {0x020E0B02}, {0x020B0E02}, {0xFF000002}, {0x020D0C02}, /* 524 */
  {0x020C0D02}, {0x020E0A02}, {0x020A0E02}, {0xFF000002}, {0x020D0B02}, /* 529 */
  {0x020B0D02}, {0x020C0C02}, {0x020E0902}, {0xFF000002}, {0x02090E02}, /* 534 */
  {0x020D0A02}, {0x020A0D02}, {0x020C0B02}, {0xFF000002}, {0x020B0C02}, /* 539 */
  {0x020E0802}, {0x02080E02}, {0x020D0902}, {0xFF000002}, {0x02090D02}, /* 544 */
  {0x020E0702}, {0x02070E02}, {0x020C0A02}, {0xFF000002}, {0x020A0C02}, /* 549 */
  {0x020B0B02}, {0x020D0802}, {0x02080D02}, {0xFF000003}, {0x030E0001}, /* 554 */
  {0x03000E01}, {0x020D0001}, {0x020D0001}, {0x01060E02}, {0x01060E02}, /* 559 */
  {0x01060E02}, {0x01060E02}, {0xFF000002}, {0x020E0602}, {0x020C0902}, /* 564 */
  {0x01090C02}, {0x01090C02}, {0xFF000001}, {0x010E0502}, {0x010A0B02}, /* 569 */
  {0xFF000002}, {0x01050E02}, {0x01050E02}, {0x020B0A02}, {0x020D0702}, /* 574 */
  {0xFF000001}, {0x01070D02}, {0x01040E02}, {0xFF000001}, {0x010C0802}, /* 579 */
  {0x01080C02}, {0xFF000002}, {0x020E0402}, {0x020E0202}, {0x010E0302}, /* 584 */
  {0x010E0302}, {0xFF000001}, {0x010D0602}, {0x01060D02}, {0xFF000001}, /* 589 */
  {0x01030E02}, {0x010B0902}, {0xFF000001}, {0x01090B02}, {0x010A0A02}, /* 594 */
  {0xFF000001}, {0x01020E02}, {0x010E0102}, {0xFF000001}, {0x01010E02}, /* 599 */
  {0x010D0502}, {0xFF000001}, {0x01050D02}, {0x010C0702}, {0xFF000001}, /* 604 */
  {0x01070C02}, {0x010D0402}, {0xFF000001}, {0x010B0802}, {0x01080B02}, /* 609 */
  {0xFF000001}, {0x01040D02}, {0x010A0902}, {0xFF000001}, {0x01090A02}, /* 614 */
  {0x010C0602}, {0xFF000001}, {0x01060C02}, {0x010D0302}, {0xFF000001}, /* 619 */
  {0x01030D02}, {0x010D0202}, {0xFF000001}, {0x01020D02}, {0x010D0102}, /* 624 */
  {0xFF000001}, {0x010B0702}, {0x01070B02}, {0xFF000001}, {0x01010D02}, /* 629 */
  {0x010C0502}, {0xFF000001}, {0x01050C02}, {0x010A0802}, {0xFF000001}, /* 634 */
  {0x01080A02}, {0x01090902}, {0xFF000001}, {0x010C0402}, {0x01040C02}, /* 639 */
  {0xFF000001}, {0x010B0602}, {0x01060B02}, {0xFF000002}, {0x02000D01}, /* 644 */
  {0x020C0001}, {0x010C0302}, {0x010C0302}, {0xFF000001}, {0x01030C02}, /* 649 */
  {0x010A0702}, {0xFF000001}, {0x01070A02}, {0x010C0202}, {0xFF000001}, /* 654 */
  {0x01020C02}, {0x010B0502}, {0xFF000001}, {0x01050B02}, {0x010C0102}, /* 659 */
  {0xFF000001}, {0x01090802}, {0x01080902}, {0xFF000001}, {0x01010C02}, /* 664 */
  {0x010B0402}, {0xFF000002}, {0x02000C01}, {0x020B0001}, {0x010B0302}, /* 669 */
  {0x010B0302}, {0xFF000002}, {0x02000B01}, {0x020A0001}, {0x010A0102}, /* 674 */
  {0x010A0102}, {0xFF000001}, {0x010A0602}, {0x01060A02}, {0xFF000001}, /* 679 */
  {0x01090702}, {0x01070902}, {0xFF000002}, {0x02000A01}, {0x02090001}, /* 684 */
  {0x01000901}, {0x01000901}, {0xFF000001}, {0x010B0202}, {0x010A0502}, /* 689 */
  {0xFF000001}, {0x01050A02}, {0x010B0102}, {0xFF000001}, {0x01010B02}, /* 694 */
  {0x01090602}, {0xFF000001}, {0x010A0402}, {0x01080702}, {0xFF000001}, /* 699 */
  {0x01080001}, {0x01000801}, {0xFF000001}, {0x01070001}, {0x01000701}, /* 704 */
};

/* max table bits  9 */
/* NO XING TABLE 25 */
/* NO XING TABLE 26 */
/* NO XING TABLE 27 */
/* NO XING TABLE 28 */
/* NO XING TABLE 29 */
/* NO XING TABLE 30 */
/* NO XING TABLE 31 */
/* done */

/********************************************************************************/
/*                                                                              */
/*  uph.c                                                                       */
/*                                                                              */
/********************************************************************************/

/****  uph.c  ***************************************************

Layer 3 audio
 huffman decode


******************************************************************/


//#include "L3.h"

/*===============================================================*/

/* max bits required for any lookup - change if htable changes */
/* quad required 10 bit w/signs  must have (MAXBITS+2) >= 10   */
#define MAXBITS 9

static const HUFF_ELEMENT huff_table_0[4] =
{{0}, {0}, {0}, {64}};      /* dummy must not use */

//#include "htable.h"

/*-- 6 bit lookup (purgebits, value) --*/
static const unsigned char quad_table_a[][2] =
{
  {6, 11}, {6, 15}, {6, 13}, {6, 14}, {6, 7}, {6, 5}, {5, 9},
  {5, 9}, {5, 6}, {5, 6}, {5, 3}, {5, 3}, {5, 10}, {5, 10},
  {5, 12}, {5, 12}, {4, 2}, {4, 2}, {4, 2}, {4, 2}, {4, 1},
  {4, 1}, {4, 1}, {4, 1}, {4, 4}, {4, 4}, {4, 4}, {4, 4},
  {4, 8}, {4, 8}, {4, 8}, {4, 8}, {1, 0}, {1, 0}, {1, 0},
  {1, 0}, {1, 0}, {1, 0}, {1, 0}, {1, 0}, {1, 0}, {1, 0},
  {1, 0}, {1, 0}, {1, 0}, {1, 0}, {1, 0}, {1, 0}, {1, 0},
  {1, 0}, {1, 0}, {1, 0}, {1, 0}, {1, 0}, {1, 0}, {1, 0},
  {1, 0}, {1, 0}, {1, 0}, {1, 0}, {1, 0}, {1, 0}, {1, 0},
  {1, 0},
};


typedef struct
{
   const HUFF_ELEMENT *table;
   int linbits;
   int ncase;
}
HUFF_SETUP;

#define no_bits       0
#define one_shot      1
#define no_linbits    2
#define have_linbits  3
#define quad_a        4
#define quad_b        5


static const HUFF_SETUP table_look[] =
{
  {huff_table_0, 0, no_bits},
  {huff_table_1, 0, one_shot},
  {huff_table_2, 0, one_shot},
  {huff_table_3, 0, one_shot},
  {huff_table_0, 0, no_bits},
  {huff_table_5, 0, one_shot},
  {huff_table_6, 0, one_shot},
  {huff_table_7, 0, no_linbits},
  {huff_table_8, 0, no_linbits},
  {huff_table_9, 0, no_linbits},
  {huff_table_10, 0, no_linbits},
  {huff_table_11, 0, no_linbits},
  {huff_table_12, 0, no_linbits},
  {huff_table_13, 0, no_linbits},
  {huff_table_0, 0, no_bits},
  {huff_table_15, 0, no_linbits},
  {huff_table_16, 1, have_linbits},
  {huff_table_16, 2, have_linbits},
  {huff_table_16, 3, have_linbits},
  {huff_table_16, 4, have_linbits},
  {huff_table_16, 6, have_linbits},
  {huff_table_16, 8, have_linbits},
  {huff_table_16, 10, have_linbits},
  {huff_table_16, 13, have_linbits},
  {huff_table_24, 4, have_linbits},
  {huff_table_24, 5, have_linbits},
  {huff_table_24, 6, have_linbits},
  {huff_table_24, 7, have_linbits},
  {huff_table_24, 8, have_linbits},
  {huff_table_24, 9, have_linbits},
  {huff_table_24, 11, have_linbits},
  {huff_table_24, 13, have_linbits},
  {huff_table_0, 0, quad_a},
  {huff_table_0, 0, quad_b},
};

/*========================================================*/
extern BITDAT bitdat;

/*------------- get n bits from bitstream -------------*/
/* unused
static unsigned int bitget(int n)
{
   unsigned int x;

   if (bitdat.bits < n)
   {      */  /* refill bit buf if necessary */
/*      while (bitdat.bits <= 24)
      {
   bitdat.bitbuf = (bitdat.bitbuf << 8) | *bitdat.bs_ptr++;
   bitdat.bits += 8;
      }
   }
   bitdat.bits -= n;
   x = bitdat.bitbuf >> bitdat.bits;
   bitdat.bitbuf -= x << bitdat.bits;
   return x;
}
*/
/*----- get n bits  - checks for n+2 avail bits (linbits+sign) -----*/
static unsigned int bitget_lb(int n)
{
   unsigned int x;

   if (bitdat._bits < (n + 2))
   {        /* refill bit buf if necessary */
      while (bitdat._bits <= 24)
      {
   bitdat.bitbuf = (bitdat.bitbuf << 8) | *bitdat.bs_ptr++;
   bitdat._bits += 8;
      }
   }
   bitdat._bits -= n;
   x = bitdat.bitbuf >> bitdat._bits;
   bitdat.bitbuf -= x << bitdat._bits;
   return x;
}




/*------------- get n bits but DO NOT remove from bitstream --*/
static unsigned int bitget2(int n)
{
   unsigned int x;

   if (bitdat._bits < (MAXBITS + 2))
   {        /* refill bit buf if necessary */
      while (bitdat._bits <= 24)
      {
   bitdat.bitbuf = (bitdat.bitbuf << 8) | *bitdat.bs_ptr++;
   bitdat._bits += 8;
      }
   }
   x = bitdat.bitbuf >> (bitdat._bits - n);
   return x;
}
/*------------- remove n bits from bitstream ---------*/
/* unused
static void bitget_purge(int n)
{
   bitdat.bits -= n;
   bitdat.bitbuf -= (bitdat.bitbuf >> bitdat.bits) << bitdat.bits;
}
*/
/*------------- get 1 bit from bitstream NO CHECK -------------*/
/* unused
static unsigned int bitget_1bit()
{
   unsigned int x;

   bitdat.bits--;
   x = bitdat.bitbuf >> bitdat.bits;
   bitdat.bitbuf -= x << bitdat.bits;
   return x;
}
*/
/*========================================================*/
/*========================================================*/
#define mac_bitget_check(n) if( bitdat._bits < (n) ) {                   \
    while( bitdat._bits <= 24 ) {            \
        bitdat.bitbuf = (bitdat.bitbuf << 8) | *bitdat.bs_ptr++; \
        bitdat._bits += 8;                   \
    }                                       \
}
/*---------------------------------------------------------*/
#define mac_bitget2(n)  (bitdat.bitbuf >> (bitdat._bits-n));
/*---------------------------------------------------------*/
#define mac_bitget(n) ( bitdat._bits -= n,           \
         code  = bitdat.bitbuf >> bitdat._bits,     \
         bitdat.bitbuf -= code << bitdat._bits,     \
         code )
/*---------------------------------------------------------*/
#define mac_bitget_purge(n) bitdat._bits -= n,                    \
    bitdat.bitbuf -= (bitdat.bitbuf >> bitdat._bits) << bitdat._bits;
/*---------------------------------------------------------*/
#define mac_bitget_1bit() ( bitdat._bits--,                           \
         code  = bitdat.bitbuf >> bitdat._bits,    \
         bitdat.bitbuf -= code << bitdat._bits,  \
         code )
/*========================================================*/
/*========================================================*/
void unpack_huff(int xy[][2], int n, int ntable)
{
   int i;
   const HUFF_ELEMENT *t;
   const HUFF_ELEMENT *t0;
   int linbits;
   int _bits;
   int code;
   int x, y;

   if (n <= 0)
      return;
   n = n >> 1;      /* huff in pairs */
/*-------------*/
   t0 = table_look[ntable].table;
   linbits = table_look[ntable].linbits;
   switch (table_look[ntable].ncase)
   {
      default:
/*------------------------------------------*/
      case no_bits:
/*- table 0, no data, x=y=0--*/
   for (i = 0; i < n; i++)
   {
      xy[i][0] = 0;
      xy[i][1] = 0;
   }
   return;
/*------------------------------------------*/
      case one_shot:
/*- single lookup, no escapes -*/
   for (i = 0; i < n; i++)
   {
      mac_bitget_check((MAXBITS + 2));
      _bits = t0[0].b.signbits;
      code = mac_bitget2(_bits);
      mac_bitget_purge(t0[1 + code].b.purgebits);
      x = t0[1 + code].b.x;
      y = t0[1 + code].b.y;
      if (x)
         if (mac_bitget_1bit())
      x = -x;
      if (y)
         if (mac_bitget_1bit())
      y = -y;
      xy[i][0] = x;
      xy[i][1] = y;
      if (bitdat.bs_ptr > bitdat.bs_ptr_end)
         break;   // bad data protect

   }
   return;
/*------------------------------------------*/
      case no_linbits:
   for (i = 0; i < n; i++)
   {
      t = t0;
      for (;;)
      {
         mac_bitget_check((MAXBITS + 2));
         _bits = t[0].b.signbits;
         code = mac_bitget2(_bits);
         if (t[1 + code].b.purgebits)
      break;
         t += t[1 + code].ptr;  /* ptr include 1+code */
         mac_bitget_purge(_bits);
      }
      mac_bitget_purge(t[1 + code].b.purgebits);
      x = t[1 + code].b.x;
      y = t[1 + code].b.y;
      if (x)
         if (mac_bitget_1bit())
      x = -x;
      if (y)
         if (mac_bitget_1bit())
      y = -y;
      xy[i][0] = x;
      xy[i][1] = y;
      if (bitdat.bs_ptr > bitdat.bs_ptr_end)
         break;   // bad data protect

   }
   return;
/*------------------------------------------*/
      case have_linbits:
   for (i = 0; i < n; i++)
   {
      t = t0;
      for (;;)
      {
         _bits = t[0].b.signbits;
         code = bitget2(_bits);
         if (t[1 + code].b.purgebits)
      break;
         t += t[1 + code].ptr;  /* ptr includes 1+code */
         mac_bitget_purge(_bits);
      }
      mac_bitget_purge(t[1 + code].b.purgebits);
      x = t[1 + code].b.x;
      y = t[1 + code].b.y;
      if (x == 15)
         x += bitget_lb(linbits);
      if (x)
         if (mac_bitget_1bit())
      x = -x;
      if (y == 15)
         y += bitget_lb(linbits);
      if (y)
         if (mac_bitget_1bit())
      y = -y;
      xy[i][0] = x;
      xy[i][1] = y;
      if (bitdat.bs_ptr > bitdat.bs_ptr_end)
         break;   // bad data protect

   }
   return;
   }
/*--- end switch ---*/

}
/*==========================================================*/
int unpack_huff_quad(int vwxy[][4], int n, int nbits, int ntable)
{
   int i;
   int code;
   int x, y, v, w;
   int tmp;
   int i_non_zero, tmp_nz;

   tmp_nz = 15;
   i_non_zero = -1;

   n = n >> 2;      /* huff in quads */

   if (ntable)
      goto case_quad_b;

/* case_quad_a: */
   for (i = 0; i < n; i++)
   {
      if (nbits <= 0)
   break;
      mac_bitget_check(10);
      code = mac_bitget2(6);
      nbits -= quad_table_a[code][0];
      mac_bitget_purge(quad_table_a[code][0]);
      tmp = quad_table_a[code][1];
      if (tmp)
      {
   i_non_zero = i;
   tmp_nz = tmp;
      }
      v = (tmp >> 3) & 1;
      w = (tmp >> 2) & 1;
      x = (tmp >> 1) & 1;
      y = tmp & 1;
      if (v)
      {
   if (mac_bitget_1bit())
      v = -v;
   nbits--;
      }
      if (w)
      {
   if (mac_bitget_1bit())
      w = -w;
   nbits--;
      }
      if (x)
      {
   if (mac_bitget_1bit())
      x = -x;
   nbits--;
      }
      if (y)
      {
   if (mac_bitget_1bit())
      y = -y;
   nbits--;
      }
      vwxy[i][0] = v;
      vwxy[i][1] = w;
      vwxy[i][2] = x;
      vwxy[i][3] = y;
      if (bitdat.bs_ptr > bitdat.bs_ptr_end)
   break;     // bad data protect

   }
   if (i && nbits < 0)
   {
      i--;
      vwxy[i][0] = 0;
      vwxy[i][1] = 0;
      vwxy[i][2] = 0;
      vwxy[i][3] = 0;
   }

   i_non_zero = (i_non_zero + 1) << 2;

   if ((tmp_nz & 3) == 0)
      i_non_zero -= 2;

   return i_non_zero;

/*--------------------*/
 case_quad_b:
   for (i = 0; i < n; i++)
   {
      if (nbits < 4)
    break;
      nbits -= 4;
      mac_bitget_check(8);
      tmp = mac_bitget(4) ^ 15; /* one's complement of bitstream */
      if (tmp)
      {
   i_non_zero = i;
   tmp_nz = tmp;
      }
      v = (tmp >> 3) & 1;
      w = (tmp >> 2) & 1;
      x = (tmp >> 1) & 1;
      y = tmp & 1;
      if (v)
      {
   if (mac_bitget_1bit())
      v = -v;
   nbits--;
      }
      if (w)
      {
   if (mac_bitget_1bit())
      w = -w;
   nbits--;
      }
      if (x)
      {
   if (mac_bitget_1bit())
      x = -x;
   nbits--;
      }
      if (y)
      {
   if (mac_bitget_1bit())
      y = -y;
   nbits--;
      }
      vwxy[i][0] = v;
      vwxy[i][1] = w;
      vwxy[i][2] = x;
      vwxy[i][3] = y;
      if (bitdat.bs_ptr > bitdat.bs_ptr_end)
   break;     // bad data protect

   }
   if (nbits < 0)
   {
      i--;
      vwxy[i][0] = 0;
      vwxy[i][1] = 0;
      vwxy[i][2] = 0;
      vwxy[i][3] = 0;
   }

   i_non_zero = (i_non_zero + 1) << 2;

   if ((tmp_nz & 3) == 0)
      i_non_zero -= 2;

   return i_non_zero;   /* return non-zero sample (to nearest pair) */

}
/*-----------------------------------------------------*/

/********************************************************************************/
/*                                                                              */
/*  upsf.c                                                                      */
/*                                                                              */
/********************************************************************************/

//#include "L3.h"

//extern int iframe;

unsigned int bitget(int n);

/*------------------------------------------------------------*/
static const int slen_table[16][2] =
{
  {0, 0}, {0, 1},
  {0, 2}, {0, 3},
  {3, 0}, {1, 1},
  {1, 2}, {1, 3},
  {2, 1}, {2, 2},
  {2, 3}, {3, 1},
  {3, 2}, {3, 3},
  {4, 2}, {4, 3},
};

/* nr_table[size+3*is_right][block type 0,1,3  2, 2+mixed][4]  */
/* for bt=2 nr is count for group of 3 */
static const int nr_table[6][3][4] =
{
 {{6, 5, 5, 5},
  {3, 3, 3, 3},
  {6, 3, 3, 3}},

 {{6, 5, 7, 3},
  {3, 3, 4, 2},
  {6, 3, 4, 2}},

 {{11, 10, 0, 0},
  {6, 6, 0, 0},
  {6, 3, 6, 0}},      /* adjusted *//* 15, 18, 0, 0,   */
/*-intensity stereo right chan--*/
 {{7, 7, 7, 0},
  {4, 4, 4, 0},
  {6, 5, 4, 0}},

 {{6, 6, 6, 3},
  {4, 3, 3, 2},
  {6, 4, 3, 2}},

 {{8, 8, 5, 0},
  {5, 4, 3, 0},
  {6, 6, 3, 0}},
};

/*=============================================================*/
void unpack_sf_sub_MPEG1(SCALEFACT sf[],
       GR * grdat,
       int scfsi, /* bit flag */
       int gr)
{
   int sfb;
   int slen0, slen1;
   int block_type, mixed_block_flag, scalefac_compress;


   block_type = grdat->block_type;
   mixed_block_flag = grdat->mixed_block_flag;
   scalefac_compress = grdat->scalefac_compress;

   slen0 = slen_table[scalefac_compress][0];
   slen1 = slen_table[scalefac_compress][1];


   if (block_type == 2)
   {
      if (mixed_block_flag)
      {       /* mixed */
   for (sfb = 0; sfb < 8; sfb++)
      sf[0].l[sfb] = bitget(slen0);
   for (sfb = 3; sfb < 6; sfb++)
   {
      sf[0].s[0][sfb] = bitget(slen0);
      sf[0].s[1][sfb] = bitget(slen0);
      sf[0].s[2][sfb] = bitget(slen0);
   }
   for (sfb = 6; sfb < 12; sfb++)
   {
      sf[0].s[0][sfb] = bitget(slen1);
      sf[0].s[1][sfb] = bitget(slen1);
      sf[0].s[2][sfb] = bitget(slen1);
   }
   return;
      }
      for (sfb = 0; sfb < 6; sfb++)
      {
   sf[0].s[0][sfb] = bitget(slen0);
   sf[0].s[1][sfb] = bitget(slen0);
   sf[0].s[2][sfb] = bitget(slen0);
      }
      for (; sfb < 12; sfb++)
      {
   sf[0].s[0][sfb] = bitget(slen1);
   sf[0].s[1][sfb] = bitget(slen1);
   sf[0].s[2][sfb] = bitget(slen1);
      }
      return;
   }

/* long blocks types 0 1 3, first granule */
   if (gr == 0)
   {
      for (sfb = 0; sfb < 11; sfb++)
   sf[0].l[sfb] = bitget(slen0);
      for (; sfb < 21; sfb++)
   sf[0].l[sfb] = bitget(slen1);
      return;
   }

/* long blocks 0, 1, 3, second granule */
   sfb = 0;
   if (scfsi & 8)
      for (; sfb < 6; sfb++)
   sf[0].l[sfb] = sf[-2].l[sfb];
   else
      for (; sfb < 6; sfb++)
   sf[0].l[sfb] = bitget(slen0);
   if (scfsi & 4)
      for (; sfb < 11; sfb++)
   sf[0].l[sfb] = sf[-2].l[sfb];
   else
      for (; sfb < 11; sfb++)
   sf[0].l[sfb] = bitget(slen0);
   if (scfsi & 2)
      for (; sfb < 16; sfb++)
   sf[0].l[sfb] = sf[-2].l[sfb];
   else
      for (; sfb < 16; sfb++)
   sf[0].l[sfb] = bitget(slen1);
   if (scfsi & 1)
      for (; sfb < 21; sfb++)
   sf[0].l[sfb] = sf[-2].l[sfb];
   else
      for (; sfb < 21; sfb++)
   sf[0].l[sfb] = bitget(slen1);



   return;
}
/*=============================================================*/
void unpack_sf_sub_MPEG2(SCALEFACT sf[],
       GR * grdat,
       int is_and_ch, IS_SF_INFO * sf_info)
{
   int sfb;
   int slen1, slen2, slen3, slen4;
   int nr1, nr2, nr3, nr4;
   int i, k;
   int preflag, intensity_scale;
   int block_type, mixed_block_flag, scalefac_compress;


   block_type = grdat->block_type;
   mixed_block_flag = grdat->mixed_block_flag;
   scalefac_compress = grdat->scalefac_compress;

   preflag = 0;
   intensity_scale = 0;   /* to avoid compiler warning */
   if (is_and_ch == 0)
   {
      if (scalefac_compress < 400)
      {
   slen2 = scalefac_compress >> 4;
   slen1 = slen2 / 5;
   slen2 = slen2 % 5;
   slen4 = scalefac_compress & 15;
   slen3 = slen4 >> 2;
   slen4 = slen4 & 3;
   k = 0;
      }
      else if (scalefac_compress < 500)
      {
   scalefac_compress -= 400;
   slen2 = scalefac_compress >> 2;
   slen1 = slen2 / 5;
   slen2 = slen2 % 5;
   slen3 = scalefac_compress & 3;
   slen4 = 0;
   k = 1;
      }
      else
      {
   scalefac_compress -= 500;
   slen1 = scalefac_compress / 3;
   slen2 = scalefac_compress % 3;
   slen3 = slen4 = 0;
   if (mixed_block_flag)
   {
      slen3 = slen2;  /* adjust for long/short mix logic */
      slen2 = slen1;
   }
   preflag = 1;
   k = 2;
      }
   }
   else
   {        /* intensity stereo ch = 1 (right) */
      intensity_scale = scalefac_compress & 1;
      scalefac_compress >>= 1;
      if (scalefac_compress < 180)
      {
   slen1 = scalefac_compress / 36;
   slen2 = scalefac_compress % 36;
   slen3 = slen2 % 6;
   slen2 = slen2 / 6;
   slen4 = 0;
   k = 3 + 0;
      }
      else if (scalefac_compress < 244)
      {
   scalefac_compress -= 180;
   slen3 = scalefac_compress & 3;
   scalefac_compress >>= 2;
   slen2 = scalefac_compress & 3;
   slen1 = scalefac_compress >> 2;
   slen4 = 0;
   k = 3 + 1;
      }
      else
      {
   scalefac_compress -= 244;
   slen1 = scalefac_compress / 3;
   slen2 = scalefac_compress % 3;
   slen3 = slen4 = 0;
   k = 3 + 2;
      }
   }

   i = 0;
   if (block_type == 2)
      i = (mixed_block_flag & 1) + 1;
   nr1 = nr_table[k][i][0];
   nr2 = nr_table[k][i][1];
   nr3 = nr_table[k][i][2];
   nr4 = nr_table[k][i][3];


/* return is scale factor info (for right chan is mode) */
   if (is_and_ch)
   {
      sf_info->nr[0] = nr1;
      sf_info->nr[1] = nr2;
      sf_info->nr[2] = nr3;
      sf_info->slen[0] = slen1;
      sf_info->slen[1] = slen2;
      sf_info->slen[2] = slen3;
      sf_info->intensity_scale = intensity_scale;
   }
   grdat->preflag = preflag;  /* return preflag */

/*--------------------------------------*/
   if (block_type == 2)
   {
      if (mixed_block_flag)
      {       /* mixed */
   if (slen1 != 0)  /* long block portion */
      for (sfb = 0; sfb < 6; sfb++)
         sf[0].l[sfb] = bitget(slen1);
   else
      for (sfb = 0; sfb < 6; sfb++)
         sf[0].l[sfb] = 0;
   sfb = 3;   /* start sfb for short */
      }
      else
      {       /* all short, initial short blocks */
   sfb = 0;
   if (slen1 != 0)
      for (i = 0; i < nr1; i++, sfb++)
      {
         sf[0].s[0][sfb] = bitget(slen1);
         sf[0].s[1][sfb] = bitget(slen1);
         sf[0].s[2][sfb] = bitget(slen1);
      }
   else
      for (i = 0; i < nr1; i++, sfb++)
      {
         sf[0].s[0][sfb] = 0;
         sf[0].s[1][sfb] = 0;
         sf[0].s[2][sfb] = 0;
      }
      }
/* remaining short blocks */
      if (slen2 != 0)
   for (i = 0; i < nr2; i++, sfb++)
   {
      sf[0].s[0][sfb] = bitget(slen2);
      sf[0].s[1][sfb] = bitget(slen2);
      sf[0].s[2][sfb] = bitget(slen2);
   }
      else
   for (i = 0; i < nr2; i++, sfb++)
   {
      sf[0].s[0][sfb] = 0;
      sf[0].s[1][sfb] = 0;
      sf[0].s[2][sfb] = 0;
   }
      if (slen3 != 0)
   for (i = 0; i < nr3; i++, sfb++)
   {
      sf[0].s[0][sfb] = bitget(slen3);
      sf[0].s[1][sfb] = bitget(slen3);
      sf[0].s[2][sfb] = bitget(slen3);
   }
      else
   for (i = 0; i < nr3; i++, sfb++)
   {
      sf[0].s[0][sfb] = 0;
      sf[0].s[1][sfb] = 0;
      sf[0].s[2][sfb] = 0;
   }
      if (slen4 != 0)
   for (i = 0; i < nr4; i++, sfb++)
   {
      sf[0].s[0][sfb] = bitget(slen4);
      sf[0].s[1][sfb] = bitget(slen4);
      sf[0].s[2][sfb] = bitget(slen4);
   }
      else
   for (i = 0; i < nr4; i++, sfb++)
   {
      sf[0].s[0][sfb] = 0;
      sf[0].s[1][sfb] = 0;
      sf[0].s[2][sfb] = 0;
   }
      return;
   }


/* long blocks types 0 1 3 */
   sfb = 0;
   if (slen1 != 0)
      for (i = 0; i < nr1; i++, sfb++)
   sf[0].l[sfb] = bitget(slen1);
   else
      for (i = 0; i < nr1; i++, sfb++)
   sf[0].l[sfb] = 0;

   if (slen2 != 0)
      for (i = 0; i < nr2; i++, sfb++)
   sf[0].l[sfb] = bitget(slen2);
   else
      for (i = 0; i < nr2; i++, sfb++)
   sf[0].l[sfb] = 0;

   if (slen3 != 0)
      for (i = 0; i < nr3; i++, sfb++)
   sf[0].l[sfb] = bitget(slen3);
   else
      for (i = 0; i < nr3; i++, sfb++)
   sf[0].l[sfb] = 0;

   if (slen4 != 0)
      for (i = 0; i < nr4; i++, sfb++)
   sf[0].l[sfb] = bitget(slen4);
   else
      for (i = 0; i < nr4; i++, sfb++)
   sf[0].l[sfb] = 0;


}
/*-------------------------------------------------*/

/********************************************************************************/
/*                                                                              */
/*  msis.c                                                                      */
/*                                                                              */
/********************************************************************************/

/****  msis.c  ***************************************************
  Layer III  
 antialias, ms and is stereo precessing

**** is_process assumes never switch 
      from short to long in is region *****
     
is_process does ms or stereo in "forbidded sf regions"
    //ms_mode = 0 
    lr[0][i][0] = 1.0f;
    lr[0][i][1] = 0.0f;
    // ms_mode = 1, in is bands is routine does ms processing 
    lr[1][i][0] = 1.0f;
    lr[1][i][1] = 1.0f;

******************************************************************/


typedef float ARRAY2[2];
typedef float ARRAY8_2[8][2];

float csa[8][2];    /* antialias */   // effectively constant

/* pMP3Stream->nBand[0] = long, pMP3Stream->nBand[1] = short */
////@@@@extern int pMP3Stream->nBand[2][22];
////@@@@extern int pMP3Stream->sfBandIndex[2][22];  /* [long/short][cb] */

/* intensity stereo */
/* if ms mode quant pre-scales all values by 1.0/sqrt(2.0) ms_mode in table
   compensates   */
static float lr[2][8][2]; /* [ms_mode 0/1][sf][left/right]  */  // effectively constant


/* intensity stereo MPEG2 */
/* lr2[intensity_scale][ms_mode][sflen_offset+sf][left/right] */
typedef float ARRAY2_64_2[2][64][2];
typedef float ARRAY64_2[64][2];
static float lr2[2][2][64][2];    // effectively constant


/*===============================================================*/
ARRAY2 *alias_init_addr()
{
   return csa;
}
/*-----------------------------------------------------------*/
ARRAY8_2 *msis_init_addr()
{
/*-------
pi = 4.0*atan(1.0);
t = pi/12.0;
for(i=0;i<7;i++) {
    s = sin(i*t);
    c = cos(i*t);
    // ms_mode = 0 
    lr[0][i][0] = (float)(s/(s+c));
    lr[0][i][1] = (float)(c/(s+c));
    // ms_mode = 1 
    lr[1][i][0] = (float)(sqrt(2.0)*(s/(s+c)));
    lr[1][i][1] = (float)(sqrt(2.0)*(c/(s+c)));
}
//sf = 7 
//ms_mode = 0 
lr[0][i][0] = 1.0f;
lr[0][i][1] = 0.0f;
// ms_mode = 1, in is bands is routine does ms processing 
lr[1][i][0] = 1.0f;
lr[1][i][1] = 1.0f;
------------*/

   return lr;
}
/*-------------------------------------------------------------*/
ARRAY2_64_2 *msis_init_addr_MPEG2()
{
   return lr2;
}
/*===============================================================*/
void antialias(float x[], int n)
{
   int i, k;
   float a, b;

   for (k = 0; k < n; k++)
   {
      for (i = 0; i < 8; i++)
      {
   a = x[17 - i];
   b = x[18 + i];
   x[17 - i] = a * csa[i][0] - b * csa[i][1];
   x[18 + i] = b * csa[i][0] + a * csa[i][1];
      }
      x += 18;
   }
}
/*===============================================================*/
void ms_process(float x[][1152], int n)   /* sum-difference stereo */
{
   int i;
   float xl, xr;

/*-- note: sqrt(2) done scaling by dequant ---*/
   for (i = 0; i < n; i++)
   {
      xl = x[0][i] + x[1][i];
      xr = x[0][i] - x[1][i];
      x[0][i] = xl;
      x[1][i] = xr;
   }
   return;
}
/*===============================================================*/
void is_process_MPEG1(float x[][1152],  /* intensity stereo */
          SCALEFACT * sf,
          CB_INFO cb_info[2], /* [ch] */
          int nsamp, int ms_mode)
{
   int i, j, n, cb, w;
   float fl, fr;
   int m;
   int isf;
   float fls[3], frs[3];
   int cb0;


   cb0 = cb_info[1].cbmax;  /* start at end of right */
   i = pMP3Stream->sfBandIndex[cb_info[1].cbtype][cb0];
   cb0++;
   m = nsamp - i;   /* process to len of left */

   if (cb_info[1].cbtype)
      goto short_blocks;
/*------------------------*/
/* long_blocks: */
   for (cb = cb0; cb < 21; cb++)
   {
      isf = sf->l[cb];
      n = pMP3Stream->nBand[0][cb];
      fl = lr[ms_mode][isf][0];
      fr = lr[ms_mode][isf][1];
      for (j = 0; j < n; j++, i++)
      {
   if (--m < 0)
      goto exit;
   x[1][i] = fr * x[0][i];
   x[0][i] = fl * x[0][i];
      }
   }
   return;
/*------------------------*/
 short_blocks:
   for (cb = cb0; cb < 12; cb++)
   {
      for (w = 0; w < 3; w++)
      {
   isf = sf->s[w][cb];
   fls[w] = lr[ms_mode][isf][0];
   frs[w] = lr[ms_mode][isf][1];
      }
      n = pMP3Stream->nBand[1][cb];
      for (j = 0; j < n; j++)
      {
   m -= 3;
   if (m < 0)
      goto exit;
   x[1][i] = frs[0] * x[0][i];
   x[0][i] = fls[0] * x[0][i];
   x[1][1 + i] = frs[1] * x[0][1 + i];
   x[0][1 + i] = fls[1] * x[0][1 + i];
   x[1][2 + i] = frs[2] * x[0][2 + i];
   x[0][2 + i] = fls[2] * x[0][2 + i];
   i += 3;
      }
   }

 exit:
   return;
}
/*===============================================================*/
void is_process_MPEG2(float x[][1152],  /* intensity stereo */
          SCALEFACT * sf,
          CB_INFO cb_info[2], /* [ch] */
          IS_SF_INFO * is_sf_info,
          int nsamp, int ms_mode)
{
   int i, j, k, n, cb, w;
   float fl, fr;
   int m;
   int isf;
   int il[21];
   int tmp;
   int r;
   ARRAY2 *lr;
   int cb0, cb1;

   lr = lr2[is_sf_info->intensity_scale][ms_mode];

   if (cb_info[1].cbtype)
      goto short_blocks;

/*------------------------*/
/* long_blocks: */
   cb0 = cb_info[1].cbmax;  /* start at end of right */
   i = pMP3Stream->sfBandIndex[0][cb0];
   m = nsamp - i;   /* process to len of left */
/* gen sf info */
   for (k = r = 0; r < 3; r++)
   {
      tmp = (1 << is_sf_info->slen[r]) - 1;
      for (j = 0; j < is_sf_info->nr[r]; j++, k++)
   il[k] = tmp;
   }
   for (cb = cb0 + 1; cb < 21; cb++)
   {
      isf = il[cb] + sf->l[cb];
      fl = lr[isf][0];
      fr = lr[isf][1];
      n = pMP3Stream->nBand[0][cb];
      for (j = 0; j < n; j++, i++)
      {
   if (--m < 0)
      goto exit;
   x[1][i] = fr * x[0][i];
   x[0][i] = fl * x[0][i];
      }
   }
   return;
/*------------------------*/
 short_blocks:

   for (k = r = 0; r < 3; r++)
   {
      tmp = (1 << is_sf_info->slen[r]) - 1;
      for (j = 0; j < is_sf_info->nr[r]; j++, k++)
   il[k] = tmp;
   }

   for (w = 0; w < 3; w++)
   {
      cb0 = cb_info[1].cbmax_s[w];  /* start at end of right */
      i = pMP3Stream->sfBandIndex[1][cb0] + w;
      cb1 = cb_info[0].cbmax_s[w];  /* process to end of left */

      for (cb = cb0 + 1; cb <= cb1; cb++)
      {
   isf = il[cb] + sf->s[w][cb];
   fl = lr[isf][0];
   fr = lr[isf][1];
   n = pMP3Stream->nBand[1][cb];
   for (j = 0; j < n; j++)
   {
      x[1][i] = fr * x[0][i];
      x[0][i] = fl * x[0][i];
      i += 3;
   }
      }

   }

 exit:
   return;
}
/*===============================================================*/

/********************************************************************************/
/*                                                                              */
/*  l3dq.c                                                                      */
/*                                                                              */
/********************************************************************************/
/*----------
static struct  {
int l[23];
int s[14];} sfBandTable[3] =   
{{{0,4,8,12,16,20,24,30,36,44,52,62,74,90,110,134,162,196,238,288,342,418,576},
 {0,4,8,12,16,22,30,40,52,66,84,106,136,192}},
{{0,4,8,12,16,20,24,30,36,42,50,60,72,88,106,128,156,190,230,276,330,384,576},
 {0,4,8,12,16,22,28,38,50,64,80,100,126,192}},
{{0,4,8,12,16,20,24,30,36,44,54,66,82,102,126,156,194,240,296,364,448,550,576},
 {0,4,8,12,16,22,30,42,58,78,104,138,180,192}}};
----------*/

/*--------------------------------*/
static const int pretab[2][22] =
{
   {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
   {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 3, 3, 3, 2, 0},
};


////@@@@extern int nBand[2][22];  /* long = nBand[0][i], short = nBand[1][i] */

/* 8 bit plus 2 lookup x = pow(2.0, 0.25*(global_gain-210)) */
/* two extra slots to do 1/sqrt(2) scaling for ms */
/* 4 extra slots to do 1/2 scaling for cvt to mono */
static float look_global[256 + 2 + 4];    // effectively constant

/*-------- scaling lookup
x = pow(2.0, -0.5*(1+scalefact_scale)*scalefac + preemp)
look_scale[scalefact_scale][preemp][scalefac]
-----------------------*/
static float look_scale[2][4][32];      // effectively constant
typedef float LS[4][32];


/*--- iSample**(4/3) lookup, -32<=i<=31 ---*/
#define ISMAX 32
static float look_pow[2 * ISMAX];     // effectively constant

/*-- pow(2.0, -0.25*8.0*subblock_gain) --*/
static float look_subblock[8];        // effectively constant

/*-- reorder buffer ---*/
static float re_buf[192][3];        // used by dequant() below, but only during func (as workspace)
typedef float ARRAY3[3];


/*=============================================================*/
float *quant_init_global_addr()
{
   return look_global;
}
/*-------------------------------------------------------------*/
LS *quant_init_scale_addr()
{
   return look_scale;
}
/*-------------------------------------------------------------*/
float *quant_init_pow_addr()
{
   return look_pow;
}
/*-------------------------------------------------------------*/
float *quant_init_subblock_addr()
{
   return look_subblock;
}
/*=============================================================*/

void dequant(_SAMPLE Sample[], int *nsamp,
       SCALEFACT * sf,
       GR * gr,
       CB_INFO * cb_info, int ncbl_mixed)
{
   int i, j;
   int cb, n, w;
   float x0, xs;
   float xsb[3];
   double tmp;
   int ncbl;
   int cbs0;
   ARRAY3 *buf;     /* short block reorder */
   int nbands;
   int i0;
   int non_zero;
   int cbmax[3];

   nbands = *nsamp;


   ncbl = 22;     /* long block cb end */
   cbs0 = 12;     /* short block cb start */
/* ncbl_mixed = 8 or 6  mpeg1 or 2 */
   if (gr->block_type == 2)
   {
      ncbl = 0;
      cbs0 = 0;
      if (gr->mixed_block_flag)
      {
   ncbl = ncbl_mixed;
   cbs0 = 3;
      }
   }
/* fill in cb_info -- */
   /* This doesn't seem used anywhere...
   cb_info->lb_type = gr->block_type;
   if (gr->block_type == 2)
      cb_info->lb_type;
   */
   cb_info->cbs0 = cbs0;
   cb_info->ncbl = ncbl;

   cbmax[2] = cbmax[1] = cbmax[0] = 0;
/* global gain pre-adjusted by 2 if ms_mode, 0 otherwise */
   x0 = look_global[(2 + 4) + gr->global_gain];
   i = 0;
/*----- long blocks ---*/
   for (cb = 0; cb < ncbl; cb++)
   {
      non_zero = 0;
      xs = x0 * look_scale[gr->scalefac_scale][pretab[gr->preflag][cb]][sf->l[cb]];
      n = pMP3Stream->nBand[0][cb];
      for (j = 0; j < n; j++, i++)
      {
   if (Sample[i].s == 0)
      Sample[i].x = 0.0F;
   else
   {
      non_zero = 1;
      if ((Sample[i].s >= (-ISMAX)) && (Sample[i].s < ISMAX))
         Sample[i].x = xs * look_pow[ISMAX + Sample[i].s];
      else
      {
    float tmpConst = (float)(1.0/3.0);
         tmp = (double) Sample[i].s;
         Sample[i].x = (float) (xs * tmp * pow(fabs(tmp), tmpConst));
      }
   }
      }
      if (non_zero)
   cbmax[0] = cb;
      if (i >= nbands)
   break;
   }

   cb_info->cbmax = cbmax[0];
   cb_info->cbtype = 0;   // type = long

   if (cbs0 >= 12)
      return;
/*---------------------------
block type = 2  short blocks
----------------------------*/
   cbmax[2] = cbmax[1] = cbmax[0] = cbs0;
   i0 = i;      /* save for reorder */
   buf = re_buf;
   for (w = 0; w < 3; w++)
      xsb[w] = x0 * look_subblock[gr->subblock_gain[w]];
   for (cb = cbs0; cb < 13; cb++)
   {
      n = pMP3Stream->nBand[1][cb];
      for (w = 0; w < 3; w++)
      {
   non_zero = 0;
   xs = xsb[w] * look_scale[gr->scalefac_scale][0][sf->s[w][cb]];
   for (j = 0; j < n; j++, i++)
   {
      if (Sample[i].s == 0)
         buf[j][w] = 0.0F;
      else
      {
         non_zero = 1;
         if ((Sample[i].s >= (-ISMAX)) && (Sample[i].s < ISMAX))
      buf[j][w] = xs * look_pow[ISMAX + Sample[i].s];
         else
         {
      float tmpConst = (float)(1.0/3.0);
      tmp = (double) Sample[i].s;
      buf[j][w] = (float) (xs * tmp * pow(fabs(tmp), tmpConst));
         }
      }
   }
   if (non_zero)
      cbmax[w] = cb;
      }
      if (i >= nbands)
   break;
      buf += n;
   }


   memmove(&Sample[i0].x, &re_buf[0][0], sizeof(float) * (i - i0));

   *nsamp = i;      /* update nsamp */
   cb_info->cbmax_s[0] = cbmax[0];
   cb_info->cbmax_s[1] = cbmax[1];
   cb_info->cbmax_s[2] = cbmax[2];
   if (cbmax[1] > cbmax[0])
      cbmax[0] = cbmax[1];
   if (cbmax[2] > cbmax[0])
      cbmax[0] = cbmax[2];

   cb_info->cbmax = cbmax[0];
   cb_info->cbtype = 1;   /* type = short */


   return;
}


/*-------------------------------------------------------------*/


/********************************************************************************/
/*                                                                              */
/*  l3init.c                                                                     */
/*                                                                              */
/********************************************************************************/

/****  tinit.c  ***************************************************
  Layer III  init tables


******************************************************************/
/*---------- quant ---------------------------------*/
/* 8 bit lookup x = pow(2.0, 0.25*(global_gain-210)) */
float *quant_init_global_addr();


/* x = pow(2.0, -0.5*(1+scalefact_scale)*scalefac + preemp) */
typedef float LS[4][32];
LS *quant_init_scale_addr();


float *quant_init_pow_addr();
float *quant_init_subblock_addr();

typedef int iARRAY22[22];
iARRAY22 *quant_init_band_addr();

/*---------- antialias ---------------------------------*/
typedef float PAIR[2];
PAIR *alias_init_addr();

static const float Ci[8] =
{
   -0.6f, -0.535f, -0.33f, -0.185f, -0.095f, -0.041f, -0.0142f, -0.0037f};


void hwin_init();   /* hybrid windows -- */
void imdct_init();


/*typedef struct
{
   float *w;
   float *w2;
   void *coef;
}
IMDCT_INIT_BLOCK;*/

void msis_init();
void msis_init_MPEG2();

/*=============================================================*/
int L3table_init()
{
   int i;
   float *x;
   LS *ls;
   int scalefact_scale, preemp, scalefac;
   double tmp;
   PAIR *csa;

   static int iOnceOnly = 0;

    if (!iOnceOnly++)
  {
/*================ quant ===============================*/

  /* 8 bit plus 2 lookup x = pow(2.0, 0.25*(global_gain-210)) */
  /* extra 2 for ms scaling by 1/sqrt(2) */
  /* extra 4 for cvt to mono scaling by 1/2 */
     x = quant_init_global_addr();
     for (i = 0; i < 256 + 2 + 4; i++)
      x[i] = (float) pow(2.0, 0.25 * ((i - (2 + 4)) - 210 + GLOBAL_GAIN_SCALE));



  /* x = pow(2.0, -0.5*(1+scalefact_scale)*scalefac + preemp) */
     ls = quant_init_scale_addr();
     for (scalefact_scale = 0; scalefact_scale < 2; scalefact_scale++)
     {
      for (preemp = 0; preemp < 4; preemp++)
      {
     for (scalefac = 0; scalefac < 32; scalefac++)
     {
      ls[scalefact_scale][preemp][scalefac] =
         (float) pow(2.0, -0.5 * (1 + scalefact_scale) * (scalefac + preemp));
     }
      }
     }

  /*--- iSample**(4/3) lookup, -32<=i<=31 ---*/
     x = quant_init_pow_addr();
     for (i = 0; i < 64; i++)
     {
      tmp = i - 32;
      x[i] = (float) (tmp * pow(fabs(tmp), (1.0 / 3.0)));
     }


  /*-- pow(2.0, -0.25*8.0*subblock_gain)  3 bits --*/
     x = quant_init_subblock_addr();
     for (i = 0; i < 8; i++)
     {
      x[i] = (float) pow(2.0, 0.25 * -8.0 * i);
     }

  /*-------------------------*/
  // quant_init_sf_band(sr_index);   replaced by code in sup.c


/*================ antialias ===============================*/
     // onceonly!!!!!!!!!!!!!!!!!!!!!
     csa = alias_init_addr();
     for (i = 0; i < 8; i++)
     {
      csa[i][0] = (float) (1.0 / sqrt(1.0 + Ci[i] * Ci[i]));
      csa[i][1] = (float) (Ci[i] / sqrt(1.0 + Ci[i] * Ci[i]));
     }
   }

   // these 4 are iOnceOnly-protected inside...

/*================ msis ===============================*/
   msis_init();
   msis_init_MPEG2();

/*================ imdct ===============================*/
   imdct_init();

/*--- hybrid windows ------------*/
   hwin_init();

   return 0;
}
/*====================================================================*/
typedef float ARRAY36[36];
ARRAY36 *hwin_init_addr();

/*--------------------------------------------------------------------*/
void hwin_init()
{
   int i, j;
   double pi;
   ARRAY36 *win;

   static int iOnceOnly = 0;

   if (!iOnceOnly++)
   {
     win = hwin_init_addr();

     pi = 4.0 * atan(1.0);

  /* type 0 */
     for (i = 0; i < 36; i++)
      win[0][i] = (float) sin(pi / 36 * (i + 0.5));

  /* type 1 */
     for (i = 0; i < 18; i++)
      win[1][i] = (float) sin(pi / 36 * (i + 0.5));
     for (i = 18; i < 24; i++)
      win[1][i] = 1.0F;
     for (i = 24; i < 30; i++)
      win[1][i] = (float) sin(pi / 12 * (i + 0.5 - 18));
     for (i = 30; i < 36; i++)
      win[1][i] = 0.0F;

  /* type 3 */
     for (i = 0; i < 6; i++)
      win[3][i] = 0.0F;
     for (i = 6; i < 12; i++)
      win[3][i] = (float) sin(pi / 12 * (i + 0.5 - 6));
     for (i = 12; i < 18; i++)
      win[3][i] = 1.0F;
     for (i = 18; i < 36; i++)
      win[3][i] = (float) sin(pi / 36 * (i + 0.5));

  /* type 2 */
     for (i = 0; i < 12; i++)
      win[2][i] = (float) sin(pi / 12 * (i + 0.5));
     for (i = 12; i < 36; i++)
      win[2][i] = 0.0F;

  /*--- invert signs by region to match mdct 18pt --> 36pt mapping */
     for (j = 0; j < 4; j++)
     {
      if (j == 2)
     continue;
      for (i = 9; i < 36; i++)
     win[j][i] = -win[j][i];
     }

  /*-- invert signs for short blocks --*/
     for (i = 3; i < 12; i++)
      win[2][i] = -win[2][i];
   }   
}
/*=============================================================*/
typedef float ARRAY4[4];
const IMDCT_INIT_BLOCK *imdct_init_addr_18();
const IMDCT_INIT_BLOCK *imdct_init_addr_6();

/*-------------------------------------------------------------*/
void imdct_init()
{
   int k, p, n;
   double t, pi;
   const IMDCT_INIT_BLOCK *addr;
   float *w, *w2;
   float *v, *v2, *coef87;
   ARRAY4 *coef;

   static int iOnceOnly = 0;

   if (!iOnceOnly++)
   {
  /*--- 18 point --*/
     addr = imdct_init_addr_18();
     w = addr->w;
     w2 = addr->w2;
     coef = (float (*)[4]) addr->coef; //CORTEX FIX
  /*----*/
     n = 18;
     pi = 4.0 * atan(1.0);
     t = pi / (4 * n);
     for (p = 0; p < n; p++)
      w[p] = (float) (2.0 * cos(t * (2 * p + 1)));
     for (p = 0; p < 9; p++)
      w2[p] = (float) 2.0 *cos(2 * t * (2 * p + 1));

     t = pi / (2 * n);
     for (k = 0; k < 9; k++)
     {
      for (p = 0; p < 4; p++)
     coef[k][p] = (float) cos(t * (2 * k) * (2 * p + 1));
     }

  /*--- 6 point */
     addr = imdct_init_addr_6();
     v = addr->w;
     v2 = addr->w2;
     coef87 = (float*)addr->coef;
  /*----*/
     n = 6;
     pi = 4.0 * atan(1.0);
     t = pi / (4 * n);
     for (p = 0; p < n; p++)
      v[p] = (float) 2.0 *cos(t * (2 * p + 1));

     for (p = 0; p < 3; p++)
      v2[p] = (float) 2.0 *cos(2 * t * (2 * p + 1));

     t = pi / (2 * n);
     k = 1;
     p = 0;
     *coef87 = (float) cos(t * (2 * k) * (2 * p + 1));
  /* adjust scaling to save a few mults */
     for (p = 0; p < 6; p++)
      v[p] = v[p] / 2.0f;
     *coef87 = (float) 2.0 *(*coef87);

   }   
}
/*===============================================================*/
typedef float ARRAY8_2[8][2];
ARRAY8_2 *msis_init_addr();

/*-------------------------------------------------------------*/
void msis_init()
{
   int i;
   double s, c;
   double pi;
   double t;
   ARRAY8_2 *lr;

   static int iOnceOnly = 0;

   if (!iOnceOnly++)
   {
     lr = msis_init_addr();


     pi = 4.0 * atan(1.0);
     t = pi / 12.0;
     for (i = 0; i < 7; i++)
     {
      s = sin(i * t);
      c = cos(i * t);
    /* ms_mode = 0 */
      lr[0][i][0] = (float) (s / (s + c));
      lr[0][i][1] = (float) (c / (s + c));
    /* ms_mode = 1 */
      lr[1][i][0] = (float) (sqrt(2.0) * (s / (s + c)));
      lr[1][i][1] = (float) (sqrt(2.0) * (c / (s + c)));
     }
  /* sf = 7 */
  /* ms_mode = 0 */
     lr[0][i][0] = 1.0f;
     lr[0][i][1] = 0.0f;
  /* ms_mode = 1, in is bands is routine does ms processing */
     lr[1][i][0] = 1.0f;
     lr[1][i][1] = 1.0f;


  /*-------
  for(i=0;i<21;i++) nBand[0][i] = 
        sfBandTable[sr_index].l[i+1] - sfBandTable[sr_index].l[i];
  for(i=0;i<12;i++) nBand[1][i] = 
        sfBandTable[sr_index].s[i+1] - sfBandTable[sr_index].s[i];
  -------------*/
   }
}
/*-------------------------------------------------------------*/
/*===============================================================*/
typedef float ARRAY2_64_2[2][64][2];
ARRAY2_64_2 *msis_init_addr_MPEG2();

/*-------------------------------------------------------------*/
void msis_init_MPEG2()
{
   int k, n;
   double t;
   ARRAY2_64_2 *lr2;
   int intensity_scale, ms_mode, sf, sflen;
   float ms_factor[2];

   static int iOnceOnly = 0;

   if (!iOnceOnly++)
   {
     ms_factor[0] = 1.0;
     ms_factor[1] = (float) sqrt(2.0);

     lr2 = msis_init_addr_MPEG2();

  /* intensity stereo MPEG2 */
  /* lr2[intensity_scale][ms_mode][sflen_offset+sf][left/right] */

     for (intensity_scale = 0; intensity_scale < 2; intensity_scale++)
     {
      t = pow(2.0, -0.25 * (1 + intensity_scale));
      for (ms_mode = 0; ms_mode < 2; ms_mode++)
      {

     n = 1;
     k = 0;
     for (sflen = 0; sflen < 6; sflen++)
     {
      for (sf = 0; sf < (n - 1); sf++, k++)
      {
         if (sf == 0)
         {
        lr2[intensity_scale][ms_mode][k][0] = ms_factor[ms_mode] * 1.0f;
        lr2[intensity_scale][ms_mode][k][1] = ms_factor[ms_mode] * 1.0f;
         }
         else if ((sf & 1))
         {
        lr2[intensity_scale][ms_mode][k][0] =
         (float) (ms_factor[ms_mode] * pow(t, (sf + 1) / 2));
        lr2[intensity_scale][ms_mode][k][1] = ms_factor[ms_mode] * 1.0f;
         }
         else
         {
        lr2[intensity_scale][ms_mode][k][0] = ms_factor[ms_mode] * 1.0f;
        lr2[intensity_scale][ms_mode][k][1] =
         (float) (ms_factor[ms_mode] * pow(t, sf / 2));
         }
      }

      /* illegal is_pos used to do ms processing */
      if (ms_mode == 0)
      {     /* ms_mode = 0 */
         lr2[intensity_scale][ms_mode][k][0] = 1.0f;
         lr2[intensity_scale][ms_mode][k][1] = 0.0f;
      }
      else
      {
       /* ms_mode = 1, in is bands is routine does ms processing */
         lr2[intensity_scale][ms_mode][k][0] = 1.0f;
         lr2[intensity_scale][ms_mode][k][1] = 1.0f;
      }
      k++;
      n = n + n;
     }
      }
     }
   }

}
/*-------------------------------------------------------------*/

/********************************************************************************/
/*                                                                              */
/*  cupl3.c                                                                     */
/*                                                                              */
/********************************************************************************/

/*
#include "L3.h"
#include "jdw.h"

#include "mp3struct.h"*/

/*====================================================================*/
/*static const int mp_sr20_table[2][4] =
{{441, 480, 320, -999}, {882, 960, 640, -999}};
static const int mp_br_tableL3[2][16] =
{{0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160, 0}, // mpeg 2 
 {0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 0}};
*/
/*====================================================================*/

/*-- global band tables */
/*-- short portion is 3*x !! --*/
////@@@@int nBand[2][22];   /* [long/short][cb] */    
////@@@@int sfBandIndex[2][22];   /* [long/short][cb] */  

/*====================================================================*/

/*----------------*/
extern DEC_INFO decinfo;  ////@@@@ this is ok, only written to during init, then chucked.

/*----------------*/
////@@@@static int pMP3Stream->mpeg25_flag;   // L3 only

//int iframe;

/*-------*/
////@@@@static int pMP3Stream->band_limit = (576);    // L3 only
////@@@@static int pMP3Stream->band_limit21 = (576);  // limit for sf band 21 // L3 only
////@@@@static int pMP3Stream->band_limit12 = (576);  // limit for sf band 12 short //L3 only

////@@@@int band_limit_nsb = 32;  /* global for hybrid */ 
////@@@@static int pMP3Stream->nsb_limit = 32;  
////@@@@static int pMP3Stream->gain_adjust = 0; /* adjust gain e.g. cvt to mono */  // L3 only
////@@@@static int id; // L3 only
////@@@@static int pMP3Stream->ncbl_mixed;    /* number of long cb's in mixed block 8 or 6 */   // L3 only
////@@@@static int pMP3Stream->sr_index;  // L3 only (99%)

//@@@@
////@@@@static int pMP3Stream->outvalues;   //
////@@@@static int pMP3Stream->outbytes;    //
////@@@@static int pMP3Stream->half_outbytes; // L3 only
////@@@@static int pMP3Stream->framebytes;    //

//static int padframebytes;
////@@@@static int pMP3Stream->crcbytes;    // L3 only
////@@@@static int pMP3Stream->pad;       //
//static int stereo_flag; // only written to
////@@@@static int pMP3Stream->nchan;     // L3 only
////@@@@static int pMP3Stream->ms_mode;     // L3 only (99%)
////@@@@static int pMP3Stream->is_mode;     // L3 only
////@@@@static unsigned int pMP3Stream->zero_level_pcm = 0; // L3 only

/* cb_info[igr][ch], compute by dequant, used by joint */
static CB_INFO cb_info[2][2]; // L3 only  ############ I think this is ok, only a scratchpad?
static IS_SF_INFO is_sf_info; /* MPEG-2 intensity stereo */ // L3 only   ############## scratchpad?

/*---------------------------------*/
//!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
/* main data bit buffer */
/*@@@@
#define NBUF (8*1024)
#define BUF_TRIGGER (NBUF-1500)
static unsigned char buf[NBUF];
static int buf_ptr0 = 0;  // !!!!!!!!!!!
static int buf_ptr1 = 0;  // !!!!!!!!!!!
static int main_pos_bit;
*/
/*---------------------------------*/
static SIDE_INFO side_info;   // ####### scratchpad?

static SCALEFACT sf[2][2];  /* [gr][ch] */   // ########## scratchpad?

static int nsamp[2][2];   /* must start = 0, for nsamp[igr_prev] */ // ########## scratchpad?

/*- sample union of int/float  sample[ch][gr][576] */
/* static _SAMPLE sample[2][2][576]; */
// @@@@FINDME
////@@@@extern _SAMPLE sample[2][2][576];            ////////////????? suspicious, mainly used in decode loop, but zeroed init code
static float yout[576];   /* hybrid out, sbt in */  //////////// scratchpad

////@@@@typedef void (*SBT_FUNCTION) (float *sample, short *pcm, int ch);
void sbt_dual_L3(float *sample, short *pcm, int n);
////@@@@static SBT_FUNCTION sbt_L3 = sbt_dual_L3; // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

////@@@@typedef void (*XFORM_FUNCTION) (void *pcm, int igr);
static void Xform_dual(void *pcm, int igr);
////@@@@static XFORM_FUNCTION Xform = Xform_dual; // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

IN_OUT L3audio_decode_MPEG1(unsigned char *bs, unsigned char *pcm);
IN_OUT L3audio_decode_MPEG2(unsigned char *bs, unsigned char *pcm);
////@@@@typedef IN_OUT(*DECODE_FUNCTION) (unsigned char *bs, unsigned char *pcm);
////@@@@static DECODE_FUNCTION decode_function = L3audio_decode_MPEG1;    <------------------ needs streaming, ditto above!!!


/*====================================================================*/
/*int hybrid(void *xin, void *xprev, float *y,
     int btype, int nlong, int ntot, int nprev);
int hybrid_sum(void *xin, void *xin_left, float *y,
         int btype, int nlong, int ntot);
void sum_f_bands(void *a, void *b, int n);
void FreqInvert(float *y, int n);
void antialias(void *x, int n);
void ms_process(void *x, int n);  // sum-difference stereo 
void is_process_MPEG1(void *x,  // intensity stereo 
          SCALEFACT * sf,
          CB_INFO cb_info[2], // [ch] 
          int nsamp, int ms_mode);
void is_process_MPEG2(void *x,  // intensity stereo 
          SCALEFACT * sf,
          CB_INFO cb_info[2], // [ch] 
          IS_SF_INFO * is_sf_info,
          int nsamp, int ms_mode);

void unpack_huff(void *xy, int n, int ntable);
int unpack_huff_quad(void *vwxy, int n, int nbits, int ntable);
void dequant(_SAMPLE __sample[], int *nsamp,
       SCALEFACT * sf,
       GR * gr,
       CB_INFO * cb_info, int ncbl_mixed);
void unpack_sf_sub_MPEG1(SCALEFACT * scalefac, GR * gr,
       int scfsi, // bit flag 
       int igr);
void unpack_sf_sub_MPEG2(SCALEFACT sf[],  // return intensity scale 
       GR * grdat,
       int is_and_ch, IS_SF_INFO * is_sf_info);
*/
/*====================================================================*/
/* get bits from bitstream in endian independent way */

BITDAT bitdat;      /* global for inline use by Huff */   // !!!!!!!!!!!!!!!!!!!

/*------------- initialize bit getter -------------*/
static void bitget_init(unsigned char *buf)
{
   bitdat.bs_ptr0 = bitdat.bs_ptr = buf;
   bitdat._bits = 0;
   bitdat.bitbuf = 0;
}
/*------------- initialize bit getter -------------*/
static void bitget_init_end(unsigned char *buf_end)
{
   bitdat.bs_ptr_end = buf_end;
}
/*------------- get n bits from bitstream -------------*/
int bitget_bits_used()
{
   int n;     /* compute bits used from last init call */

   n = ((bitdat.bs_ptr - bitdat.bs_ptr0) << 3) - bitdat._bits;
   return n;
}
/*------------- check for n bits in bitbuf -------------*/
void bitget_check(int n)
{
   if (bitdat._bits < n)
   {
      while (bitdat._bits <= 24)
      {
   bitdat.bitbuf = (bitdat.bitbuf << 8) | *bitdat.bs_ptr++;
   bitdat._bits += 8;
      }
   }
}
/*------------- get n bits from bitstream -------------*/
unsigned int bitget(int n)
{
   unsigned int x;

   if (bitdat._bits < n)
   {        /* refill bit buf if necessary */
      while (bitdat._bits <= 24)
      {
   bitdat.bitbuf = (bitdat.bitbuf << 8) | *bitdat.bs_ptr++;
   bitdat._bits += 8;
      }
   }
   bitdat._bits -= n;
   x = bitdat.bitbuf >> bitdat._bits;
   bitdat.bitbuf -= x << bitdat._bits;
   return x;
}
/*------------- get 1 bit from bitstream -------------*/
unsigned int bitget_1bit()
{
   unsigned int x;

   if (bitdat._bits <= 0)
   {        /* refill bit buf if necessary */
      while (bitdat._bits <= 24)
      {
   bitdat.bitbuf = (bitdat.bitbuf << 8) | *bitdat.bs_ptr++;
   bitdat._bits += 8;
      }
   }
   bitdat._bits--;
   x = bitdat.bitbuf >> bitdat._bits;
   bitdat.bitbuf -= x << bitdat._bits;
   return x;
}
/*====================================================================*/
static void Xform_mono(void *pcm, int igr)
{
   int igr_prev, n1, n2;

/*--- hybrid + sbt ---*/
   n1 = n2 = nsamp[igr][0]; /* total number bands */
   if (side_info.gr[igr][0].block_type == 2)
   {        /* long bands */
      n1 = 0;
      if (side_info.gr[igr][0].mixed_block_flag)
   n1 = pMP3Stream->sfBandIndex[0][pMP3Stream->ncbl_mixed - 1];
   }
   if (n1 > pMP3Stream->band_limit)
      n1 = pMP3Stream->band_limit;
   if (n2 > pMP3Stream->band_limit)
      n2 = pMP3Stream->band_limit;
   igr_prev = igr ^ 1;

//   nsamp[igr][0] = hybrid(pMP3Stream->__sample[0][igr], pMP3Stream->__sample[0][igr_prev], yout, side_info.gr[igr][0].block_type, n1, n2, nsamp[igr_prev][0]);
   nsamp[igr][0] = hybrid((float*)pMP3Stream->__sample[0][igr], (float*)pMP3Stream->__sample[0][igr_prev], (float (*)[32] )yout, side_info.gr[igr][0].block_type, n1, n2, nsamp[igr_prev][0]);   ///Cortex FiX

//   FreqInvert(yout, nsamp[igr][0]);
   FreqInvert((float (*)[32] )yout, nsamp[igr][0]);

   //pMP3Stream->sbt_L3(yout, pcm, 0);
   pMP3Stream->sbt_L3(yout, (short int *)pcm, 0);  //Cortex FIX

}
/*--------------------------------------------------------------------*/
static void Xform_dual_right(void *pcm, int igr)
{
   int igr_prev, n1, n2;

/*--- hybrid + sbt ---*/
   n1 = n2 = nsamp[igr][1]; /* total number bands */
   if (side_info.gr[igr][1].block_type == 2)
   {        /* long bands */
      n1 = 0;
      if (side_info.gr[igr][1].mixed_block_flag)
   n1 = pMP3Stream->sfBandIndex[0][pMP3Stream->ncbl_mixed - 1];
   }
   if (n1 > pMP3Stream->band_limit)
      n1 = pMP3Stream->band_limit;
   if (n2 > pMP3Stream->band_limit)
      n2 = pMP3Stream->band_limit;
   igr_prev = igr ^ 1;
   //nsamp[igr][1] = hybrid(pMP3Stream->__sample[1][igr], pMP3Stream->__sample[1][igr_prev], yout, side_info.gr[igr][1].block_type, n1, n2, nsamp[igr_prev][1]);
   nsamp[igr][1] = hybrid((float *)pMP3Stream->__sample[1][igr],(float *) pMP3Stream->__sample[1][igr_prev], (float (*)[32] )yout, side_info.gr[igr][1].block_type, n1, n2, nsamp[igr_prev][1]);
   
   

//   FreqInvert(yout, nsamp[igr][1]);
   FreqInvert((float (*)[32] )yout, nsamp[igr][1]);
   
   ///pMP3Stream->sbt_L3(yout, pcm, 0);
   pMP3Stream->sbt_L3(yout, (short int *)pcm, 0);  //Cortex FIX

}
/*--------------------------------------------------------------------*/
static void Xform_dual(void *pcm, int igr)
{
   int ch;
   int igr_prev, n1, n2;

/*--- hybrid + sbt ---*/
   igr_prev = igr ^ 1;
   for (ch = 0; ch < pMP3Stream->nchan; ch++)
   {
      n1 = n2 = nsamp[igr][ch]; /* total number bands */
      if (side_info.gr[igr][ch].block_type == 2)
      {       /* long bands */
   n1 = 0;
   if (side_info.gr[igr][ch].mixed_block_flag)
      n1 = pMP3Stream->sfBandIndex[0][pMP3Stream->ncbl_mixed - 1];
      }
      if (n1 > pMP3Stream->band_limit)
   n1 = pMP3Stream->band_limit;
      if (n2 > pMP3Stream->band_limit)
   n2 = pMP3Stream->band_limit;
//      nsamp[igr][ch] = hybrid(pMP3Stream->__sample[ch][igr], pMP3Stream->__sample[ch][igr_prev], yout, side_info.gr[igr][ch].block_type, n1, n2, nsamp[igr_prev][ch]);
      nsamp[igr][ch] = hybrid((float*)pMP3Stream->__sample[ch][igr], (float*) pMP3Stream->__sample[ch][igr_prev], (float (*)[32] )yout, side_info.gr[igr][ch].block_type, n1, n2, nsamp[igr_prev][ch]);

//   FreqInvert(yout, nsamp[igr][ch]);
   FreqInvert((float (*)[32] )yout, nsamp[igr][ch]);
      
      ///pMP3Stream->sbt_L3(yout, pcm, 0);
      pMP3Stream->sbt_L3(yout, (short int *)pcm, 0);  //Cortex FIX
  }

}
/*--------------------------------------------------------------------*/
static void Xform_dual_mono(void *pcm, int igr)
{
   int igr_prev, n1, n2, n3;

/*--- hybrid + sbt ---*/
   igr_prev = igr ^ 1;
   if ((side_info.gr[igr][0].block_type == side_info.gr[igr][1].block_type)
       && (side_info.gr[igr][0].mixed_block_flag == 0)
       && (side_info.gr[igr][1].mixed_block_flag == 0))
   {

      n2 = nsamp[igr][0]; /* total number bands max of L R */
      if (n2 < nsamp[igr][1])
   n2 = nsamp[igr][1];
      if (n2 > pMP3Stream->band_limit)
   n2 = pMP3Stream->band_limit;
      n1 = n2;      /* n1 = number long bands */
      if (side_info.gr[igr][0].block_type == 2)
   n1 = 0;
      sum_f_bands((float*)pMP3Stream->__sample[0][igr], (float*)pMP3Stream->__sample[1][igr], n2);

      ///n3 = nsamp[igr][0] = hybrid(pMP3Stream->__sample[0][igr], pMP3Stream->__sample[0][igr_prev], yout, side_info.gr[igr][0].block_type, n1, n2, nsamp[igr_prev][0]);  //Cortex FIX
      n3 = nsamp[igr][0] = hybrid((float*)pMP3Stream->__sample[0][igr], (float*)pMP3Stream->__sample[0][igr_prev], (float (*)[32] )yout, side_info.gr[igr][0].block_type, n1, n2, nsamp[igr_prev][0]);
      
   }
   else
   {        /* transform and then sum (not tested - never happens in test) */
/*-- left chan --*/
      n1 = n2 = nsamp[igr][0];  /* total number bands */
      if (side_info.gr[igr][0].block_type == 2)
      {
   n1 = 0;    /* long bands */
   if (side_info.gr[igr][0].mixed_block_flag)
      n1 = pMP3Stream->sfBandIndex[0][pMP3Stream->ncbl_mixed - 1];
      }
      ///n3 = nsamp[igr][0] = hybrid(pMP3Stream->__sample[0][igr], pMP3Stream->__sample[0][igr_prev], yout, side_info.gr[igr][0].block_type, n1, n2, nsamp[igr_prev][0]); //Cortex FIX
      n3 = nsamp[igr][0] = hybrid((float*)pMP3Stream->__sample[0][igr],(float*) pMP3Stream->__sample[0][igr_prev],(float (*)[32] ) yout, side_info.gr[igr][0].block_type, n1, n2, nsamp[igr_prev][0]);


      
/*-- right chan --*/
      n1 = n2 = nsamp[igr][1];  /* total number bands */
      if (side_info.gr[igr][1].block_type == 2)
      {
   n1 = 0;    /* long bands */
   if (side_info.gr[igr][1].mixed_block_flag)
      n1 = pMP3Stream->sfBandIndex[0][pMP3Stream->ncbl_mixed - 1];
      }
      //nsamp[igr][1] = hybrid_sum(pMP3Stream->__sample[1][igr], pMP3Stream->__sample[0][igr], yout, side_info.gr[igr][1].block_type, n1, n2); //Cortex Fix
      nsamp[igr][1] = hybrid_sum((float*)pMP3Stream->__sample[1][igr],(float*) pMP3Stream->__sample[0][igr], (float (*)[32] )yout, side_info.gr[igr][1].block_type, n1, n2); //Cortex Fix
      if (n3 < nsamp[igr][1])
   n1 = nsamp[igr][1];
   }

/*--------*/
   //FreqInvert(yout, n3);
   FreqInvert((float (*)[32] )yout, n3);
   
   //pMP3Stream->sbt_L3(yout, pcm, 0);
   pMP3Stream->sbt_L3(yout, (short int *)pcm, 0);  //Cortex FIX

}
/*--------------------------------------------------------------------*/
/*====================================================================*/
static int unpack_side_MPEG1()
{
   int prot;
   int br_index;
   int igr, ch;
   int side_bytes;

/* decode partial header plus initial side info */
/* at entry bit getter points at id, sync skipped by caller */

   pMP3Stream->id = bitget(1);    /* id */
   bitget(2);     /* skip layer */
   prot = bitget(1);    /* bitget prot bit */
   br_index = bitget(4);
   pMP3Stream->sr_index = bitget(2);
   pMP3Stream->pad = bitget(1);
   bitget(1);     /* skip to mode */
   side_info.mode = bitget(2);  /* mode */
   side_info.mode_ext = bitget(2);  /* mode ext */

   if (side_info.mode != 1)
      side_info.mode_ext = 0;

/* adjust global gain in ms mode to avoid having to mult by 1/sqrt(2) */
   pMP3Stream->ms_mode = side_info.mode_ext >> 1;
   pMP3Stream->is_mode = side_info.mode_ext & 1;


   pMP3Stream->crcbytes = 0;
   if (prot)
      bitget(4);    /* skip to data */
   else
   {
      bitget(20);   /* skip crc */
      pMP3Stream->crcbytes = 2;
   }

   if (br_index > 0)    /* pMP3Stream->framebytes fixed for free format */
  {
      pMP3Stream->framebytes =
   2880 * mp_br_tableL3[pMP3Stream->id][br_index] / mp_sr20_table[pMP3Stream->id][pMP3Stream->sr_index];
   }

   side_info.main_data_begin = bitget(9);
   if (side_info.mode == 3)
   {
      side_info.private_bits = bitget(5);
      pMP3Stream->nchan = 1;
//      stereo_flag = 0;
      side_bytes = (4 + 17);
/*-- with header --*/
   }
   else
   {
      side_info.private_bits = bitget(3);
      pMP3Stream->nchan = 2;
//      stereo_flag = 1;
      side_bytes = (4 + 32);
/*-- with header --*/
   }
   for (ch = 0; ch < pMP3Stream->nchan; ch++)
      side_info.scfsi[ch] = bitget(4);
/* this always 0 (both igr) for short blocks */

   for (igr = 0; igr < 2; igr++)
   {
      for (ch = 0; ch < pMP3Stream->nchan; ch++)
      {
   side_info.gr[igr][ch].part2_3_length = bitget(12);
   side_info.gr[igr][ch].big_values = bitget(9);
   side_info.gr[igr][ch].global_gain = bitget(8) + pMP3Stream->gain_adjust;
   if (pMP3Stream->ms_mode)
      side_info.gr[igr][ch].global_gain -= 2;
   side_info.gr[igr][ch].scalefac_compress = bitget(4);
   side_info.gr[igr][ch].window_switching_flag = bitget(1);
   if (side_info.gr[igr][ch].window_switching_flag)
   {
      side_info.gr[igr][ch].block_type = bitget(2);
      side_info.gr[igr][ch].mixed_block_flag = bitget(1);
      side_info.gr[igr][ch].table_select[0] = bitget(5);
      side_info.gr[igr][ch].table_select[1] = bitget(5);
      side_info.gr[igr][ch].subblock_gain[0] = bitget(3);
      side_info.gr[igr][ch].subblock_gain[1] = bitget(3);
      side_info.gr[igr][ch].subblock_gain[2] = bitget(3);
    /* region count set in terms of long block cb's/bands */
    /* r1 set so r0+r1+1 = 21 (lookup produces 576 bands ) */
    /* if(window_switching_flag) always 36 samples in region0 */
      side_info.gr[igr][ch].region0_count = (8 - 1);  /* 36 samples */
      side_info.gr[igr][ch].region1_count = 20 - (8 - 1);
   }
   else
   {
      side_info.gr[igr][ch].mixed_block_flag = 0;
      side_info.gr[igr][ch].block_type = 0;
      side_info.gr[igr][ch].table_select[0] = bitget(5);
      side_info.gr[igr][ch].table_select[1] = bitget(5);
      side_info.gr[igr][ch].table_select[2] = bitget(5);
      side_info.gr[igr][ch].region0_count = bitget(4);
      side_info.gr[igr][ch].region1_count = bitget(3);
   }
   side_info.gr[igr][ch].preflag = bitget(1);
   side_info.gr[igr][ch].scalefac_scale = bitget(1);
   side_info.gr[igr][ch].count1table_select = bitget(1);
      }
   }



/* return  bytes in header + side info */
   return side_bytes;
}
/*====================================================================*/
static int unpack_side_MPEG2(int igr)
{
   int prot;
   int br_index;
   int ch;
   int side_bytes;

/* decode partial header plus initial side info */
/* at entry bit getter points at id, sync skipped by caller */

   pMP3Stream->id = bitget(1);    /* id */
   bitget(2);     /* skip layer */
   prot = bitget(1);    /* bitget prot bit */
   br_index = bitget(4);
   pMP3Stream->sr_index = bitget(2);
   pMP3Stream->pad = bitget(1);
   bitget(1);     /* skip to mode */
   side_info.mode = bitget(2);  /* mode */
   side_info.mode_ext = bitget(2);  /* mode ext */

   if (side_info.mode != 1)
      side_info.mode_ext = 0;

/* adjust global gain in ms mode to avoid having to mult by 1/sqrt(2) */
   pMP3Stream->ms_mode = side_info.mode_ext >> 1;
   pMP3Stream->is_mode = side_info.mode_ext & 1;

   pMP3Stream->crcbytes = 0;
   if (prot)
      bitget(4);    /* skip to data */
   else
   {
      bitget(20);   /* skip crc */
      pMP3Stream->crcbytes = 2;
   }

   if (br_index > 0)
   {        /* pMP3Stream->framebytes fixed for free format */
      if (pMP3Stream->mpeg25_flag == 0)
      {
   pMP3Stream->framebytes =
      1440 * mp_br_tableL3[pMP3Stream->id][br_index] / mp_sr20_table[pMP3Stream->id][pMP3Stream->sr_index];
      }
      else
      {
   pMP3Stream->framebytes =
      2880 * mp_br_tableL3[pMP3Stream->id][br_index] / mp_sr20_table[pMP3Stream->id][pMP3Stream->sr_index];
       //if( pMP3Stream->sr_index == 2 ) return 0;  // fail mpeg25 8khz
      }
   }
   side_info.main_data_begin = bitget(8);
   if (side_info.mode == 3)
   {
      side_info.private_bits = bitget(1);
      pMP3Stream->nchan = 1;
//      stereo_flag = 0;
      side_bytes = (4 + 9);
/*-- with header --*/
   }
   else
   {
      side_info.private_bits = bitget(2);
      pMP3Stream->nchan = 2;
//      stereo_flag = 1;
      side_bytes = (4 + 17);
/*-- with header --*/
   }
   side_info.scfsi[1] = side_info.scfsi[0] = 0;


   for (ch = 0; ch < pMP3Stream->nchan; ch++)
   {
      side_info.gr[igr][ch].part2_3_length = bitget(12);
      side_info.gr[igr][ch].big_values = bitget(9);
      side_info.gr[igr][ch].global_gain = bitget(8) + pMP3Stream->gain_adjust;
      if (pMP3Stream->ms_mode)
   side_info.gr[igr][ch].global_gain -= 2;
      side_info.gr[igr][ch].scalefac_compress = bitget(9);
      side_info.gr[igr][ch].window_switching_flag = bitget(1);
      if (side_info.gr[igr][ch].window_switching_flag)
      {
   side_info.gr[igr][ch].block_type = bitget(2);
   side_info.gr[igr][ch].mixed_block_flag = bitget(1);
   side_info.gr[igr][ch].table_select[0] = bitget(5);
   side_info.gr[igr][ch].table_select[1] = bitget(5);
   side_info.gr[igr][ch].subblock_gain[0] = bitget(3);
   side_info.gr[igr][ch].subblock_gain[1] = bitget(3);
   side_info.gr[igr][ch].subblock_gain[2] = bitget(3);
       /* region count set in terms of long block cb's/bands  */
       /* r1 set so r0+r1+1 = 21 (lookup produces 576 bands ) */
       /* bt=1 or 3       54 samples */
       /* bt=2 mixed=0    36 samples */
       /* bt=2 mixed=1    54 (8 long sf) samples? or maybe 36 */
       /* region0 discussion says 54 but this would mix long */
       /* and short in region0 if scale factors switch */
       /* at band 36 (6 long scale factors) */
   if ((side_info.gr[igr][ch].block_type == 2))
   {
      side_info.gr[igr][ch].region0_count = (6 - 1);  /* 36 samples */
      side_info.gr[igr][ch].region1_count = 20 - (6 - 1);
   }
   else
   {      /* long block type 1 or 3 */
      side_info.gr[igr][ch].region0_count = (8 - 1);  /* 54 samples */
      side_info.gr[igr][ch].region1_count = 20 - (8 - 1);
   }
      }
      else
      {
   side_info.gr[igr][ch].mixed_block_flag = 0;
   side_info.gr[igr][ch].block_type = 0;
   side_info.gr[igr][ch].table_select[0] = bitget(5);
   side_info.gr[igr][ch].table_select[1] = bitget(5);
   side_info.gr[igr][ch].table_select[2] = bitget(5);
   side_info.gr[igr][ch].region0_count = bitget(4);
   side_info.gr[igr][ch].region1_count = bitget(3);
      }
      side_info.gr[igr][ch].preflag = 0;
      side_info.gr[igr][ch].scalefac_scale = bitget(1);
      side_info.gr[igr][ch].count1table_select = bitget(1);
   }

/* return  bytes in header + side info */
   return side_bytes;
}
/*-----------------------------------------------------------------*/
static void unpack_main(unsigned char *pcm, int igr)
{
   int ch;
   int bit0;
   int n1, n2, n3, n4, nn2, nn3;
   int nn4;
   int qbits;
   int m0;


   for (ch = 0; ch < pMP3Stream->nchan; ch++)
   {
      bitget_init(pMP3Stream->buf + (pMP3Stream->main_pos_bit >> 3));
      bit0 = (pMP3Stream->main_pos_bit & 7);
      if (bit0)
   bitget(bit0);
      pMP3Stream->main_pos_bit += side_info.gr[igr][ch].part2_3_length;
      bitget_init_end(pMP3Stream->buf + ((pMP3Stream->main_pos_bit + 39) >> 3));
/*-- scale factors --*/
      if (pMP3Stream->id)
   unpack_sf_sub_MPEG1(&sf[igr][ch],&side_info.gr[igr][ch], side_info.scfsi[ch], igr);
      else
   unpack_sf_sub_MPEG2(&sf[igr][ch],&side_info.gr[igr][ch], pMP3Stream->is_mode & ch, &is_sf_info);
/*--- huff data ---*/
      n1 = pMP3Stream->sfBandIndex[0][side_info.gr[igr][ch].region0_count];
      n2 = pMP3Stream->sfBandIndex[0][side_info.gr[igr][ch].region0_count
        + side_info.gr[igr][ch].region1_count + 1];
      n3 = side_info.gr[igr][ch].big_values;
      n3 = n3 + n3;


      if (n3 > pMP3Stream->band_limit)
   n3 = pMP3Stream->band_limit;
      if (n2 > n3)
   n2 = n3;
      if (n1 > n3)
   n1 = n3;
      nn3 = n3 - n2;
      nn2 = n2 - n1;
      
/*      unpack_huff(pMP3Stream->__sample[ch][igr], n1, side_info.gr[igr][ch].table_select[0]);
      unpack_huff(pMP3Stream->__sample[ch][igr] + n1, nn2, side_info.gr[igr][ch].table_select[1]);
      unpack_huff(pMP3Stream->__sample[ch][igr] + n2, nn3, side_info.gr[igr][ch].table_select[2]);*/   //Cortex FIX

      unpack_huff((int (*)[2])pMP3Stream->__sample[ch][igr], n1, side_info.gr[igr][ch].table_select[0]);
      unpack_huff((int (*)[2])pMP3Stream->__sample[ch][igr] + n1, nn2, side_info.gr[igr][ch].table_select[1]);
      unpack_huff((int (*)[2])pMP3Stream->__sample[ch][igr] + n2, nn3, side_info.gr[igr][ch].table_select[2]);  


      
      qbits = side_info.gr[igr][ch].part2_3_length - (bitget_bits_used() - bit0);
      
//      nn4 = unpack_huff_quad(pMP3Stream->__sample[ch][igr] + n3, pMP3Stream->band_limit - n3, qbits, side_info.gr[igr][ch].count1table_select);
      nn4 = unpack_huff_quad((int (*)[4])pMP3Stream->__sample[ch][igr] + n3, pMP3Stream->band_limit - n3, qbits, side_info.gr[igr][ch].count1table_select);  //Cortex FIX

      n4 = n3 + nn4;
      nsamp[igr][ch] = n4;
    //limit n4 or allow deqaunt to sf band 22
      if (side_info.gr[igr][ch].block_type == 2)
   n4 = min(n4, pMP3Stream->band_limit12);
      else
   n4 = min(n4, pMP3Stream->band_limit21);
      if (n4 < 576)
   memset(pMP3Stream->__sample[ch][igr] + n4, 0, sizeof(_SAMPLE) * (576 - n4));
      if (bitdat.bs_ptr > bitdat.bs_ptr_end)
      {       // bad data overrun

   memset(pMP3Stream->__sample[ch][igr], 0, sizeof(_SAMPLE) * (576));
      }
   }



/*--- dequant ---*/
   for (ch = 0; ch < pMP3Stream->nchan; ch++)
   {
      dequant(pMP3Stream->__sample[ch][igr],
        &nsamp[igr][ch],  /* nsamp updated for shorts */
        &sf[igr][ch], &side_info.gr[igr][ch],
        &cb_info[igr][ch], pMP3Stream->ncbl_mixed);
   }

/*--- ms stereo processing  ---*/
   if (pMP3Stream->ms_mode)
   {
      if (pMP3Stream->is_mode == 0)
      {
   m0 = nsamp[igr][0];  /* process to longer of left/right */
   if (m0 < nsamp[igr][1])
      m0 = nsamp[igr][1];
      }
      else
      {       /* process to last cb in right */
   m0 = pMP3Stream->sfBandIndex[cb_info[igr][1].cbtype][cb_info[igr][1].cbmax];
      }
      //ms_process(pMP3Stream->__sample[0][igr], m0); CORTEX FIX
      ms_process((float (*)[1152])pMP3Stream->__sample[0][igr], m0);

      
   }

/*--- is stereo processing  ---*/
   if (pMP3Stream->is_mode)
   {
      if (pMP3Stream->id)
//   is_process_MPEG1(pMP3Stream->__sample[0][igr], &sf[igr][1], cb_info[igr], nsamp[igr][0], pMP3Stream->ms_mode); CORTEX FIX
   is_process_MPEG1((float (*)[1152])pMP3Stream->__sample[0][igr], &sf[igr][1], cb_info[igr], nsamp[igr][0], pMP3Stream->ms_mode);

      
      else
      
   //is_process_MPEG2(pMP3Stream->__sample[0][igr], &sf[igr][1], cb_info[igr], &is_sf_info, nsamp[igr][0], pMP3Stream->ms_mode); CORTEX FIX
   is_process_MPEG2((float (*)[1152])pMP3Stream->__sample[0][igr], &sf[igr][1], cb_info[igr], &is_sf_info, nsamp[igr][0], pMP3Stream->ms_mode);
   }

/*-- adjust ms and is modes to max of left/right */
   if (side_info.mode_ext)
   {
      if (nsamp[igr][0] < nsamp[igr][1])
   nsamp[igr][0] = nsamp[igr][1];
      else
   nsamp[igr][1] = nsamp[igr][0];
   }

/*--- antialias ---*/
   for (ch = 0; ch < pMP3Stream->nchan; ch++)
   {
      if (cb_info[igr][ch].ncbl == 0)
   continue;    /* have no long blocks */
      if (side_info.gr[igr][ch].mixed_block_flag)
   n1 = 1;    /* 1 -> 36 samples */
      else
   n1 = (nsamp[igr][ch] + 7) / 18;
      if (n1 > 31)
   n1 = 31;
      //antialias(pMP3Stream->__sample[ch][igr], n1);
      antialias((float*)pMP3Stream->__sample[ch][igr], n1); //CORTEX FIX
      
      n1 = 18 * n1 + 8;   /* update number of samples */
      if (n1 > nsamp[igr][ch])
   nsamp[igr][ch] = n1;
   }



/*--- hybrid + sbt ---*/
   pMP3Stream->Xform(pcm, igr);


/*-- done --*/
}
/*--------------------------------------------------------------------*/
/*-----------------------------------------------------------------*/
IN_OUT L3audio_decode(unsigned char *bs, unsigned char *pcm)
{
   return pMP3Stream->decode_function(bs, pcm);
}

/*--------------------------------------------------------------------*/
extern unsigned char *gpNextByteAfterData;
IN_OUT L3audio_decode_MPEG1(unsigned char *bs, unsigned char *pcm)
{
   int sync;
   IN_OUT in_out;
   int side_bytes;
   int nbytes;

   int padframebytes; ////@@@@
   
//   iframe++;

   bitget_init(bs);   /* initialize bit getter */
/* test sync */
   in_out.in_bytes = 0;   /* assume fail */
   in_out.out_bytes = 0;
   sync = bitget(12);

   if (sync != 0xFFF)
      return in_out;    /* sync fail */
/*-----------*/

/*-- unpack side info --*/
   side_bytes = unpack_side_MPEG1();
   padframebytes = pMP3Stream->framebytes + pMP3Stream->pad;

   if (bs + padframebytes > gpNextByteAfterData)
     return in_out; // error check if we're about to read off the end of the legal memory (caused by certain MP3 writers' goofy comment formats) -ste.
   in_out.in_bytes = padframebytes;

/*-- load main data and update buf pointer --*/
/*------------------------------------------- 
   if start point < 0, must just cycle decoder 
   if jumping into middle of stream, 
w---------------------------------------------*/
   pMP3Stream->buf_ptr0 = pMP3Stream->buf_ptr1 - side_info.main_data_begin; /* decode start point */
   if (pMP3Stream->buf_ptr1 > BUF_TRIGGER)
   {        /* shift buffer */
      memmove(pMP3Stream->buf, pMP3Stream->buf + pMP3Stream->buf_ptr0, side_info.main_data_begin);
      pMP3Stream->buf_ptr0 = 0;
      pMP3Stream->buf_ptr1 = side_info.main_data_begin;
   }
   nbytes = padframebytes - side_bytes - pMP3Stream->crcbytes;

   // RAK: This is no bueno. :-(
  if (nbytes < 0 || nbytes > NBUF)
  {
      in_out.in_bytes = 0;
     return in_out;
   }

  if (bFastEstimateOnly)
  {
    in_out.out_bytes = pMP3Stream->outbytes;
    return in_out;
  }

   memmove(pMP3Stream->buf + pMP3Stream->buf_ptr1, bs + side_bytes + pMP3Stream->crcbytes, nbytes);
   pMP3Stream->buf_ptr1 += nbytes;
/*-----------------------*/

   if (pMP3Stream->buf_ptr0 >= 0)
   {
// dump_frame(buf+buf_ptr0, 64);
      pMP3Stream->main_pos_bit = pMP3Stream->buf_ptr0 << 3;
      unpack_main(pcm, 0);
      unpack_main(pcm + pMP3Stream->half_outbytes, 1);
      in_out.out_bytes = pMP3Stream->outbytes;
   }
   else
   {
      memset(pcm, pMP3Stream->zero_level_pcm, pMP3Stream->outbytes);  /* fill out skipped frames */
      in_out.out_bytes = pMP3Stream->outbytes;
/* iframe--;  in_out.out_bytes = 0;  // test test */
   }

   return in_out;
}
/*--------------------------------------------------------------------*/
/*--------------------------------------------------------------------*/
IN_OUT L3audio_decode_MPEG2(unsigned char *bs, unsigned char *pcm)
{
   int sync;
   IN_OUT in_out;
   int side_bytes;
   int nbytes;
   static int igr = 0;

   int padframebytes; ////@@@@

//   iframe++;


   bitget_init(bs);   /* initialize bit getter */
/* test sync */
   in_out.in_bytes = 0;   /* assume fail */
   in_out.out_bytes = 0;
   sync = bitget(12);

// if( sync != 0xFFF ) return in_out;       /* sync fail */

   pMP3Stream->mpeg25_flag = 0;
   if (sync != 0xFFF)
   {
      pMP3Stream->mpeg25_flag = 1;    /* mpeg 2.5 sync */
      if (sync != 0xFFE)
   return in_out;   /* sync fail */
   }
/*-----------*/


/*-- unpack side info --*/
   side_bytes = unpack_side_MPEG2(igr);
   padframebytes = pMP3Stream->framebytes + pMP3Stream->pad;
   in_out.in_bytes = padframebytes;

   pMP3Stream->buf_ptr0 = pMP3Stream->buf_ptr1 - side_info.main_data_begin; /* decode start point */
   if (pMP3Stream->buf_ptr1 > BUF_TRIGGER)
   {        /* shift buffer */
      memmove(pMP3Stream->buf, pMP3Stream->buf + pMP3Stream->buf_ptr0, side_info.main_data_begin);
      pMP3Stream->buf_ptr0 = 0;
      pMP3Stream->buf_ptr1 = side_info.main_data_begin;
   }
   nbytes = padframebytes - side_bytes - pMP3Stream->crcbytes;
   // RAK: This is no bueno. :-(
  if (nbytes < 0 || nbytes > NBUF)
  {
      in_out.in_bytes = 0;
     return in_out;
   }

  if (bFastEstimateOnly)
  {
    in_out.out_bytes = pMP3Stream->outbytes;
    return in_out;
  }

   memmove(pMP3Stream->buf + pMP3Stream->buf_ptr1, bs + side_bytes + pMP3Stream->crcbytes, nbytes);
   pMP3Stream->buf_ptr1 += nbytes;
/*-----------------------*/

   if (pMP3Stream->buf_ptr0 >= 0)
   {
      pMP3Stream->main_pos_bit = pMP3Stream->buf_ptr0 << 3;
      unpack_main(pcm, igr);
      in_out.out_bytes = pMP3Stream->outbytes;
   }
   else
   {
      memset(pcm, pMP3Stream->zero_level_pcm, pMP3Stream->outbytes);  /* fill out skipped frames */
      in_out.out_bytes = pMP3Stream->outbytes;
// iframe--;  in_out.out_bytes = 0; return in_out;// test test */
   }



   igr = igr ^ 1;
   return in_out;
}
/*--------------------------------------------------------------------*/
/*--------------------------------------------------------------------*/
/*--------------------------------------------------------------------*/
/*static const int sr_table[8] =
{22050, 24000, 16000, 1,
 44100, 48000, 32000, 1};*/

static const struct
{
   int l[23];
   int s[14];
}
sfBandIndexTable[3][3] =
{
/* mpeg-2 */
   {
      {
   {
      0, 6, 12, 18, 24, 30, 36, 44, 54, 66, 80, 96, 116, 140, 168, 200, 238, 284, 336, 396, 464, 522, 576
   }
   ,
   {
      0, 4, 8, 12, 18, 24, 32, 42, 56, 74, 100, 132, 174, 192
   }
      }
      ,
      {
   {
      0, 6, 12, 18, 24, 30, 36, 44, 54, 66, 80, 96, 114, 136, 162, 194, 232, 278, 332, 394, 464, 540, 576
   }
   ,
   {
      0, 4, 8, 12, 18, 26, 36, 48, 62, 80, 104, 136, 180, 192
   }
      }
      ,
      {
   {
      0, 6, 12, 18, 24, 30, 36, 44, 54, 66, 80, 96, 116, 140, 168, 200, 238, 284, 336, 396, 464, 522, 576
   }
   ,
   {
      0, 4, 8, 12, 18, 26, 36, 48, 62, 80, 104, 134, 174, 192
   }
      }
      ,
   }
   ,
/* mpeg-1 */
   {
      {
   {
      0, 4, 8, 12, 16, 20, 24, 30, 36, 44, 52, 62, 74, 90, 110, 134, 162, 196, 238, 288, 342, 418, 576
   }
   ,
   {
      0, 4, 8, 12, 16, 22, 30, 40, 52, 66, 84, 106, 136, 192
   }
      }
      ,
      {
   {
      0, 4, 8, 12, 16, 20, 24, 30, 36, 42, 50, 60, 72, 88, 106, 128, 156, 190, 230, 276, 330, 384, 576
   }
   ,
   {
      0, 4, 8, 12, 16, 22, 28, 38, 50, 64, 80, 100, 126, 192
   }
      }
      ,
      {
   {
      0, 4, 8, 12, 16, 20, 24, 30, 36, 44, 54, 66, 82, 102, 126, 156, 194, 240, 296, 364, 448, 550, 576
   }
   ,
   {
      0, 4, 8, 12, 16, 22, 30, 42, 58, 78, 104, 138, 180, 192
   }
      }
   }
   ,

/* mpeg-2.5, 11 & 12 KHz seem ok, 8 ok */
   {
      {
   {
      0, 6, 12, 18, 24, 30, 36, 44, 54, 66, 80, 96, 116, 140, 168, 200, 238, 284, 336, 396, 464, 522, 576
   }
   ,
   {
      0, 4, 8, 12, 18, 26, 36, 48, 62, 80, 104, 134, 174, 192
   }
      }
      ,
      {
   {
      0, 6, 12, 18, 24, 30, 36, 44, 54, 66, 80, 96, 116, 140, 168, 200, 238, 284, 336, 396, 464, 522, 576
   }
   ,
   {
      0, 4, 8, 12, 18, 26, 36, 48, 62, 80, 104, 134, 174, 192
   }
      }
      ,
// this 8khz table, and only 8khz, from mpeg123)
      {
   {
      0, 12, 24, 36, 48, 60, 72, 88, 108, 132, 160, 192, 232, 280, 336, 400, 476, 566, 568, 570, 572, 574, 576
   }
   ,
   {
      0, 8, 16, 24, 36, 52, 72, 96, 124, 160, 162, 164, 166, 192
   }
      }
      ,
   }
   ,
};


void sbt_mono_L3(float *sample, signed short *pcm, int ch);
void sbt_dual_L3(float *sample, signed short *pcm, int ch);
void sbt16_mono_L3(float *sample, signed short *pcm, int ch);
void sbt16_dual_L3(float *sample, signed short *pcm, int ch);
void sbt8_mono_L3(float *sample, signed short *pcm, int ch);
void sbt8_dual_L3(float *sample, signed short *pcm, int ch);

void sbtB_mono_L3(float *sample, unsigned char *pcm, int ch);
void sbtB_dual_L3(float *sample, unsigned char *pcm, int ch);
void sbtB16_mono_L3(float *sample, unsigned char *pcm, int ch);
void sbtB16_dual_L3(float *sample, unsigned char *pcm, int ch);
void sbtB8_mono_L3(float *sample, unsigned char *pcm, int ch);
void sbtB8_dual_L3(float *sample, unsigned char *pcm, int ch);


/*
static const SBT_FUNCTION sbt_table[2][3][2] =
{
{{ (SBT_FUNCTION) sbt_mono_L3,
   (SBT_FUNCTION) sbt_dual_L3 } ,
 { (SBT_FUNCTION) sbt16_mono_L3,
   (SBT_FUNCTION) sbt16_dual_L3 } ,
 { (SBT_FUNCTION) sbt8_mono_L3,
   (SBT_FUNCTION) sbt8_dual_L3 }} ,
//-- 8 bit output 
{{ (SBT_FUNCTION) sbtB_mono_L3,
   (SBT_FUNCTION) sbtB_dual_L3 },
 { (SBT_FUNCTION) sbtB16_mono_L3,
   (SBT_FUNCTION) sbtB16_dual_L3 },
 { (SBT_FUNCTION) sbtB8_mono_L3,
   (SBT_FUNCTION) sbtB8_dual_L3 }}
};
*/

void Xform_mono(void *pcm, int igr);
void Xform_dual(void *pcm, int igr);
void Xform_dual_mono(void *pcm, int igr);
void Xform_dual_right(void *pcm, int igr);

static XFORM_FUNCTION xform_table[5] =
{
   Xform_mono,
   Xform_dual,
   Xform_dual_mono,
   Xform_mono,      /* left */
   Xform_dual_right,
};
int L3table_init();
void msis_init();
void sbt_init();
typedef int iARRAY22[22];
iARRAY22 *quant_init_band_addr();
iARRAY22 *msis_init_band_addr();

/*---------------------------------------------------------*/
/* mpeg_head defined in mhead.h  frame bytes is without pMP3Stream->pad */
////@@@@INIT
int L3audio_decode_init(MPEG_HEAD * h, int framebytes_arg,
       int reduction_code, int transform_code, int convert_code,
      int freq_limit)
{
   int i, j, k;
   // static int first_pass = 1;
   int samprate;
   int limit;
   int bit_code;
   int out_chans;

   pMP3Stream->buf_ptr0 = 0;
   pMP3Stream->buf_ptr1 = 0;

/* check if code handles */
   if (h->option != 1)
      return 0;     /* layer III only */

   if (h->id)
      pMP3Stream->ncbl_mixed = 8;   /* mpeg-1 */
   else
      pMP3Stream->ncbl_mixed = 6;   /* mpeg-2 */

   pMP3Stream->framebytes = framebytes_arg;

   transform_code = transform_code; /* not used, asm compatability */
   bit_code = 0;
   if (convert_code & 8)
      bit_code = 1;
   convert_code = convert_code & 3; /* higher bits used by dec8 freq cvt */
   if (reduction_code < 0)
      reduction_code = 0;
   if (reduction_code > 2)
      reduction_code = 2;
   if (freq_limit < 1000)
      freq_limit = 1000;


   samprate = sr_table[4 * h->id + h->sr_index];
   if ((h->sync & 1) == 0)
      samprate = samprate / 2;  // mpeg 2.5 
/*----- compute pMP3Stream->nsb_limit --------*/
   pMP3Stream->nsb_limit = (freq_limit * 64L + samprate / 2) / samprate;
/*- caller limit -*/
   limit = (32 >> reduction_code);
   if (limit > 8)
      limit--;
   if (pMP3Stream->nsb_limit > limit)
      pMP3Stream->nsb_limit = limit;
   limit = 18 * pMP3Stream->nsb_limit;

   k = h->id;
   if ((h->sync & 1) == 0)
      k = 2;      // mpeg 2.5 

   if (k == 1)
   {
      pMP3Stream->band_limit12 = 3 * sfBandIndexTable[k][h->sr_index].s[13];
      pMP3Stream->band_limit = pMP3Stream->band_limit21 = sfBandIndexTable[k][h->sr_index].l[22];
   }
   else
   {
      pMP3Stream->band_limit12 = 3 * sfBandIndexTable[k][h->sr_index].s[12];
      pMP3Stream->band_limit = pMP3Stream->band_limit21 = sfBandIndexTable[k][h->sr_index].l[21];
   }
   pMP3Stream->band_limit += 8;   /* allow for antialias */
   if (pMP3Stream->band_limit > limit)
      pMP3Stream->band_limit = limit;

   if (pMP3Stream->band_limit21 > pMP3Stream->band_limit)
      pMP3Stream->band_limit21 = pMP3Stream->band_limit;
   if (pMP3Stream->band_limit12 > pMP3Stream->band_limit)
      pMP3Stream->band_limit12 = pMP3Stream->band_limit;


   pMP3Stream->band_limit_nsb = (pMP3Stream->band_limit + 17) / 18; /* limit nsb's rounded up */
/*----------------------------------------------*/
   pMP3Stream->gain_adjust = 0;   /* adjust gain e.g. cvt to mono sum channel */
   if ((h->mode != 3) && (convert_code == 1))
      pMP3Stream->gain_adjust = -4;

   pMP3Stream->outvalues = 1152 >> reduction_code;
   if (h->id == 0)
      pMP3Stream->outvalues /= 2;

   out_chans = 2;
   if (h->mode == 3)
      out_chans = 1;
   if (convert_code)
      out_chans = 1;

   pMP3Stream->sbt_L3 = sbt_table[bit_code][reduction_code][out_chans - 1];
   k = 1 + convert_code;
   if (h->mode == 3)
      k = 0;
   pMP3Stream->Xform = xform_table[k];


   pMP3Stream->outvalues *= out_chans;

   if (bit_code)
      pMP3Stream->outbytes = pMP3Stream->outvalues;
   else
      pMP3Stream->outbytes = sizeof(short) * pMP3Stream->outvalues;

   if (bit_code)
      pMP3Stream->zero_level_pcm = 128; /* 8 bit output */
   else
      pMP3Stream->zero_level_pcm = 0;


   decinfo.channels = out_chans;
   decinfo.outvalues = pMP3Stream->outvalues;
   decinfo.samprate = samprate >> reduction_code;
   if (bit_code)
      decinfo._bits = 8;
   else
      decinfo._bits = sizeof(short) * 8;

   decinfo.framebytes = pMP3Stream->framebytes;
   decinfo.type = 0;

   pMP3Stream->half_outbytes = pMP3Stream->outbytes / 2;
/*------------------------------------------*/

/*- init band tables --*/


   k = h->id;
   if ((h->sync & 1) == 0)
      k = 2;      // mpeg 2.5 

   for (i = 0; i < 22; i++)
      pMP3Stream->sfBandIndex[0][i] = sfBandIndexTable[k][h->sr_index].l[i + 1];
   for (i = 0; i < 13; i++)
      pMP3Stream->sfBandIndex[1][i] = 3 * sfBandIndexTable[k][h->sr_index].s[i + 1];
   for (i = 0; i < 22; i++)
      pMP3Stream->nBand[0][i] = sfBandIndexTable[k][h->sr_index].l[i + 1] - sfBandIndexTable[k][h->sr_index].l[i];
   for (i = 0; i < 13; i++)
      pMP3Stream->nBand[1][i] = sfBandIndexTable[k][h->sr_index].s[i + 1] - sfBandIndexTable[k][h->sr_index].s[i];


/* init tables */
   L3table_init();
/* init ms and is stereo modes */
   msis_init();

/*----- init sbt ---*/
   sbt_init();



/*--- clear buffers --*/
   for (i = 0; i < 576; i++)
      yout[i] = 0.0f;
   for (j = 0; j < 2; j++)
   {
      for (k = 0; k < 2; k++)
      {
   for (i = 0; i < 576; i++)
   {
      pMP3Stream->__sample[j][k][i].x = 0.0f;
      pMP3Stream->__sample[j][k][i].s = 0;
   }
      }
   }

   if (h->id == 1)
      pMP3Stream->decode_function = L3audio_decode_MPEG1;
   else
      pMP3Stream->decode_function = L3audio_decode_MPEG2;

   return 1;
}

/********************************************************************************/
/*                                                                              */
/*  cupl1.c                                                                     */
/*                                                                              */
/********************************************************************************/

static const int bat_bit_masterL1[] =
{
   0, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16
};
////@@@@static float *pMP3Stream->cs_factorL1 = &pMP3Stream->cs_factor[0];  // !!!!!!!!!!!!!!!!
static float look_c_valueL1[16];  // effectively constant
////@@@@static int nbatL1 = 32;

/*======================================================================*/
static void unpack_baL1()
{
   int j;
   int nstereo;

   pMP3Stream->bit_skip = 0;
   nstereo = pMP3Stream->stereo_sb;

   for (j = 0; j < pMP3Stream->nbatL1; j++)
   {
      mac_load_check(4);
      ballo[j] = samp_dispatch[j] = mac_load(4);
      if (j >= pMP3Stream->nsb_limit)
    pMP3Stream->bit_skip += bat_bit_masterL1[samp_dispatch[j]];
      c_value[j] = look_c_valueL1[samp_dispatch[j]];
      if (--nstereo < 0)
      {
   ballo[j + 1] = ballo[j];
   samp_dispatch[j] += 15;  /* flag as joint */
   samp_dispatch[j + 1] = samp_dispatch[j]; /* flag for sf */
   c_value[j + 1] = c_value[j];
   j++;
      }
   }
/*-- terminate with bit skip and end --*/
   samp_dispatch[pMP3Stream->nsb_limit] = 31;
   samp_dispatch[j] = 30;
}
/*-------------------------------------------------------------------------*/
static void unpack_sfL1(void) /* unpack scale factor */
{       /* combine dequant and scale factors */
   int i;

   for (i = 0; i < pMP3Stream->nbatL1; i++)
   {
      if (ballo[i])
      {
   mac_load_check(6);
   pMP3Stream->cs_factorL1[i] = c_value[i] * sf_table[mac_load(6)];
      }
   }
/*-- done --*/
}
/*-------------------------------------------------------------------------*/
#define UNPACKL1_N(n) s[k]     =  pMP3Stream->cs_factorL1[k]*(load(n)-((1 << (n-1)) -1));  \
    goto dispatch;
#define UNPACKL1J_N(n) tmp        =  (load(n)-((1 << (n-1)) -1));                 \
    s[k]       =  pMP3Stream->cs_factorL1[k]*tmp;                        \
    s[k+1]     =  pMP3Stream->cs_factorL1[k+1]*tmp;                      \
    k++;                                                     \
    goto dispatch;
/*-------------------------------------------------------------------------*/
static void unpack_sampL1() /* unpack samples */
{
   int j, k;
   float *s;
   long tmp;

   s = ___sample;
   for (j = 0; j < 12; j++)
   {
      k = -1;
    dispatch:switch (samp_dispatch[++k])
      {
   case 0:
      s[k] = 0.0F;
      goto dispatch;
   case 1:
      UNPACKL1_N(2) /*  3 levels */
   case 2:
      UNPACKL1_N(3) /*  7 levels */
   case 3:
      UNPACKL1_N(4) /* 15 levels */
   case 4:
      UNPACKL1_N(5) /* 31 levels */
   case 5:
      UNPACKL1_N(6) /* 63 levels */
   case 6:
      UNPACKL1_N(7) /* 127 levels */
   case 7:
      UNPACKL1_N(8) /* 255 levels */
   case 8:
      UNPACKL1_N(9) /* 511 levels */
   case 9:
      UNPACKL1_N(10)  /* 1023 levels */
   case 10:
      UNPACKL1_N(11)  /* 2047 levels */
   case 11:
      UNPACKL1_N(12)  /* 4095 levels */
   case 12:
      UNPACKL1_N(13)  /* 8191 levels */
   case 13:
      UNPACKL1_N(14)  /* 16383 levels */
   case 14:
      UNPACKL1_N(15)  /* 32767 levels */
/* -- joint ---- */
   case 15 + 0:
      s[k + 1] = s[k] = 0.0F;
      k++;    /* skip right chan dispatch */
      goto dispatch;
/* -- joint ---- */
   case 15 + 1:
      UNPACKL1J_N(2)  /*  3 levels */
   case 15 + 2:
      UNPACKL1J_N(3)  /*  7 levels */
   case 15 + 3:
      UNPACKL1J_N(4)  /* 15 levels */
   case 15 + 4:
      UNPACKL1J_N(5)  /* 31 levels */
   case 15 + 5:
      UNPACKL1J_N(6)  /* 63 levels */
   case 15 + 6:
      UNPACKL1J_N(7)  /* 127 levels */
   case 15 + 7:
      UNPACKL1J_N(8)  /* 255 levels */
   case 15 + 8:
      UNPACKL1J_N(9)  /* 511 levels */
   case 15 + 9:
      UNPACKL1J_N(10) /* 1023 levels */
   case 15 + 10:
      UNPACKL1J_N(11) /* 2047 levels */
   case 15 + 11:
      UNPACKL1J_N(12) /* 4095 levels */
   case 15 + 12:
      UNPACKL1J_N(13) /* 8191 levels */
   case 15 + 13:
      UNPACKL1J_N(14) /* 16383 levels */
   case 15 + 14:
      UNPACKL1J_N(15) /* 32767 levels */

/* -- end of dispatch -- */
   case 31:
      skip(pMP3Stream->bit_skip);
   case 30:
      s += 64;
      }       /* end switch */
   }        /* end j loop */

/*-- done --*/
}
/*-------------------------------------------------------------------*/
IN_OUT L1audio_decode(unsigned char *bs, signed short *pcm)
{
   int sync, prot;
   IN_OUT in_out;

   load_init(bs);   /* initialize bit getter */
/* test sync */
   in_out.in_bytes = 0;   /* assume fail */
   in_out.out_bytes = 0;
   sync = load(12);
   if (sync != 0xFFF)
      return in_out;    /* sync fail */


   load(3);     /* skip id and option (checked by init) */
   prot = load(1);    /* load prot bit */
   load(6);     /* skip to pad */
   pMP3Stream->pad = (load(1)) << 2;
   load(1);     /* skip to mode */
   pMP3Stream->stereo_sb = look_joint[load(4)];
   if (prot)
      load(4);      /* skip to data */
   else
      load(20);     /* skip crc */

   unpack_baL1();   /* unpack bit allocation */
   unpack_sfL1();   /* unpack scale factor */
   unpack_sampL1();   /* unpack samples */

   pMP3Stream->sbt(___sample, pcm, 12);
/*-----------*/
   in_out.in_bytes = pMP3Stream->framebytes + pMP3Stream->pad;
   in_out.out_bytes = pMP3Stream->outbytes;

   return in_out;
}
/*-------------------------------------------------------------------------*/
int L1audio_decode_init(MPEG_HEAD * h, int framebytes_arg,
       int reduction_code, int transform_code, int convert_code,
      int freq_limit)
{
   int i, k;
   static int first_pass = 1;
   long samprate;
   int limit;
   long step;
   int bit_code;

/*--- sf init done by layer II init ---*/
   if (first_pass)
   {
      for (step = 4, i = 1; i < 16; i++, step <<= 1)
   look_c_valueL1[i] = (float) (2.0 / (step - 1));
      first_pass = 0;
   }
   pMP3Stream->cs_factorL1 = pMP3Stream->cs_factor[0];

   transform_code = transform_code; /* not used, asm compatability */

   bit_code = 0;
   if (convert_code & 8)
      bit_code = 1;
   convert_code = convert_code & 3; /* higher bits used by dec8 freq cvt */
   if (reduction_code < 0)
      reduction_code = 0;
   if (reduction_code > 2)
      reduction_code = 2;
   if (freq_limit < 1000)
      freq_limit = 1000;


   pMP3Stream->framebytes = framebytes_arg;
/* check if code handles */
   if (h->option != 3)
      return 0;     /* layer I only */

   pMP3Stream->nbatL1 = 32;
   pMP3Stream->max_sb = pMP3Stream->nbatL1;
/*----- compute pMP3Stream->nsb_limit --------*/
   samprate = sr_table[4 * h->id + h->sr_index];
   pMP3Stream->nsb_limit = (freq_limit * 64L + samprate / 2) / samprate;
/*- caller limit -*/
/*---- limit = 0.94*(32>>reduction_code);  ----*/
   limit = (32 >> reduction_code);
   if (limit > 8)
      limit--;
   if (pMP3Stream->nsb_limit > limit)
      pMP3Stream->nsb_limit = limit;
   if (pMP3Stream->nsb_limit > pMP3Stream->max_sb)
      pMP3Stream->nsb_limit = pMP3Stream->max_sb;

   pMP3Stream->outvalues = 384 >> reduction_code;
   if (h->mode != 3)
   {        /* adjust for 2 channel modes */
      pMP3Stream->nbatL1 *= 2;
      pMP3Stream->max_sb *= 2;
      pMP3Stream->nsb_limit *= 2;
   }

/* set sbt function */
   k = 1 + convert_code;
   if (h->mode == 3)
   {
      k = 0;
   }
   pMP3Stream->sbt = sbt_table[bit_code][reduction_code][k];
   pMP3Stream->outvalues *= out_chans[k];

   if (bit_code)
      pMP3Stream->outbytes = pMP3Stream->outvalues;
   else
      pMP3Stream->outbytes = sizeof(short) * pMP3Stream->outvalues;

   decinfo.channels = out_chans[k];
   decinfo.outvalues = pMP3Stream->outvalues;
   decinfo.samprate = samprate >> reduction_code;
   if (bit_code)
      decinfo._bits = 8;
   else
      decinfo._bits = sizeof(short) * 8;

   decinfo.framebytes = pMP3Stream->framebytes;
   decinfo.type = 0;


/* clear sample buffer, unused sub bands must be 0 */
   for (i = 0; i < 768; i++)
      ___sample[i] = 0.0F;


/* init sub-band transform */
   sbt_init();

   return 1;
}
/*---------------------------------------------------------*/

/********************************************************************************/
/*                                                                              */
/*  cdct.c                                                                      */
/*                                                                              */
/********************************************************************************/

/****  cdct.c  ***************************************************

mod 5/16/95 first stage in 8 pt dct does not drop last sb mono


MPEG audio decoder, dct
portable C

******************************************************************/



float coef32[31]; /* 32 pt dct coefs */   // !!!!!!!!!!!!!!!!!! (only generated once (always to same value)

/*------------------------------------------------------------*/
float *dct_coef_addr()
{
   return coef32;
}
/*------------------------------------------------------------*/
static void forward_bf(int m, int n, float x[], float f[], float coef[])
{
   int i, j, n2;
   int p, q, p0, k;

   p0 = 0;
   n2 = n >> 1;
   for (i = 0; i < m; i++, p0 += n)
   {
      k = 0;
      p = p0;
      q = p + n - 1;
      for (j = 0; j < n2; j++, p++, q--, k++)
      {
   f[p] = x[p] + x[q];
   f[n2 + p] = coef[k] * (x[p] - x[q]);
      }
   }
}
/*------------------------------------------------------------*/
static void back_bf(int m, int n, float x[], float f[])
{
   int i, j, n2, n21;
   int p, q, p0;

   p0 = 0;
   n2 = n >> 1;
   n21 = n2 - 1;
   for (i = 0; i < m; i++, p0 += n)
   {
      p = p0;
      q = p0;
      for (j = 0; j < n2; j++, p += 2, q++)
   f[p] = x[q];
      p = p0 + 1;
      for (j = 0; j < n21; j++, p += 2, q++)
   f[p] = x[q] + x[q + 1];
      f[p] = x[q];
   }
}
/*------------------------------------------------------------*/


void fdct32(float x[], float c[])
{
   float a[32];     /* ping pong buffers */
   float b[32];
   int p, q;

   float *src = x;

/* special first stage */
   for (p = 0, q = 31; p < 16; p++, q--)
   {
      a[p] = src[p] + src[q];
      a[16 + p] = coef32[p] * (src[p] - src[q]);
   }
   forward_bf(2, 16, a, b, coef32 + 16);
   forward_bf(4, 8, b, a, coef32 + 16 + 8);
   forward_bf(8, 4, a, b, coef32 + 16 + 8 + 4);
   forward_bf(16, 2, b, a, coef32 + 16 + 8 + 4 + 2);
   back_bf(8, 4, a, b);
   back_bf(4, 8, b, a);
   back_bf(2, 16, a, b);
   back_bf(1, 32, b, c);
}
/*------------------------------------------------------------*/
void fdct32_dual(float x[], float c[])
{
   float a[32];     /* ping pong buffers */
   float b[32];
   int p, pp, qq;

/* special first stage for dual chan (interleaved x) */
   pp = 0;
   qq = 2 * 31;
   for (p = 0; p < 16; p++, pp += 2, qq -= 2)
   {
      a[p] = x[pp] + x[qq];
      a[16 + p] = coef32[p] * (x[pp] - x[qq]);
   }
   forward_bf(2, 16, a, b, coef32 + 16);
   forward_bf(4, 8, b, a, coef32 + 16 + 8);
   forward_bf(8, 4, a, b, coef32 + 16 + 8 + 4);
   forward_bf(16, 2, b, a, coef32 + 16 + 8 + 4 + 2);
   back_bf(8, 4, a, b);
   back_bf(4, 8, b, a);
   back_bf(2, 16, a, b);
   back_bf(1, 32, b, c);
}
/*---------------convert dual to mono------------------------------*/
void fdct32_dual_mono(float x[], float c[])
{
   float a[32];     /* ping pong buffers */
   float b[32];
   float t1, t2;
   int p, pp, qq;

/* special first stage  */
   pp = 0;
   qq = 2 * 31;
   for (p = 0; p < 16; p++, pp += 2, qq -= 2)
   {
      t1 = 0.5F * (x[pp] + x[pp + 1]);
      t2 = 0.5F * (x[qq] + x[qq + 1]);
      a[p] = t1 + t2;
      a[16 + p] = coef32[p] * (t1 - t2);
   }
   forward_bf(2, 16, a, b, coef32 + 16);
   forward_bf(4, 8, b, a, coef32 + 16 + 8);
   forward_bf(8, 4, a, b, coef32 + 16 + 8 + 4);
   forward_bf(16, 2, b, a, coef32 + 16 + 8 + 4 + 2);
   back_bf(8, 4, a, b);
   back_bf(4, 8, b, a);
   back_bf(2, 16, a, b);
   back_bf(1, 32, b, c);
}
/*------------------------------------------------------------*/
/*---------------- 16 pt fdct -------------------------------*/
void fdct16(float x[], float c[])
{
   float a[16];     /* ping pong buffers */
   float b[16];
   int p, q;

/* special first stage (drop highest sb) */
   a[0] = x[0];
   a[8] = coef32[16] * x[0];
   for (p = 1, q = 14; p < 8; p++, q--)
   {
      a[p] = x[p] + x[q];
      a[8 + p] = coef32[16 + p] * (x[p] - x[q]);
   }
   forward_bf(2, 8, a, b, coef32 + 16 + 8);
   forward_bf(4, 4, b, a, coef32 + 16 + 8 + 4);
   forward_bf(8, 2, a, b, coef32 + 16 + 8 + 4 + 2);
   back_bf(4, 4, b, a);
   back_bf(2, 8, a, b);
   back_bf(1, 16, b, c);
}
/*------------------------------------------------------------*/
/*---------------- 16 pt fdct dual chan---------------------*/
void fdct16_dual(float x[], float c[])
{
   float a[16];     /* ping pong buffers */
   float b[16];
   int p, pp, qq;

/* special first stage for interleaved input */
   a[0] = x[0];
   a[8] = coef32[16] * x[0];
   pp = 2;
   qq = 2 * 14;
   for (p = 1; p < 8; p++, pp += 2, qq -= 2)
   {
      a[p] = x[pp] + x[qq];
      a[8 + p] = coef32[16 + p] * (x[pp] - x[qq]);
   }
   forward_bf(2, 8, a, b, coef32 + 16 + 8);
   forward_bf(4, 4, b, a, coef32 + 16 + 8 + 4);
   forward_bf(8, 2, a, b, coef32 + 16 + 8 + 4 + 2);
   back_bf(4, 4, b, a);
   back_bf(2, 8, a, b);
   back_bf(1, 16, b, c);
}
/*------------------------------------------------------------*/
/*---------------- 16 pt fdct dual to mono-------------------*/
void fdct16_dual_mono(float x[], float c[])
{
   float a[16];     /* ping pong buffers */
   float b[16];
   float t1, t2;
   int p, pp, qq;

/* special first stage  */
   a[0] = 0.5F * (x[0] + x[1]);
   a[8] = coef32[16] * a[0];
   pp = 2;
   qq = 2 * 14;
   for (p = 1; p < 8; p++, pp += 2, qq -= 2)
   {
      t1 = 0.5F * (x[pp] + x[pp + 1]);
      t2 = 0.5F * (x[qq] + x[qq + 1]);
      a[p] = t1 + t2;
      a[8 + p] = coef32[16 + p] * (t1 - t2);
   }
   forward_bf(2, 8, a, b, coef32 + 16 + 8);
   forward_bf(4, 4, b, a, coef32 + 16 + 8 + 4);
   forward_bf(8, 2, a, b, coef32 + 16 + 8 + 4 + 2);
   back_bf(4, 4, b, a);
   back_bf(2, 8, a, b);
   back_bf(1, 16, b, c);
}
/*------------------------------------------------------------*/
/*---------------- 8 pt fdct -------------------------------*/
void fdct8(float x[], float c[])
{
   float a[8];      /* ping pong buffers */
   float b[8];
   int p, q;

/* special first stage  */

   b[0] = x[0] + x[7];
   b[4] = coef32[16 + 8] * (x[0] - x[7]);
   for (p = 1, q = 6; p < 4; p++, q--)
   {
      b[p] = x[p] + x[q];
      b[4 + p] = coef32[16 + 8 + p] * (x[p] - x[q]);
   }

   forward_bf(2, 4, b, a, coef32 + 16 + 8 + 4);
   forward_bf(4, 2, a, b, coef32 + 16 + 8 + 4 + 2);
   back_bf(2, 4, b, a);
   back_bf(1, 8, a, c);
}
/*------------------------------------------------------------*/
/*---------------- 8 pt fdct dual chan---------------------*/
void fdct8_dual(float x[], float c[])
{
   float a[8];      /* ping pong buffers */
   float b[8];
   int p, pp, qq;

/* special first stage for interleaved input */
   b[0] = x[0] + x[14];
   b[4] = coef32[16 + 8] * (x[0] - x[14]);
   pp = 2;
   qq = 2 * 6;
   for (p = 1; p < 4; p++, pp += 2, qq -= 2)
   {
      b[p] = x[pp] + x[qq];
      b[4 + p] = coef32[16 + 8 + p] * (x[pp] - x[qq]);
   }
   forward_bf(2, 4, b, a, coef32 + 16 + 8 + 4);
   forward_bf(4, 2, a, b, coef32 + 16 + 8 + 4 + 2);
   back_bf(2, 4, b, a);
   back_bf(1, 8, a, c);
}
/*------------------------------------------------------------*/
/*---------------- 8 pt fdct dual to mono---------------------*/
void fdct8_dual_mono(float x[], float c[])
{
   float a[8];      /* ping pong buffers */
   float b[8];
   float t1, t2;
   int p, pp, qq;

/* special first stage  */
   t1 = 0.5F * (x[0] + x[1]);
   t2 = 0.5F * (x[14] + x[15]);
   b[0] = t1 + t2;
   b[4] = coef32[16 + 8] * (t1 - t2);
   pp = 2;
   qq = 2 * 6;
   for (p = 1; p < 4; p++, pp += 2, qq -= 2)
   {
      t1 = 0.5F * (x[pp] + x[pp + 1]);
      t2 = 0.5F * (x[qq] + x[qq + 1]);
      b[p] = t1 + t2;
      b[4 + p] = coef32[16 + 8 + p] * (t1 - t2);
   }
   forward_bf(2, 4, b, a, coef32 + 16 + 8 + 4);
   forward_bf(4, 2, a, b, coef32 + 16 + 8 + 4 + 2);
   back_bf(2, 4, b, a);
   back_bf(1, 8, a, c);
}
/*------------------------------------------------------------*/


/********************************************************************************/
/*                                                                              */
/*  cwin.c                                                                      */
/*                                                                              */
/********************************************************************************/

/****  cwin.c  ***************************************************

include to cwinm.c

MPEG audio decoder, float window routines
portable C

******************************************************************/


/*-------------------------------------------------------------------------*/
void window(float *vbuf, int vb_ptr, short *pcm)
{
   int i, j;
   int si, bx;
   const float *coef;
   float sum;
   long tmp;

   si = vb_ptr + 16;
   bx = (si + 32) & 511;
   coef = wincoef;

/*-- first 16 --*/
   for (i = 0; i < 16; i++)
   {
      sum = 0.0F;
      for (j = 0; j < 8; j++)
      {
   sum += (*coef++) * vbuf[si];
   si = (si + 64) & 511;
   sum -= (*coef++) * vbuf[bx];
   bx = (bx + 64) & 511;
      }
      si++;
      bx--;
      tmp = (long) sum;
      if (tmp > 32767)
   tmp = 32767;
      else if (tmp < -32768)
   tmp = -32768;
      *pcm++ = tmp;
   }
/*--  special case --*/
   sum = 0.0F;
   for (j = 0; j < 8; j++)
   {
      sum += (*coef++) * vbuf[bx];
      bx = (bx + 64) & 511;
   }
   tmp = (long) sum;
   if (tmp > 32767)
      tmp = 32767;
   else if (tmp < -32768)
      tmp = -32768;
   *pcm++ = tmp;
/*-- last 15 --*/
   coef = wincoef + 255;  /* back pass through coefs */
   for (i = 0; i < 15; i++)
   {
      si--;
      bx++;
      sum = 0.0F;
      for (j = 0; j < 8; j++)
      {
   sum += (*coef--) * vbuf[si];
   si = (si + 64) & 511;
   sum += (*coef--) * vbuf[bx];
   bx = (bx + 64) & 511;
      }
      tmp = (long) sum;
      if (tmp > 32767)
   tmp = 32767;
      else if (tmp < -32768)
   tmp = -32768;
      *pcm++ = tmp;
   }
}



/*------------------------------------------------------------*/
void window_dual(float *vbuf, int vb_ptr, short *pcm)
{
   int i, j;      /* dual window interleaves output */
   int si, bx;
   const float *coef;
   float sum;
   long tmp;

   si = vb_ptr + 16;
   bx = (si + 32) & 511;
   coef = wincoef;

/*-- first 16 --*/
   for (i = 0; i < 16; i++)
   {
      sum = 0.0F;
      for (j = 0; j < 8; j++)
      {
   sum += (*coef++) * vbuf[si];
   si = (si + 64) & 511;
   sum -= (*coef++) * vbuf[bx];
   bx = (bx + 64) & 511;
      }
      si++;
      bx--;
      tmp = (long) sum;
      if (tmp > 32767)
   tmp = 32767;
      else if (tmp < -32768)
   tmp = -32768;
      *pcm = tmp;
      pcm += 2;
   }
/*--  special case --*/
   sum = 0.0F;
   for (j = 0; j < 8; j++)
   {
      sum += (*coef++) * vbuf[bx];
      bx = (bx + 64) & 511;
   }
   tmp = (long) sum;
   if (tmp > 32767)
      tmp = 32767;
   else if (tmp < -32768)
      tmp = -32768;
   *pcm = tmp;
   pcm += 2;
/*-- last 15 --*/
   coef = wincoef + 255;  /* back pass through coefs */
   for (i = 0; i < 15; i++)
   {
      si--;
      bx++;
      sum = 0.0F;
      for (j = 0; j < 8; j++)
      {
   sum += (*coef--) * vbuf[si];
   si = (si + 64) & 511;
   sum += (*coef--) * vbuf[bx];
   bx = (bx + 64) & 511;
      }
      tmp = (long) sum;
      if (tmp > 32767)
   tmp = 32767;
      else if (tmp < -32768)
   tmp = -32768;
      *pcm = tmp;
      pcm += 2;
   }
}
/*------------------------------------------------------------*/
/*------------------- 16 pt window ------------------------------*/
void window16(float *vbuf, int vb_ptr, short *pcm)
{
   int i, j;
   unsigned char si, bx;
   const float *coef;
   float sum;
   long tmp;

   si = vb_ptr + 8;
   bx = si + 16;
   coef = wincoef;

/*-- first 8 --*/
   for (i = 0; i < 8; i++)
   {
      sum = 0.0F;
      for (j = 0; j < 8; j++)
      {
   sum += (*coef++) * vbuf[si];
   si += 32;
   sum -= (*coef++) * vbuf[bx];
   bx += 32;
      }
      si++;
      bx--;
      coef += 16;
      tmp = (long) sum;
      if (tmp > 32767)
   tmp = 32767;
      else if (tmp < -32768)
   tmp = -32768;
      *pcm++ = tmp;
   }
/*--  special case --*/
   sum = 0.0F;
   for (j = 0; j < 8; j++)
   {
      sum += (*coef++) * vbuf[bx];
      bx += 32;
   }
   tmp = (long) sum;
   if (tmp > 32767)
      tmp = 32767;
   else if (tmp < -32768)
      tmp = -32768;
   *pcm++ = tmp;
/*-- last 7 --*/
   coef = wincoef + 255;  /* back pass through coefs */
   for (i = 0; i < 7; i++)
   {
      coef -= 16;
      si--;
      bx++;
      sum = 0.0F;
      for (j = 0; j < 8; j++)
      {
   sum += (*coef--) * vbuf[si];
   si += 32;
   sum += (*coef--) * vbuf[bx];
   bx += 32;
      }
      tmp = (long) sum;
      if (tmp > 32767)
   tmp = 32767;
      else if (tmp < -32768)
   tmp = -32768;
      *pcm++ = tmp;
   }
}
/*--------------- 16 pt dual window (interleaved output) -----------------*/
void window16_dual(float *vbuf, int vb_ptr, short *pcm)
{
   int i, j;
   unsigned char si, bx;
   const float *coef;
   float sum;
   long tmp;

   si = vb_ptr + 8;
   bx = si + 16;
   coef = wincoef;

/*-- first 8 --*/
   for (i = 0; i < 8; i++)
   {
      sum = 0.0F;
      for (j = 0; j < 8; j++)
      {
   sum += (*coef++) * vbuf[si];
   si += 32;
   sum -= (*coef++) * vbuf[bx];
   bx += 32;
      }
      si++;
      bx--;
      coef += 16;
      tmp = (long) sum;
      if (tmp > 32767)
   tmp = 32767;
      else if (tmp < -32768)
   tmp = -32768;
      *pcm = tmp;
      pcm += 2;
   }
/*--  special case --*/
   sum = 0.0F;
   for (j = 0; j < 8; j++)
   {
      sum += (*coef++) * vbuf[bx];
      bx += 32;
   }
   tmp = (long) sum;
   if (tmp > 32767)
      tmp = 32767;
   else if (tmp < -32768)
      tmp = -32768;
   *pcm = tmp;
   pcm += 2;
/*-- last 7 --*/
   coef = wincoef + 255;  /* back pass through coefs */
   for (i = 0; i < 7; i++)
   {
      coef -= 16;
      si--;
      bx++;
      sum = 0.0F;
      for (j = 0; j < 8; j++)
      {
   sum += (*coef--) * vbuf[si];
   si += 32;
   sum += (*coef--) * vbuf[bx];
   bx += 32;
      }
      tmp = (long) sum;
      if (tmp > 32767)
   tmp = 32767;
      else if (tmp < -32768)
   tmp = -32768;
      *pcm = tmp;
      pcm += 2;
   }
}
/*------------------- 8 pt window ------------------------------*/
void window8(float *vbuf, int vb_ptr, short *pcm)
{
   int i, j;
   int si, bx;
   const float *coef;
   float sum;
   long tmp;

   si = vb_ptr + 4;
   bx = (si + 8) & 127;
   coef = wincoef;

/*-- first 4 --*/
   for (i = 0; i < 4; i++)
   {
      sum = 0.0F;
      for (j = 0; j < 8; j++)
      {
   sum += (*coef++) * vbuf[si];
   si = (si + 16) & 127;
   sum -= (*coef++) * vbuf[bx];
   bx = (bx + 16) & 127;
      }
      si++;
      bx--;
      coef += 48;
      tmp = (long) sum;
      if (tmp > 32767)
   tmp = 32767;
      else if (tmp < -32768)
   tmp = -32768;
      *pcm++ = tmp;
   }
/*--  special case --*/
   sum = 0.0F;
   for (j = 0; j < 8; j++)
   {
      sum += (*coef++) * vbuf[bx];
      bx = (bx + 16) & 127;
   }
   tmp = (long) sum;
   if (tmp > 32767)
      tmp = 32767;
   else if (tmp < -32768)
      tmp = -32768;
   *pcm++ = tmp;
/*-- last 3 --*/
   coef = wincoef + 255;  /* back pass through coefs */
   for (i = 0; i < 3; i++)
   {
      coef -= 48;
      si--;
      bx++;
      sum = 0.0F;
      for (j = 0; j < 8; j++)
      {
   sum += (*coef--) * vbuf[si];
   si = (si + 16) & 127;
   sum += (*coef--) * vbuf[bx];
   bx = (bx + 16) & 127;
      }
      tmp = (long) sum;
      if (tmp > 32767)
   tmp = 32767;
      else if (tmp < -32768)
   tmp = -32768;
      *pcm++ = tmp;
   }
}
/*--------------- 8 pt dual window (interleaved output) -----------------*/
void window8_dual(float *vbuf, int vb_ptr, short *pcm)
{
   int i, j;
   int si, bx;
   const float *coef;
   float sum;
   long tmp;

   si = vb_ptr + 4;
   bx = (si + 8) & 127;
   coef = wincoef;

/*-- first 4 --*/
   for (i = 0; i < 4; i++)
   {
      sum = 0.0F;
      for (j = 0; j < 8; j++)
      {
   sum += (*coef++) * vbuf[si];
   si = (si + 16) & 127;
   sum -= (*coef++) * vbuf[bx];
   bx = (bx + 16) & 127;
      }
      si++;
      bx--;
      coef += 48;
      tmp = (long) sum;
      if (tmp > 32767)
   tmp = 32767;
      else if (tmp < -32768)
   tmp = -32768;
      *pcm = tmp;
      pcm += 2;
   }
/*--  special case --*/
   sum = 0.0F;
   for (j = 0; j < 8; j++)
   {
      sum += (*coef++) * vbuf[bx];
      bx = (bx + 16) & 127;
   }
   tmp = (long) sum;
   if (tmp > 32767)
      tmp = 32767;
   else if (tmp < -32768)
      tmp = -32768;
   *pcm = tmp;
   pcm += 2;
/*-- last 3 --*/
   coef = wincoef + 255;  /* back pass through coefs */
   for (i = 0; i < 3; i++)
   {
      coef -= 48;
      si--;
      bx++;
      sum = 0.0F;
      for (j = 0; j < 8; j++)
      {
   sum += (*coef--) * vbuf[si];
   si = (si + 16) & 127;
   sum += (*coef--) * vbuf[bx];
   bx = (bx + 16) & 127;
      }
      tmp = (long) sum;
      if (tmp > 32767)
   tmp = 32767;
      else if (tmp < -32768)
   tmp = -32768;
      *pcm = tmp;
      pcm += 2;
   }
}
/*------------------------------------------------------------*/


/********************************************************************************/
/*                                                                              */
/*  towave.c                                                                    */
/*                                                                              */
/********************************************************************************/

/*____________________________________________________________________________
  
  FreeAmp - The Free MP3 Player

        MP3 Decoder originally Copyright (C) 1995-1997 Xing Technology
        Corp.  http://www.xingtech.com

  Portions Copyright (C) 1998-1999 EMusic.com

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
  
  $Id: towave.c,v 1.3 1999/10/19 07:13:09 elrod Exp $
____________________________________________________________________________*/

/* ------------------------------------------------------------------------

      NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE

        This file exists for reference only. It is not actually used
        in the FreeAmp project. There is no need to mess with this 
        file. There is no need to flatten the beavers, either.

      NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE

/*---- towave.c --------------------------------------------
  32 bit version only

decode mpeg Layer I/II/III file using portable ANSI C decoder,
output to pcm wave file.

mod 8/19/98 decode 22 sf bands

mod 5/14/98  allow mpeg25 (dec8 not supported for mpeg25 samp rate)

mod 3/4/98 bs_trigger  bs_bufbytes  made signed, unsigned may 
            not terminate properly.  Also extra test in bs_fill.

mod 8/6/96 add 8 bit output to standard decoder

ver 1.4 mods 7/18/96 32 bit and add asm option

mods 6/29/95  allow MS wave file for u-law.  bugfix u-law table dec8.c

mods 2/95 add sample rate reduction, freq_limit and conversions.
          add _decode8 for 8Ks output, 16bit 8bit, u-law output.
          add additional control parameters to init.
          add _info function

mod 5/12/95 add quick window cwinq.c

mod 5/19/95 change from stream io to handle io

mod 11/16/95 add Layer I

mod 1/5/95   integer overflow mod iup.c

ver 1.3
mod 2/5/96   portability mods
             drop Tom and Gloria pcm file types

ver 2.0
mod 1/7/97   Layer 3 (float mpeg-1 only)
    2/6/97   Layer 3 MPEG-2

ver 3.01     Layer III bugfix crc problem 8/18/97
ver 3.02     Layer III fix wannabe.mp3 problem 10/9/97
ver 3.03     allow mpeg 2.5  5/14/98
  
Decoder functions for _decode8 are defined in dec8.c.  Useage
is same as regular decoder.

Towave illustrates use of decoder.  Towave converts
mpeg audio files to 16 bit (short) pcm.  Default pcm file
format is wave. Other formats can be accommodated by
adding alternative write_pcm_header and write_pcm_tailer
functions.  The functions kbhit and getch used in towave.c
may not port to other systems.

The decoder handles all mpeg1 and mpeg2 Layer I/II  bitstreams.  

For compatability with the asm decoder and future C versions,
source code users are discouraged from making modifications
to the decoder proper.  MS Windows applications can use wrapper
functions in a separate module if decoder functions need to
be exported.

NOTE additional control parameters.

mod 8/6/96 standard decoder adds 8 bit output

decode8 (8Ks output) convert_code:
   convert_code = 4*bit_code + chan_code
       bit_code:   1 = 16 bit linear pcm
                   2 =  8 bit (unsigned) linear pcm
                   3 = u-law (8 bits unsigned)
       chan_code:  0 = convert two chan to mono
                   1 = convert two chan to mono
                   2 = convert two chan to left chan
                   3 = convert two chan to right chan

decode (standard decoder) convert_code:
             0 = two chan output
             1 = convert two chan to mono
             2 = convert two chan to left chan
             3 = convert two chan to right chan
     or with 8 = 8 bit output 
          (other bits ignored)

decode (standard decoder) reduction_code:
             0 = full sample rate output
             1 = half rate
             2 = quarter rate

-----------------------------------------------------------*/

typedef struct id3v1_1 {
    char id[3];
    char title[30];   // <file basename>
    char artist[30];  // "Raven Software"
    char album[30];   // "#UNCOMP %d"   // needed
    char year[4];   // "2000"
    char comment[28]; // "#MAXVOL %g"   // needed
    char zero;
    char track;
    char genre;
} id3v1_1;  // 128 bytes in size

id3v1_1 *gpTAG;
#define BYTESREMAINING_ACCOUNT_FOR_REAR_TAG(_pvData, _iBytesRemaining)                \
                                                  \
  /* account for trailing ID3 tag in _iBytesRemaining */                      \
  gpTAG = (id3v1_1*) (((byte *)_pvData + _iBytesRemaining)-sizeof(id3v1_1));  /* sizeof = 128 */  \
  if (!strncmp(gpTAG->id, "TAG", 3))                                \
  {                                               \
    _iBytesRemaining -= sizeof(id3v1_1);                            \
  }





/********  pcm buffer ********/

//cortex redirect
////// #define PCM_BUFBYTES  60000U  // more than enough to cover the largest that one packet will ever expand to
char PCM_Buffer[PCM_BUFBYTES];  // better off being declared, so we don't do mallocs in this module (MAC reasons)

   typedef struct
   {
      int (*decode_init) (MPEG_HEAD * h, int framebytes_arg,
        int reduction_code, int transform_code,
        int convert_code, int freq_limit);
      void (*decode_info) (DEC_INFO * info);
      IN_OUT(*decode) (unsigned char *bs, short *pcm, unsigned char *pNextByteAfterData);
   }
   AUDIO;

/*   
#if 0
   // fuck this...
   static AUDIO audio_table[2][2] =
   {
      {
   {audio_decode_init, audio_decode_info, audio_decode},
   {audio_decode8_init, audio_decode8_info, audio_decode8},
      },
      {
   {i_audio_decode_init, i_audio_decode_info, i_audio_decode},
   {audio_decode8_init, audio_decode8_info, audio_decode8},
      }
   };
#else*/
   static AUDIO audio_table[2][2] =
   {
      {
    {audio_decode_init, audio_decode_info, audio_decode},
    {audio_decode_init, audio_decode_info, audio_decode},
      },
      {
    {audio_decode_init, audio_decode_info, audio_decode},
    {audio_decode_init, audio_decode_info, audio_decode},     
      }
   };
/*#endif*/

   static const AUDIO audio = {audio_decode_init, audio_decode_info, audio_decode}; //audio_table[0][0];


// char *return is NZ for any errors (no trailing CR!)
//
char *C_MP3_IsValid(void *pvData, int iDataLen, int bStereoDesired)
{
//  char sTemp[1024]; /////////////////////////////////////////////////
  unsigned int iRealDataStart;
  MPEG_HEAD head;
  DEC_INFO  decinfo;

  int iBitRate;
  int iFrameBytes;

#ifdef _DEBUG
//  int iIgnoreThisForNowIJustNeedItToBreakpointOnToReadAValue = sizeof(MP3STREAM);
//  iBitRate = iIgnoreThisForNowIJustNeedItToBreakpointOnToReadAValue;  // get rid of unused-variable warning
#endif

  memset(pMP3Stream,0,sizeof(*pMP3Stream));

///  iFrameBytes = head_info3( pvData, iDataLen/2, &head, &iBitRate, &iRealDataStart);
  iFrameBytes = head_info3( (unsigned char *)pvData, iDataLen/2, &head, &iBitRate, &iRealDataStart); //Cortex FIX...
  if (iFrameBytes == 0)
  {
    return "MP3ERR: Bad or unsupported file!";
  }

  // check for files with bad frame unpack sizes (that would crash the game), or stereo files.
  //
  // although the decoder can convert stereo to mono (apparently), we want to know about stereo files
  //  because they're a waste of source space... (all FX are mono, and moved via panning)
  //
  if (head.mode != 3 && !bStereoDesired && iDataLen > 98000)  //3 seems to mean mono
  {// we'll allow it for small files even if stereo
    if ( iDataLen != 1050024 ) //fixme, make cinematic_1 play as music instead
    { 
      return "MP3ERR: Sound file is stereo!";
    }
  }   
  if (audio.decode_init(&head, iFrameBytes, reduction_code, iRealDataStart, bStereoDesired?convert_code_stereo:convert_code_mono, freq_limit))
  {
    if (bStereoDesired)
    {
      if (pMP3Stream->outbytes > 4608)
      {
        return "MP3ERR: Source file has output packet size > 2304 (*2 for stereo) bytes!";
      }
    }   
    else
    {
      if (pMP3Stream->outbytes > 2304)
      {
        return "MP3ERR: Source file has output packet size > 2304 bytes!";
      }
    }   

    audio.decode_info(&decinfo);

    if (decinfo._bits != 16)
    {
      return "MP3ERR: Source file is not 16bit!"; // will this ever happen? oh well...
    }

    if (decinfo.samprate != 44100)
    {
      return "MP3ERR: Source file is not sampled @ 44100!";
    }
    if (bStereoDesired && decinfo.channels != 2)
    {
      return "MP3ERR: Source file is not stereo!";  // sod it, I'm going to count this as an error now
    }   

  }
  else
  {
    return "MP3ERR: Decoder failed to initialise";
  }

  // file seems to be valid...
  //
  return NULL;
}



// char *return is NZ for any errors (no trailing CR!)
//
char* C_MP3_GetHeaderData(void *pvData, int iDataLen, int *piRate, int *piWidth, int *piChannels, int bStereoDesired)
{
  unsigned int iRealDataStart;
  MPEG_HEAD head;
  DEC_INFO  decinfo;

  int iBitRate;
  int iFrameBytes;

  memset(pMP3Stream,0,sizeof(*pMP3Stream));

  //iFrameBytes = head_info3( pvData, iDataLen/2, &head, &iBitRate, &iRealDataStart);
  iFrameBytes = head_info3( (unsigned char *)pvData, iDataLen/2, &head, &iBitRate, &iRealDataStart); //Cortex FIX...

  if (iFrameBytes == 0)
  {
    return "MP3ERR: Bad or unsupported file!";
  }

  if (audio.decode_init(&head, iFrameBytes, reduction_code, iRealDataStart, bStereoDesired?convert_code_stereo:convert_code_mono, freq_limit))
  {
    audio.decode_info(&decinfo);

    *piRate   = decinfo.samprate; // rate (eg 22050, 44100 etc)
    *piWidth  = decinfo._bits/8; // 1 for 8bit, 2 for 16 bit
    *piChannels = decinfo.channels; // 1 for mono, 2 for stereo
  }
  else
  {
    return "MP3ERR: Decoder failed to initialise";
  }

  // everything ok...
  //
  return NULL;
}




// this duplicates work done in C_MP3_IsValid(), but it avoids global structs, and means that you can call this anytime
//  if you just want info for some reason
//
// ( size is now workd out just by decompressing each packet header, not the whole stream. MUCH faster :-)
//
// char *return is NZ for any errors (no trailing CR!)
//
char *C_MP3_GetUnpackedSize(void *pvData, int iSourceBytesRemaining, int *piUnpackedSize, int bStereoDesired )
{
  int iReadLimit;
  unsigned int iRealDataStart;
  MPEG_HEAD head;
  int iBitRate;

  char *pPCM_Buffer = PCM_Buffer;
  char *psReturn = NULL;
//  int  iSourceReadIndex = 0;
  int  iDestWriteIndex = 0;

  int iFrameBytes;
  int iFrameCounter;

  DEC_INFO decinfo;
  IN_OUT   x;
  
  memset(pMP3Stream,0,sizeof(*pMP3Stream));

#define iSourceReadIndex iRealDataStart

//  iFrameBytes = head_info2( pvData, 0, &head, &iBitRate);

  //iFrameBytes = head_info3( pvData, iSourceBytesRemaining/2, &head, &iBitRate, &iRealDataStart);
  
  iFrameBytes = head_info3( (unsigned char *)pvData, iSourceBytesRemaining/2, &head, &iBitRate, &iRealDataStart); //Cortex FIX...
  
  BYTESREMAINING_ACCOUNT_FOR_REAR_TAG(pvData, iSourceBytesRemaining)
  iSourceBytesRemaining -= iRealDataStart;

  iReadLimit = iSourceReadIndex + iSourceBytesRemaining;

  if (iFrameBytes)
  {
    //pPCM_Buffer = Z_Malloc(PCM_BUFBYTES);

    //if (pPCM_Buffer)
    {
      // init decoder...

      if (audio.decode_init(&head, iFrameBytes, reduction_code, iRealDataStart, bStereoDesired?convert_code_stereo:convert_code_mono, freq_limit))
      {
        audio.decode_info(&decinfo);

        // decode...
        //
        for (iFrameCounter = 0;;iFrameCounter++)
        {
          if ( iSourceBytesRemaining == 0 || iSourceBytesRemaining < iFrameBytes)
            break;  // end of file

          bFastEstimateOnly = 1;  ///////////////////////////////

              x = audio.decode((unsigned char *)pvData + iSourceReadIndex, (short *) ((char *)pPCM_Buffer
                                              //+ iDestWriteIndex   // keep decoding over the same spot since we're only counting bytes in this function
                                              ),
                      (unsigned char *)pvData + iReadLimit
                      );  

          bFastEstimateOnly = 0;  ///////////////////////////////
          
          iSourceReadIndex    += x.in_bytes;
          iSourceBytesRemaining -= x.in_bytes;
          iDestWriteIndex     += x.out_bytes;

          if (x.in_bytes <= 0)
          {
            //psReturn = "MP3ERR: Bad sync in file";
            break;
          }
        }

        *piUnpackedSize = iDestWriteIndex;  // yeeehaaa!
      }
      else
      {
        psReturn = "MP3ERR: Decoder failed to initialise";
      }
    }
//    else
//    {
//      psReturn = "MP3ERR: Unable to alloc temp decomp buffer";    
//    }
  }
  else
  {
    psReturn = "MP3ERR: Bad or Unsupported MP3 file!";
  }


//  if (pPCM_Buffer)
//  {
//    Z_Free(pPCM_Buffer);
//    pPCM_Buffer = NULL; // I know, I know...
//  }

  return psReturn;

#undef iSourceReadIndex
}




char *C_MP3_UnpackRawPCM( void *pvData, int iSourceBytesRemaining, int *piUnpackedSize, void *pbUnpackBuffer, int bStereoDesired)
{
  int iReadLimit;
  unsigned int iRealDataStart;
  MPEG_HEAD head;
  int iBitRate;
  
  char *psReturn = NULL;
//  int  iSourceReadIndex = 0;
  int  iDestWriteIndex = 0;

  int iFrameBytes;
  int iFrameCounter;

  DEC_INFO decinfo;
  IN_OUT   x;

  memset(pMP3Stream,0,sizeof(*pMP3Stream));

#define iSourceReadIndex iRealDataStart

//  iFrameBytes = head_info2( pvData, 0, &head, &iBitRate);

//  iFrameBytes = head_info3( pvData, iSourceBytesRemaining/2, &head, &iBitRate, &iRealDataStart);

  iFrameBytes = head_info3( (unsigned char *)pvData, iSourceBytesRemaining/2, &head, &iBitRate, &iRealDataStart); //Cortex FIX...


  BYTESREMAINING_ACCOUNT_FOR_REAR_TAG(pvData, iSourceBytesRemaining)
  iSourceBytesRemaining -= iRealDataStart;

  iReadLimit = iSourceReadIndex + iSourceBytesRemaining;

  if (iFrameBytes)
  {
//    if (1)////////////////////////pPCM_Buffer)
    {
      // init decoder...

      if (audio.decode_init(&head, iFrameBytes, reduction_code, iRealDataStart, bStereoDesired?convert_code_stereo:convert_code_mono, freq_limit))
      {
        audio.decode_info(&decinfo);

//        printf("\n output samprate = %6ld",decinfo.samprate);
//        printf("\n output channels = %6d", decinfo.channels);
//        printf("\n output bits     = %6d", decinfo._bits);
//        printf("\n output type     = %6d", decinfo.type);

//===============
        
        // decode...
        //
        for (iFrameCounter = 0;;iFrameCounter++)
        {
          if ( iSourceBytesRemaining == 0 || iSourceBytesRemaining < iFrameBytes)
            break;  // end of file

          x = audio.decode((unsigned char *)pvData + iSourceReadIndex, (short *) ((char *)pbUnpackBuffer + iDestWriteIndex),
                   (unsigned char *)pvData + iReadLimit
                  );
          
          iSourceReadIndex    += x.in_bytes;
          iSourceBytesRemaining -= x.in_bytes;
          iDestWriteIndex     += x.out_bytes;

          if (x.in_bytes <= 0)
          {
            //psReturn = "MP3ERR: Bad sync in file";
            break;
          }
        }

        *piUnpackedSize = iDestWriteIndex;  // yeeehaaa!
      }
      else
      {
        psReturn = "MP3ERR: Decoder failed to initialise";
      }
    }
  }
  else
  {
    psReturn = "MP3ERR: Bad or Unsupported MP3 file!";
  }

  return psReturn;

#undef iSourceReadIndex
}


// called once, after we've decided to keep something as MP3. This just sets up the decoder for subsequent stream-calls.
//            
// (the struct pSFX_MP3Stream is cleared internally, so pass as args anything you want stored in it)
//
// char * return is NULL for ok, else error string
//
char *C_MP3Stream_DecodeInit( LP_MP3STREAM pSFX_MP3Stream, void *pvSourceData, int iSourceBytesRemaining,
                int iGameAudioSampleRate, int iGameAudioSampleBits, int bStereoDesired )
{
  char      *psReturn = NULL;
  MPEG_HEAD   head;     // only relevant within this function during init 
  DEC_INFO    decinfo;    //   " "
  int       iBitRate;   // not used after being filled in by head_info3()

  pMP3Stream = pSFX_MP3Stream;

  memset(pMP3Stream,0,sizeof(*pMP3Stream));
  
  pMP3Stream->pbSourceData      = (byte *) pvSourceData;
  pMP3Stream->iSourceBytesRemaining = iSourceBytesRemaining;
  pMP3Stream->iSourceFrameBytes   = head_info3( (byte *) pvSourceData, iSourceBytesRemaining/2, &head, &iBitRate, (unsigned int*)&pMP3Stream->iSourceReadIndex ); 

  // hack, do NOT do this for stereo, since music files are now streamed and therefore the data isn't actually fully
  //  loaded at this point, only about 4k or so for the header is actually in memory!!!...
  //
  if (!bStereoDesired)
  {
    BYTESREMAINING_ACCOUNT_FOR_REAR_TAG(pvSourceData, pMP3Stream->iSourceBytesRemaining);
    pMP3Stream->iSourceBytesRemaining  -= pMP3Stream->iSourceReadIndex;
  }

  // backup a couple of fields so we can play this again later...
  //
  pMP3Stream->iRewind_SourceReadIndex   = pMP3Stream->iSourceReadIndex;
  pMP3Stream->iRewind_SourceBytesRemaining= pMP3Stream->iSourceBytesRemaining;

  assert(pMP3Stream->iSourceFrameBytes);
  
  if (pMP3Stream->iSourceFrameBytes)
  {
    if (audio.decode_init(&head, pMP3Stream->iSourceFrameBytes, reduction_code, pMP3Stream->iSourceReadIndex, bStereoDesired?convert_code_stereo:convert_code_mono, freq_limit))
    {
      pMP3Stream->iRewind_FinalReductionCode = reduction_code;  // default = 0 (no reduction), 1=half, 2 = quarter

      pMP3Stream->iRewind_FinalConvertCode   = bStereoDesired?convert_code_stereo:convert_code_mono;
                                    // default = 1 (mono), OR with 8 for 8-bit output

      // only now can we ask what kind of properties this file has, and then adjust to fit what the game wants...
      //
      audio.decode_info(&decinfo);

//      printf("\n output samprate = %6ld",decinfo.samprate);
//      printf("\n output channels = %6d", decinfo.channels);
//      printf("\n output bits     = %6d", decinfo._bits);
//      printf("\n output type     = %6d", decinfo.type);

      // decoder offers half or quarter rate adjustement only...
      //
      if (iGameAudioSampleRate == decinfo.samprate>>1)
        pMP3Stream->iRewind_FinalReductionCode = 1;
      else
      if (iGameAudioSampleRate == decinfo.samprate>>2)
        pMP3Stream->iRewind_FinalReductionCode = 2;

      if (iGameAudioSampleBits == decinfo._bits>>1)  // if game wants 8 bit sounds, then setup for that
        pMP3Stream->iRewind_FinalConvertCode |= 8;

      if (audio.decode_init(&head, pMP3Stream->iSourceFrameBytes, pMP3Stream->iRewind_FinalReductionCode, pMP3Stream->iSourceReadIndex, pMP3Stream->iRewind_FinalConvertCode, freq_limit))
      {
        audio.decode_info(&decinfo);
#ifdef _DEBUG
        assert( iGameAudioSampleRate == decinfo.samprate );
        assert( iGameAudioSampleBits == decinfo._bits );
#endif

        // sod it, no harm in one last check... (should never happen)
        //
        if ( iGameAudioSampleRate != decinfo.samprate || iGameAudioSampleBits != decinfo._bits )
        {
          psReturn = "MP3ERR: Decoder unable to convert to current game audio settings";
        }
      }
      else
      {
        psReturn = "MP3ERR: Decoder failed to initialise for pass 2 sample adjust";
      }
    }
    else
    {
      psReturn = "MP3ERR: Decoder failed to initialise";
    }
  }
  else
  {
    psReturn = "MP3ERR: Errr.... something's broken with this MP3 file";  // should never happen by this point
  }

  // restore global stream ptr before returning to normal functions (so the rest of the MP3 code still works)...
  //
  pMP3Stream = &_MP3Stream;

  return psReturn;
}

// return value is decoded bytes for this packet, which is effectively a BOOL, NZ for not finished decoding yet...
//
unsigned int C_MP3Stream_Decode( LP_MP3STREAM pSFX_MP3Stream, int bFastForwarding )
{
  unsigned int uiDecoded = 0; // default to "finished"
  IN_OUT   x;

  pMP3Stream = pSFX_MP3Stream;

  do
  {
    if ( pSFX_MP3Stream->iSourceBytesRemaining == 0 )//|| pSFX_MP3Stream->iSourceBytesRemaining < pSFX_MP3Stream->iSourceFrameBytes)
    {
      uiDecoded = 0;  // finished
      break;
    }


    
    bFastEstimateOnly = bFastForwarding;  ///////////////////////////////

    x = audio.decode(pSFX_MP3Stream->pbSourceData + pSFX_MP3Stream->iSourceReadIndex, (short *) (pSFX_MP3Stream->bDecodeBuffer),
             pSFX_MP3Stream->pbSourceData + pSFX_MP3Stream->iRewind_SourceReadIndex + pSFX_MP3Stream->iRewind_SourceBytesRemaining
            );

    bFastEstimateOnly = 0;  ///////////////////////////////



#ifdef _DEBUG
    pSFX_MP3Stream->iSourceFrameCounter++;
#endif

    pSFX_MP3Stream->iSourceReadIndex    += x.in_bytes;
    pSFX_MP3Stream->iSourceBytesRemaining -= x.in_bytes;
    pSFX_MP3Stream->iBytesDecodedTotal    += x.out_bytes;
    pSFX_MP3Stream->iBytesDecodedThisPacket  = x.out_bytes;

    uiDecoded = x.out_bytes;

    if (x.in_bytes <= 0)
    {
      //psReturn = "MP3ERR: Bad sync in file";      
      uiDecoded= 0; // finished
      break;
    }
  }
  #pragma warning (disable : 4127 ) // conditional expression is constant
  while (0);  // <g>
  #pragma warning (default : 4127 ) // conditional expression is constant
  
  // restore global stream ptr before returning to normal functions (so the rest of the MP3 code still works)...
  //
  pMP3Stream = &_MP3Stream;

  return uiDecoded;
}


// ret is char* errstring, else NULL for ok
//
char *C_MP3Stream_Rewind( LP_MP3STREAM pSFX_MP3Stream )
{
  char*   psReturn = NULL;
  MPEG_HEAD head;     // only relevant within this function during init 
  int     iBitRate;   // ditto
  int     iNULL;      

  pMP3Stream = pSFX_MP3Stream;

  pMP3Stream->iSourceReadIndex    = pMP3Stream->iRewind_SourceReadIndex;
  pMP3Stream->iSourceBytesRemaining = pMP3Stream->iRewind_SourceBytesRemaining; // already adjusted for tags etc  

  // I'm not sure that this is needed, but where else does decode_init get passed useful data ptrs?...
  //
  if (pMP3Stream->iSourceFrameBytes == head_info3( pMP3Stream->pbSourceData, pMP3Stream->iSourceBytesRemaining/2, &head, &iBitRate, (unsigned int*)&iNULL ) )
  { 
    if (audio.decode_init(&head, pMP3Stream->iSourceFrameBytes, pMP3Stream->iRewind_FinalReductionCode, pMP3Stream->iSourceReadIndex, pMP3Stream->iRewind_FinalConvertCode, freq_limit))
    {
      // we should always get here...
      //
    }
    else
    {
      psReturn = "MP3ERR: Failed to re-init decoder for rewind!";
    }
  }
  else
  {
    psReturn = "MP3ERR: Frame bytes mismatch during rewind header-read!";
  }

  // restore global stream ptr before returning to normal functions (so the rest of the MP3 code still works)...
  //
  pMP3Stream = &_MP3Stream;

  return psReturn;
}


/*****************************************************************************************************************************************************************************************/

//CORTEX EXTENSION

char *C_MP3_UnpackRawPCMSector( void *pvData, int iSourceBytesRemaining, int *piUnpackedSize, void *pbUnpackBuffer, int bStereoDesired)
{
  unsigned int iReadLimit;
  unsigned int iRealDataStart;
  MPEG_HEAD head;
  int iBitRate;
  
  char *psReturn = NULL;
  unsigned int  iSourceReadIndex = 0;
  unsigned int  iDestWriteIndex = 0;

  unsigned int iFrameBytes;
  unsigned int iFrameCounter;

  DEC_INFO decinfo;
  IN_OUT   x;


  Serial.println();
  Serial.println();
  Serial.print(sizeof(*pMP3Stream));
  
  memset(pMP3Stream,0,sizeof(*pMP3Stream));

  Serial.println(" bytes memset proceed.");

#define iSourceReadIndex iRealDataStart

//  iFrameBytes = head_info2( pvData, 0, &head, &iBitRate);

//  iFrameBytes = head_info3( pvData, iSourceBytesRemaining/2, &head, &iBitRate, &iRealDataStart);

  iFrameBytes = head_info3( (unsigned char *)pvData, iSourceBytesRemaining/2, &head, &iBitRate, &iRealDataStart); //Cortex FIX...


  BYTESREMAINING_ACCOUNT_FOR_REAR_TAG(pvData, iSourceBytesRemaining)
  iSourceBytesRemaining -= iRealDataStart;

  Serial.println();
  Serial.print(iRealDataStart);
  Serial.println(" bytes iRealDataStart.");


  iReadLimit = iSourceReadIndex + iSourceBytesRemaining;

  Serial.println();
  Serial.print(iReadLimit);
  Serial.println(" bytes iReadLimit.");

  if (iFrameBytes)
  {
//    if (1)////////////////////////pPCM_Buffer)
    {
      // init decoder...

      if (audio.decode_init(&head, iFrameBytes, reduction_code, iRealDataStart, bStereoDesired?convert_code_stereo:convert_code_mono, freq_limit))
      {
        audio.decode_info(&decinfo);

        Serial.println();
        Serial.print("\n frame size = ");
        Serial.println(iFrameBytes);
        Serial.println();
        

        Serial.print("\n output samprate = ");
        Serial.println(decinfo.samprate);
        Serial.print("\n output channels = ");
        Serial.println(decinfo.channels);
        Serial.print("\n output bits = ");
        Serial.println(decinfo._bits);
        Serial.print("\n output type = ");
        Serial.println(decinfo.type);

       
//===============
        
        // decode...
        //
        for (iFrameCounter = 0;;iFrameCounter++)
        {
          Serial.print(" frame: ");
          Serial.print(iFrameCounter);


          

          if ( iSourceBytesRemaining == 0 || iSourceBytesRemaining < iFrameBytes) {
            ///////////psReturn=(char*)pbUnpackBuffer;
            break;  // end of file
          }  

//          x = audio.decode((unsigned char *)pvData + iSourceReadIndex, (short *) ((char *)pbUnpackBuffer + iDestWriteIndex), (unsigned char *)pvData + iReadLimit );
          x = audio.decode((unsigned char *)pvData  +iSourceReadIndex , (short *) ((unsigned char *)&pbUnpackBuffer + iDestWriteIndex), (unsigned char *)pvData + iReadLimit  );

          
          iSourceReadIndex    += x.in_bytes;
          iSourceBytesRemaining -= x.in_bytes;
          iDestWriteIndex     += x.out_bytes;

          Serial.print(" iSourceReadIndex: ");
          Serial.print(iSourceReadIndex);
          Serial.print(" iReadLimit: ");
          Serial.print(iReadLimit);

          Serial.print(" in Bytes: ");
          Serial.print(x.in_bytes);
          Serial.print(" out Bytes: ");
          Serial.print(x.out_bytes);
          Serial.print(" Bytes Remaining: ");
          Serial.println(iSourceBytesRemaining);

          if (x.in_bytes <= 0)
          {
            psReturn = "MP3ERR: Bad sync in file";
            break;
          }
        }

        

        *piUnpackedSize = iDestWriteIndex;  // yeeehaaa!
      }
      else
      {
        psReturn = "MP3ERR: Decoder failed to initialise";
      }
    }
  }
  else
  {
    psReturn = "MP3ERR: Bad or Unsupported MP3 file!";
  }

  return psReturn;

#undef iSourceReadIndex
}





