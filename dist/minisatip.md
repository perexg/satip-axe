minisatip configuration in satip-axe firmware
=============================================

The idl4k boxes have integrated universal switch matrix for all four inputs.
It allows very complex usage. This document tries to explain some special
configurations. For simplicity, the input are numbered as 0-3 here, but
the physical F-type connector on the box have numering 1-4.


Standard DiseqC timing and setup
--------------------------------

### Example: '-q "\*:5-15-0-5-5-0" -d "\*:\*1-0"'

Note: It can be used only with minisatip5 (v0.5+).

Power
-----

Since build 11, you may control the power on box inputs. If you
use '-P 1' option, the inputs will be powered down when no active
client is using the minisatip. This behaviour is similar to minisatip
implementation in build 10 or bellow.

If you keep the standard behaviour '-P 0', the inputs are powered down
when they are not used by any client. There is a delay of 30 seconds
before the power is turned off. Depending on the coaxial wiring and
used reception components, the independent input power down might
affect the signal quality during the power down phase.

Free inputs
-----------

This mode is most powerful but requires a client which known
the limitations like tvheadend. In this mode all sources (src=)
defined by the SAT>IP specification are absolute (shared for
all tuners). So any tuner can use any position but with
physical restrictions.

### Example: '-A 0:0:0,0:1:0,1:0:1,1:1:1,2:2:0,3:2:1'

The first number defines the SAT>IP source minus one (0-max).
The second number defines the physical input (0-3). The third
number defines the diseqc position (0-31). You may configure
the diseqc more through -q and -d options. The adapter number
is like input number for them.

Back to the example, the input configuration is like this:

- input 0
  - src=1: diseqc position 0 - 23.5E (configuration 0:0:0)
  - src=2: diseqc position 1 - 1W    (configuration 1:0:1)
- input 1
  - src=1: diseqc position 0 - 23.5E (configuration 0:1:0)
  - src=2: diseqc position 1 - 1W    (configuration 1:1:1)
- input 2:
  - src=3: diseqc position 0 - 28.2E (configuration 2:2:0)
  - src=4: diseqc position 1 - 19.2E (configuration 3:2:1)

So, four satellite positions, for 23.5E and 1W, there are
2 shared inputs, for rest, only one input is available.

TVheadend can define network limits and network groups.
In this case, we have 2 groups of inputs (23.5E/1W) and
(28.2E/19.2E) and limits 2 for 23.5E/1W and 1 for rest.

Tvheadend should be configured like this (for all tuners):

- assign S23.5E to position 1, network limit 2, network group 1
- assign 1W     to position 2, network limit 2, network group 1
- assign 28.2E  to position 3, network limit 1, network group 2
- assign 19.2E  to position 4, network limit 1, network group 2

Linked inputs
-------------

### Example1: '-L 0:2,1:3'
### Example2: '-L 0:1,0:2,0:3'

The first number means master tuner and the second slave tuner.
The tuners in same group (master can have multiple slaves) can
use only same combination of source (src=X), band (low/high)
and polarization at a time.


Quattro LNB
------------

The quattro LNB can be configured using -Q option for minisatip. All
diseqc (AA), voltage and tone setup is executed, so you may eventually
connect a multiswitch or quad LNB (it makes sense only with
the -Z option).

The -Z option can eventually reduce the used input to 2 - filtering the
loband (inputs 2,3) or hiband (inputs 0,1) only. For example, 23.5E
satellite positions have useable transponders only in hiband. So, it is
enough to connect coaxial cable only to input 0 and 1 and use other
two inputs for other LNBs (standard way). Note that SAT>IP source number 1
is always handled for the inputs 0 and 1 (to use full 4 tuners for the
loband/hiband inputs).

### Example: '-Q -Z 1'

    box input 0 ---- multiswitch with diseqc AA = 23.5E
    box input 1 ---- multiswitch with diseqc AA = 23.5E
    box input 2 ----+---- diseqc AA = 28.2E (SAT>IP src=2)
                    +---- diseqc AB = 19.2E (SAT>IP src=3)
                    +---- diseqc BA = 16E   (SAT>IP src=4)
                    \---- diseqc BB = 13E   (SAT>IP src=5)
    box input 3 ----+---- diseqc AA = 0.8W  (SAT>IP src=2)
                    \---- diseqc AB = 9E    (SAT>IP src=3)

As you can see, all tuners have src=1 (23.5E), tuner 2 has four
additional diseqc sources and tuner 3 two additional sources.

Note: This requires build 11 or above of the satip-axe firmware.


Unicable/Unicable II
--------------------


### Example 1: '-u 0:1-1420,1:0-1210,2:2-1680,3:3-2040'

0:1-1420

  - 0 = minisatip/box input (0-3)
  - 1 = unicable slot number (refer to your unicable equipment)
  - 1420 = transpoder frequency for the tuner

Note: 0:1-*1420 means that only 13V (voltage) will be used.

Note2: If you don't specify the adapter as unicable, it can be used to
control the standard LNBs/diseqc equipment.


### Multiple unicable groups

You may connect multiple unicable LNBs through multiple coaxial
wires to the box. In this case, it is necessary to tell which tuners
will use which physical input. In this case '-X T1,T2,T3,T4' option
should be used wheren you can define the parent inputs for all four
tuners like: '-X 0,0,2,2' where two unicable groups are connected
to minisatip input 0 and 2.

    box input 0 ---+--- wire 1 (unicable group 1)
    box input 1 ---/

    box input 2 ---+--- wire 2 (unicable group 2)
    box input 3 ---/

Note: This requires build 11 or above of the satip-axe firmware.
