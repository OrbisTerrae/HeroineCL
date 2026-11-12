/*! @file GPMF_mp4reader.c
 *
 *  @brief Way Too Crude MP4 reader
 *
 *  @version 1.1.1
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

/* This is not an elegant MP4 parser, only used to help demonstrate extraction of GPMF */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include "GPMF_mp4reader.h"
#include "GPMF_parser.h"

#define PRINT_MP4_STRUCTURE		0

#ifdef WIN32
#define LONGSEEK  _fseeki64
#else
#define LONGSEEK  fseeko
#endif


struct GPMF_Source
{
	uint32_t *metasizes;
	uint32_t metasize_count;
	uint64_t *metaoffsets;
	uint32_t indexcount;
	double videolength;
	double metadatalength;
	uint32_t clockdemon, clockcount;
	uint32_t trak_clockdemon, trak_clockcount;
	uint32_t meta_clockdemon, meta_clockcount;
	uint32_t basemetadataduration;
	uint32_t basemetadataoffset;
	FILE *fp;
	SampleToChunk *metastsc;
	uint32_t metastsc_count;
};

GPMF_Source *GPMF_SourceCreate(void)
{
	return (GPMF_Source *)calloc(1, sizeof(GPMF_Source));
}

void GPMF_SourceDestroy(GPMF_Source *source)
{
	if (source)
	{
		CloseGPMFSource(source);
		free(source);
	}
}


typedef struct media_header
{
	uint8_t version_flags[4];
	uint32_t creation_time;
	uint32_t modification_time;
	uint32_t time_scale;
	uint32_t duration;
	uint16_t language;
	uint16_t quality;
} media_header;




#define MAKETAG(d,c,b,a)		(((d&0xff)<<24)|((c&0xff)<<16)|((b&0xff)<<8)|(a&0xff))



uint32_t GetNumberGPMFPayloads(GPMF_Source *source)
{
	return source ? source->indexcount : 0;
}


uint32_t *GetGPMFPayload(GPMF_Source *source, uint32_t *lastpayload, uint32_t index)
{
	if (!source || index >= source->indexcount || !source->fp || !source->metasizes || !source->metaoffsets || source->metasizes[index] == 0)
		return NULL;
	if (LONGSEEK(source->fp, (off_t)source->metaoffsets[index], SEEK_SET) != 0)
		return NULL;
	uint32_t *buffer = (uint32_t *)malloc(source->metasizes[index]);
	if (!buffer)
		return NULL;
	if (fread(buffer, 1, source->metasizes[index], source->fp) != source->metasizes[index])
	{
		free(buffer);
		return NULL;
	}
	free(lastpayload);
	return buffer;
}



void FreeGPMFPayload(GPMF_Source *source, uint32_t *lastpayload)
{
	(void)source;
	free(lastpayload);
}


uint32_t GetGPMFPayloadSize(GPMF_Source *source, uint32_t index)
{
	return (source && index < source->indexcount && source->metasizes) ? source->metasizes[index] : 0;
}

static int load_payload(GPMF_Source *source, uint32_t **payload, uint32_t index)
{
	uint32_t *next = GetGPMFPayload(source, *payload, index);
	if (!next) return 0;
	*payload = next;
	return 1;
}


#define TRAK_TYPE		MAKEID('m', 'e', 't', 'a')		// track is the type for metadata
#define TRAK_SUBTYPE	MAKEID('g', 'p', 'm', 'd')		// subtype is GPMF

#define NESTSIZE(x) { int i = nest; while (i > 0 && nestsize[i] > 0) { nestsize[i] -= x; if(nestsize[i]>=0 && nestsize[i] <= 8) { nestsize[i]=0; nest--; } i--; } }



double OpenGPMFSourceUDTA(GPMF_Source *source, const char *filename)
{
	if (!source || !filename) return 0.0;
	CloseGPMFSource(source);
#ifdef _WINDOWS
	fopen_s(&source->fp, filename, "rb");
#else
	source->fp = fopen(filename, "rb");
#endif

	source->metasizes = NULL;
	source->metaoffsets = NULL;
	source->indexcount = 0;
	source->videolength = 0.0;
	source->metadatalength = 0.0;
	source->basemetadataduration = 0;
	source->basemetadataoffset = 0;

	if (source->fp)
	{
		uint32_t qttag, qtsize32, len;
		int32_t nest = 0;
		uint64_t nestsize[64] = { 0 };
		uint64_t lastsize = 0, qtsize;

		do
		{
			len = fread(&qtsize32, 1, 4, source->fp);
			len += fread(&qttag, 1, 4, source->fp);
			if (len == 8)
			{
				if (!GPMF_VALID_FOURCC(qttag))
				{
					LONGSEEK(source->fp, lastsize - 8 - 8, SEEK_CUR);

					NESTSIZE(lastsize - 8);
					continue;
				}

				qtsize32 = BYTESWAP32(qtsize32);

				if (qtsize32 == 1) // 64-bit Atom
				{
					fread(&qtsize, 1, 8, source->fp);
					qtsize = BYTESWAP64(qtsize) - 8;
				}
				else
					qtsize = qtsize32;

				nest++;

				nestsize[nest] = qtsize;
				lastsize = qtsize;

				if (qttag == MAKEID('m', 'd', 'a', 't') ||
					qttag == MAKEID('f', 't', 'y', 'p'))
				{
					LONGSEEK(source->fp, qtsize - 8, SEEK_CUR);
					NESTSIZE(qtsize);
					continue;
				}

				if (qttag == MAKEID('G', 'P', 'M', 'F'))
				{
					source->videolength += 1.0;
					source->metadatalength += 1.0;

					source->indexcount = (int)source->metadatalength;

					source->metasizes = (uint32_t *)malloc(source->indexcount * 4 + 4);  memset(source->metasizes, 0, source->indexcount * 4 + 4);
					source->metaoffsets = (uint64_t *)malloc(source->indexcount * 8 + 8);  memset(source->metaoffsets, 0, source->indexcount * 8 + 8);

					source->metasizes[0] = (int)qtsize-8;
					source->metaoffsets[0] = ftell(source->fp);

					return source->metadatalength;  // not an MP4, RAW GPMF which has not inherent timing, assigning a during of 1second.
				}
				if (qttag != MAKEID('m', 'o', 'o', 'v') && //skip over all but these atoms
					qttag != MAKEID('u', 'd', 't', 'a'))
				{
					LONGSEEK(source->fp, qtsize - 8, SEEK_CUR);
					NESTSIZE(qtsize);
					continue;
				}
				else
				{
					NESTSIZE(8);
				}
			}
		} while (len > 0);
	}
	return source->metadatalength;
}


double OpenGPMFSource(GPMF_Source *source, const char *filename)  //RAW or within MP4
{
	if (!source || !filename) return 0.0;
	CloseGPMFSource(source);
#ifdef _WINDOWS
	fopen_s(&source->fp, filename, "rb");
#else
	source->fp = fopen(filename, "rb");
#endif

	source->metasizes = NULL;
	source->metaoffsets = NULL;
	source->indexcount = 0;
	source->videolength = 0.0;
	source->metadatalength = 0.0;
	source->basemetadataduration = 0;
	source->basemetadataoffset = 0;
	
	if (source->fp)
	{
		uint32_t tag, qttag, qtsize32, skip, type = 0, subtype = 0, num;
		size_t len;
		int32_t nest = 0;
		uint64_t nestsize[64] = { 0 };
		uint64_t lastsize = 0, qtsize;

		len = fread(&tag, 1, 4, source->fp);
		if (tag == GPMF_KEY_DEVICE) // RAW GPMF data, not in an MP4
		{
			int filesize;

			source->videolength += 1.0;
			source->metadatalength += 1.0;

			fseek(source->fp, 0, SEEK_END);
			filesize = ftell(source->fp);
			fseek(source->fp, 0, SEEK_SET);

			source->indexcount = (int)source->metadatalength;
			LONGSEEK(source->fp, 0, SEEK_SET); // back to start

			source->metasizes = (uint32_t *)malloc(source->indexcount * 4 + 4);
			if (source->metasizes == NULL)
				return 0;

			source->metaoffsets = (uint64_t *)malloc(source->indexcount * 8 + 8);
			if (source->metaoffsets == NULL)
			{
				free(source->metasizes);
				source->metasizes = 0;
				return 0;
			}

			memset(source->metasizes, 0, source->indexcount * 4 + 4);
			memset(source->metaoffsets, 0, source->indexcount * 8 + 8);

			source->metasizes[0] = (filesize)&~3;
			source->metaoffsets[0] = 0;

			return source->metadatalength;  // not an MP4, RAW GPMF which has not inherent timing, assigning a during of 1second.
		}
		LONGSEEK(source->fp, 0, SEEK_SET); // back to start

		do
		{
			len = fread(&qtsize32, 1, 4, source->fp);
			len += fread(&qttag, 1, 4, source->fp);
			if (len == 8)
			{
				if (!GPMF_VALID_FOURCC(qttag))
				{
					LONGSEEK(source->fp, lastsize - 8 - 8, SEEK_CUR);

					NESTSIZE(lastsize - 8);
					continue;
				}

				qtsize32 = BYTESWAP32(qtsize32);

				if (qtsize32 == 1) // 64-bit Atom
				{
					fread(&qtsize, 1, 8, source->fp);
					qtsize = BYTESWAP64(qtsize) - 8;
				}
				else
					qtsize = qtsize32;

				nest++;

				nestsize[nest] = qtsize;
				lastsize = qtsize;

#if PRINT_MP4_STRUCTURE	

				for (int i = 1; i < nest; i++) printf("    ");
				printf("%c%c%c%c (%lld)\n", (qttag & 0xff), ((qttag >> 8) & 0xff), ((qttag >> 16) & 0xff), ((qttag >> 24) & 0xff), qtsize);

				if (qttag == MAKEID('m', 'd', 'a', 't') ||
					qttag == MAKEID('f', 't', 'y', 'p') ||
					qttag == MAKEID('u', 'd', 't', 'a'))
				{
					LONGSEEK(source->fp, qtsize - 8, SEEK_CUR);

					NESTSIZE(qtsize);

					continue;
				}
#else
				if (qttag != MAKEID('m', 'o', 'o', 'v') && //skip over all but these atoms
					qttag != MAKEID('m', 'v', 'h', 'd') &&
					qttag != MAKEID('t', 'r', 'a', 'k') &&
					qttag != MAKEID('m', 'd', 'i', 'a') &&
					qttag != MAKEID('m', 'd', 'h', 'd') &&
					qttag != MAKEID('m', 'i', 'n', 'f') &&
					qttag != MAKEID('g', 'm', 'i', 'n') &&
					qttag != MAKEID('d', 'i', 'n', 'f') &&
					qttag != MAKEID('a', 'l', 'i', 's') &&
					qttag != MAKEID('s', 't', 's', 'd') &&
					qttag != MAKEID('a', 'l', 'i', 's') &&
					qttag != MAKEID('a', 'l', 'i', 's') &&
					qttag != MAKEID('s', 't', 'b', 'l') &&
					qttag != MAKEID('s', 't', 't', 's') &&
					qttag != MAKEID('s', 't', 's', 'c') &&
					qttag != MAKEID('s', 't', 's', 'z') &&
					qttag != MAKEID('s', 't', 'c', 'o') &&
					qttag != MAKEID('c', 'o', '6', '4') &&
					qttag != MAKEID('h', 'd', 'l', 'r'))
				{
					LONGSEEK(source->fp, qtsize - 8, SEEK_CUR);

					NESTSIZE(qtsize);
				}
				else 
#endif
				if (qttag == MAKEID('m', 'v', 'h', 'd')) //mvhd  movie header
				{
					len = fread(&skip, 1, 4, source->fp);
					len += fread(&skip, 1, 4, source->fp);
					len += fread(&skip, 1, 4, source->fp);
					len += fread(&source->clockdemon, 1, 4, source->fp); source->clockdemon = BYTESWAP32(source->clockdemon);
					len += fread(&source->clockcount, 1, 4, source->fp); source->clockcount = BYTESWAP32(source->clockcount);
					LONGSEEK(source->fp, qtsize - 8 - len, SEEK_CUR); // skip over mvhd

					NESTSIZE(qtsize);
				}
				else if (qttag == MAKEID('m', 'd', 'h', 'd')) //mdhd  media header
				{
					media_header md;
					len = fread(&md, 1, sizeof(md), source->fp);
					if (len == sizeof(md))
					{
						md.creation_time = BYTESWAP32(md.creation_time);
						md.modification_time = BYTESWAP32(md.modification_time);
						md.time_scale = BYTESWAP32(md.time_scale);
						md.duration = BYTESWAP32(md.duration);

						source->trak_clockdemon = md.time_scale;
						source->trak_clockcount = md.duration;

						if (source->videolength == 0.0) // Get the video length from the first track
						{
							source->videolength = (double)source->trak_clockcount / (double)source->trak_clockdemon;
						}
					}
					LONGSEEK(source->fp, qtsize - 8 - len, SEEK_CUR); // skip over mvhd

					NESTSIZE(qtsize);
				}
				else if (qttag == MAKEID('h', 'd', 'l', 'r')) //hldr
				{
					uint32_t temp;
					len = fread(&skip, 1, 4, source->fp);
					len += fread(&skip, 1, 4, source->fp);
					len += fread(&temp, 1, 4, source->fp);  // type will be 'meta' for the correct trak.

					if (temp != MAKEID('a', 'l', 'i', 's'))
						type = temp;

					LONGSEEK(source->fp, qtsize - 8 - len, SEEK_CUR); // skip over hldr

					NESTSIZE(qtsize);

				}
				else if (qttag == MAKEID('s', 't', 's', 'd')) //read the sample decription to determine the type of metadata
				{
					if (type == TRAK_TYPE) // meta 
					{
						len = fread(&skip, 1, 4, source->fp);
						len += fread(&skip, 1, 4, source->fp);
						len += fread(&skip, 1, 4, source->fp);
						len += fread(&subtype, 1, 4, source->fp);  // type will be 'meta' for the correct trak.
						if (len == 16)
						{
							if (subtype != TRAK_SUBTYPE) // GPMF metadata 
							{
								type = 0; // GPMF
							}
						}
						LONGSEEK(source->fp, qtsize - 8 - len, SEEK_CUR); // skip over stsd
					}
					else
						LONGSEEK(source->fp, qtsize - 8, SEEK_CUR);

					NESTSIZE(qtsize);
				}
				else if (qttag == MAKEID('s', 't', 's', 'c')) // metadata stsc - offset chunks
				{
					if (type == TRAK_TYPE) // meta
					{
						len = fread(&skip, 1, 4, source->fp);
						len += fread(&num, 1, 4, source->fp);
						source->metastsc_count = num = BYTESWAP32(num);
						if (source->metastsc) free(source->metastsc);
						source->metastsc = (SampleToChunk *)malloc(num * 12);
						if (source->metastsc)
						{
							len += fread(source->metastsc, 1, num * sizeof(SampleToChunk), source->fp);

							do
							{
								num--;
								source->metastsc[num].chunk_num = BYTESWAP32(source->metastsc[num].chunk_num);
								source->metastsc[num].samples = BYTESWAP32(source->metastsc[num].samples);
								source->metastsc[num].id = BYTESWAP32(source->metastsc[num].id);
							} while (num > 0);
						}

						if (source->metastsc_count == 1 && source->metastsc[0].samples == 1) // Simplify if the stsc is not reporting any grouped chunks.
						{
							if (source->metastsc) free(source->metastsc);
							source->metastsc = NULL;
							source->metastsc_count = 0;
						}
						LONGSEEK(source->fp, qtsize - 8 - len, SEEK_CUR); // skip over stsz
					}
					else
						LONGSEEK(source->fp, qtsize - 8, SEEK_CUR);

					NESTSIZE(qtsize);
				}
				else if (qttag == MAKEID('s', 't', 's', 'z')) // metadata stsz - sizes
				{
					if (type == TRAK_TYPE) // meta
					{
						uint32_t equalsamplesize;
						
						len = fread(&skip, 1, 4, source->fp);
						len += fread(&equalsamplesize, 1, 4, source->fp);
						len += fread(&num, 1, 4, source->fp);
						source->metasize_count = num = BYTESWAP32(num);
						if (source->metasizes) free(source->metasizes);
						source->metasizes = (uint32_t *)malloc(num * 4);
						if (source->metasizes)
						{
							if (equalsamplesize == 0)
							{
								len += fread(source->metasizes, 1, num * 4, source->fp);
								do
								{
									num--;
									source->metasizes[num] = BYTESWAP32(source->metasizes[num]);
								} while (num > 0);
							}
							else
							{
								equalsamplesize = BYTESWAP32(equalsamplesize);
								do
								{
									num--;
									source->metasizes[num] = equalsamplesize;
								} while (num > 0);
							}
						}
						LONGSEEK(source->fp, qtsize - 8 - len, SEEK_CUR); // skip over stsz
					}
					else
						LONGSEEK(source->fp, qtsize - 8, SEEK_CUR);

					NESTSIZE(qtsize);
				}
				else if (qttag == MAKEID('s', 't', 'c', 'o')) // metadata stco - offsets
				{
					if (type == TRAK_TYPE) // meta
					{
						len = fread(&skip, 1, 4, source->fp);
						len += fread(&num, 1, 4, source->fp);
						num = BYTESWAP32(num);
						if (source->metastsc_count > 0 && num != source->metasize_count)
						{
							source->indexcount = source->metasize_count;
							if (source->metaoffsets) free(source->metaoffsets);
							source->metaoffsets = (uint64_t *)malloc(source->metasize_count * 8);
							if (source->metaoffsets)
							{
								uint32_t *metaoffsets32 = NULL;
								metaoffsets32 = (uint32_t *)malloc(num * 4);
								if (metaoffsets32)
								{
									uint64_t fileoffset = 0;
									int stsc_pos = 0;
									int stco_pos = 0;
									len += fread(metaoffsets32, 1, num * 4, source->fp);
									do
									{
										num--;
										metaoffsets32[num] = BYTESWAP32(metaoffsets32[num]);
									} while (num > 0);

									fileoffset = metaoffsets32[0];
									source->metaoffsets[0] = fileoffset;

									num = 1;
									while (num < source->metasize_count)
									{
										if (num != source->metastsc[stsc_pos].chunk_num - 1 && 0 == (num - (source->metastsc[stsc_pos].chunk_num - 1)) % source->metastsc[stsc_pos].samples)
										{
											stco_pos++;
											fileoffset = (uint64_t)metaoffsets32[stco_pos];
										}
										else
										{
											fileoffset += (uint64_t)source->metasizes[num - 1];
										}

										source->metaoffsets[num] = fileoffset;
										num++;
									}

								if (source->metastsc) free(source->metastsc), source->metastsc = NULL;
								source->metastsc_count = 0;

									free(metaoffsets32);
								}
							}
						}
						else
						{
							source->indexcount = num;
							if (source->metaoffsets) free(source->metaoffsets);
							source->metaoffsets = (uint64_t *)malloc(num * 8);
							if (source->metaoffsets)
							{
								uint32_t *metaoffsets32 = NULL;
								metaoffsets32 = (uint32_t *)malloc(num * 4);
								if (metaoffsets32)
								{
									len += fread(metaoffsets32, 1, num * 4, source->fp);
									do
									{
										num--;
										source->metaoffsets[num] = BYTESWAP32(metaoffsets32[num]);
									} while (num > 0);

									free(metaoffsets32);
								}
							}
						}
						LONGSEEK(source->fp, qtsize - 8 - len, SEEK_CUR); // skip over stco
					}
					else
						LONGSEEK(source->fp, qtsize - 8, SEEK_CUR);

					NESTSIZE(qtsize);
				}

				else if (qttag == MAKEID('c', 'o', '6', '4')) // metadata stco - offsets
				{
					if (type == TRAK_TYPE) // meta
					{
						len = fread(&skip, 1, 4, source->fp);
						len += fread(&num, 1, 4, source->fp);
						num = BYTESWAP32(num);
						if (source->metastsc_count > 0 && num != source->metasize_count)
						{
							source->indexcount = source->metasize_count;
							if (source->metaoffsets) free(source->metaoffsets);
							source->metaoffsets = (uint64_t *)malloc(source->metasize_count * 8);
							if (source->metaoffsets)
							{
								uint64_t *metaoffsets64 = NULL;
								metaoffsets64 = (uint64_t *)malloc(num * 8);
								if (metaoffsets64)
								{
									uint64_t fileoffset = 0;
									int stsc_pos = 0;
									int stco_pos = 0;
									len += fread(metaoffsets64, 1, num * 8, source->fp);
									do
									{
										num--;
										metaoffsets64[num] = BYTESWAP64(metaoffsets64[num]);
									} while (num > 0);

									fileoffset = metaoffsets64[0];
									source->metaoffsets[0] = fileoffset;
									//printf("%3d:%08x, delta = %08x\n", 0, (int)fileoffset, 0);

									num = 1;
									while (num < source->metasize_count)
									{
										if (num != source->metastsc[stsc_pos].chunk_num - 1 && 0 == (num - (source->metastsc[stsc_pos].chunk_num - 1)) % source->metastsc[stsc_pos].samples)
										{
											stco_pos++;
											fileoffset = (uint64_t)metaoffsets64[stco_pos];
										}
										else
										{
											fileoffset += (uint64_t)source->metasizes[num - 1];
										}

										source->metaoffsets[num] = fileoffset;
										//int delta = source->metaoffsets[num] - source->metaoffsets[num - 1];
										//printf("%3d:%08x, delta = %08x\n", num, (int)fileoffset, delta);

										num++;
									}

								if (source->metastsc) free(source->metastsc), source->metastsc = NULL;
								source->metastsc_count = 0;

									free(metaoffsets64);
								}
							}
						}
						else
						{
							source->indexcount = num;
							if (source->metaoffsets) free(source->metaoffsets);
							source->metaoffsets = (uint64_t *)malloc(num * 8);
							if (source->metaoffsets)
							{
								len += fread(source->metaoffsets, 1, num * 8, source->fp);
								do
								{
									num--;
									source->metaoffsets[num] = BYTESWAP64(source->metaoffsets[num]);
								} while (num > 0);
							}
						}
						LONGSEEK(source->fp, qtsize - 8 - len, SEEK_CUR); // skip over stco
					}
					else
						LONGSEEK(source->fp, qtsize - 8, SEEK_CUR);

					NESTSIZE(qtsize);
				}
				else if (qttag == MAKEID('s', 't', 't', 's')) // time to samples
				{
					if (type == TRAK_TYPE) // meta 
					{
						uint32_t totaldur = 0;
						int32_t entries = 0;
						len = fread(&skip, 1, 4, source->fp);
						len += fread(&num, 1, 4, source->fp);
						num = BYTESWAP32(num);
						entries = num;

						source->meta_clockdemon = source->trak_clockdemon;
						source->meta_clockcount = source->trak_clockcount;

						while (entries > 0)
						{
							int32_t samplecount;
							int32_t duration;
							len += fread(&samplecount, 1, 4, source->fp);
							samplecount = BYTESWAP32(samplecount);
							len += fread(&duration, 1, 4, source->fp);
							duration = BYTESWAP32(duration);

							if (samplecount > 1)
							{
								source->basemetadataoffset = totaldur;
								source->basemetadataduration = duration;
							}
							entries--;

							totaldur += duration;
							source->metadatalength += ((double)samplecount * (double)duration / (double)source->meta_clockdemon);
						}
						LONGSEEK(source->fp, qtsize - 8 - len, SEEK_CUR); // skip over stco
					}
					else
						LONGSEEK(source->fp, qtsize - 8, SEEK_CUR);

					NESTSIZE(qtsize);
				}
				else
				{
					NESTSIZE(8);
				}
			}
			else
			{
				break;
			}
		} while (len > 0);
	}

	return source->metadatalength;
}


void CloseGPMFSource(GPMF_Source *source)
{
	if (!source) return;
	if (source->fp) fclose(source->fp), source->fp = NULL;
	if (source->metasizes) free(source->metasizes), source->metasizes = 0;
	if (source->metaoffsets) free(source->metaoffsets), source->metaoffsets = 0;
	if (source->metastsc) free(source->metastsc), source->metastsc = NULL;
	source->metasize_count = source->indexcount = source->metastsc_count = 0;
	source->videolength = source->metadatalength = 0.0;
	source->clockdemon = source->clockcount = source->trak_clockdemon = source->trak_clockcount = 0;
	source->meta_clockdemon = source->meta_clockcount = 0;
	source->basemetadataduration = source->basemetadataoffset = 0;
}


uint32_t GetGPMFPayloadTime(GPMF_Source *source, uint32_t index, double *in, double *out)
{
	if (!source || index >= source->indexcount || source->metaoffsets == 0 || source->basemetadataduration == 0 || source->meta_clockdemon == 0 || in == NULL || out == NULL) return 1;

	*in = ((double)index * (double)source->basemetadataduration / (double)source->meta_clockdemon);
	*out = ((double)(index+1) * (double)source->basemetadataduration / (double)source->meta_clockdemon);
	return 0;
}



double GetGPMFSampleRate(GPMF_Source *source, uint32_t fourcc, uint32_t flags)
{
	if (!source) return 0.0;
	GPMF_stream metadata_stream, *ms = &metadata_stream;
	uint32_t teststart = 0;
	uint32_t testend = source->indexcount;
	double rate = 0.0;
	uint32_t *repeatarray = NULL;

	if (source->indexcount < 1)
		return 0.0;

	if (source->indexcount > 3) // Skip startup and shutdown samples when estimating sensor rate.
	{
		teststart++;
		testend--;
	}

	uint32_t *payload = GetGPMFPayload(source, NULL, teststart); // second payload
	uint32_t payloadsize = GetGPMFPayloadSize(source, teststart);
	int32_t ret = GPMF_Init(ms, payload, payloadsize);

	if (ret != GPMF_OK)
		goto cleanup;

	{
		uint32_t startsamples = 0;
		uint32_t endsamples = 0;
		uint32_t missing_samples = 0;

		while (ret == GPMF_OK && GPMF_OK != GPMF_FindNext(ms, fourcc, GPMF_RECURSE_LEVELS))
		{
			missing_samples = 1;
			teststart++;
			if (!load_payload(source, &payload, teststart)) { ret = GPMF_ERROR_MEMORY; goto cleanup; } // next payload
			payloadsize = GetGPMFPayloadSize(source, teststart);
			ret = GPMF_Init(ms, payload, payloadsize);
		}

		if (missing_samples)
		{
			teststart++;   //samples after sensor start are statistically the best
			if (!load_payload(source, &payload, teststart)) { ret = GPMF_ERROR_MEMORY; goto cleanup; }
			payloadsize = GetGPMFPayloadSize(source, teststart);
			ret = GPMF_Init(ms, payload, payloadsize);
		}

		if (ret == GPMF_OK)
		{
			uint32_t samples = GPMF_Repeat(ms);
			GPMF_stream find_stream;
			GPMF_CopyState(ms, &find_stream);

			if (!(flags & GPMF_SAMPLE_RATE_PRECISE) && GPMF_OK == GPMF_FindPrev(&find_stream, GPMF_KEY_TOTAL_SAMPLES, GPMF_CURRENT_LEVEL))
			{
				startsamples = BYTESWAP32(*(uint32_t *)GPMF_RawData(&find_stream)) - samples;

				if (!load_payload(source, &payload, testend)) { ret = GPMF_ERROR_MEMORY; goto cleanup; }
				payloadsize = GetGPMFPayloadSize(source, testend);
				ret = GPMF_Init(ms, payload, payloadsize);
				if (ret != GPMF_OK)
					goto cleanup;

				if (GPMF_OK == GPMF_FindNext(ms, fourcc, GPMF_RECURSE_LEVELS))
				{
					GPMF_CopyState(ms, &find_stream);
					if (GPMF_OK == GPMF_FindPrev(&find_stream, GPMF_KEY_TOTAL_SAMPLES, GPMF_CURRENT_LEVEL))
					{
						endsamples = BYTESWAP32(*(uint32_t *)GPMF_RawData(&find_stream));
						rate = (double)(endsamples - startsamples) / (source->metadatalength * ((double)(testend - teststart + 1)) / (double)source->indexcount);
						goto cleanup;
					}
				}

				rate = (double)(samples) / (source->metadatalength * ((double)(testend - teststart + 1)) / (double)source->indexcount);
			}
			else // for increased precision, for older GPMF streams sometimes missing the total sample count 
			{
				uint32_t payloadpos = 0, payloadcount = 0;
				double slope, top = 0.0, bot = 0.0, meanX = 0, meanY = 0;
				if ((size_t)source->indexcount > SIZE_MAX / sizeof(*repeatarray) - 1)
					goto cleanup;
				repeatarray = calloc((size_t)source->indexcount + 1, sizeof(*repeatarray));

				samples = 0;

				for (payloadpos = teststart; payloadpos < testend; payloadcount++, payloadpos++)
				{
					if (!load_payload(source, &payload, payloadpos)) { ret = GPMF_ERROR_MEMORY; goto cleanup; }
					payloadsize = GetGPMFPayloadSize(source, payloadpos);
					ret = GPMF_Init(ms, payload, payloadsize);

					if (ret != GPMF_OK)
						goto cleanup;

					if (GPMF_OK == GPMF_FindNext(ms, fourcc, GPMF_RECURSE_LEVELS))
					{
						GPMF_stream find_stream2;
						GPMF_CopyState(ms, &find_stream2);

						if (GPMF_OK == GPMF_FindNext(&find_stream2, fourcc, GPMF_CURRENT_LEVEL)) // Count the instances, not the repeats
						{
							if (repeatarray)
							{
								double in, out;

								do
								{
									samples++;
								} while (GPMF_OK == GPMF_FindNext(ms, fourcc, GPMF_CURRENT_LEVEL));

								repeatarray[payloadpos] = samples;
								meanY += (double)samples;

									GetGPMFPayloadTime(source, payloadpos, &in, &out);
								meanX += out;
							} 
						}
						else
						{
							uint32_t repeat = GPMF_Repeat(ms);
							samples += repeat;

							if (repeatarray)
							{
								double in, out;

								repeatarray[payloadpos] = samples;
								meanY += (double)samples;

								GetGPMFPayloadTime(source, payloadpos, &in, &out);
								meanX += out;
							}						
						} 
					}
				}

				// Compute the line of best fit for a jitter removed sample rate.  
				// This does assume an unchanging clock, even though the IMU data can thermally impacted causing small clock changes.  
				// TODO: Next enhancement would be a low order polynominal fit the compensate for any thermal clock drift.
 				if (repeatarray)
				{
					meanY /= (double)payloadcount;
					meanX /= (double)payloadcount;

					for (payloadpos = teststart; payloadpos < testend; payloadpos++)
					{
						double in, out;
						GetGPMFPayloadTime(source, payloadpos, &in, &out);

						top += ((double)out - meanX)*((double)repeatarray[payloadpos] - meanY);
						bot += ((double)out - meanX)*((double)out - meanX);
					}

					slope = top / bot;

#if 0
					// This sample code might be useful for compare data latency between channels.
					{
						double intercept;
						intercept = meanY - slope*meanX;
						printf("%c%c%c%c start offset = %f (%.3fms)\n", PRINTF_4CC(fourcc), intercept, 1000.0 * intercept / slope );
					}
#endif
					rate = slope;
				}
				else
				{
					rate = (double)(samples) / (source->metadatalength * ((double)(testend - teststart + 1)) / (double)source->indexcount);
				}
				
				free(repeatarray);
				repeatarray = NULL;

				goto cleanup;
			}
		}
	}

cleanup:
	free(repeatarray);
	if (payload) FreeGPMFPayload(source, payload);

	return rate;
}


double GetGPMFSampleRateAndTimes(GPMF_Source *source, GPMF_stream *gs, double rate, uint32_t index, double *in, double *out)
{
	if (!source) return 1.0;
	uint32_t key, insamples;
	uint32_t repeat, outsamples;
	GPMF_stream find_stream;

	if (source == NULL || gs == NULL || index >= source->indexcount || source->metaoffsets == 0 || source->indexcount == 0 || source->basemetadataduration == 0 || source->meta_clockdemon == 0 || in == NULL || out == NULL) return 1;

	key = GPMF_Key(gs);
	repeat = GPMF_Repeat(gs);
	if (rate == 0.0)
		rate = GetGPMFSampleRate(source, key, GPMF_SAMPLE_RATE_FAST);

	if (rate == 0.0)
	{
		*in = *out = 0.0;
		return 0.0;
	}

	GPMF_CopyState(gs, &find_stream);
	if (GPMF_OK == GPMF_FindPrev(&find_stream, GPMF_KEY_TOTAL_SAMPLES, GPMF_CURRENT_LEVEL))
	{
		outsamples = BYTESWAP32(*(uint32_t *)GPMF_RawData(&find_stream));
		insamples = outsamples - repeat;

		*in = ((double)insamples / (double)rate);
		*out = ((double)outsamples / (double)rate);
	}
	else
	{
		// might too costly in some applications read all the samples to determine the clock jitter, here I return the estimate from the MP4 track.
		*in = ((double)index * (double)source->basemetadataduration / (double)source->meta_clockdemon);
		*out = ((double)(index + 1) * (double)source->basemetadataduration / (double)source->meta_clockdemon);
	}
	return rate;
}
