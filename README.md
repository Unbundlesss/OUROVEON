![](doc/ouroveon_vec.svg)

Experimental audio projects built to interact with data from multiplayer music collaboration app [Endlesss](https://endlesss.fm). **OUROVEON** is a set of interconnected apps, built from modular components around an evolving, homebrew "Endlesss SDK" (as an official SDK does not exist, nor does a sanctioned API). One part a learning exercise in lower-level audio coding and one part laboratory for ideas that build upon Endlesss' particular data model.

**These tools and all code within are not affiliated with or endorsed by Endlesss Ltd. Use at your own risk.**

While considered in perpetual beta, hundreds of hours of development, testing and play have gone into their development. Hopefully they should give you a robust, lean and fun way to interact with all the music you've made on Endlesss.

They are mature enough to have been used to help create published albums, such as both [This Twilight Garden](https://oddstar.bandcamp.com/album/this-twilight-garden-live-looping-sessions-volume-1) and its [Second Volume](https://oddstar.bandcamp.com/album/this-twilight-garden-live-looping-sessions-volume-2) by [oddSTAR](https://endlesss.fm/oddstar) as well as the less-polished-but-certainly-still-technically-music [Horse Science Sessions](http://ishanisv.org/hs/).

<br>
<hr>

[Pre-built releases](https://github.com/Unbundlesss/OUROVEON/releases) are available for
 * **Windows** - *x86_64, Windows 8.1 or later*
 * **MacOS** - *Universal, Catalina (10.15) or later (notarised, signed, ready to run)

Privately we additionally build and test on Ubuntu Linux 23.04

<hr>
<br>


<br>

Shared Features

 * $\textcolor{orange}{\textsf{Endlesss Power}}$ ~ The custom SDK provides comprehensive support for many facets of **Endlesss**' services, including

   * Authenticated sign-in, or fallback to a limited API set using public endpoints
   * Support for Endlesss' lossless mode (as of v0.8.0)
   * Comprehensive jam browser, offering a users' subscribed jams, solo jam, all current publics and a huge archive of previous open/public jams stretching back
   * *Shared Riff* feed parsing
   * Audio data from Endlesss is stored as-is in a trivially auditable offline stem cache for fast playback and archival
   * Support for full archival / restoration of stem and database data per-jam, allowing for backup or trading of entire jam datasets
   * Deals with quirky and damaged Endlesss data, all the way back to the earliest jams in 2019
   * Built in retry/adapt fixes for unstable networks or servers

 * $\textcolor{orange}{\textsf{Audio Engine}}$

   * Lean audio mixers built on [PortAudio](https://www.portaudio.com/)
   * High-quality resampling using [r8brain](https://github.com/avaneev/r8brain-free-src)
   * Beat-analysis and energy estimation via Qlib, driving UI and OpenGL shader-based visualisers
   * Support for FLAC and Ogg Vorbis source formats
   * Support for FLAC and WAV output - 9 simultaneous, asynchronous streams (depending on your hardware)
   * (*Windows Only*) VST effect hosting & automation
   * (*Windows Only*) IPC real-time exchange of mixer state, beats, energy, jammer names, etc

 * $\textcolor{orange}{\textsf{UI Rendering}}$

   * Cross-platform front-end using [GLFW](https://www.glfw.org/)
   * Layout and controls running on [Dear ImGui](https://github.com/ocornut/imgui)

<br>
<hr>
<br>

# LORE

### __Deluxe Jam Archeology__

**Endlesss** is incredible for making a lot of music, quickly. It's *really terrible* at navigating back through that music, finding what you want and getting it exported for use elsewhere. **LORE** was born to comprehensively solve these issues as well as offering an opportunity for long-term offline archival with easy storage & browsing of years' worth of music.

Documentation available [here](/doc/LORE.MD)

![LORE UI gif](doc/080/lore_ui_1.gif)

Features

* $\textcolor{orange}{\textsf{Built For Scale}}$ ~ Download and synchronise even the largest (techno) jams on the platform. Navigating through 50,000+ riffs in LORE is a breeze, offering various data visualisation systems and username-searching.

* $\textcolor{orange}{\textsf{Built For Speed}}$ ~ Rapid and durable even on older, limited hardware. Fully multithreaded.

* $\textcolor{orange}{\textsf{Local Ownership}}$ ~ Download, explore, tag, export & archive your Endlesss music forever, even if the Endlesss cloud service disappears.

* $\textcolor{orange}{\textsf{Song Sketching}}$ ~ Pick and instantly play any riff, build simple sequences with transition timing, create bookmarks to help plan future tracks and exports.

* $\textcolor{orange}{\textsf{Open Data Formats}}$ ~ Everything is stored in easy-to-read `sqlite3` or JSON, trivially accessible or extended by other 3rd party tools.

<br>
<hr>
<br>

# BEAM

### __Live Jam Broadcast & Recording__

*Get the very best out of your live Endlesss performances*

### NOTE: **BEAM** has not been actively developed recently as **LORE** has taken priority. It may end up being heavily refactored.

![](doc/080/beam_ui_1.gif)

_BEAM_ connects to a chosen jam, watches for changes, syncs live stems and produces a high-quality broadcastable mix. Additionally use a websocket connection (including from LORE) to push arbitrary riff sequences for blending.

Features

* $\textcolor{orange}{\textsf{Smooth Riff Transitions}}$ ~ with configurable blending and timing. no more glitchy hard cuts at weird times between riffs.

* $\textcolor{orange}{\textsf{Multitrack}}$ ~ record all 8 channels to individual FLAC outputs on disk, live

* $\textcolor{orange}{\textsf{Performance Compression}}$ ~ for multitrack; reduce repetition in final recordings by ensuring each riff gets at most one full loop before recording is paused


_BEAM_ has so far broadcast over 50 hours of jam sessions without missing a beat, including running for a sustained [24-hour live set](https://www.youtube.com/watch?v=DHh6k6ehYDg).

The _BEAM_ live visualisation sync functionality over IPC was used along with the [NESTDROP](https://nestimmersion.ca/nestdrop.php) visualiser and the Unity 3D engine to broadcast [this hour-long jam](https://www.youtube.com/watch?v=cQ2DRpkBmyE)


<br>
<hr>
<br>

$\textcolor{orange}{\textsf{BUILDING}}$

**OUROVEON** is written in C++20, built using [Premake](premake.github.io). 
