# DX7Dump

DX7Dump is a command line tool for transforming Yamaha DX7 sysex files into a human readable format. This makes it ideal for learning FM synthesis by manually programming existing patches into a DX7 or compatible synthesiser.

This repository contains a forked version of the DX7Dump tool by Ted Felix. More information on that version of the tool can be found on [Ted's Yahama DX7 page](http://tedfelix.com/yamaha-dx7/index.html), and the original [SourceForge repository](https://sourceforge.net/u/tedfelix/dx7dump/ci/master/tree/).

# Usage

    dx7dump [OPTIONS] filename

    Options:    -l      List all parameters for all 32 patches.
                -p n    List all parameters for the specified patch.
                -v      Display version information.
                -h      Display this help information.

# Changes

I've made the following changes in this fork:

* Updated the struct packing so that it works on modern systems.
* Updated the file reading method to use streams.
* Updated the console outputs to use streams.
* Implemented missing single patch output functionality.
* Rearranged the outputs to match the Korg Volca FM parameter names and ordering.
* Retrieved and included the referenced sysex-format.txt file.
* General changes to output messages and formatting.
* General refactoring to more idiomatic C++ code.

# Original Readme by Ted Felix

Yamaha DX7 Sysex Dump
Copyright 2012, Ted Felix (www.tedfelix.com)
License: GPLv3+

Takes a Yamaha DX7 sysex file and formats it as human readable text.
The format is also conducive to using diff (or meld) to examine differences
between patches.

Based on info from:
http://homepages.abdn.ac.uk/mth192/pages/dx7/sysex-format.txt

Build:
  g++ -o dx7dump dx7dump.cpp
