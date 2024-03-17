# BEAM

This documentation was prepared using version 0.9.9 in March 2024.

### What Is BEAM For?

BEAM is designed to help with the audio glitching and hard cuts that happen if you try and do live broadcasting using Endlesss Studio - while listening to a jam, new riffs will play as soon as they arrive, often cutting in at any point, causing audio pops and generally sounding a bit cheap. 

By comparison, BEAM watches a jam for changes, fetches new riffs as they arrive, fully resolves their stem data and then schedules this result for smooth blending with whatever is currently playing.

![](/doc/beam-099/beam-overview.svg)

The timing and length of the blends is configurable. BEAM can also record out the result as an 8-channel multitrack recording for later editing and mastering.

As an example, check out a [live Endlesss performance made with BEAM](https://www.youtube.com/watch?v=cQ2DRpkBmyE)

BEAM can also be manually told riff sequences to play and blend to make smoother, more refined riff-hops.

<br>
<br>

### BEAM Documentation

* [Starting Up](/doc/OURO.start.MD) - install **BEAM** and go through initial configuration; this process is shared between the **OUROVEON** applications.
* [UI Layout](/doc/LORE.ui-layout.MD) - although this documentation is centered around using **LORE**, **BEAM** uses the same UI methodology.

<hr>

* [Connecting to a Jam](/doc/BEAM.connect.MD) - connect to a jam, configure blending 