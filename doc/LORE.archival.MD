# LORE : Full Download & Archival of Jams

When you synchronise a Jam from Endlesss into LORE (as described in the [Warehouse](/doc/LORE.warehouse.MD) docs), this contains all the metadata for the riffs and stems that make up the Jam.

When you play riffs from that Jam, LORE will download the stem audio on demand and cache those files in the OUROVEON cache folder. 

When you play that riff again in the future, LORE will check to see if it already has the stems downloaded - and then load them off disk if so.

This approach means that if you were to play all the riffs from a Jam, all the stems would end up locally cached and you would no longer need the Endlesss servers to browse and enjoy your Jam. Y'know, for example, if they suddenly shut the servers down with only 8 days warning, for example.

Instead of having to manually play all the riffs to ensure all the stems are downloaded, LORE contains some simple tools to:

* Download all stems for a given jam
* Package up all the stems and all the database metadata for a jam into an easy-to-share pair of archival files
* Import those archival files back into LORE, giving you the means to load an archived jam and all the audio data ready for completely offline browsing and playback

Lets see how to do each of those in turn...

---

## Full Jam Download

### Step 1 
Let's assume you have a Jam synchronised. Read the other docs if you haven't got that far!

![jam sync](/doc/lore-085/archive-step1.png)

LORE will only download what it knows about, so ensure you've done a **Sync** and fetched all the latest data from the servers. 

### Step 2

Switch the **Data Warehouse** panel to the second page. If you want to know how to change pages, check the [UI layout](/doc/LORE.ui-layout.MD) doc.

![cache page](/doc/lore-085/archive-step2.png)

Click the **Cache ...** button next to the jam you wish to download.

![cache dlg 1](/doc/lore-085/archive-step2-dialog1.png)

The cache dialog will open, describing the tool's function. Click on **Fetch Stem Workload** once you're ready to proceed. This will go fetch all the stems we need to try and download from the local Warehouse database.

![cache dlg 2](/doc/lore-085/archive-step2-dialog2.png)

After it gathers the work required, it will inform you how many stems the jam consists of and an estimate of how much disk space is required. Note that some of these stems might already be in your local cache. As it works through the list, it will only download ones that it cannot find locally.

Click **Begin Download** to begin the process. Note that the download can be interrupted and restarted at any time.

![cache progress](/doc/lore-085/archive-download.gif)

Click **Close** to stop the download, or wait until all stems are downloaded. Note that there can be server issues that cause some stems to fail to download on the first try .. and some that cannot be downloaded, due to strange problems on the Endlesss CDN. Re-run the **Cache** operation multiple times if you get failed downloads, see if second or third attempts will help.

<br>

<br>

---

## Exporting A Jam

Once you've synchronised a jam and cached all the stems, you can choose to bundle up all that data into a pair of files that contain everything one might need to browse and play with in the future. 

Tools to do that are in the *third* page of the Warehouse panel. If you want to know how to change pages, check the [UI layout](/doc/LORE.ui-layout.MD) doc.

![export panel](/doc/lore-085/export-jam-1.png)

There are two things we can export - **Data** and **Stems**.

* **Data** is the metadata stored in the Warehouse database - all the riff descriptions, stem links, timestamps, etc. This is exported as a plain text `YAML` file, format explained [here](https://en.wikipedia.org/wiki/YAML)

* **Stems** is, as you might guess, the audio data in `.OGG` or `.FLAC` format as downloaded directly from the Endlesss servers, untouched. When exported, they are bundled together into a single archive file using the `.TAR` format (similar to `.zip`, but without compression)


Clicking on either Data or Stems button will start the export process. Note to have a complete offline-playable Jam, you need to export both. The process can take a while, LORE will show a busy indicator at the bottom-right of the screen as it works and pop a toast notification once done.

![export panel](/doc/lore-085/export-progress.gif)

The resulting files can be found under your Output directory - use the Export menu to jump there.

![export panel](/doc/lore-085/export-output-folder.png)

inside `\lore\$database_exports` you will find the files ( in this example they are for the jam "Trianglekult live 1")

* `orx.banddb1dbad771.trianglekult_live_1.tar`
* `orx.banddb1dbad771.trianglekult_live_1.yaml`

The name of the file includes:

* `orx` ('OuRoveon eXport')
* `band#########` (the original Endlesss unique ID for the jam)
* a filename-sanitised version of the jam name

With BOTH of these files, you can import the whole jam into a new LORE installation or send them to a friend if they want to browse that jam themselves without downloading from Endlesss.


### Details For Nerds

* Each YAML contains:
    * the export date in Unix timestamp format
    * the version of OUROVEON used to export
    * a schema for the Riff and Stem metadata as a YAML comment
    * both 'plain' floating-point values as well as hex-float variants for perfect round-trip parsing

* TAR files were chosen as the audio data is already compressed and TAR therefore provides the safest, easiest to recover container format for longer-term storage. Even if LORE vanished, you can unpack and use the stem files trivially.

<br>

<br>

---

## Importing A Jam

Got hold of a couple ORX backup files have ya? Let's get them imported into LORE so you can browse that jam.

For ease of import, I recommend you put the `.YAML` and `.TAR` files into the `\lore\$database_exports` path discussed above.

![export panel](/doc/lore-085/export-output-folder.png)

To jump to your local Output folder where you'll find it.

You'll need to be on the *third* page of the Data Warehouse panel. If you want to know how to change pages, check the [UI layout](/doc/LORE.ui-layout.MD) doc.

![export panel](/doc/lore-085/import-jam-1.png)


You use the **Import Data** button to load `.YAML` files into the local database. This loads up all the riff and stem metadata, ready to browse.

You use the **Import Stems** button to unpack `.TAR` files full of stem audio into your local cache, meaning you no longer have to ask the Endlesss servers for audio data - it will be loaded automatically from your local cache.

Import the Data and Stems - doesn't matter which order you do so - each one may take a moment, LORE will display a Busy indicator at the bottom-right as it works and pop a toast notification once they are done.

![export panel](/doc/lore-085/import-progress.gif)

You can now browse and play back riffs from the jam offline!
