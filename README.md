## Synopsis

This is the Simplified Staging Transport, a component of v2.0 of the ADIOS I/O system, developed as part of the U.S. Department of Energy Exascale Computing Program.

## License

ADIOS >= 2.0 is licensed under the Apache License v2.0. 

## Directory Layout

* cp - Code that implements the control plane
* dp - Code that implements the data plane
* framework - eventually this will be a testing framework, now simple reader
and writer programs
* CMake - Project-specific CMake modules

## Building

- First build and install EVPath.  This code relies upon some recent tweaks
to EVPath installation (in particular the `evpath_config` program which was
added/updated in July 2017).  Best to build the most recent stable release.
There are instructions at http://www.cc.gatech.edu/systems/projects/EVPath/,
but the short form is something like:
1.    cd ~/
2.    mkdir evpath_build
3.    cd evpath_build
4.    wget http://www.cc.gatech.edu/systems/projects/EVPath/chaos_bootstrap.pl
5.    perl ./chaos_bootstrap.pl
6.    perl ./chaos_build.pl

This will install EVPath in ${HOME}/{bin,lib,include}.  If you install
evpath elsewhere, set the EVPATH_ROOT environment variable before the next
step.

To build SST, do:
1.  git clone git@github.com:eisenhauer/sst.git
2.  cd sst;
3.  mkdir build
4.  cmake ..
5.  make all

At this point there are 'reader' and 'writer' programs in build/bin.  To
exercise SST, run them each under MPI, starting the writer first.  Verbosity
is turned on right now, so this might be more easily understood running them
in separate windows with small cohort sizes.  `reader` and `writer` only
open a single stream named "test" and contact info is written into and read
from the current working directory, so both must be cd'd to the same
directory for them to find each other.
