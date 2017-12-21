/*! @file HeroineCLI.c
 *
 *  @brief Convert gopro MPç to GPX
 *
 *  @version 0.2
 *
 *  (C) Copyright 2017 OrbisTerrae
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

#include "GPMF_parser.h"
#include "GPMF_mp4reader.h"


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
    int min =0;
    int sec =0;
    double msec =0;
    int hour = 0;
    int fractpart, intpart;
    float tempc = -273.15;

    time(&timer);
    tm_info = localtime(&timer);
    
    strftime(TimeStr, 26, "%Y-%m-%dT%H:%M:%SZ", tm_info);

	// get file return data
	if (argc != 2)
	{
		printf("usage: %s <file_with_GPMF>\n", argv[0]);
		return -1;
	}

	metadatalength = OpenGPMFSource(argv[1]);

	if (metadatalength > 0.0)
	{
		uint32_t index, payloads = GetNumberGPMFPayloads();

        printf("<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"no\" ?>\n");
        printf("<gpx xmlns=\"http://www.topografix.com/GPX/1/1\"\n");
        printf("     xmlns:gpxx=\"http://www.garmin.com/xmlschemas/GpxExtensions/v3\"\n");
        printf("     xmlns:gpxtrkx=\"http://www.garmin.com/xmlschemas/TrackStatsExtension/v1\"\n");
        printf("     xmlns:gpxtrkoffx=\"http://www.garmin.com/xmlschemas/TrackMovieOffsetExtension/v1\"\n");
        printf("     xmlns:wptx1=\"http://www.garmin.com/xmlschemas/WaypointExtension/v1\"\n");
        printf("     xmlns:gpxtpx=\"http://www.garmin.com/xmlschemas/TrackPointExtension/v1\"\n");
        printf("     xmlns:gpxpx=\"http://www.garmin.com/xmlschemas/PowerExtension/v1\"\n");
        printf("     xmlns:gpxacc=\"http://www.garmin.com/xmlschemas/AccelerationExtension/v1\"\n");
        printf("     creator=\"VIRB Elite\"\n");
        printf("     version=\"1.1\"\n");
        printf("     xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\">\n");
        printf("     <metadata>\n");
        printf("       <link href=\"https://sites.google.com/site/oterrae/\">\n");
        printf("          <text>Orbis Terrae</text>\n");
        printf("       </link>\n");
        printf("       <time>%s</time>\n",TimeStr);
        printf("     </metadata>\n");
        printf("<trk>\n");
        printf("<name>%s</name>\n", argv[1]);
        printf("     <extensions>\n");
        printf("         <gpxx:TrackExtension>\n");
        printf("         <gpxx:DisplayColor>Cyan</gpxx:DisplayColor>\n");
        printf("         </gpxx:TrackExtension>\n");
        printf("         <gpxtrkoffx:TrackMovieOffsetExtension>\n");
        printf("         <gpxtrkoffx:StartOffsetSecs>0</gpxtrkoffx:StartOffsetSecs>\n");
        printf("         </gpxtrkoffx:TrackMovieOffsetExtension>\n");
        printf("     </extensions>\n");
        

		for (index = 0; index < payloads; index++)
		{
            // Find all the available Streams and compute they sample rates
            while (GPMF_OK == GPMF_FindNext(ms, GPMF_KEY_STREAM, GPMF_RECURSE_LEVELS))
            {
                if (GPMF_OK == GPMF_SeekToSamples(ms)) //find the last FOURCC within the stream
                {
                    uint32_t fourcc = GPMF_Key(ms);
                    double rate = GetGPMFSampleRate(fourcc, GPMF_SAMPLE_RATE_PRECISE);// GPMF_SAMPLE_RATE_FAST);
                    //printf("%c%c%c%c sampling rate = %f Hz\n", PRINTF_4CC(fourcc), rate);
                    if(fourcc == STR2FOURCC("GPS5")){
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
				while (GPMF_OK == GPMF_FindNext(ms, GPMF_KEY_STREAM, GPMF_RECURSE_LEVELS))
				{
					if (GPMF_OK == GPMF_SeekToSamples(ms)) //find the last FOURCC within the stream
					{
						uint32_t key = GPMF_Key(ms);
						GPMF_SampleType type = GPMF_Type(ms);
						uint32_t elements = GPMF_ElementsInStruct(ms);
						//uint32_t samples = GPMF_Repeat(ms);
						uint32_t samples = GPMF_PayloadSampleCount(ms);

						if (samples)
						{
							printf("<!-- STRM of %c%c%c%c found -->\n", PRINTF_4CC(key));

							if (type == GPMF_TYPE_COMPLEX)
							{
								GPMF_stream find_stream;
								GPMF_CopyState(ms, &find_stream);

								if (GPMF_OK == GPMF_FindPrev(&find_stream, GPMF_KEY_TYPE, GPMF_CURRENT_LEVEL))
								{
									char tmp[64];
									char *data = (char *)GPMF_RawData(&find_stream);
									int size = GPMF_RawDataSize(&find_stream);

									if (size < sizeof(tmp))
									{
										memcpy(tmp, data, size);
										tmp[size] = 0;
								//		printf("of type %s ", tmp);
									}
								}

							}
							else
							{
								//printf("of type %c ", type);
							}
						}
					}
				}
				GPMF_ResetState(ms);
				printf("\n");
			}


            printf("<trkseg>\n");

	// Find GPS values and return scaled doubles.

            
            printf("<!-- frame %d -->\n", index);
            if (GPMF_OK == GPMF_FindNext(ms, STR2FOURCC("GPS5"), GPMF_RECURSE_LEVELS) || //GoPro Hero5 GPS
                GPMF_OK == GPMF_FindNext(ms, STR2FOURCC("GPRI"), GPMF_RECURSE_LEVELS) || //GoPro Karma
                GPMF_OK == GPMF_FindNext(ms, STR2FOURCC("TMPC"), GPMF_RECURSE_LEVELS))   //GoPro temp
				{
					uint32_t key = GPMF_Key(ms);
					uint32_t samples = GPMF_Repeat(ms);
					uint32_t elements = GPMF_ElementsInStruct(ms);
					uint32_t buffersize = samples * elements * sizeof(double);
					GPMF_stream find_stream;
					double *ptr, *tmpbuffer = malloc(buffersize);
					char units[10][6] = { "" };
					uint32_t unit_samples = 1;

//                    printf("MP4 Payload time %.3f to %.3f seconds\n", in, out);
//SRT                    printf("00:00:%.3f --> 00:00:%.3f\n", in, out);

					if (tmpbuffer && samples)
					{
						uint32_t i, j;

						//Search for any units to display
						GPMF_CopyState(ms, &find_stream);
						if (GPMF_OK == GPMF_FindPrev(&find_stream, GPMF_KEY_SI_UNITS, GPMF_CURRENT_LEVEL) ||
							GPMF_OK == GPMF_FindPrev(&find_stream, GPMF_KEY_UNITS, GPMF_CURRENT_LEVEL))
						{
							char *data = (char *)GPMF_RawData(&find_stream);
							int ssize = GPMF_StructSize(&find_stream);
							unit_samples = GPMF_Repeat(&find_stream);

							for (i = 0; i < unit_samples; i++)
							{
								memcpy(units[i], data, ssize);
								units[i][ssize] = 0;
								data += ssize;
							}
						}

						//GPMF_FormattedData(ms, tmpbuffer, buffersize, 0, samples); // Output data in LittleEnd, but no scale
						GPMF_ScaledData(ms, tmpbuffer, buffersize, 0, samples, GPMF_TYPE_DOUBLE);  //Output scaled data as floats
                        
                       
						ptr = tmpbuffer;
                        if(key == STR2FOURCC("TMPC")){
                            printf ("<!--     TMPC Temperature detected  ------------------------------------------------------------------------- -->\n");
                            tempc = ptr[0];
                            //not tested, likely to break
                        }

                        /* There are 18 samples (18.177936Hz) per 1s data     */
                        strftime(TimeStr, 26, "%Y-%m-%dT", tm_info);
                        // Time must be in the following format:  <time>2017-12-19T18:22:01.001Z</time>
                        
                        // first sample of the 1 second with extension - potentially
                        printf("  <trkpt lat=\"%.14f\" lon=\"%.14f\">\n", ptr[0],ptr[1]);
                        printf("    <ele>%f</ele>\n", ptr[2]);
                        //printf("    <2DSpeed>%f</2dSpeed>\n", ptr[3]);
                        //printf("    <3DSpeed>%f</3dSpeed>\n", ptr[4]);
                        msec = (in)*1000;
                        sec = (int) (msec / 1000) % 60;
                        min = (int) ((int)(msec / (1000*60)) % 60);
                        hour = (int) ((int)(msec / (1000*60*60)) % 24);
                        double2Ints(msec/1000, 3, &intpart, &fractpart);
                        
                        printf("    <time>%s%02D:%02D:%02D.%03DZ</time>\n", TimeStr, hour, min, sec, fractpart);
                        if(tempc >= -273){
                            printf("   <extensions>\n");
                            printf("    <gpxtpx:TrackPointExtension>\n");
                            printf("         <gpxtpx:atemp>%f</gpxtpx:atemp>\n",tempc);
                            printf("         <gpxtpx:hr>%d</gpxtpx:hr>\n",75);
                            printf("     </gpxtpx:TrackPointExtension>\n");
                            printf("   </extensions>\n");
                        }
                        printf("  </trkpt>\n");
                        
                        //following 17 samples without extension
						for (i = 1; i < samples; i++)
						{
                            printf("<-- %c%c%c%c lat:%.2f lon:%.2f alt:%.2f 2DS:%.2f 3DS:%.2f -->\n", PRINTF_4CC(key),ptr[0],ptr[1],ptr[2],ptr[3],ptr[4]);
                            printf("  <trkpt lat=\"%.14f\" lon=\"%.14f\">\n", ptr[0],ptr[1]);
                            printf("    <ele>%f</ele>\n", ptr[2]);
                            //printf("    <2DSpeed>%f</2dSpeed>\n", ptr[3]);
                            //printf("    <3DSpeed>%f</3dSpeed>\n", ptr[4]);
                            
                            msec = (in + (i/GPSrate))*1000;
                            sec = (int) (msec / 1000) % 60;
                            min = (int) ((int)(msec / (1000*60)) % 60);
                            hour = (int) ((int)(msec / (1000*60*60)) % 24);
                            double2Ints(msec/1000, 3, &intpart, &fractpart);
                            printf("    <time>%s%02D:%02D:%02D.%03DZ</time>\n", TimeStr, hour, min, sec, fractpart);
                            printf("  </trkpt>\n");
                            
						}
                
                        
						free(tmpbuffer);
					}
				}
				GPMF_ResetState(ms);
				printf("\n");


		}
        /*
        </trkseg>
        </trk>
        </gpx>
         */
        
        printf("  </trkseg>\n"
               "</trk>\n"
               "</gpx>\n");



	cleanup:
		if (payload) FreeGPMFPayload(payload); payload = NULL;
		CloseGPMFSource();
	}

	return ret;
}
