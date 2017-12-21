/*! @file GPMF_demo.c
 *
 *  @brief Demo to extract GPMF from an MP4
 *
 *  @version 1.0.1
 *
 *  (C) Copyright 2017 GoPro Inc (http://gopro.com/).
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

	//metadatalength = OpenGPMFSourceUDTA(argv[1]);

	if (metadatalength > 0.0)
	{
		uint32_t index, payloads = GetNumberGPMFPayloads();

    /*
     <?xml version="1.0" encoding="UTF-8" standalone="no" ?>
     <gpx xmlns="http://www.topografix.com/GPX/1/1"
     xmlns:gpxx="http://www.garmin.com/xmlschemas/GpxExtensions/v3"
     xmlns:gpxtrkx="http://www.garmin.com/xmlschemas/TrackStatsExtension/v1"
     xmlns:gpxtrkoffx="http://www.garmin.com/xmlschemas/TrackMovieOffsetExtension/v1"
     xmlns:wptx1="http://www.garmin.com/xmlschemas/WaypointExtension/v1"
     xmlns:gpxtpx="http://www.garmin.com/xmlschemas/TrackPointExtension/v1"
     xmlns:gpxpx="http://www.garmin.com/xmlschemas/PowerExtension/v1"
     xmlns:gpxacc="http://www.garmin.com/xmlschemas/AccelerationExtension/v1"
     creator="VIRB Elite"
     version="1.1"
     xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance">
     <metadata>
     <link href="https://sites.google.com/site/oterrae/">
     <text>Orbis Terrae</text>
     </link>
     <time>2016-02-21T10:56:06Z</time>
     </metadata>
     <trk>
     */
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

        /*
         <name>DJI_0001.SRT.GPX</name>
         <extensions>
         <gpxx:TrackExtension>
         <gpxx:DisplayColor>Cyan</gpxx:DisplayColor>
         </gpxx:TrackExtension>
         <gpxtrkoffx:TrackMovieOffsetExtension>
         <gpxtrkoffx:StartOffsetSecs>0</gpxtrkoffx:StartOffsetSecs>
         </gpxtrkoffx:TrackMovieOffsetExtension>
         </extensions>
         <trkseg>
         */
        
        printf("<name>%s</name>\n", argv[1]);
        printf("     <extensions>\n");
        printf("         <gpxx:TrackExtension>\n");
        printf("         <gpxx:DisplayColor>Cyan</gpxx:DisplayColor>\n");
        printf("         </gpxx:TrackExtension>\n");
        printf("         <gpxtrkoffx:TrackMovieOffsetExtension>\n");
        printf("         <gpxtrkoffx:StartOffsetSecs>0</gpxtrkoffx:StartOffsetSecs>\n");
        printf("         </gpxtrkoffx:TrackMovieOffsetExtension>\n");
        printf("     </extensions>\n");
//        printf("<!-- found %.2fs of metadata, from %d payloads, within %s-->\n", metadatalength, payloads, argv[1]);
        printf("<trkseg>\n");
        
        
/*
#if 1
		if (payloads == 1) // Printf the contents of the single payload
		{
			uint32_t payloadsize = GetGPMFPayloadSize(0);
			payload = GetGPMFPayload(payload, 0);
			if(payload == NULL)
				goto cleanup;

			ret = GPMF_Init(ms, payload, payloadsize);
			if (ret != GPMF_OK)
				goto cleanup;

			// Output (printf) all the contained GPMF data within this payload
			ret = GPMF_Validate(ms, GPMF_RECURSE_LEVELS); // optional
			if (GPMF_OK != ret)
			{
				printf("Invalid Structure\n");
				goto cleanup;
			}

			GPMF_ResetState(ms);
			do
			{
				PrintGPMF(ms);  // printf current GPMF KLV
			} while (GPMF_OK == GPMF_Next(ms, GPMF_RECURSE_LEVELS));
			GPMF_ResetState(ms);
			printf("\n");

		}
#endif
*/


		for (index = 0; index < payloads; index++)
		{
#if 1
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
                        //printf("<!-- Setting sampling rate for GPS5 to %f Hz-->\n", GPSrate);
                    }
                }
            }
#endif
            
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

#if 1		// Find all the available Streams and the data carrying FourCC
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
							//printf("  STRM of %c%c%c%c ", PRINTF_4CC(key));

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
/*
							printf("with %d sample%s ", samples, samples > 1 ? "s" : "");

							if (elements > 1)
								printf("-- %d elements per sample", elements);

							printf("\n");
 */
                            
						}
					}
				}
				GPMF_ResetState(ms);
				printf("\n");
			}
#endif 




#if 1		// Find GPS values and return scaled doubles. 

            printf("<!-- frame %d -->\n", index);
            if (GPMF_OK == GPMF_FindNext(ms, STR2FOURCC("GPS5"), GPMF_RECURSE_LEVELS) || //GoPro Hero5 GPS
					GPMF_OK == GPMF_FindNext(ms, STR2FOURCC("GPRI"), GPMF_RECURSE_LEVELS))   //GoPro Karma GPS
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
/*
                        printf("HOME(%.3f, %.3f) 2018.01.01 15:15:15\n",ptr[0],ptr[1]);
                        printf("GPS(%.3f, %.3f, %.0f) BAROMETER: %.1f\n",ptr[0],ptr[1],ptr[2],ptr[2]);
                        printf("ISO:478 Shutter:30 EV: 0 Fnum:F2.8 \n");
                        printf("\n");
  */
    /* There are 18 samples (18.177936Hz) per 1s data - we print only the first - good enought for STR, not for GPX
    GPS5 33.126deg, -117.327deg, -21.144m, 1.671m/s, 1.660m/s,
     */
                        strftime(TimeStr, 26, "%Y-%m-%dT", tm_info);
                        // must be in the following format:  <time>2017-12-19T18:22:01.001Z</time>
                        
                        
						for (i = 0; i < samples; i++)
						{
							//printf("%c%c%c%c ", PRINTF_4CC(key));
                            /*
                             <trkpt lat="45.751900000000000" lon="6.280200000000000">
                            <ele>6.8</ele>
                            <time>2016-02-26T18:08:37Z</time>
                            </trkpt>
                             */
                            printf("  <trkpt lat=\"%.14f\" lon=\"%.14f\">\n", ptr[0],ptr[1]);
                            printf("    <ele>%f</ele>\n", ptr[2]);
                            
                            // to check if months and days are always using a leading 0.
                            msec = (in + (i/GPSrate))*1000;
                            sec = (int) (msec / 1000) % 60;
                            min = (int) ((int)(msec / (1000*60)) % 60);
                            hour = (int) ((int)(msec / (1000*60*60)) % 24);
                            double2Ints(msec/1000, 3, &intpart, &fractpart);
                            
                            //printf("time:%s - hour:%d - min:%d - sec:%d - ms:%d - int:%d - fract:%d\n", TimeStr, hour, min, sec, fractpart, intpart, fractpart);
                            printf("<time>%s%02D:%02D:%02D.%03DZ</time>\n", TimeStr, hour, min, sec, fractpart);
                            printf("  </trkpt>\n");
						}
						free(tmpbuffer);
					}
				}
				GPMF_ResetState(ms);
				printf("\n");

#endif
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
