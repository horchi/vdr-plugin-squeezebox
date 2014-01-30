/*
 * common.h: EPG2VDR plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#ifndef __COMMON_H
#define __COMMON_H

#include <stdint.h>   // uint_64_t
#include <stdlib.h>
#include <iconv.h>
#include <errno.h>
#include <string.h>

#include <string>

#include <openssl/md5.h> // MD5_*

struct MemoryStruct;
extern int loglevel;
extern int logstdout;
extern int logstamp;

typedef unsigned char byte;
typedef unsigned short word;
typedef short sword;
typedef unsigned int dword;

//***************************************************************************
// Basics 
//***************************************************************************

#ifndef VDR_PLUGIN
  inline long min(long a, long b) { return a < b ? a : b; }
  inline long max(long a, long b) { return a > b ? a : b; }
#endif

enum Misc
{
   success = 0,
   done    = success,
   fail    = -1,
   na      = -1,
   ignore  = -2,
   all     = -3,
   end     = -4,
   back    = -5,
   yes     = 1,
   on      = 1,
   off     = 0,
   no      = 0,
   TB      = 1,

   sizeMd5 = 2 * MD5_DIGEST_LENGTH,

   tmeSecondsPerMinute = 60,
   tmeSecondsPerHour = 60 * tmeSecondsPerMinute,
   tmeSecondsPerDay = 24 * tmeSecondsPerHour
};

//***************************************************************************
// MemoryStruct for curl callbacks
//***************************************************************************

struct MemoryStruct 
{
   MemoryStruct()   { memory = 0; clear(); }
   ~MemoryStruct()  { clear(); }

   // data

   char* memory;
   size_t size;

   // tag attribute

   char tag[100];           // the tag to be compared 
   char name[100];          // content name (filename)
   int headerOnly;

   int isEmpty() { return memory == 0; }

   void clear() 
   {
      free(memory);
      memory = 0;
      size = 0;
      *tag = 0;
      *name = 0;
      headerOnly = no;
   }
};

//***************************************************************************
// Tell
//***************************************************************************

enum Eloquence
{
   eloOff = na,              // -1
   eloAlways,                // 0
   eloDetail,                // 1
   eloDebug,                 // 2
   eloDebug2,                // 3
   eloDebug3                 // 4
};

void tell(int eloquence, const char* format, ...);

//***************************************************************************
// Tools
//***************************************************************************

unsigned int getHostId();
char* getMac();

byte crc(const byte* data, int size);

int toUTF8(char* out, int outMax, const char* in, const char* from_code = 0);

void removeChars(std::string& str, const char* ignore);
void removeCharsExcept(std::string& str, const char* except);
void removeWord(std::string& pattern, std::string word);
void prepareCompressed(std::string& pattern);

const char* plural(int n, const char* s = "s");
char* rTrim(char* buf);
char* lTrim(char* buf);
char* allTrim(char* buf);
int isNum(const char* value);
char* sstrcpy(char* dest, const char* src, int max);
std::string num2Str(int num);
std::string num2Str(double num);
std::string l2pTime(time_t t);
const char* toElapsed(int seconds, char* buf);

int chkDir(const char* path);
int fileExists(const char* path);
int createLink(const char* link, const char* dest, int force);
int isLink(const char* path);
int isEmpty(const char* str);
int removeFile(const char* filename);
int downloadFile(const char* url, MemoryStruct* data, int timeout = 30, const char* httpproxy = 0);
int storeFile(MemoryStruct* data, const char* filename, const char* path = "");

const char* getHostName();
const char* getFirstIp();

#ifdef USEMD5
  typedef char md5Buf[sizeMd5+TB];
  typedef char md5;
  int createMd5(const char* buf, md5* md5);
  int createMd5OfFile(const char* path, const char* name, md5* md5);
#endif

#ifdef WITH_GUNZIP

//***************************************************************************
// Zip
//***************************************************************************

int gunzip(MemoryStruct* zippedData, MemoryStruct* unzippedData);
void tellZipError(int errorCode, const char* op, const char* msg);

#endif // WITH_GUNZIP

//***************************************************************************
// 
//***************************************************************************

class cRetBuf
{
   public:

      cRetBuf(const char* buf = 0);
      cRetBuf(const cRetBuf& buf);
      virtual ~cRetBuf();

      operator const void* () const { return s; } // to catch cases where operator*() should be used
      operator const char* () const { return s; } // for use in (const char *) context
      const char* operator*() const { return s; } // for use in (const void *) context (printf() etc.)
      cRetBuf& operator = (const cRetBuf& buf);
      cRetBuf& operator = (const char* buf);

   private:

      char* s;
};

#ifndef VDR_PLUGIN

//***************************************************************************
// cTimeMs 
//***************************************************************************

class cTimeMs 
{
   public:

      cTimeMs(int Ms = 0);
      static uint64_t Now();
      void Set(int Ms = 0);
      bool TimedOut();
      uint64_t Elapsed();

   private:

      uint64_t begin;
};

#endif // VDR_PLUGIN

//***************************************************************************
// Log Duration
//***************************************************************************

class LogDuration
{
   public:

      LogDuration(const char* aMessage, int aLogLevel = 2);
      ~LogDuration();

      void show(const char* label = "");

   protected:

      char message[1000];
      uint64_t durationStart;
      int logLevel;
};

//***************************************************************************
// 
//***************************************************************************

#include <sys/sem.h>

class Sem
{
   public:

      Sem(int aKey)
      {
         locked = no;
         key = aKey;

         if ((id = semget(key, 1, 0666 | IPC_CREAT)) == -1)
            tell(eloAlways, "Error: Can't get semaphore, errno (%d) '%s'", 
                 errno, strerror(errno));
      }

      ~Sem() 
      { 
         if (locked)
            v();
      }

      int p()
      {
         sembuf sops[2];
         
         sops[0].sem_num = 0;
         sops[0].sem_op = 0;                        // wait for lock
         sops[0].sem_flg = SEM_UNDO;
         
         sops[1].sem_num = 0;
         sops[1].sem_op = 1;                        // increment 
         sops[1].sem_flg = SEM_UNDO | IPC_NOWAIT;
         
         if (semop(id, sops, 2) == -1)
         {
            tell(eloAlways, "Error: Can't lock semaphore, errno (%d) '%s'", 
                 errno, strerror(errno));
            
            return fail;
         }

         locked = yes;

         return success;
      }

      int v()
      {
         sembuf sops;
         
         sops.sem_num = 0;
         sops.sem_op = -1;                          // release control
         sops.sem_flg = SEM_UNDO | IPC_NOWAIT;
         
         if (semop(id, &sops, 1) == -1)
         {
            tell(eloAlways, "Error: Can't unlock semaphore, errno (%d) '%s'",
                 errno, strerror(errno));
            
            return fail;
         }
         
         locked = no;

         return success;
      }
      
   private:

      int key;
      int id;
      int locked;
};

//***************************************************************************
#endif // ___COMMON_H
