This is a fork of Q2PRO with some modifications for Linux handhelds and the PortMaster service.  I'm posting the modifications here as per Q2PRO's GPL2 license.
* Removed root user check (I know it's for security, but most CFWs run as root)
* A and B can navigate the menus

To build a PortMaster compatible version:
```
meson setup builddir
meson configure -Dsystem-wide=false builddir
meson configure -Dlibcurl=disabled builddir
meson configure -Dx11=disabled builddir
meson configure -Dwayland=disabled builddir
meson configure -Dsdl2=enabled builddir
meson compile -C builddir
```

Original Q2Pro readme below.

Q2PRO
=====

Q2PRO is an enhanced Quake 2 client and server for Windows and Linux. Supported
features include:

* unified OpenGL renderer with support for wide range of OpenGL versions
* enhanced console command completion
* persistent and searchable console command history
* rendering / physics / packet rate separation
* ZIP packfiles (.pkz)
* JPEG/PNG textures
* MD3 and MD5 (re-release) models
* Ogg Vorbis music and Ogg Theora cinematics
* fast and secure HTTP downloads
* multichannel sound using OpenAL
* stereo WAV files support
* forward and backward seeking in demos
* recording from demos
* server side multiview demos
* live game broadcasting capabilities
* network protocol extensions for larger maps
* won't crash if game data is corrupted

For building Q2PRO, consult the INSTALL.md file.

Q2PRO doesn't have releases. It is always recommended to use the git master
version.

For information on using and configuring Q2PRO, refer to client and server
manuals available in doc/ subdirectory.
