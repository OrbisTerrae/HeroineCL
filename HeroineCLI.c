#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <getopt.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "GPMF_parser.h"
#include "GPMF_mp4reader.h"

#define DEFAULT_TEMPERATURE 20.0
#define DEFAULT_HEART_RATE 65
#define FALLBACK_GPS_RATE 18.177936

typedef struct Options {
    const char *input;
    const char *output_prefix;
    int verbose;
    int include_temperature;
    int include_heart_rate;
    double temperature;
    int heart_rate;
} Options;

static void usage(FILE *stream, const char *program)
{
    fprintf(stream,
            "Usage: %s [-v] [-t Celsius] [-r bpm] [-o output-prefix] input.mp4\n"
            "Convert GoPro GPMF GPS telemetry to GPX and KML.\n"
            "  -h             Show this help\n"
            "  -v             Print progress\n"
            "  -t Celsius     Add a constant temperature extension\n"
            "  -r bpm         Add a constant heart-rate extension\n"
            "  -o prefix      Write prefix.GPX and prefix.KML\n",
            program);
}

static int parse_number(const char *text, double min, double max, double *value)
{
    char *end = NULL;
    errno = 0;
    double parsed = strtod(text, &end);
    if (errno || end == text || *end != '\0' || !isfinite(parsed) || parsed < min || parsed > max)
        return 0;
    *value = parsed;
    return 1;
}

static int parse_options(int argc, char **argv, Options *options)
{
    int option;
    memset(options, 0, sizeof(*options));
    options->temperature = DEFAULT_TEMPERATURE;
    options->heart_rate = DEFAULT_HEART_RATE;
    while ((option = getopt(argc, argv, "hvt:r:o:")) != -1) {
        double value;
        switch (option) {
        case 'h': usage(stdout, argv[0]); return 2;
        case 'v': options->verbose = 1; break;
        case 't':
            if (!parse_number(optarg, -100.0, 100.0, &options->temperature)) {
                fprintf(stderr, "Invalid temperature: %s\n", optarg); return 0;
            }
            options->include_temperature = 1;
            break;
        case 'r':
            if (!parse_number(optarg, 0.0, 300.0, &value) || floor(value) != value) {
                fprintf(stderr, "Invalid heart rate: %s\n", optarg); return 0;
            }
            options->heart_rate = (int)value;
            options->include_heart_rate = 1;
            break;
        case 'o':
            if (!optarg || !*optarg) { fprintf(stderr, "Output prefix must not be empty\n"); return 0; }
            options->output_prefix = optarg;
            break;
        default: usage(stderr, argv[0]); return 0;
        }
    }
    if (argc - optind != 1) {
        usage(stderr, argv[0]);
        return 0;
    }
    options->input = argv[optind];
    return 1;
}

static char *make_path(const char *prefix, const char *extension)
{
    size_t a = strlen(prefix), b = strlen(extension);
    if (a > SIZE_MAX - b - 1) return NULL;
    char *path = malloc(a + b + 1);
    if (path) {
        memcpy(path, prefix, a);
        memcpy(path + a, extension, b + 1);
    }
    return path;
}

static FILE *open_temporary_output(const char *target, char **temporary_path)
{
    *temporary_path = make_path(target, ".tmp.XXXXXX");
    if (!*temporary_path) return NULL;
    int descriptor = mkstemp(*temporary_path);
    if (descriptor < 0) return NULL;
    FILE *file = fdopen(descriptor, "w");
    if (!file) {
        close(descriptor);
        unlink(*temporary_path);
    }
    return file;
}

static const char *file_name(const char *path)
{
    const char *slash = strrchr(path, '/');
    return slash ? slash + 1 : path;
}

static void write_xml_text(FILE *file, const char *text)
{
    for (; *text; ++text) {
        switch (*text) {
        case '&': fputs("&amp;", file); break;
        case '<': fputs("&lt;", file); break;
        case '>': fputs("&gt;", file); break;
        case '"': fputs("&quot;", file); break;
        case '\'': fputs("&apos;", file); break;
        default: fputc((unsigned char)*text, file); break;
        }
    }
}

static int write_headers(FILE *gpx, FILE *kml, const char *input_name)
{
    fputs("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
          "<gpx version=\"1.1\" creator=\"HeroineCLI\" xmlns=\"http://www.topografix.com/GPX/1/1\" xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" xmlns:gpxtpx=\"http://www.garmin.com/xmlschemas/TrackPointExtension/v1\">\n"
          " <trk><name>", gpx);
    write_xml_text(gpx, input_name);
    fputs("</name><trkseg>\n", gpx);
    fputs("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
          "<kml xmlns=\"http://www.opengis.net/kml/2.2\"><Document><name>", kml);
    write_xml_text(kml, input_name);
    fputs("</name><Placemark><LineString><altitudeMode>absolute</altitudeMode><coordinates>\n", kml);
    return !ferror(gpx) && !ferror(kml);
}

static int write_time(FILE *file, time_t base, double offset)
{
    double whole;
    double fraction = modf(offset, &whole);
    if (fraction < 0) { fraction += 1.0; whole -= 1.0; }
    time_t stamp = base + (time_t)whole;
    struct tm utc;
    char date[32];
    if (!gmtime_r(&stamp, &utc) || !strftime(date, sizeof(date), "%Y-%m-%dT%H:%M:%S", &utc))
        return 0;
    long millis = lround(fraction * 1000.0);
    if (millis == 1000) {
        ++stamp;
        if (!gmtime_r(&stamp, &utc) || !strftime(date, sizeof(date), "%Y-%m-%dT%H:%M:%S", &utc))
            return 0;
        millis = 0;
    }
    return fprintf(file, "%s.%03ldZ", date, millis) >= 0;
}

static int write_point(FILE *gpx, FILE *kml, const double *gps, time_t base,
                       double offset, const Options *options)
{
    if (!isfinite(gps[0]) || !isfinite(gps[1]) || !isfinite(gps[2]) ||
        gps[0] < -90.0 || gps[0] > 90.0 || gps[1] < -180.0 || gps[1] > 180.0)
        return 1;
    if (fprintf(gpx, " <trkpt lat=\"%.8f\" lon=\"%.8f\"><ele>%.3f</ele><time>",
                gps[0], gps[1], gps[2]) < 0 || !write_time(gpx, base, offset)) return 0;
    if (options->include_temperature || options->include_heart_rate) {
        fputs("</time><extensions><gpxtpx:TrackPointExtension>", gpx);
        if (options->include_temperature)
            fprintf(gpx, "<gpxtpx:atemp>%.1f</gpxtpx:atemp>", options->temperature);
        if (options->include_heart_rate)
            fprintf(gpx, "<gpxtpx:hr>%d</gpxtpx:hr>", options->heart_rate);
        fputs("</gpxtpx:TrackPointExtension></extensions></trkpt>\n", gpx);
    } else {
        fputs("</time></trkpt>\n", gpx);
    }
    return fprintf(kml, "%.8f,%.8f,%.3f ", gps[1], gps[0], gps[2]) >= 0 &&
           !ferror(gpx) && !ferror(kml);
}

static int close_outputs(FILE **gpx, FILE **kml)
{
    int ok = 1;
    if (*gpx && fclose(*gpx) != 0) ok = 0;
    if (*kml && fclose(*kml) != 0) ok = 0;
    *gpx = NULL;
    *kml = NULL;
    return ok;
}

int main(int argc, char **argv)
{
    Options options;
    int parsed = parse_options(argc, argv, &options);
    if (parsed != 1) return parsed == 2 ? 0 : 2;

    int result = 1;
    struct stat input_stat;
    FILE *gpx = NULL, *kml = NULL;
    uint32_t *payload = NULL;
    GPMF_stream stream = {0};
    GPMF_Source *source = GPMF_SourceCreate();
    char *gpx_path = NULL, *kml_path = NULL, *gpx_temp = NULL, *kml_temp = NULL;
    const char *prefix = options.output_prefix ? options.output_prefix : options.input;
    const char *name = file_name(options.input);
    time_t base_time;

    if (!source) { fprintf(stderr, "Out of memory creating GPMF reader\n"); goto cleanup; }

    if (stat(options.input, &input_stat) != 0) {
        fprintf(stderr, "Cannot access input '%s': %s\n", options.input, strerror(errno));
        goto cleanup;
    }
    base_time = input_stat.st_mtime;
    gpx_path = make_path(prefix, ".GPX");
    kml_path = make_path(prefix, ".KML");
    if (!gpx_path || !kml_path) { fprintf(stderr, "Out of memory creating output paths\n"); goto cleanup; }

    double metadata_length = OpenGPMFSource(source, options.input);
    if (metadata_length <= 0.0 || GetNumberGPMFPayloads(source) == 0) {
        fprintf(stderr, "No readable GPMF metadata found in '%s'\n", options.input);
        goto cleanup;
    }
    gpx = open_temporary_output(gpx_path, &gpx_temp);
    if (!gpx) { fprintf(stderr, "Cannot create temporary GPX output for '%s': %s\n", gpx_path, strerror(errno)); goto cleanup; }
    kml = open_temporary_output(kml_path, &kml_temp);
    if (!kml) { fprintf(stderr, "Cannot create temporary KML output for '%s': %s\n", kml_path, strerror(errno)); goto cleanup; }
    if (!write_headers(gpx, kml, name)) { fprintf(stderr, "Failed writing output headers\n"); goto cleanup; }

    uint32_t payload_count = GetNumberGPMFPayloads(source);
    double gps_rate = GetGPMFSampleRate(source, STR2FOURCC("GPS5"), GPMF_SAMPLE_RATE_PRECISE);
    if (!(gps_rate > 0.0) || !isfinite(gps_rate)) gps_rate = FALLBACK_GPS_RATE;
    uint64_t points = 0;
    for (uint32_t index = 0; index < payload_count; ++index) {
        uint32_t payload_size = GetGPMFPayloadSize(source, index);
        if (!payload_size || payload_size > INT32_MAX) { fprintf(stderr, "Invalid metadata payload size at index %u\n", index); goto cleanup; }
        uint32_t *next_payload = GetGPMFPayload(source, payload, index);
        if (!next_payload) { fprintf(stderr, "Cannot read metadata payload %u\n", index); goto cleanup; }
        payload = next_payload;
        if (stream.cbhandle) GPMF_Free(&stream);
        if (GPMF_Init(&stream, payload, payload_size) != GPMF_OK) {
            fprintf(stderr, "Invalid GPMF payload %u\n", index); goto cleanup;
        }
        double in = 0.0, out = 0.0;
        if (GetGPMFPayloadTime(source, index, &in, &out) != GPMF_OK) {
            fprintf(stderr, "Cannot determine timestamp for payload %u\n", index); goto cleanup;
        }
        if (GPMF_OK == GPMF_FindNext(&stream, STR2FOURCC("GPS5"), GPMF_RECURSE_LEVELS)) {
            uint32_t samples = GPMF_Repeat(&stream);
            uint32_t elements = GPMF_ElementsInStruct(&stream);
            if (elements < 3 || samples > UINT32_MAX / elements ||
                samples * elements > UINT32_MAX / (uint32_t)sizeof(double)) {
                fprintf(stderr, "Invalid GPS5 sample dimensions in payload %u\n", index); goto cleanup;
            }
            uint32_t buffer_size = samples * elements * (uint32_t)sizeof(double);
            double *values = malloc(buffer_size);
            if (!values) { fprintf(stderr, "Out of memory decoding GPS5 payload %u\n", index); goto cleanup; }
            GPMF_ERR decode = GPMF_ScaledData(&stream, values, buffer_size, 0, samples, GPMF_TYPE_DOUBLE);
            if (decode != GPMF_OK) { free(values); fprintf(stderr, "Cannot decode GPS5 payload %u (error %u)\n", index, decode); goto cleanup; }
            for (uint32_t sample = 0; sample < samples; ++sample) {
                double *gps = values + sample * elements;
                double offset = in + (double)sample / gps_rate;
                if (!write_point(gpx, kml, gps, base_time, offset, &options)) {
                    free(values); fprintf(stderr, "Failed writing telemetry output\n"); goto cleanup;
                }
                if (isfinite(gps[0]) && gps[0] >= -90.0 && gps[0] <= 90.0 &&
                    isfinite(gps[1]) && gps[1] >= -180.0 && gps[1] <= 180.0)
                    ++points;
            }
            free(values);
        }
        if (options.verbose && (index % 100 == 0)) fprintf(stderr, "Processed payload %u/%u\n", index + 1, payload_count);
    }
    if (points == 0) { fprintf(stderr, "No valid GPS5 points found in '%s'\n", options.input); goto cleanup; }
    fputs("</trkseg></trk></gpx>\n", gpx);
    fputs("</coordinates></LineString></Placemark></Document></kml>\n", kml);
    if (ferror(gpx) || ferror(kml) || !close_outputs(&gpx, &kml)) {
        fprintf(stderr, "Failed to finish output files\n"); goto cleanup;
    }
    struct stat target_stat;
    if ((stat(gpx_path, &target_stat) == 0 && S_ISDIR(target_stat.st_mode)) ||
        (stat(kml_path, &target_stat) == 0 && S_ISDIR(target_stat.st_mode))) {
        fprintf(stderr, "An output target is a directory\n"); goto cleanup;
    }
    if (rename(gpx_temp, gpx_path) != 0) { fprintf(stderr, "Cannot publish '%s': %s\n", gpx_path, strerror(errno)); goto cleanup; }
    gpx_temp[0] = '\0';
    if (rename(kml_temp, kml_path) != 0) { fprintf(stderr, "Cannot publish '%s': %s\n", kml_path, strerror(errno)); goto cleanup; }
    kml_temp[0] = '\0';
    if (options.verbose) fprintf(stderr, "Wrote %llu points to %s and %s\n", (unsigned long long)points, gpx_path, kml_path);
    result = 0;

cleanup:
    if (gpx || kml) close_outputs(&gpx, &kml);
    if (stream.cbhandle) GPMF_Free(&stream);
    if (payload) FreeGPMFPayload(source, payload);
    GPMF_SourceDestroy(source);
    if (gpx_temp && *gpx_temp) unlink(gpx_temp);
    if (kml_temp && *kml_temp) unlink(kml_temp);
    free(gpx_path);
    free(kml_path);
    free(gpx_temp);
    free(kml_temp);
    return result;
}
