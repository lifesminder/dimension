# Dimension
Dimension is a free and open-source animation tool (in future), intended to make animations without paying a single dimme for them. Dimension is the first program defining programmed animation, because we believe - programming is art, and art can be programming, too!

![Demo of the Dimension](./docs/demo.png)

The [previous version](https://git.lmd.vg/ash/dimension-diploma) of the program was created as a college diploma work, but the idea remained and started to improve gradually.

## Building
For building Dimension, you need Visual Studio 2026 or later, because of it's `.slnx` new format.
After running, build the entire solution and navigate to appropriate folders, based on configuration you have set:

* `(ARCHITECTURE)_(RELEASE)` - architecture (x86, x64) and release (Debug and Release) - here are your compiled binaries,
  * `dimcore.dll` - core definitions of Dimension, such as engine, parser and etc,
  * `dimension.exe` - the main client of Dimension, here all visible magic happens :)

## Future
Since Dimension is a program in-works, I have some plans to improve it later:

* Make Dimension fully finished with tuned base and without cross-platform lockers inside core - **In Progress**,
* Port the program into Linux and other operating systems,
* Extend the specifications, and define custom standard of animation,
* Uplift it's look and feel so it will be even easier to use later.
