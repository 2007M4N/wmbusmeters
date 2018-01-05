# wmbusmeters
The program receives and decodes C1 telegrams
(using the wireless mbus protocol) to acquire
utility meter readings.

| OS/Compiler        | Status           |
| ------------- |:-------------:|
|Linux G++| [![Build Status](https://travis-ci.org/weetmuts/wmbusmeters.svg?branch=master)](https://travis-ci.org/weetmuts/wmbusmeters.svg?branch=master) |

```
wmbusmeters version: 0.2
Usage: wmbusmeters {options} [usbdevice] { [meter_name] [meter_id] [meter_key] }*

Add more meter triplets to listen to more meters.
Add --verbose for detailed debug information.
    --robot for json output.
    --meterfiles to create status files below tmp,
          named /tmp/meter_name, containing the latest reading.
    --oneshot wait for an update from each meter, then quit.
```

No meter triplets means listen for telegram traffic and print any id heard.

# Builds and runs on GNU/Linux:

```
make
./build/wmbusmeters /dev/ttyUSB0 MyTapWater 12345678 00112233445566778899AABBCCDDEEFF
```

Example output:
`MyTapWater     12345678         6.375 m3       2017-08-31 09:09.08      3.040 m3      DRY(dry 22-31 days)`

`./build/wmbusmeters --verbose /dev/ttyUSB0 MyTapWater 12345678 00112233445566778899AABBCCDDEEFF`

`./build/wmbusmeters --robot /dev/ttyUSB0 MyTapWater 12345678 00112233445566778899AABBCCDDEEFF`

Robot output:
`{"name":"MyTapWater","id":"12345678","total_m3":6.375,"target_m3":3.040,"current_status":"","time_dry":"22-31 days","time_reversed":"","time_leaking":"","time_bursting":"","timestamp":"2017-08-31T09:07:18Z"}`

`make HOST=arm`

Binary generated: `./build_arm/wmbusmeters`

`make DEBUG=true`

Binary generated: `./build_debug/wmbusmeters`

`make DEBUG=true HOST=arm`

Binary generated: `./build_arm_debug/wmbusmeters`

If the meter does not use encryption of its meter data, then enter an empty key on the command line.
(you must enter "")

`./build/wmbusmeters --robot --meterfiles /dev/ttyUSB0 MyTapWater 12345678 ""`

# System configuration

Add yourself to the dialout group to get access to the newly plugged in im871A USB stick.
Or even better, add this udev rule:

Create the file: `/etc/udev/rules.d/99-usb-serial.rules` with the content
```
SUBSYSTEM=="tty", ATTRS{idVendor}=="10c4", ATTRS{idProduct}=="ea60", SYMLINK+="im871a",MODE="0660", GROUP="yourowngroup"
```
This will create a symlink named `/dev/im871a` to the particular USB port that the dongle got assigned.

# Limitations

Currently only supports the USB stick receiver im871A
and the water meter Multical21. The source code is modular
and it should be relatively straightforward to add
more receivers and meters.

# Good documents on the wireless mbus protocol:

http://www.m-bus.com/files/w4b21021.pdf

https://www.infineon.com/dgdl/TDA5340_AN_WMBus_v1.0.pdf

http://fastforward.ag/downloads/docu/FAST_EnergyCam-Protocol-wirelessMBUS.pdf

http://www.multical.hu/WiredMBus-water.pdf

http://uu.diva-portal.org/smash/get/diva2:847898/FULLTEXT02.pdf

The AES source code is copied from:

https://github.com/kokke/tiny-AES128-C

The following other github projects were of great help:

https://github.com/ffcrg/ecpiww

https://github.com/tobiasrask/wmbus-client

https://github.com/CBrunsch/scambus/

TODO: CRC checks are still missing. If the wrong AES key
is supplied you probably get zero readings and
sometimes warnings about wrong type of frames.
