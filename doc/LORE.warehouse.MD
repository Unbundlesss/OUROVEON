# LORE : The Warehouse

Building on the overview in [UI Concepts](/doc/LORE.ui-concepts.MD), this document runs through how to add a new jam to the **Warehouse** and browse it in style.

#### Add Jam via Browser

![add jam button](/doc/lore-081/button-add-jam.png)

Click **`Add Jam...`** to launch the $\textcolor{orange}{\textsf{Jam Browser}}$ . This pop-up window will let you explore **LORE**'s stack of available jams to begin downloading. 

![jam browser](/doc/lore-081/jam-browser.png)

The tabs at the top let you choose between various streams of data:

* **Subscribed / Private** - these are the jams you have started or subscribed to; what you would see if you visit the *My Jams* page in Endlesss itself.
* **Current Join-In** - fetched whenever you update your **Jam Cache** during start-up, this is the list of public jams available in Endlesss.
* **Public Archive** - an ever-growing privately-captured list of all known jams that have been posted on the Endlesss Discord or that have been previously been Publics.
* **Collectibles** - also (optionally) fetched during **Jam Cache** update, this is the list of all mintable, collectible jams on the system
* **Custom** - where you can add your **Solo Jam** to the **Warehouse**, or any jam via its internal ID (assuming you have permissions to do so)

Click on any jam listed to begin the process of adding it to the **Warehouse**

#### Syncing

A newly added jam will immediately begin spooling the data from Endlesss' servers. Depending on the size of the jam, this can take a while - for a 10,000 riff jam, between 5-10 minutes depending on your internet connection.

![jam browser](/doc/lore-081/jam-sync-running.png)

While the sync is running, the **Warehouse** table will display the number of riffs (eg. $\textcolor{orange}{\textsf{+7307}}$ above) and stems left to sync.

If you decide you don't want to wait, you can click the 🚫 button and the download process will try and abort. This will leave the jam partially downloaded and the remaining un-fetched riffs will be displayed.

#### Re-Syncing

Click 🔁 to get **LORE** to fetch the latest data for the jam from Endlesss. If new riffs and stems are found, the download process will begin as before.

#### Viewing

Click the button in the first column to load the chosen jam in the $\textcolor{orange}{\textsf{Jam View}}$ - note that you cannot view a jam while it is being synchronised, this button will be disabled.
