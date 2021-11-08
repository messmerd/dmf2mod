dmf2mod [![Build Status](https://github.com/messmerd/dmf2mod/workflows/build/badge.svg)](https://github.com/messmerd/dmf2mod/actions?query=workflow%3Abuild)
======

A cross-platform command-line utility for converting Deflemask's DMF files to other trackers' module files.

⭢ *Check it out [in your browser](https://messmerd.github.io/dmf2mod)!*

### Currently supported conversions:
- DMF (Game Boy only) ⭢ MOD

### Possible future conversions:
- MOD ⭢ DMF
- DMF ⭢ XM
- XM ⭢ DMF
- MOD ⭢ XM

## Build
#### Command-line application:

```
make
```

Requires GCC (MinGW on Windows) and make. Works on Windows, Linux, and MacOS.
Other compilers besides GCC would probably work, though this hasn't been tested.

#### Web application:

```
make webapp
```

Requires the Emscripten SDK and make.

## Usage
```
dmf2mod output.[ext] input.dmf [options]
dmf2mod [ext] input.dmf [options]
```
Options:
```
-f, --force              Overwrite output file.
--help [module type]     Displays the help message. Provide module type (i.e. mod) for module-specific options.
-s, --silent             Print nothing to console except errors and/or warnings.
```

Options when converting to MOD:
```
--downsample             Allow wavetables to lose information through downsampling if needed.
--effects=[min,max]      The number of ProTracker effects to use. (Default: max)
```
 
## DMF⭢MOD Limitations
Because of the limitations of the MOD format, there are several restrictions on the DMF files that can be converted to MOD. For example, DMF files must use the Game Boy system, patterns must have 64 rows, only one effect column is allowed per channel, etc. 

The range of notes that ProTracker can play is about half that of Deflemask, and because of this, dmf2mod may have to downsample some wavetables to play them at higher frequencies in MOD. This is not done by default: You must pass the `--downsample` flag to allow dmf2mod to do this.

Only Deflemask modules with a BPM between 16 and 127.5 can be converted without changing the BPM.

Unlike Deflemask, the MOD format implements volume changes as effects, and since the MOD format is already severely limited by only one effects column per channel, most conversions will require the `--effects=min` flag which tells dmf2mod to only convert Deflemask volume changes (not including volume envelopes - see below) and ignore all Deflemask effects except for wavetable changes, position jumps, and the like.

Deflemask instruments are unsupported, so if you want to change the volume of a channel or the WAVE channel's wavetable for example, your module will need to use only the built-in commands in Deflemask's pattern editor.

Currently, dmf2mod converts notes, volume changes, initial tempo, and the following Deflemask effects:
- **Bxx**  - Position Jump
- **10xx** - Set WAVE
- **12xx** - Set Duty Cycle

Effects 10xx and 12xx are implemented by changing the sample in MOD rather than as an effect. As such, they do not count towards the 1 effect per channel limit.

SQ1, SQ2, and WAVE channels are supported.
 
In later updates, I may add:
- Pattern Break (Dxx)
- Tempo changes
- More effects
- Support for patterns of different sizes than 64

Instruments will not receive support due to MOD limitations, and neither will the Noise channel because it relies heavily on Instruments and their envelopes to sound good. However, support for copying only the Noise channel notes may be implemented in the future.

______
Created by Dalton Messmer <messmer.dalton(at)gmail(dot)com>.

zlib library created by Jean-loup Gailly and Mark Adler (https://zlib.net/).
