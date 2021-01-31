# Change log

This change log will be updated whenever a new official release is made

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
