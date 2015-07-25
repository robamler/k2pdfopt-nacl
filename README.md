K2pdfopt for Google Chrome Native Client
========================================

This repository is a fork of [k2pdfopt](http://www.willus.com/k2pdfopt/), which was written by William Menninger.
This fork turns k2pdfopt into a Google Chrome Portable Native Client (PNaCl) module.
PNaCl modules are (almost) binaries that can be run by the Google Chrome web browser inside a sandbox without requiring any extra permissions from the user.

K2pdfopt-nacl is free software: you can redistribute it and/or modify it under the terms of the [GNU Affero General Public License](http://www.gnu.org/licenses/agpl-3.0.html) as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.

The owner of this repository is not affiliated with Google, Inc.


Prerequisites
-------------

You need the following tools to compile k2pdfopt-nacl:

1. Some standard build tools; on Debian/Ubuntu:<br>
   ```
   sudo apt-get install autoconf automake cmake gettext libtool pkg-config xsltproc
   ```

2. The [Google Chrome browser](https://www.google.de/chrome/browser/desktop/) (to actually run the module).

3. The Native Client SDK:
   [Download](https://developer.chrome.com/native-client/sdk/download) the zip file and extract it to (e.g.) `~/nacl_sdk`.
   Then find out your version of the Chrome browser and check out the corresponding version of the NaCl SDK.
   E.g., if you have Chrome version 44, then run
   ```
   cd ~/nacl_sdk
   ./naclsdk update pepper_44
   export NACL_SDK_ROOT=`pwd`/pepper_44
   ```
   You may want to set `$NACL_SDK_ROOT` in your `~/.bashrc`.

4. Install `gclient` as described [here](http://dev.chromium.org/developers/how-tos/install-depot-tools):
   ```
   cd ~
   git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git
   export PATH=`pwd`/depot_tools:"$PATH"
   ```
   Again, you may want to add a line to your your `~/.bashrc` that sets `$PATH` permanently.

5. The [naclports](https://code.google.com/p/naclports/) project on Google Code facilitates the compilation of some commonly used libraries for Native Client. Install it as follows:
   ```
   mkdir ~/naclports
   cd ~/naclports
   gclient config --name=src https://chromium.googlesource.com/external/naclports.git
   gclient sync
   cd src
   git checkout -b pepper_44 origin/pepper_44  # should match your version of the NaCl SDK
   gclient sync # ignore error message, don't use --force
   ```
   The last command may produce an error message asking you to use the `--force` command line option.
   *Don't* do that and just ignore the error message.
   To make sure that you're on the right branch, try `git branch`.
   It should indicate "pepper_XX" as your current branch (where XX corresponds to the version you chose).

6. Clone this repository and its subrepositories:
   ```
   cd ~
   git clone https://github.com/robamler/k2pdfopt-nacl.git
   cd k2pdfopt-nacl
   git submodule update --init --recursive
   ```


Compilation
-----------

1. Go to the "naclports" directory you created in step 5 under "Prerequisites" above and compile some required libraries:
   ```
   cd ~/naclports/src
   NACL_ARCH=pnacl make zlib libpng freetype jpeg8d bzip2
   ```

2. Compile the MuPDF submodule:
   ```
   cd ~/k2pdfopt-nacl/mupdf
   make generate
   make OS=pnacl-cross build=release install-nacl-libs
   ```

2. Compile willuslib:
   ```
   cd ~/k2pdfopt-nacl/k2pdfopt/willuslib
   make
   ```

3. Compile the k2pdfopt NaCl module:
   ```
   cd ~/k2pdfopt-nacl
   make
   ```

4. (Optionally) reduce the file size of the generated NaCl module by removing some unneccessary stuff:
   ```
   cd ~/k2pdfopt-nacl/pnacl/Release
   $NACL_SDK_ROOT/toolchain/linux_pnacl/bin/pnacl-compress k2pdfopt.pexe
   ```


Usage
-----

The repository contains a very simple example page that lets you run the k2pdfopt NaCl module inside Google Chrome.

1. Start a simple HTTP server, e.g.
   ```
   cd ~/k2pdfopt-nacl
   python -m SimpleHTTPServer
   ```

2. Load the URL http://localhost:8000/ in Google Chrome.
