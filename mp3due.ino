/******************************************************************************/
/**                                                                          **/
/**    MPEG Layer 3 decoder for Arduino Due by Dr.NCX.Cortex                 **/
/**                                                                          **/
/******************************************************************************/
//
// 32kbps 44100Hz mono TESTED....
//
// Working on...


#include <SPI.h>
#include <SdFat.h>
#include <SdFatUtil.h>

#include <Audio.h>

// Size of read/write.
#define BUF_SIZE 576 
#define BUF_SIZE_STEREO 576*2

// SD chip select pin
const uint8_t chipSelect = 4;

// file system object
SdFat sd;

// create Serial stream
ArduinoOutStream cout(Serial);

char fileNameIn[] = "riddle4.mp3";
char fileNameOut[] = "testfile.mmm";


// test file
SdFile fileIn;
SdFile fileOut;


/* SD card SPI
 **************************************
 ** VCC - +3.3V                      **
 ** GND - GND                        **
 ** MISO - pin 108 = A.25 (HW SPI)   ** 
 ** MOSI - pin 109 = A.26 (HW SPI)   **
 ** CLK - pin 110 = A.27 (HW SPI)    **
 ** CS - pin 4                       **
 **************************************/ 




//------------------------------------------------------------------------------
#include "mp3due.h" //load MP3 library
//------------------------------------------------------------------------------

void write_FRAME_bytes(char* buf) {    //writes 576 bytes
  uint32_t frame_size= BUF_SIZE;
  
  if (fileOut.write(buf, frame_size) != frame_size) {
    Serial.println("write failed");
  }
}

void write_FRAME_words(uint16_t* buf) {    //writes 1152 bytes
  uint32_t frame_size= BUF_SIZE_STEREO;
  
  if (fileOut.write(buf, frame_size) != frame_size) {
    Serial.println("write failed");
  }
}





void setup() {
  
  Serial.begin(9600);
  while (!Serial) {} // wait for Leonardo

  delay(400);  // catch Due reset problem

  if (!sd.begin(chipSelect, SPI_FULL_SPEED)) {
    sd.initErrorHalt();
  }

  // open or create file - truncate existing file.
  if (!fileIn.open(fileNameIn, O_READ)) {
    Serial.println("open IN file failed");
  }

  // open or create file - truncate existing file.
  if (!fileOut.open(fileNameOut, O_CREAT | O_TRUNC | O_RDWR)) {
    Serial.println("open OUT file failed");
  }



    while(Get_Filepos() != C_EOF) {
      if(Read_Frame() == OK) Decode_L3();
      else if(Get_Filepos() == C_EOF) break;
      else {
         ERR("Not enough maindata to decode frame\n");
         break;
      }
    }


  fileIn.close();
  fileOut.close();  

  Serial.println("mp3 decodding finished.");


  // 44100Khz stereo => 88200 sample rate
  // 100 mSec of prebuffering.
  Audio.begin(22500, 400);
}

void loop() {
  
  int count = 0;

  // open wave file from sdcard
  File myFile = sd.open("testfile.mmm");
  if (!myFile) {
    // if the file didn't open, print an error and stop
    Serial.println("error opening testfile");
    while (true);
  }

  const int S = 576; // Number of samples to read in block
  short buffer[S];

  Serial.print("Playing");
  // until the file is not finished
  while (myFile.available()) {
    // read from the file into buffer
    myFile.read(buffer, sizeof(buffer));

    // Prepare samples
    int volume = 500;
    Audio.prepare(buffer, S, volume);
    // Feed samples to audio
    Audio.write(buffer, S);

    // Every 100 block print a '.'
    count++;
    if (count == 100) {
      Serial.print(".");
      count = 0;
    }
  }
  myFile.close();

  Serial.println("End of file. Thank you for listening!");
  while (true) ;  
  
  
  }
