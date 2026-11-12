/*! @file GPMF_mp4reader.h
 *
 *  @brief Way Too Crude MP4 reader 
 *
 *  @version 1.0.0
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

#ifndef _GPMF_MP4READER_H
#define _GPMF_MP4READER_H

#include "GPMF_parser.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct SampleToChunk
{
	uint32_t chunk_num;
	uint32_t samples;
	uint32_t id;
} SampleToChunk;

typedef struct GPMF_Source GPMF_Source;

GPMF_Source *GPMF_SourceCreate(void);
void GPMF_SourceDestroy(GPMF_Source *source);
double OpenGPMFSource(GPMF_Source *source, const char *filename);
double OpenGPMFSourceUDTA(GPMF_Source *source, const char *filename);
void CloseGPMFSource(GPMF_Source *source);
uint32_t GetNumberGPMFPayloads(GPMF_Source *source);
uint32_t *GetGPMFPayload(GPMF_Source *source, uint32_t *lastpayload, uint32_t index);
void FreeGPMFPayload(GPMF_Source *source, uint32_t *lastpayload);
uint32_t GetGPMFPayloadSize(GPMF_Source *source, uint32_t index);
uint32_t GetGPMFPayloadTime(GPMF_Source *source, uint32_t index, double *in, double *out); // MP4 payload timestamps

#define GPMF_SAMPLE_RATE_FAST		0
#define GPMF_SAMPLE_RATE_PRECISE	1

double GetGPMFSampleRate(GPMF_Source *source, uint32_t fourcc, uint32_t flags);
double GetGPMFSampleRateAndTimes(GPMF_Source *source, GPMF_stream *gs, double lastrate, uint32_t index, double *in, double *out);

#ifdef __cplusplus
}
#endif

#endif
