# satip-axe

![Build firmware](https://github.com/Jalle19/satip-axe/workflows/Build%20firmware/badge.svg)

A maintained fork of [perexg/satip-axe](https://github.com/perexg/satip-axe), a firmware with minisatip for Inverto IDL-400s/Grundig GSS.BOX/Telestar Digibit R1

## Releases

Releases can be found [here](https://github.com/Jalle19/satip-axe/releases).

## Improvements in this fork

* Uses upstream minisatip without any custom patches
* DVB-CSA support in minisatip (due to CPU limitations only 1-2 streams can be decoded simultaneously)
* Uses newer version of OScam
* Reworked build system for easier development
* Leaner firmware image (obsolete versions of minisatip, tvheadend and Python have been removed)

## Build instructions

The build system used in this repository uses Docker. To build a new release, simply run:

```bash
make docker-clean-release
```

The release build will be in the `out/` directory.

## More information

For general information, see [upstream's README](https://github.com/perexg/satip-axe#readme) and [upstream's debug/README](https://github.com/perexg/satip-axe/blob/master/debug/README.md)
