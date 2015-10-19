//#include <avr/pgmspace.h>

#define PCM_BUFBYTES  3000U  // more than enough to cover the largest that one packet will ever expand to

#include "mp3due.h"
#include "riddle.h"

#define sboolean int //rww - argh (in SP qboolean type is merely #define'd as an int, but I do not want to do that for MP over the whole base)
#define qtrue  true
#define qfalse  false



byte raw_buffer[8192]; //8kB



/**************************************************************************************************************************************************************************/


// fake cvar stuff
  //
  typedef struct
  {
    int integer;
  } cvar_t;




// expects data already loaded, filename arg is for error printing only
//
// returns success/fail
//
sboolean MP3_IsValid( const char *psLocalFilename, void *pvData, int iDataLen, sboolean bStereoDesired /* = qfalse */)
{
  char *psError = C_MP3_IsValid(pvData, iDataLen, bStereoDesired);
  if (psError)
  {
    Serial.print(sprintf("%s(%s)\n",psError, psLocalFilename));
  }
  return !psError;
}

// expects data already loaded, filename arg is for error printing only
//
// returns unpacked length, or 0 for errors (which will be printed internally)
//
int MP3_GetUnpackedSize( const char *psLocalFilename, void *pvData, int iDataLen, sboolean qbIgnoreID3Tag /* = qfalse */ , sboolean bStereoDesired /* = qfalse */ )
{
  int iUnpackedSize = 0;  

  // always do this now that we have fast-unpack code for measuring output size... (much safer than relying on tags that may have been edited, or if MP3 has been re-saved with same tag)
  //
  if (1)//qbIgnoreID3Tag || !MP3_ReadSpecialTagInfo((byte *)pvData, iDataLen, NULL, &iUnpackedSize))
  { 
    char *psError = C_MP3_GetUnpackedSize( pvData, iDataLen, &iUnpackedSize, bStereoDesired);

    if (psError)
    {
      Serial.print(sprintf("%s\n(File: %s)\n",psError, psLocalFilename));
      return 0;
    }
  } 

  return iUnpackedSize;
}

// expects data already loaded, filename arg is for error printing only
//
// returns byte count of unpacked data (effectively a success/fail bool)
//
int MP3_UnpackRawPCM( const char *psLocalFilename, void *pvData, int iDataLen, void *pbUnpackBuffer, sboolean bStereoDesired /* = qfalse */)
{
  int iUnpackedSize;
  char *psError = C_MP3_UnpackRawPCM( pvData, iDataLen, &iUnpackedSize, pbUnpackBuffer, bStereoDesired);

  if (psError)
  {
      Serial.print(sprintf("%s\n(File: %s)\n",psError, psLocalFilename));
    return 0;
  }

  return iUnpackedSize;
}


// psLocalFilename is just for error reporting (if any)...
//
sboolean MP3Stream_InitPlayingTimeFields( LP_MP3STREAM lpMP3Stream, const char *psLocalFilename, void *pvData, int iDataLen, sboolean bStereoDesired /* = qfalse */)
{
  sboolean bRetval = qfalse;

  int iRate, iWidth, iChannels;

  char *psError = C_MP3_GetHeaderData(pvData, iDataLen, &iRate, &iWidth, &iChannels, bStereoDesired );
  if (psError)
  {
    //Com_Printf(va(S_COLOR_RED"MP3Stream_InitPlayingTimeFields(): %s\n(File: %s)\n",psError, psLocalFilename));
      Serial.print(sprintf("MP3Stream_InitPlayingTimeFields(): %s\n(File: %s)\n",psError, psLocalFilename));
  }
  else
  {
    int iUnpackLength = MP3_GetUnpackedSize( psLocalFilename, pvData, iDataLen, qfalse, // sboolean qbIgnoreID3Tag 
                          bStereoDesired);
    if (iUnpackLength)
    {
      lpMP3Stream->iTimeQuery_UnpackedLength  = iUnpackLength;
      lpMP3Stream->iTimeQuery_SampleRate    = iRate;
      lpMP3Stream->iTimeQuery_Channels    = iChannels;
      lpMP3Stream->iTimeQuery_Width     = iWidth;

      bRetval = qtrue;
    }
  }

  return bRetval;
}

float MP3Stream_GetPlayingTimeInSeconds( LP_MP3STREAM lpMP3Stream )
{
  if (lpMP3Stream->iTimeQuery_UnpackedLength) // fields initialised?
    return (float)((((double)lpMP3Stream->iTimeQuery_UnpackedLength / (double)lpMP3Stream->iTimeQuery_SampleRate) / (double)lpMP3Stream->iTimeQuery_Channels) / (double)lpMP3Stream->iTimeQuery_Width);

  return 0.0f;
}

/*
float MP3Stream_GetRemainingTimeInSeconds( LP_MP3STREAM lpMP3Stream )
{
  if (lpMP3Stream->iTimeQuery_UnpackedLength) // fields initialised?
    return (float)(((((double)(lpMP3Stream->iTimeQuery_UnpackedLength - (lpMP3Stream->iBytesDecodedTotal * (lpMP3Stream->iTimeQuery_SampleRate / dma.speed)))) / (double)lpMP3Stream->iTimeQuery_SampleRate) / (double)lpMP3Stream->iTimeQuery_Channels) / (double)lpMP3Stream->iTimeQuery_Width);

  return 0.0f;
}*/



void setup() {
  
  Serial.begin(9600);  
  if (MP3_IsValid( "riddle.mp3", riddle, riddle_size, false)) Serial.println("mp3 has valid format.");

  Serial.print( MP3_GetUnpackedSize(  "riddle.mp3", riddle, riddle_size, true, false) );

  //Serial.print( MP3Stream_GetPlayingTimeInSeconds

  Serial.print( MP3_UnpackRawPCM( "riddle.mp3", riddle, riddle_size, raw_buffer, false));
  
/*   int iUnpackedSize;
  Serial.print( C_MP3_UnpackRawPCMSector(riddle, riddle_size, &iUnpackedSize, raw_buffer, false));

  Serial.println();
  Serial.print("Total unpacked size:");
  Serial.println(iUnpackedSize);



  /*for (int tmp=0;tmp<sizeof(look_u);tmp++) {
     Serial.print(look_u[tmp]); 
     if (tmp%64==0) Serial.println();
  }*/

    for (int tmp=0;tmp<sizeof(raw_buffer);tmp++) {
     Serial.print(raw_buffer[tmp]); 
     if (tmp%64==0) Serial.println();
  }

}

  
void loop() {
  
}
