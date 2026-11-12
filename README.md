# HeroineCLI

## Project goal

HeroineCLI turns GPS telemetry recorded by a GoPro camera into sidecar track files that mapping and activity applications can use. Given an MP4 containing GoPro Metadata Format (GPMF) data, it writes a GPX track and a KML track next to the video or at a chosen output prefix. It reads the video and does not alter it.

The project is a small command-line utility for workflows that need the camera's recorded GPS path outside the MP4 container, including importing a track into Garmin VIRB Edit or viewing it in Google Earth.

## How it works

```text
GoPro MP4
   │
   ├─ MP4 reader locates GPMF payloads and their timing
   │
   ├─ GoPro GPMF parser decodes and scales GPS5 samples
   │
   └─ HeroineCLI writes matching GPX and KML tracks
```

1. The MP4 reader walks the container metadata, records the byte ranges of GPMF payloads, and provides each payload's time offset.
2. The GoPro parser validates each payload and decodes the `GPS5` stream into latitude, longitude, and altitude samples.
3. HeroineCLI associates each sample with a UTC timestamp derived from the video's modification time and the sample's MP4 offset/rate.
4. It writes the points into GPX and KML temporary files, then publishes the completed files with the `.GPX` and `.KML` suffixes.

The reader stores its state in a `GPMF_Source` context, so separate MP4 readers do not share file handles or payload indexes. The vendored GPMF payload parser is from [GoPro's `gpmf-parser` repository at commit `a9445b4`](https://github.com/gopro/gpmf-parser/tree/a9445b4bf46ac7d66bb3bad54b6abfd8e68ad4be); its Apache and MIT license texts are included in this repository.

## Supported data and behavior

- Input: an MP4 with GoPro GPMF telemetry and a `GPS5` stream.
- Outputs: a GPX 1.1 track and a KML line track. Other telemetry streams are skipped; a file with no valid GPS5 points is reported as an error.
- Time: timestamps are UTC and start from the input file's modification time, advanced by the MP4 payload offset and GPS sample rate.
- Optional GPX extensions: `-t` writes the supplied constant temperature at every point; `-r` writes the supplied constant heart rate at every point. These values are user-provided and are not inferred from other sensors.
- Output names: without `-o`, the program appends `.GPX` and `.KML` to the input path. With `-o prefix`, it writes `prefix.GPX` and `prefix.KML`.

## Build

Build with `make` and a C23-capable C compiler:

```sh
make
```

The executable is written to `build/HeroineCLI`; compiled executables and build products are not versioned. The build uses the system C library and math library; there are no package-manager dependencies.

## Usage

```sh
build/HeroineCLI [-h] [-v] [-t Celsius] [-r bpm] [-o output-prefix] input.mp4
```

- `-h`: show help.
- `-v`: print conversion progress.
- `-t Celsius`: add constant temperature values, from -100 through 100 °C.
- `-r bpm`: add constant heart-rate values, from 0 through 300 bpm.
- `-o prefix`: choose the output path prefix.
- `input.mp4`: the single source video to read.

The command fails with a diagnostic for an inaccessible input, missing GPMF metadata, invalid GPS samples, or output errors. Existing output files are replaced only after conversion completes successfully.

## Tests

Run the argument, file-error, output, timestamp-rollover, XML-escaping, and output-structure checks with:

```sh
make test
```

To also test a real GPS5 conversion and that two MP4 reader contexts remain independent, provide a GoPro sample. A second sample without GPS5 can exercise graceful rejection:

```sh
make test \
  GPMF_SAMPLE=/path/to/gps5-sample.mp4 \
  GPMF_NO_GPS_SAMPLE=/path/to/no-gps5-sample.mp4
```
