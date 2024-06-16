# Building

**OUROVEON** is written in C++20, built using [Premake](premake.github.io). 

## LORE : MacOS

Building _LORE_ on MacOS requires MacOS Sonoma. The build will work on earlier systems, but building requires Sonoma. There should be no need to compile LORE yourself unless you have a specific  reason. You can download [Pre-built releases](https://github.com/Unbundlesss/OUROVEON/releases)  for Mac and Windows. You will need to use the Mac terminal, `git` and Xcode 15. You will need to know your Administrator Password.

### Pull repo and generate project

In the terminal, clone the repository. If you do not have a location for storing your Git repositories, start by making one:

```shell
mkdir -p ~/repos
cd ~/repos
```

Then use git to clone the OUROVEON repo, which is what contains the source code for LORE. Then open the repository folder.

```shell
git clone https://github.com/Unbundlesss/OUROVEON.git
cd OUROVEON
```

OUROVEON depends on external repositories called submodules. We can easily pull these too and initialize them. 

```shell
git submodule update --init
```

Next we run a script that calls`premake` for our operating system. This creates the Xcode project for you, so you don't have to. The script is found in the `build` folder. You should see a lot of text go by.

> **Note**
> Don't forget to type the `./` in `./premake-osh.sh`

```shell
cd build
./premake-osh.sh
```

### Configure Xcode project format, target and signing

 The project is been created in `build/_gen`. Lets open it in Xcode. Here is the easiest way to do that:
```shell
open _gen/LORE.xcodeproj
```
- On the left side panel in xcode, select the top item `LORE` (see arrow 1 in image below). 
- Select the Project `LORE` as well (2). 
- If the right panel is missing, press the button at the top of the window to show it (3). 
- Then on the right panel under `Project Document` heading, select "Xcode 15.3" under `Project Format` (4). 
- Next in the middle window select `Project > LORE > info tab > Deployment Target > macOS Deployment Target` (arrow 5) and select "10.15"

![](/doc/build/xcode-step1.png)

- Next we will handle signing. We're going to sign it so that we can run it on our computer.
- In the middle window go to the `TARGETS > LORE` (see arrow 6 in picture below).
- Select the `Signing & Capabilites` tab (7) and the `Release` tab.
- Set Team to "None" (8) and set Signing Certificate to "Sign in to Run Locally"

![](/doc/build/xcode-step2.png)

- Finally, we set distribution to "release".  Open the menu option `Product > Scheme > Edit Scheme`
- Select `Run` (arrow 10) and change `Build Configuration` to "Release" and close the window.


![](/doc/build/xcode-step3.png)

### Build

Now you are ready to build the binary:

- Select the "Product" menu, then "Run" (or command-B)

When it succeeds, we will go back to your terminal to assemble the binary into a Mac application. 

Assuming you are still in `OUROVEON/build`:

```shell
cd macos-app
./assemble.sh LORE "Me"
```

You'll get a message about failing notarization and being unable to find a key chain password.  Ignore it for now, you are just going to run it on your own computer. 

This command has created a zip file in the same folder. Lets unzip it remove any already installed versions, and move the new one into place:

```
unzip *.LORE.zip
sudo rm -rf /Applications/LORE.app
sudo mv LORE.app /Applications/
```

If that all worked, you should have you custom build LORE in your Application directory.
