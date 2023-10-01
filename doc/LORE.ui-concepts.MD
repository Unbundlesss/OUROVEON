# LORE : UI Concepts

![populated UI](/doc/lore-081/default-ui-populated.png)

**LORE**'s main purpose is to allow the download, storage and navigation of the music made on Endlesss. 

There are a few different parts servicing that mission:

<hr>

#### $\textcolor{orange}{\textsf{Playback}}$ 

![populated UI](/doc/lore-081/window-playback.png)

This group of windows are concerned with controlling the playback and export of riffs; as there is a lot going on, the finer details have been split out into their own document [here](/doc/LORE.playback.MD).

<hr>

#### $\textcolor{orange}{\textsf{Data Warehouse}}$ 

When you want to explore a jam, you first add it to the **Warehouse**. This process involves **LORE** downloading all the structural data for the jam from the Endlesss servers, piece by piece -- *which can take a while for big jams!* -- and store it in a database on your PC. 

![populated UI](/doc/lore-081/window-data-warehouse.png)

Once downloaded and indexed, this data can then be rapidly & flexibly queried rather than having to repeatedly talk to Endlesss again. This also allows us to archive off jam data in case the Endlesss service ever stops being available. 

Read more details on the [Warehouse overview page](/doc/LORE.warehouse.MD).

<br>

Internally, the **Warehouse** is a [sqlite3](https://www.sqlite.org/index.html) database that stores all the riff, stem and jam metadata in a logical, efficient format that is also designed to be easy to parse by other tools.

If you wish to examine the internals, the database lives at `\cache\common\warehouse.db3` inside your [Data Storage](/doc/LORE.start.MD) path. A tool like [DB4S](https://sqlitebrowser.org/) can be used to dig around inside.

<hr>

#### $\textcolor{orange}{\textsf{Jam View}}$ 

Making use of all that data in the **Warehouse**, the $\textcolor{orange}{\textsf{Jam View}}$ can display a configurable and interactive visualisation of an entire jam, regardless of the number of riffs. This view has been tested with the largest jams on the platform, over 50,000 riffs.

![jam viewer](/doc/lore-081/window-jam-view.png)

Each square represents a single riff. The view can apply various data-driven colour schemes to the riffs, helping decode the flow and changes in a jam over time - for example, a heat-map gradient driven by how many users are contributing to the jam at any time, effectively giving a quick way to find busier sections.

Riffs can have identifiers added based on the contributing username, letting you find not only your own riffs but anyone else you wish to keep track of.

A "sightline" overlay next to the scrollbar displays ticks where specified users are found, allowing you to scroll immediately to find your (or others') commits.

Clicking a riff enqueues it for playback.

Switching to the second page of the window allows you to modify the layout & choose which users to track.

![jam viewer](/doc/lore-081/window-jam-view-page2.png)

<hr>

#### $\textcolor{orange}{\textsf{Shared Riffs}}$ 

![jam viewer](/doc/lore-081/window-shared-riffs.png)

Choose any user, hit **`Sync Latest`** and the $\textcolor{orange}{\textsf{Shared Riffs}}$ view will download all the riffs that user has shared to their public feed -- and if the user in question is yourself, it will also pull the private shared riffs, marking them with a 🔒

Each shared riff can be enqueued for playback with ▶. Click the 🔗 to launch a web browser displaying Endlesss' online riff player for that entry. The **Find** ┅ button will attempt to load the original jam in the $\textcolor{orange}{\textsf{Jam View}}$ and search for the shared riff, allowing you to discover where and when it was originally shared from. This is not available for other users' personal jams.
