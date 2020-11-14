# Change log

This change log will be updated whenever a new official release is made

## Build 16 (Nov 14 2020)

* Include `senddsq`, a tool for sending DiseqC sequences
* Use Docker to build new releases, provides a stable reproduceable environment
* Remove tvheadend and Python support in an attempt to make this easier to maintain
* Improve build speed by using multi-threading and more efficient file operations
* Update minisatip to 1.0.4 (`1.0.4-eef7333`), now using upstream directly instead of a fork
* Disable SAT>IP client support in minisatip
* Include iperf3 for debugging ethernet capacity issues
