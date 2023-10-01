# LORE : Playback & Riff Details

The playback system is built out of 3 windows - audio engine control, riff playback & sequencing and the riff details panel.

![populated UI](/doc/lore-081/window-playback.png)

### Audio Engine

At the top you can adjust the master output volume, mute the audio output and panic-stop all current playback and sequencing operations. 

The two **Disk Recorder** buttons let you begin immediately writing the audio stream out to disk. **`Final Mix`** writes out the same data that it being played out of the audio device. **`8-Track`** starts one file per channel, writing out the unmixed stem data to each. 

<br>

### Playback & Sequence

The second window down displays the playback cursor, similar to Endlesss itself.

Toggling the **`Transition Timing`** button will enable simple constraints that lock when riffs will transition to the next one in the queue. Using this lets you build simple sequences of riffs that transition on the beat (or on the one, if you've been good). 

<br>

### Riff Details

The final group is a detailed overview of what is playing in the current riff. `Mute` and `Solo` buttons are available to toggle channels on/off (and these will respect the **`Transition Timing`** mode).

Left-to-right, buttons are available to:

* set / clear **Tags**, viewable in the $\textcolor{orange}{\textsf{Jam Tags}}$ view
* export the current riff to disk as stems; you can also use `CTRL-E` as a shortcut
* find the current riff in the $\textcolor{orange}{\textsf{Jam View}}$
* copy metadata for the riff to the clipboard (mostly developer / debug use only)

