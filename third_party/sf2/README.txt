FengYin SF2 component

FluidSynth 2.6.3 (LGPL-2.1-or-later), unmodified source included:
fluidsynth-2.6.3-source.tar.gz
https://github.com/FluidSynth/fluidsynth/tree/v2.6.3
SHA256 c51ebe04c8a445f4a7dd5a86e09a27b5c5af547756574edc52fc61c88ebe8427

GCEM 012ae73c6d0a2cb09ffe86475f5c6fba3926e200 (Apache-2.0), source included:
gcem-source.zip
https://github.com/kthohr/gcem/tree/012ae73c6d0a2cb09ffe86475f5c6fba3926e200
SHA256 28159274c54e9640354852e172d10d88eb159f4e7f2fea42edbcd20105ed3526

FluidSynth is dynamically linked; the installed shared library can be replaced
with an ABI-compatible build, including your own modified build. The application
does not prevent reverse engineering solely for debugging changes to the LGPL
component. Keep the original filenames/ABI when replacing the shared library.
No soundfonts, synthesizer drivers, background services or external audio/MIDI
ports are installed by this component.

Build the supplied FluidSynth source with CMake 3.25+ and a C++17 compiler.
Use BUILD_SHARED_LIBS=ON, osal=cpp11, GCEM_INCLUDE_DIR pointing to the included
GCEM include directory. Set enable-alsa, aufile, dbus, ipv6, jack, ladspa,
libsndfile, midishare, opensles, oboe, network, oss, dsound, wasapi, waveout,
winmidi, sdl3, pulseaudio, pipewire, readline, openmp, native-dls, signalsmith,
coreaudio, coremidi, framework, systemd, portaudio to OFF. Build libfluidsynth.
Full application integration build rules are in cmake/FengYinSf2.cmake.

The wrapper defaults to dry audio, gain 0.2, 128 synth voices, one rendering
core. Wind expression is CH1 CC11; no built-in synthesizer reverb/chorus.
