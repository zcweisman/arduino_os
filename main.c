#include <stdlib.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include "os.h"
#include "ext.h"
#include "globals.h"
#include "syncro.h"
#include "SdReader.h"
#include <util/delay.h>

#define STEP 5
#define STAT_DELAY 5000
#define NUM_SONGS 13
#define BUFFER_SIZE 256
#define MAX_NAME_LEN 75

mutex_t bufferZero;
mutex_t bufferOne;
mutex_t nameMut;
char songName[MAX_NAME_LEN];
uint32_t remaining;
uint8_t songIndex = 0;
uint32_t callCount = 0;
uint32_t songSize = 0;
uint8_t readBuffer = 1;
uint8_t writeBuffer = 0;
uint8_t buffer[2][BUFFER_SIZE];

void display_stats();
void load_audio_file();
void play_audio_pwm();
char* getSongName(uint16_t index, char buffer[MAX_NAME_LEN]);


int main(void)
{
  int i;
  uint8_t sd_card_status;

  sd_card_status = sdInit(1);
  //Make sure the initialization was successful
  if(!sd_card_status)
    return 0;

  start_audio_pwm();

  os_init();
  //create_threads here
  create_thread(play_audio_pwm, NULL, 10); //10
  create_thread(load_audio_file, NULL, 468); //468
  create_thread(display_stats, NULL, 80); //???

  mutex_init(&bufferZero);
  mutex_init(&bufferOne);
  mutex_init(&nameMut);

  os_start();
  while(1){}
}

//Displays stats about the system and each individual thread
void display_stats()
{
   struct system_t* sysInfo;
   uint8_t i;
   uint8_t row = 2;
   uint8_t col = 0;
   char input;
   sysInfo = (struct system_t *)getSystemInfo();
   clear_screen();
   while(1)
   {
      //Check for input
      if(byte_available())
        input = read_byte();
      if(input == 'p')
      {
        songIndex = (songIndex - 1) % (NUM_SONGS - 1); 
        callCount = 0;
      }
      else if(input == 'n')
      {
        songIndex = (songIndex + 1) % (NUM_SONGS - 1);
        callCount = 0; 
      }
      input = 0;

      set_color(GREEN);
      set_cursor(1, col);
      
      print_string("Time: ");
      print_int(sysInfo->runtime);
      set_cursor(row, col);
      print_string("Interrupts: ");
      print_string("     ");
      set_cursor(row++, 13);
      print_int(sysInfo->interrupts / sysInfo->runtime);
      set_cursor(row++, col);
      row++;
      print_string("# threads: ");
      print_int(sysInfo->numThreads);
      set_cursor(row++, col);
      print_string("Size: ");
      print_int32(songSize);
      set_cursor(row, col);
      print_string("remaining: ");
      print_string("       ");
      set_cursor(row++, 12);
      print_int32(remaining);
      set_cursor(row, col);
      print_string("                                                        ");
      set_cursor(row, col);
      print_string(songName);
      
      //thread_sleep(STAT_DELAY);
      row = 2;
      col = 0;
      set_cursor(row, col);
  }
}

void play_audio_pwm() {
  int i = 0;
   
  while (1) {
    if(readBuffer == 0)
      mutex_lock(&bufferZero);
    else
      mutex_lock(&bufferOne);
      
 
      while(i < BUFFER_SIZE) 
      {
          OCR2B = buffer[readBuffer][i++];
          yield();
      }
      
      i = 0;
      if(readBuffer == 0)
        mutex_unlock(&bufferZero);
      else
        mutex_unlock(&bufferOne);

      readBuffer = (readBuffer + 1) % 2;
  } 
}

void load_audio_file() {
  uint8_t index = 0;
  uint8_t i;
  uint8_t currentSong = songIndex;
  getSongName(songIndex, songName);
  while(1)
  {
    if(writeBuffer == 0)
      mutex_lock(&bufferZero);
    else
      mutex_lock(&bufferOne);
    remaining = fillBuffer(songIndex, buffer[writeBuffer], callCount++);
    
    if(callCount == 1)
    {
      songSize = remaining + BUFFER_SIZE;
    }
    //We've finished reading this file switch to the next
    if(remaining < BUFFER_SIZE)
    {
      songIndex = (songIndex + 1) % (NUM_SONGS - 1);
      callCount = 0;
    }

    //Check if a new song is loaded
    if(currentSong != songIndex)
    {
      currentSong = songIndex;
      getSongName(songIndex, songName);
    }
    
    if(writeBuffer == 0)
        mutex_unlock(&bufferZero);
    else
        mutex_unlock(&bufferOne);

    writeBuffer = (writeBuffer + 1) % 2;
  }
}