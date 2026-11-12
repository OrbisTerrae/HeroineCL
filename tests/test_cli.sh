#!/bin/sh
set -eu

program=$1
sample=${2-}
no_gps_sample=${3-}
program_dir=$(CDPATH= cd -- "$(dirname -- "$program")" && pwd)
program="$program_dir/$(basename -- "$program")"
tmp=$(mktemp -d "${TMPDIR:-/tmp}/heroinecl-test.XXXXXX")
trap 'rm -rf "$tmp"' EXIT HUP INT TERM

"$program" -h >"$tmp/help.txt"
grep -q 'Usage:' "$tmp/help.txt"

if "$program" >"$tmp/no-args.txt" 2>&1; then
    echo 'expected missing input to fail' >&2
    exit 1
fi
if "$program" -r invalid /dev/null >"$tmp/bad-rate.txt" 2>&1; then
    echo 'expected invalid heart rate to fail' >&2
    exit 1
fi
if "$program" "$tmp/does-not-exist.mp4" >"$tmp/missing.txt" 2>&1; then
    echo 'expected missing input file to fail' >&2
    exit 1
fi

printf 'not an MP4 file\n' > "$tmp/not-video.mp4"
if "$program" -o "$tmp/no-metadata" "$tmp/not-video.mp4" >"$tmp/no-metadata.txt" 2>&1; then
    echo 'expected a file without GPMF metadata to fail' >&2
    exit 1
fi
[ ! -e "$tmp/no-metadata.GPX" ]
[ ! -e "$tmp/no-metadata.KML" ]

if [ -n "$sample" ]; then
    "$program" -t 18.5 -r 90 -o "$tmp/sample-output" "$sample"
    python3 - "$tmp/sample-output.GPX" "$tmp/sample-output.KML" <<'PY'
import sys
import xml.etree.ElementTree as ET

gpx = ET.parse(sys.argv[1]).getroot()
kml = ET.parse(sys.argv[2]).getroot()
ns = {'g': 'http://www.topografix.com/GPX/1/1', 'x': 'http://www.garmin.com/xmlschemas/TrackPointExtension/v1', 'k': 'http://www.opengis.net/kml/2.2'}
points = gpx.findall('.//g:trkpt', ns)
assert points, 'GPX contains no track points'
assert points[0].find('g:time', ns) is not None, 'GPX point has no timestamp'
assert points[0].find('.//x:atemp', ns) is not None, 'temperature option was not written'
assert points[0].find('.//x:hr', ns) is not None, 'heart-rate option was not written'
coords = kml.find('.//k:coordinates', ns)
assert coords is not None and coords.text and coords.text.strip(), 'KML contains no coordinates'
PY
    mkdir "$tmp/output.KML"
    if "$program" -o "$tmp/output" "$sample" >"$tmp/open-failure.txt" 2>&1; then
        echo 'expected output setup to fail when the KML target is a directory' >&2
        exit 1
    fi
    [ ! -e "$tmp/output.GPX" ]
    [ -z "$(find "$tmp" -name 'output.GPX.tmp.*' -o -name 'output.KML.tmp.*')" ]

    cp "$sample" "$tmp/new & year.mp4"
    python3 - "$tmp/new & year.mp4" <<'PY'
import os, sys
from datetime import datetime, timezone
stamp = datetime(2023, 12, 31, 23, 59, 55, tzinfo=timezone.utc).timestamp()
os.utime(sys.argv[1], (stamp, stamp))
PY
    "$program" -o "$tmp/escaped-output" "$tmp/new & year.mp4"
    python3 - "$tmp/escaped-output.GPX" "$tmp/escaped-output.KML" <<'PY'
import sys
import xml.etree.ElementTree as ET
from datetime import datetime
ns = {'g': 'http://www.topografix.com/GPX/1/1', 'k': 'http://www.opengis.net/kml/2.2'}
gpx = ET.parse(sys.argv[1]).getroot()
kml = ET.parse(sys.argv[2]).getroot()
assert gpx.find('.//g:name', ns).text == 'new & year.mp4'
points = gpx.findall('.//g:trkpt', ns)
times = [datetime.fromisoformat(point.find('g:time', ns).text.replace('Z', '+00:00')) for point in points]
assert times[0].year == 2023 and times[-1].year == 2024, 'timestamps did not roll over midnight'
assert kml.find('.//k:name', ns).text == 'new & year.mp4'
PY
fi

if [ -n "$no_gps_sample" ]; then
    printf 'keep existing output\n' > "$tmp/no-gps.GPX"
    if "$program" -o "$tmp/no-gps" "$no_gps_sample" >"$tmp/no-gps.txt" 2>&1; then
        echo 'expected a sample without GPS5 to fail gracefully' >&2
        exit 1
    fi
    grep -q 'keep existing output' "$tmp/no-gps.GPX"
    [ ! -e "$tmp/no-gps.KML" ]
fi

echo 'CLI regression checks passed'
