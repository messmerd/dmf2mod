# dmf2mod [![Build Status](https://github.com/messmerd/dmf2mod/workflows/build/badge.svg)](https://github.com/messmerd/dmf2mod/actions?query=workflow%3Abuild)

A cross-platform utility for converting Deflemask's DMF files to other trackers' module files.

&#8680; *Check it out [in your browser](https://messmerd.github.io/dmf2mod)!*

## Currently supported conversions

- DMF (Game Boy only) &#8680; MOD

Conversion to XM and other module formats may be added in the future.

## Build

### Command-line application

Linux and macOS:

```bash
cmake -S. -Bbin/Release
cmake --build ./bin/Release
```

Windows:

```bash
cmake -S. -Bbin
cmake --build .\bin --config Release
```

#### Web application

```bash
emcmake cmake -S. -Bbin/webapp
emmake make --dir=bin/webapp
```

Requires the Emscripten SDK.

## Usage

```text
dmf2mod output.[ext] input.dmf [options]
dmf2mod [ext] input.dmf [options]
dmf2mod [option]
```

Options:

```text
-f, --force                 Overwrite output file.
--help [module type]        Displays the help message. Provide module type (i.e. mod) for module-specific options.
--verbose                   Print debug info to console in addition to errors and/or warnings.
-v, --version               Display the dmf2mod version.
```

Options when converting to MOD:

```text
--amiga                     Enables the Amiga filter 
--arp                       Allow arpeggio effects 
--port                      Allow portamento up/down effects 
--port2note                 Allow portamento to note effects 
--vib                       Allow vibrato effects
--tempo=[accuracy, compat]  Prioritize tempo accuracy or compatibility with effects (Default: accuracy)
```

## DMF&#8680;MOD Conversions

Because of the severe limitations of the MOD format compared to DMF, there are several restrictions when converting DMF files to MOD. For example, DMF files must use the Game Boy system and patterns must have 64 or fewer rows.

The range of notes that ProTracker can play is less than half that of Deflemask, and because of this, dmf2mod must occasionally downsample wavetables to play higher frequency notes in MOD. Sometimes a square wave or wavetable will use two or more samples in MOD - each of which covers a separate note range. While this allows the limited ProTracker to play any note that can be played by Deflemask, it unfortunately can cause issues with portamento effects around the boundaries of these note ranges.

The MOD format is severely limited by only one effects column per channel, and this problem is exacerbated considering that the MOD format implements volume changes as effects. So to make the most of the situation, dmf2mod uses a priority system to determine which effect should be used for each MOD effects slot. Each type of nonessential effect (besides volume) has its own command-line option to enable its usage and provide the user with more control over the conversion process. By default, all these effects are disabled.

The behavior of portamento and vibrato effects depends on the speed of the MOD module. When the speed is at the default value of 6, these effects play at the correct rate. Otherwise they may play at the wrong rate or not at all. To ensure the proper initial speed value is used, use the `--tempo=compat` command-line option. This has the downside of limiting the tempo of the module to 16-127.5 BPM. By default, the `--tempo=accuracy` option is used, which enables the full tempo range of ~3.1 BPM to 765 BPM but cannot guarantee the correct behavior of effects - especially portamentos.

Deflemask instruments are unsupported, so if you want to change the volume of a channel or the WAVE channel's wavetable for example, your DMF module will need to use only the built-in commands in Deflemask's pattern editor.

Currently, dmf2mod converts notes, volume changes, initial tempo, and the following Deflemask effects:

- **0xy**  - Arpeggio (`--arp`)
- **1xx**  - Portamento Up (`--port`)
- **2xx**  - Portamento Down (`--port`)
- **3xx**  - Portamento to Note (`--port2note`)
- **4xy**  - Vibrato (`--vib`)
- **Bxx**  - Position Jump
- **Dxx**  - Pattern Break
- **10xx** - Set WAVE
- **12xx** - Set Duty Cycle

Effects 10xx and 12xx are implemented by changing the sample in MOD rather than as a MOD effect. As such, they do not count towards the 1 effect per channel limit.

SQ1, SQ2, and WAVE channels are supported.

In later updates, I may add:

- Systems besides Game Boy
- Support for patterns with over 64 rows
- More effects
- Tempo changes?
- Channel master volume?
- Noise channel?
- ...

Instruments will not receive support due to MOD limitations. The noise channel may receive support in the future if it is feasible and can be made to sound decent.

______
Created by Dalton Messmer <messmer.dalton(at)gmail(dot)com>.
