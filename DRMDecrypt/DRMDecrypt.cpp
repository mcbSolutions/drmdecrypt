#if defined(_WIN32) || defined(_WIN64)
	#define _CRT_SECURE_NO_WARNINGS
	#include <ctime>
#else
	#include <sys/timeb.h>
#endif
#include <iostream>
#include <iomanip>
#include <sstream>

using namespace std;

// defines
#define PROGRESS_WIDTH  60.0f
#define PACKET_SIZE  188
#define BLOCK_SIZE   3948     // the HDD ist best working with 4096 bytes, but we need
                              // multiples of 188 (PACKET_SIZE)

#if defined(_WIN32) || defined(_WIN64)
   #define DIR_SEP ((char)'\\')
#else
   #define DIR_SEP char('/')
#endif

#if __APPLE__
    typedef uint64_t off64_t;
#else
    //http://lists.mplayerhq.hu/pipermail/mplayer-dev-eng/2007-January/048904.html
    #define fseeko _fseeki64
    #define ftello _ftelli64

	typedef unsigned __int64 off64_t;
#endif

// Prototypes
inline unsigned char process_section (unsigned char *data);
int rijndaelKeySetupDec(unsigned int rk[], const unsigned char cipherKey[], int keyBits);
void rijndaelDecrypt(const unsigned int rk[], int Nr, const unsigned char ct[16], unsigned char pt[16]);
void printUsage();
string createFilenameFromInformation(string filename, string path);

// Globals
unsigned char	drm_key[0x10];
static unsigned int rk[44];

/// Entrypoint
///
/// @return 0 ... success
///         1 ... Invalid intput filename
///         2 ... Cannot open intput file
///         3 ... Cannot open mdb
///         4 ... Cannot open key
///         5 ... Cannot open output
///         6 ... Invalid options
int main( int argc, char *argv[] )
{
   FILE *inputfp,*outputfp, *keyfp;
   
   unsigned char buf[BLOCK_SIZE];
   
   string outfile, filename;
// cout << setfill('=') << setw(80) << '=' << endl;
   cout << "Samsung TV PVR Recording DRM Decrypt (x64)" << endl << endl;

   switch (argc) {
      case 3:
         outfile = createFilenameFromInformation(argv[1], argv[2]);

      case 2:
         
         if(argc == 2)
            outfile = createFilenameFromInformation(argv[1], "");
      
         filename = argv[1];
         if(filename.length() < 3)
         {
            printUsage();
            return 1;
         }
      
         if((inputfp = fopen(argv[1], "rb")) == NULL){
            cout << "Cannot open intput file: '" << argv[1] << "'" << endl;
            return 2;
         }
      
         filename.replace(filename.end()-3, filename.end(), "mdb");
         keyfp = fopen(filename.c_str(), "rb");
         if(keyfp) { // try .mdb
            
            fseek(keyfp, 8, SEEK_SET);
            if(keyfp) {
               fread(&buf, sizeof(unsigned char), 0x10, keyfp);
               
               for (int j = 0; j < 0x10; j++)
                  drm_key[(j&0xc)+(3-(j&3))] = buf[j];      // reorder the bytes
               
               fclose(keyfp);
            } else {
               cout << "Cannot open mdb file: '" << filename << "'" << endl;
               return 3;
            }
         } else { // try .key
            
            filename.replace(filename.end()-3, filename.end(), "key");
            keyfp = fopen(filename.c_str(), "rb");
            if(keyfp){
               fread(drm_key, sizeof(unsigned char), 0x10, keyfp);
               fclose(keyfp);
            } else {
               cout << "Cannot open key file: '" << filename << "'" << endl;
               return 4;
            }
         }
         
         outputfp = fopen(outfile.c_str(), "wb");
         if(!outputfp){
            cout << "Cannot open output file for writing: '" << outfile << "'" << endl;
            return 5;
         }
      break;
      
      default:
         printUsage();
         return 6;
   }

   fseeko(inputfp,0,2);
   off64_t filesize = ftello (inputfp);
   rewind(inputfp);
   
// cout << setfill('=') << setw(80) << '=' << endl << endl;
   cout << outfile << endl << endl;
   cout << setfill(' ') << setw(13) << filesize << " bytes to decrypt (" << filesize/1024/1024 << " MB)"<< endl;

   fread(buf, sizeof(unsigned char), BLOCK_SIZE, inputfp);
   int sync_found=0;
   for(int i=0; i<(BLOCK_SIZE - PACKET_SIZE); ++i){
      if (buf[i] == 0x47){
         sync_found = 1;
         fseeko(inputfp,i,SEEK_SET);
         cout << "sync found at " << i << endl;
         break;
      }
   }
   if (sync_found) {
      
      float percent = 0.0;
      bool sync_lost = false;
      clock_t start = clock();

      rijndaelKeySetupDec(rk, drm_key, 128);
      
      for(off64_t i = 0, j = 0; i < filesize; i += BLOCK_SIZE, ++j) {
         
         fread(buf, sizeof(unsigned char), BLOCK_SIZE, inputfp);
         
         for (int n = 0; n < BLOCK_SIZE; n += PACKET_SIZE) {
            
            if (buf[n] != 0x47)  {
               
               if(!sync_lost)
               {
                  cout << "\r[" << setw(0) <<  "lost sync at " << i ;
                  sync_lost = true;
               }
               continue;
            
            } else {
            
               if(sync_lost) {
                  cout << setw(0) << endl << "found sync at " << i << endl;
                  sync_lost = false;
               }
            
               process_section (buf+n);
            }
         }
         
         if(!(j%500)) // update the progress bar every 500 times
         {
            percent = (PROGRESS_WIDTH/filesize)*i;
            cout  << "\r[" << setfill('=') << setw(int(percent+0.5f)) << ""
            << setfill(' ') << setw(PROGRESS_WIDTH+2-int(percent+0.5f)) << "] "
            << setprecision(3) << (100.0f/filesize)*i << " % "
            << setw(3) << setfill(' ') << "";
         }
         
         fwrite(buf,sizeof(unsigned char),BLOCK_SIZE, outputfp);
      }
      cout << endl << "duration: " << double(clock() - start)/CLOCKS_PER_SEC << " Sec." << endl;
   }
   
   fclose(inputfp);
   fclose(outputfp);

   return 1;
}

/// http://en.wikipedia.org/wiki/Mpeg_transport_stream#Packet
///
/// Name                            Bits  Values
///
/// sync byte                       8		0x47
///
/// Transport Error Indicator (TEI) 1     Set by demodulator if can't correct errors in the
///                                       stream, to tell the demultiplexer that the packet
///                                       has an uncorrectable error [11]
///
/// Payload  Unit Start Indicator   1     1 means start of PES data or PSI otherwise zero only.
///
/// Transport Priority              1     1 means higher priority than other packets with the
///                                       same PID.
///
/// PID                             13    Packet ID
///
/// Scrambling control              2     '00' = Not scrambled.   The following per DVB spec:[12]
///                                       '01' = Reserved for future use,
///                                       '10' = Scrambled with even key,
///                                       '11' = Scrambled with odd key
///
/// Adaptation field                1     1 means adaptation field exist
///
/// Contains payload                1     1 means payload exist
///
/// Continuity counter              4     Incremented only when a payload is present (i.e.,
///                                       adaptation field exist is 01 or 11)[13]
///
/// @note The total number of bits above is 32 and is called the transport stream 4-byte prefix
///       or Transport Stream Header.
inline unsigned char process_section (unsigned char *data) {
	
   // 0-3 ... Transport Stream Header
   if(!(data[3] & 0xC0))               // unencrypted
      return 0;
	
   int offset = 4;
    
   if(data[3] & 0x20)
      offset += (data[4] + 1);         // skip adaption field

   data[3] &= 0x3f;                    // remove scrambling bits
   
   int rounds = (PACKET_SIZE - offset) / 0x10;

   // AES CBC
   for (int i =  0; i < rounds; ++i)
      rijndaelDecrypt(rk, 10, data + offset + i * 0x10, data + offset + i * 0x10);
   
   return 1;
}

/// https://code.google.com/p/samy-pvr-manager/wiki/InfFileStructure
///
/// RAWTITLE   0x0100 - 0x0200
/// RECTIME    0x0304 - 0x0308
/// RECTITLE   0x0318 - 0x0418
string createFilenameFromInformation(string filename, string path)
{
   if (!path.length()) {
      size_t pos = filename.rfind(DIR_SEP)+1;
      path = string(filename.begin(), filename.begin() + pos);
   } else if(*(path.end()-1) != DIR_SEP) {
      path += DIR_SEP;
   }
   
   filename.replace(filename.end()-3, filename.end(), "inf");
   
   FILE *fp = NULL;
   
   fp = fopen(filename.c_str(), "rb");
   if(!fp) {
      cout << "Cannot open '" << filename << "' file" << endl;
      filename.replace(filename.end()-4, filename.end(), ".ts");
      return filename;
   }
   
   unsigned char buf[0x0418];
   //fseeko(keyfp,0x100,SEEK_SET);
   fread(buf, sizeof(unsigned char), 0x0418, fp);
   fclose(fp);

   stringstream recordtitle;
   recordtitle << path;
   
   // try to read the raw title
   for (int n=0x0100; n<0x0200; ++n) {
      switch (buf[n]) {
         case 0:
         case ':':
         case DIR_SEP: continue;
         default : recordtitle << buf[n]; break;
      }
   }
      
   if(!recordtitle.str().length()) {
      // read the record title
      for (int n=0x318; n<0x418; ++n) {
         switch (buf[n]) {
            case 0:
            case ':':
            case DIR_SEP: continue;
            default : recordtitle << buf[n]; break;
         }
      }
   }
      
   if(!recordtitle.str().length()) {
         
      size_t pos = filename.rfind(DIR_SEP)+1;
      filename = filename.substr(pos);
      filename.replace(filename.end()-4, filename.end(), "");
      recordtitle << filename;
   }
   
   // get the record time
   char recordtime [80]; memset(recordtime, 0, 80);
   time_t t = (buf[0x0307] << 24) + (buf[0x0306] << 16) + (buf[0x0305] << 8) + buf[0x0304];
   strftime (recordtime, 80, " %Y-%m-%d %H-%M", localtime (&t));
   
   // check existing filename; append number if neccessary
   filename = recordtitle.str() + recordtime + ".ts";
   if((fp = fopen(filename.c_str(), "rb")) != NULL) {
      fclose(fp);
      int n = 1;
      stringstream num;
      num << '_' << setfill('0') << setw(4) << n;
      filename = recordtitle.str() + recordtime + num.str() + ".ts";

      while ((fp = fopen(filename.c_str(), "rb")) != NULL) {
         fclose(fp);
         
         num.str(string());
         num << '_' << setfill('0') << setw(4) << ++n;
         filename = recordtitle.str() + recordtime + num.str() + ".ts";
         
         if(n>1000)
         {
            cout << "Did not find a valid Filename after " << n-1 << " tries" << endl;
            return "";
         }
      }
   }

   return filename;
}


void printUsage()
{
   cout << "usage: DRMDecrypt full_path_to_srf_file [output_path]" << endl << endl;
   cout << "The mdb file must also be present in the" << endl;
   cout << "current working directory" << endl << endl;
}