# Change log

This change log will be updated whenever a new official release is made

## future release
* Upgrade `minisatip` to v1.2.38
* Configure `minisatip` to use `/etc/sysconfig/minisatip` as cache directory, 
  fixes "bootid" being reset every time. Might help with disappearing auto-discovered SAT>IP servers in tvheadend.

## Build 22 (Aug 9 2022)
* Upgrade Dropbear to 2022.82
* Upgrade `minisatip` to v1.2.12 (issues from the previous build have been fixed)

Note that the `-U` option has been replaced by `-A`. For example, `-U 0-2:3` would now be 
`-A 0:0:0,0:1:0,0:2:0,1:3:0` or `-A 0:0:0,0:1:0,0:2:0,1:3:1`.

## Build 21 (Mar 26 2022)
* Downgrade `minisatip` to v1.1.83 (issues reported in https://github.com/Jalle19/satip-axe/issues/28)
* Re-instante the `upgrade-fw` script, adapt it to support local upgrades only

## Build 20 (Mar 23 2022)
* Update `minisatip` to v1.1.87

## Build 19 (Nov 26 2021)
* Update `minisatip` to v1.1.50
* Re-enable SAT>IP client support in minisatip
* Enable DVB-CSA support in minisatip (due to CPU limitations only 1-2 streams can be decoded simultaneously)
* Remove the `upgrade-fw` script and some other unused/obsolete utilities
* Improve the build process (less verbosity, no root-owned files polluting the working directory)

## Build 18 (Jul 11 2021)
* Update `oscam` to revision 11693
* Update `minisatip` to v1.1.9-bf62510
* Replace bundled `senddsq` with upstream (https://github.com/akosinov/unicable, contains one minor fix since last version)
* Remove bundled `multicast-rtp` script - the firmware ships without Python so the script is essentially useless

## Build 17 (Jan 31 2021)
* Update `minisatip` to `191fe62a7a5aaada03ef274511b24238c210693c`, should fix sending of UDP packets (#3)
* Don't skip SSL certificate checks when running `wget`

## Build 16 (Nov 14 2020)

* Include `senddsq`, a tool for sending DiseqC sequences
* Use Docker to build new releases, provides a stable reproduceable environment
* Remove tvheadend and Python support in an attempt to make this easier to maintain
* Improve build speed by using multi-threading and more efficient file operations
* Update `minisatip` to 1.0.4 (`1.0.4-eef7333`), now using upstream directly instead of a fork
  * Previous versions of `minisatip` have been removed. If you've been using `minisatip7` or `minisatip8` you'll need to update your configuration to use `MINISATIP="yes"` and `MINISATIP_OPTS=` instead.
* Disable SAT>IP client support in    
* Include iperf3 for debugging ethernet capacity issues
