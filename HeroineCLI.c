/*! @file HeroineCLI.c
 *
 *  @brief Extract GPX from GoPro Hero5 MP4.
 *
 *  @version 0.2
 *
 *  (C) Copyright 2018 OrbisTerrae
 *	
 *  Licensed under either:
 *  - Apache License, Version 2.0, http://www.apache.org/licenses/LICENSE-2.0  
 *  - MIT license, http://opensource.org/licenses/MIT
 *  at your option.
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <unistd.h>
#include <ctype.h>
#include <math.h>
#include <libgen.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "GPMF_parser.h"
#include "GPMF_mp4reader.h"

#define VERSION "0.3"


#define INIT_TEMP -273 // should be set to -273 to prevent Temp end HR - to be passed as a cmd line argument
#define HR_BASE 65
#define PI 3.14159265


extern void PrintGPMF(GPMF_stream *ms);

void double2Ints(double f, int p, int *i, int *d)
{
    // f = float, p=decimal precision, i=integer, d=decimal
    int   li;
    int   prec=1;
    
    for(int x=p;x>0;x--)
    {
        prec*=10;
    };  // same as power(10,p)
    
    li = (int) f;              // get integer part
    *d = (int) ((f-li)*prec);  // get decimal part
    *i = li;
}

char* concat(const char *s1, const char *s2)
{
    char *result = malloc(strlen(s1)+strlen(s2)+1);//+1 for the null-terminator
    //in real code you would check for errors in malloc here
    strcpy(result, s1);
    strcat(result, s2);
    return result;
}

int main(int argc, char *argv[])
{
    int32_t ret = GPMF_OK;
	GPMF_stream metadata_stream, *ms = &metadata_stream;
	double metadatalength;
	uint32_t *payload = NULL; //buffer to store GPMF samples from the MP4.
    time_t timer;
    struct tm* tm_info;
    double GPSrate = 18.177936;
    char TimeStr[26];
    char* f_output_GPX=NULL;
    char* f_output_KML=NULL;
    char* f_input;
    int verbose = 0;
    int min =0;
    int sec =0;
    double msec =0;
    int hour = 0;
    int fractpart, intpart;
    float tempc = INIT_TEMP;
    int hr = HR_BASE;
    int AltInit = 0;
    int c;
    FILE *fp_GPX = NULL;
    FILE *fp_KML = NULL;
    char* version = VERSION;
    int init_temp = INIT_TEMP;
    int hr_base = HR_BASE;
    char* basec;
    struct stat st;
    
    while ((c = getopt (argc, argv, "vht:r:o:")) != -1)
     switch (c)
       {
       case 'v':
         verbose = 1;
         break;
       case 't':
           init_temp = atoi(optarg);
         break;
       case 'r':
         hr_base = atoi(optarg);
         break;
       case 'o':
         f_output_GPX = optarg;
         f_output_KML = optarg;
         break;
       case 'h':
         fprintf (stderr, "Usage: %s [-h] [-v] [-t Temperature] [-r HearRate] [-o Output] GOPRO_WITH_GPMF.MP4\n", argv[0]);
         return 1;
       case '?':
         if (optopt == 't'  || optopt == 'r'  || optopt == 'o' )
           fprintf (stderr, "Option -%c requires an argument.\n", optopt);
         else if (isprint (optopt))
           fprintf (stderr, "Unknown option `-%c'.\n", optopt);
         else
           fprintf (stderr,
                    "Unknown option character `\\x%x'.\n",
                    optopt);
         return 1;
       default:
         abort ();
       }
     if(optind < argc){
         f_input = argv[argc-1];
         
     }
     else {
         fprintf (stderr, "Usage: %s [-h] [-v] [-t Temperature] [-r HearRate] [-o Output] GOPRO_WITH_GPMF.MP4\n", argv[0]);
         return 1;
    }

//    time(&timer);
//    tm_info = localtime(&timer);
    
    
    if( stat(f_input, &st) != 0 )
        perror("stat failed");
#ifdef _DARWIN_FEATURE_64_BIT_INODE
    tm_info = localtime(&st.st_birthtimespec); 
#else
    tm_info = localtime(&st.st_ctime);
#endif
    
    if(verbose) 
        printf("File time and date: %s", asctime(tm_info));
    
    strftime(TimeStr, 26, "%Y-%m-%dT%H:%M:%SZ", tm_info);

    metadatalength = OpenGPMFSource(f_input);

    if (metadatalength > 0.0) {
        if (verbose) printf("Orbis Terrae %s %s\n", argv[0], version);
        uint32_t index, payloads = GetNumberGPMFPayloads();
        if (f_output_GPX == NULL) {
            if (verbose) printf("no file output mentioned, using %s.GPX %s.KML\n", f_input, f_input);
            f_output_GPX = concat(f_input, ".GPX");
            f_output_KML = concat(f_input, ".KML");
        }
        else{
            f_output_GPX = concat(f_output_GPX, ".GPX");
            f_output_KML = concat(f_output_KML, ".KML");
        }
        fp_GPX = fopen(f_output_GPX, "w");
        if (fp_GPX) {
            //fprintf (stderr, "%s opened\n", f_output); 
        } else {
            fprintf(stderr, "ERROR: %s not opened\n", f_output_GPX);
            return -1;
        }
        
        fp_KML = fopen(f_output_KML, "w");
        if (fp_KML) {
            //fprintf (stderr, "%s opened\n", f_output); 
        } else {
            fprintf(stderr, "ERROR: %s not opened\n", f_output_KML);
            return -1;
        }

        basec = basename(f_input);
                   
        fprintf(fp_GPX, "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"no\" ?>\n");
        fprintf(fp_GPX, "<gpx xmlns=\"http://www.topografix.com/GPX/1/1\"\n");
        fprintf(fp_GPX, "     xmlns:gpxx=\"http://www.garmin.com/xmlschemas/GpxExtensions/v3\"\n");
        fprintf(fp_GPX, "     xmlns:gpxtrkx=\"http://www.garmin.com/xmlschemas/TrackStatsExtension/v1\"\n");
        fprintf(fp_GPX, "     xmlns:gpxtrkoffx=\"http://www.garmin.com/xmlschemas/TrackMovieOffsetExtension/v1\"\n");
        fprintf(fp_GPX, "     xmlns:wptx1=\"http://www.garmin.com/xmlschemas/WaypointExtension/v1\"\n");
        fprintf(fp_GPX, "     xmlns:gpxtpx=\"http://www.garmin.com/xmlschemas/TrackPointExtension/v1\"\n");
        fprintf(fp_GPX, "     xmlns:gpxpx=\"http://www.garmin.com/xmlschemas/PowerExtension/v1\"\n");
        fprintf(fp_GPX, "     xmlns:gpxacc=\"http://www.garmin.com/xmlschemas/AccelerationExtension/v1\"\n");
        fprintf(fp_GPX, "     creator=\"VIRB Elite\"\n");
        fprintf(fp_GPX, "     version=\"1.1\"\n");
        fprintf(fp_GPX, "     xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\">\n");
        fprintf(fp_GPX, "     <metadata>\n");
        fprintf(fp_GPX, "       <link href=\"https://sites.google.com/site/oterrae/\">\n");
        fprintf(fp_GPX, "          <text>Orbis Terrae</text>\n");
        fprintf(fp_GPX, "       </link>\n");
        fprintf(fp_GPX, "       <time>%s</time>\n", TimeStr);
        fprintf(fp_GPX, "     </metadata>\n");
        fprintf(fp_GPX, "<trk>\n");
        fprintf(fp_GPX, "  <name>%s</name>\n", basec);
        fprintf(fp_GPX, "       <extensions>\n");
        fprintf(fp_GPX, "           <gpxx:TrackExtension>\n");
        fprintf(fp_GPX, "              <gpxx:DisplayColor>Cyan</gpxx:DisplayColor>\n");
        fprintf(fp_GPX, "           </gpxx:TrackExtension>\n");
        fprintf(fp_GPX, "           <gpxtrkoffx:TrackMovieOffsetExtension>\n");
        fprintf(fp_GPX, "              <gpxtrkoffx:StartOffsetSecs>0</gpxtrkoffx:StartOffsetSecs>\n");
        fprintf(fp_GPX, "           </gpxtrkoffx:TrackMovieOffsetExtension>\n");
        fprintf(fp_GPX, "       </extensions>\n");
        fprintf(fp_GPX, "  <trkseg>\n");
        
        strftime(TimeStr, 26, "%d-%m-%Y %H:%M:%S", tm_info);
        fprintf(fp_KML, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
        fprintf(fp_KML, "<kml  xmlns=\"http://earth.google.com/kml/2.2\" xmlns:gx=\"http://www.google.com/kml/ext/2.2\" xmlns:trails=\"http://www.google.com/kml/trails/1.0\">\n");
        fprintf(fp_KML, " <Document>\n");
        fprintf(fp_KML, "  <name>HeroineGPS %s</name>\n", basec);
        fprintf(fp_KML, "  <Placemark>\n");
        fprintf(fp_KML, "   <name>%s %s</name>\n",argv[0],TimeStr);
        fprintf(fp_KML, "    <Style id=\"OrbisTerrae\">\n");
        fprintf(fp_KML, "        <BalloonStyle>\n");
        fprintf(fp_KML, "        <bgColor>DF8DB2D8</bgColor>\n");
        fprintf(fp_KML, "        <textColor>FF134E96</textColor>\n");
        fprintf(fp_KML, "        <text>File: %s<br/>", basec);
        fprintf(fp_KML, "              Date: %2d-%2d-%d<br/>", tm_info->tm_mday, tm_info->tm_mon+1,tm_info->tm_year+1900);
        if(tempc >= -150){
            fprintf(fp_KML, "Temp: %f°C<br/>",tempc);
        }
        fprintf(fp_KML, "              https://sites.google.com/site/oterrae/<br/>");
        fprintf(fp_KML, "              %s v%s<br/>", argv[0],version); 
        fprintf(fp_KML, "              Copyright © 2017-%d Orbis Terrae<br/>",tm_info->tm_year+1900);
        fprintf(fp_KML, "              All rights reserved.</text>\n");
        fprintf(fp_KML, "        </BalloonStyle>\n");
        
        fprintf(fp_KML, "     <LineStyle>\n");
        fprintf(fp_KML, "      <color>DF8DB2D8</color>\n"); 
        fprintf(fp_KML, "      <width>5</width>\n");
        fprintf(fp_KML, "     </LineStyle>\n");
        fprintf(fp_KML, "     <PolyStyle>\n");
        fprintf(fp_KML, "        <color>0f00ff00</color>\n");
        fprintf(fp_KML, "     </PolyStyle>\n");
        fprintf(fp_KML, "    <LabelStyle>\n");
        fprintf(fp_KML, "        <color>EF134E96</color>\n");
        fprintf(fp_KML, "     </LabelStyle>\n");
        fprintf(fp_KML, "     <IconStyle>\n");
        fprintf(fp_KML, "        <color>EF134E96</color>\n");
        fprintf(fp_KML, "        <scale>1</scale>\n");
        fprintf(fp_KML, "        <Icon>\n");
        fprintf(fp_KML, "          <href>http://maps.google.com/mapfiles/kml/shapes/arrow.png</href>\n");
        fprintf(fp_KML, "        </Icon>\n");
        fprintf(fp_KML, "      </IconStyle>\n");
        fprintf(fp_KML, "    </Style>\n");
        fprintf(fp_KML, "    <MultiGeometry>\n");
        fprintf(fp_KML, "     <styleUrl>#OrbisTerrae</styleUrl>\n");
        fprintf(fp_KML, "     <visibility>1</visibility>\n");
        
        
        

        for (index = 0; index < payloads; index++) {
            // Find all the available Streams and compute they sample rates
            while (GPMF_OK == GPMF_FindNext(ms, GPMF_KEY_STREAM, GPMF_RECURSE_LEVELS)) {
                if (GPMF_OK == GPMF_SeekToSamples(ms)) //find the last FOURCC within the stream
                {
                    uint32_t fourcc = GPMF_Key(ms);
                    double rate = GetGPMFSampleRate(fourcc, GPMF_SAMPLE_RATE_PRECISE); // GPMF_SAMPLE_RATE_FAST);
                    if (verbose) {
                        if (index <= 1) {
                            printf("%c%c%c%c sampling rate = %f Hz\n", PRINTF_4CC(fourcc), rate);
                        }
                    }
                    if (fourcc == STR2FOURCC("GPS5")) {
                        GPSrate = rate;
                    }
                }
            }

            uint32_t payloadsize = GetGPMFPayloadSize(index);
            double in = 0.0, out = 0.0; //times
            payload = GetGPMFPayload(payload, index);
            if (payload == NULL)
                goto cleanup;

            ret = GetGPMFPayloadTime(index, &in, &out);
            if (ret != GPMF_OK)
                goto cleanup;

            ret = GPMF_Init(ms, payload, payloadsize);
            if (ret != GPMF_OK)
                goto cleanup;

            // Find all the available Streams and the data carrying FourCC
            if (index == 0) // show first payload 
            {
                while (GPMF_OK == GPMF_FindNext(ms, GPMF_KEY_STREAM, GPMF_RECURSE_LEVELS)) {
                    if (GPMF_OK == GPMF_SeekToSamples(ms)) //find the last FOURCC within the stream
                    {
                        uint32_t key = GPMF_Key(ms);
                        GPMF_SampleType type = GPMF_Type(ms);
                        uint32_t elements = GPMF_ElementsInStruct(ms);
                        //uint32_t samples = GPMF_Repeat(ms);
                        uint32_t samples = GPMF_PayloadSampleCount(ms);

                        if (samples) {
                            if (verbose) printf("STRM of %c%c%c%c found\n", PRINTF_4CC(key));

                            if (type == GPMF_TYPE_COMPLEX) {
                                GPMF_stream find_stream;
                                GPMF_CopyState(ms, &find_stream);

                                if (GPMF_OK == GPMF_FindPrev(&find_stream, GPMF_KEY_TYPE, GPMF_CURRENT_LEVEL)) {
                                    char tmp[64];
                                    char *data = (char *) GPMF_RawData(&find_stream);
                                    int size = GPMF_RawDataSize(&find_stream);

                                    if (size < sizeof (tmp)) {
                                        memcpy(tmp, data, size);
                                        tmp[size] = 0;
                                    }
                                }

                            } else {
                            }
                        }
                    }
                }
                GPMF_ResetState(ms);
                fprintf(fp_GPX, "\n");
            }

            fprintf(fp_GPX, "<!--                                frame %09D                          -->\n", index);
            if (verbose) printf("%06ds ", index);


            if (GPMF_OK == GPMF_FindNext(ms, STR2FOURCC("GPS5"), GPMF_RECURSE_LEVELS) || //GoPro Hero5 GPS
                    GPMF_OK == GPMF_FindNext(ms, STR2FOURCC("GPRI"), GPMF_RECURSE_LEVELS) || //GoPro Karma
                    GPMF_OK == GPMF_FindNext(ms, STR2FOURCC("TMPC"), GPMF_RECURSE_LEVELS)) //GoPro temp
            {
                uint32_t key = GPMF_Key(ms);
                uint32_t samples = GPMF_Repeat(ms);
                uint32_t elements = GPMF_ElementsInStruct(ms);
                uint32_t buffersize = samples * elements * sizeof (double);
                GPMF_stream find_stream;
                double *ptr, *tmpbuffer = malloc(buffersize);
                char units[10][6] = {""};
                uint32_t unit_samples = 1;

                if (tmpbuffer && samples) {
                    uint32_t i, j;

                    //Search for any units to display
                    GPMF_CopyState(ms, &find_stream);
                    if (GPMF_OK == GPMF_FindPrev(&find_stream, GPMF_KEY_SI_UNITS, GPMF_CURRENT_LEVEL) ||
                            GPMF_OK == GPMF_FindPrev(&find_stream, GPMF_KEY_UNITS, GPMF_CURRENT_LEVEL)) {
                        char *data = (char *) GPMF_RawData(&find_stream);
                        int ssize = GPMF_StructSize(&find_stream);
                        unit_samples = GPMF_Repeat(&find_stream);

                        for (i = 0; i < unit_samples; i++) {
                            memcpy(units[i], data, ssize);
                            units[i][ssize] = 0;
                            data += ssize;
                        }
                    }

                    //GPMF_FormattedData(ms, tmpbuffer, buffersize, 0, samples); // Output data in LittleEnd, but no scale
                    GPMF_ScaledData(ms, tmpbuffer, buffersize, 0, samples, GPMF_TYPE_DOUBLE); //Output scaled data as floats


                    ptr = tmpbuffer;
                    if (key == STR2FOURCC("TMPC")) {
                        printf("<!--     TMPC Temperature detected   -->\n");
                        tempc = ptr[0];
                        //not tested, likely to break
                    }
                    if (index == 0) {
                        AltInit = ptr[2];
                    }
                    /* There are 18 samples (18.177936Hz) per 1s data     */
                    strftime(TimeStr, 26, "%Y-%m-%dT", tm_info);
                    // Time must be in the following format:  <time>2017-12-19T18:22:01.001Z</time>

                    // first sample of the 1 second with extension - potentially
                    fprintf(fp_GPX, "    <trkpt lat=\"%.14f\" lon=\"%.14f\">\n", ptr[0], ptr[1]);
                    fprintf(fp_GPX, "        <ele>%.2f</ele>\n", ptr[2]);

                    tempc = init_temp - ((ptr[2] - AltInit) / 100) + ((rand() % 10) / 10) ;
                    hr = hr_base + sin(index * PI / 180) * 10 + rand() % 5;
                    
                    msec = (in)*1000 + tm_info->tm_sec *1000+ tm_info->tm_min *60*1000 + tm_info->tm_hour*60*60*1000;
                    sec = (int) ((msec / 1000)) % 60;
                    min = (int) ((int) (msec / (1000 * 60)) % 60);
                    hour = (int) ((int) (msec / (1000 * 60 * 60)) % 24);
                    double2Ints(msec / 1000, 3, &intpart, &fractpart);

                    fprintf(fp_GPX, "        <time>%s%02D:%02D:%02D.%03DZ</time>\n", TimeStr, hour, min, sec, fractpart);

                    if (tempc >= -150) {
                        fprintf(fp_GPX, "     <extensions>\n");
                        fprintf(fp_GPX, "      <gpxtpx:TrackPointExtension>\n");
                        fprintf(fp_GPX, "           <gpxtpx:atemp>%.2f</gpxtpx:atemp>\n", tempc);
                        fprintf(fp_GPX, "           <gpxtpx:hr>%d</gpxtpx:hr>\n", hr);
                        fprintf(fp_GPX, "       </gpxtpx:TrackPointExtension>\n");
                        fprintf(fp_GPX, "     </extensions>\n");
                        if (verbose) printf("Lat: %.14f - Long:%.14f - Alt:%.2fm (Temp:%.2f° - HR:%d)\n", ptr[0], ptr[1], ptr[2], tempc, hr);
                    }else{
                        if (verbose) printf("Lat: %.14f - Long:%.14f - Alt:%.2fm\n", ptr[0], ptr[1], ptr[2]);
                    }                    
                    fprintf(fp_GPX, "    </trkpt>\n");
                    
                    if(index == 0){
                        fprintf(fp_KML,"     <Point><coordinates>%.14f,%.14f,%.14f</coordinates></Point>\n",ptr[1],ptr[0],ptr[2]); 
                        fprintf(fp_KML,"     <LineString>\n");
                        fprintf(fp_KML,"      <tessellate>1</tessellate>\n");
                        fprintf(fp_KML,"      <altitudeMode>clampToGround</altitudeMode>\n");
                        fprintf(fp_KML,"       <coordinates>\n");
                    }
                    else
                    {
                        fprintf(fp_KML,"%.14f,%.14f,%.14f ",ptr[1],ptr[0],ptr[2]); 
                    }
                    

                    //following 17 samples without extension
                    for (i = 1; i < samples; i++) {
                        fprintf(fp_GPX, "<!-- %c%c%c%c lat:%.2f lon:%.2f alt:%.2f 2DS:%.2f 3DS:%.2f -->\n", PRINTF_4CC(key), ptr[0], ptr[1], ptr[2], ptr[3], ptr[4]);
                        fprintf(fp_GPX, "    <trkpt lat=\"%.14f\" lon=\"%.14f\">\n", ptr[0], ptr[1]);
                        fprintf(fp_GPX, "      <ele>%.02f</ele>\n", ptr[2]);

                        msec = (in + (i / GPSrate))*1000 + tm_info->tm_sec *1000+ tm_info->tm_min *60*1000 + tm_info->tm_hour*60*60*1000;
                        sec = (int) (msec / 1000) % 60;
                        min = (int) ((int) (msec / (1000 * 60)) % 60);
                        hour = (int) ((int) (msec / (1000 * 60 * 60)) % 24);
                        double2Ints(msec / 1000, 3, &intpart, &fractpart);
                        fprintf(fp_GPX, "      <time>%s%02D:%02D:%02D.%03DZ</time>\n", TimeStr, hour, min, sec, fractpart);
                        /*
                         if (tempc >= -150) {
                        fprintf(fp, "     <extensions>\n");
                        fprintf(fp, "      <gpxtpx:TrackPointExtension>\n");
                        fprintf(fp, "           <gpxtpx:atemp>%.2f</gpxtpx:atemp>\n", tempc);
                        fprintf(fp, "           <gpxtpx:hr>%d</gpxtpx:hr>\n", hr);
                        fprintf(fp, "       </gpxtpx:TrackPointExtension>\n");
                        fprintf(fp, "     </extensions>\n");
                       }
                        */
                      fprintf(fp_GPX, "    </trkpt>\n");

                    }


                    free(tmpbuffer);
                }
            }
            GPMF_ResetState(ms);
            fprintf(fp_GPX, "\n");


        }

        fprintf(fp_GPX, "    </trkseg>\n"
                "  </trk>\n"
                "</gpx>\n");
        
        fprintf(fp_KML,     
                    "       </coordinates>\n"
                    "      </LineString>\n"
                    "     </MultiGeometry>\n"
                    "   </Placemark>\n"
                    " </Document>\n"
                    "</kml>");
        
        fclose(fp_GPX);
        fclose(fp_KML);
        if (verbose) printf("%ds written to %s\n", index, f_output_GPX);

cleanup:
        if (payload) FreeGPMFPayload(payload);
        payload = NULL;
        CloseGPMFSource();
    }
    
    if(f_output_KML != NULL) free(f_output_KML);
    if(f_output_GPX != NULL) free(f_output_GPX);
    
    return ret;
}
