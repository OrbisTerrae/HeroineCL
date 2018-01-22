# HeroineCL © 2018 Orbis Terrae https://sites.google.com/site/oterrae/

Command Line App to transform GoPro Hero5 video to GPX (for usage in Garmin Virb Edit)

v0.2 - includes heart rate and temperature and cmd line interface

Usage: ./HeroineCL [-h] [-v] [-t Temperature] [-r HearRate] [-o Output] GOPRO_WITH_GPMF.MP4

-h = help
-v = verbose
-t = set initial temperature in °C (evolves with altitude)
-r = set base heart rate (sinus wave)
-o = output file (if skipped, used source file +".GPX")
GOPRO_WITH_GPMF.MP4 = source file with HERO5 GPMF data
