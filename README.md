# ESPHamClock

This is a snapshot of the latest source distribution from Clear Sky Institute.

Take note!  This is for the front-end only.  Much of the functionality of HamClock
is provided by a back-end server, the code for which is/was owned by Elwood Downey.
A separate search and/or inquiry must be made to gain access to this code.  If this
is not possible, then there is nothing we can do on this front-end code to preserve
HamClock going foward.

With the passing of Elwood Downey (WB0OEW) it was important to capture this version 
right away.  This ensures a stable checkpoint from which any future development can
take place.

## PROCESS

Here is what I did to create this clean repo.

1. Located and downloaded a copy of the 4.22 version of the ESPHamClock.zip file.
	* This file was located in the FreeBSD distribution repository.
2. Validated that this file matched the checksum and expected size as stated in 
the FreeBSD repository.
3. Expanded the zip file.
4. Renamed the resulting 'ESPHamClock' directory to 'src'.
5. Created and populated the ARTIFACTS directory to document the starting state
of the repository as the final state of the source distribution from Clear Sky
Institute.
6. Created this README.md.

## FOLLOWUP PROCESS

After the initial checkin, further research showed that the version of the code in
the generic ESPHamClock.zip file was a much earlier version.  Research led me to
the correct zip file for the 4.22 version.  This was downloaded, merged into the
source directory, and the ARTIFACTS updated appropriately.  I left the initial
import tagged, however that tag actually reflects the 2.80 version.  An additional
tag will be placed on the repository to reflect the actual 4.22 release.

# TAGS

There are currently two tags in the repo:
* freebsd-distcash-2.80 
  * This tag reflects the initial basline established with the ESPHamClock.zip file
    which turned out to contain version 2.80.
* clear-sky-final-V4.22,unmodified
  * This is the basline for all future maintenance and development.  The src directory
    at this point reflects the actual state of the V4.22 release from Clear Sky.

# ARTIFACTS

The actual ESPHamClock-V4.22.zip file is available as an artifact in the ARTIFACTS
directory of this repo.  Anyone wishing to download this may do so here.  Also note
that at the time of this writing, the following URL also works to download the file
directly from the clearskyinstitute.com website:

https://clearskyinstitute.com/ham/HamClock/ESPHamClock-V4.22.zip


# OWNERSHIP

I do not claim ownership of this software.  The sole intent of this repository is
to establish a point in time for the source code, preserving the final state of
ESPHamClock, and to act as a baseline for any future development that might take
place.  Refer to the [PROVENANCE.md](ARTIFACTS/PROVENANCE.md) file for more
information regarding this repository.
