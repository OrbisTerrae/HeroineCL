#HeroineCL ©2018 Orbis Terrae https://sites.google.com/site/oterrae/

Command Line App to convert GoPro Hero5 MP4 video to GPX & KML
GPX: compatible with Garmin Virb Edit and other apps
KML: Google Earth

v0.3.1
- use file creation time for GPS data
- added KML file support

Usage: ./HeroineCL [-h] [-v] [-t Temperature] [-r HearRate] [-o Output] GOPRO_WITH_GPMF.MP4
-h = help
-v = verbose
-t = set initial temperature in °C (evolves with altitude)
-r = set base heart rate (sinus wave)
-o = output file (if skipped, used source file +".GPX/KML")
GOPRO_WITH_GPMF.MP4 = source file with HERO5 GPMF data

Questions, remarks, feedback: oterrae@gmail.com
