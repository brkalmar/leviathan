# leviathan

Linux device drivers that support controlling and monitoring NZXT Kraken water coolers

NZXT is **NOT** involved in this project, do **NOT** contact them if your device is damaged while using this software.

Also, while it doesn't seem like the hardware could be damaged by silly USB messages (apart from overheating), I do **NOT** take any responsibility for any damage done to your cooler.

# Supported devices

* Driver `kraken` (for Vendor/Product ID `2433:b200`)
  * NZXT Kraken X61 
  * NZXT Kraken X41
  * NZXT Kraken X31 (Only for controlling the fan/pump speed, since there's no controllable LED on the device)
* Driver `kraken_x62` (for Vendor/Product ID `1e71:170e`)
  * NZXT Kraken X72 *(?)*
  * NZXT Kraken X62
  * NZXT Kraken X52 *(?)*
  * NZXT Kraken X42
  * NZXT Kraken M22 *(?)*

A *(?)* indicates that the device should be compatible based on the product specifications, but has not yet been tested with the driver and is therefore currently unsupported.

If you have an unsupported liquid cooler — whether it is present in the above list or not — and want to help out, see [CONTRIBUTING.md](CONTRIBUTING.md).

# Usage

The drivers can be controlled directly by the end-user, but only provide the most basic functionality.
Features like a friendly user interface, dynamic updates, etc. are left to frontends.

Each driver can be controlled with device files under `/sys/bus/usb/drivers/$DRIVER`, where `$DRIVER` is the driver name.
Each attribute `$ATTRIBUTE` for a device `$DEVICE` is exposed to the user through the file `/sys/bus/usb/drivers/$DRIVER/$DEVICE/$ATTRIBUTE`.

Find the symbolic links that point to the connected compatible devices.
In my case, there's only one Kraken connected.
```Shell
/sys/bus/usb/drivers/kraken/2-1:1.0 -> ../../../../devices/pci0000:00/0000:00:06.0/usb2/2-1/2-1:1.0
```

## Common attributes

### Update interval

Attribute `update_interval` is the number of milliseconds elapsed between successive USB updates.
Every update, the driver requests a status update and sends the changed values for any attributes which have been written to since the last update.

This is mainly useful for debugging; you probably don't need to change it from the default value.
The minimum interval is 500 ms — anything smaller is silently changed to 500.
A special value of 0 indicates that no USB updates are sent.
```Shell
$ cat /sys/bus/usb/drivers/$DRIVER/$DEVICE/update_interval
1000
$ echo $INTERVAL > /sys/bus/usb/drivers/$DRIVER/$DEVICE/update_interval
```

Module parameter `update_interval` can also be used to set the update interval at module load time.
Like the attribute, it is in milliseconds, and anything below the minimum of 500 is changed to 500.
A special value of 0 indicates that the USB update cycle is not to be started and no updates are to be sent.
```Shell
$ sudo insmod $DRIVER update_interval=$INTERVAL
```

### Syncing to the updates

Attribute `update_sync` is a special read-only attribute.
When read, it blocks the read until the next update is finished (or the waiting task has been interrupted).
Its purpose is to allow userspace programs to sync their actions to directly after the driver updates; it's not useful for users handling the driver attributes directly.
A frontend program may run in the following loop, synchronizing with the driver and thereby avoiding too frequent/infrequent reads/writes:
1. read `update_sync`,
2. read any attributes it needs,
3. write to any attributes it needs (based on the up-to-date info).

The attribute's value is `1` if the next update has finished, `0` if the waiting task has been interrupted.
```Shell
$ time -p cat /sys/bus/usb/drivers/$DRIVER/$DEVICE/update_sync
1
real 0.77
user 0.00
sys 0.00
```

## Driver-specific attributes

See the files in [doc/drivers/](doc/drivers/).

# Installation

The easiest method is using `dkms`, as it will make sure to rebuild and reinstall the modules on kernel upgrades.
You can also build and install the modules manually, but this will not persist across kernel upgrades.

## Using `dkms`

First, make sure you have `dkms` and the headers for the kernel installed.

The following command copies the source tree into `/usr/src`, builds the modules under `/var/lib/dkms`, and installs them under `/lib/modules/$VER-$ARCH`:
```Shell
sudo dkms install .
```
where `$VER-$ARCH` is the version of the currently running kernel, e.g. `4.16.0-2-amd64`.
You can also install to other kernel versions like so:
```Shell
sudo dkms install . -k $VER/$ARCH
```
If all is successful, the drivers should be loaded and load on boot.

To uninstall and remove the modules installed via `dkms`:
```Shell
sudo modprobe -r kraken kraken_x62
sudo dkms remove leviathan/0.1.0 --all
```

See the `dkms` documentation for more info.

## Manually

First, make sure the headers for the kernel are installed.

To build the drivers for the currently running kernel:
```Shell
make
```
Or to build for a specific kernel version:
```Shell
make KERNELRELEASE=$VER-$ARCH
```

To install a driver temporarily (until the next reboot):
```Shell
sudo insmod $DRIVER.ko
```
where `$DRIVER` is the name of the driver, either `kraken` or `kraken_x62`.

To install a driver permanently across reboots:
```Shell
sudo cp $DRIVER.ko /lib/modules/$VER-$ARCH/kernel/drivers/usb/misc && sudo depmod && sudo modprobe $DRIVER
```
After this, the driver should automatically load on boot.

# Troubleshooting

**If none of the following steps fixes the issue, consider reporting it as a bug.**

First uninstall the modules as described above if you have installed them via `dkms`
Then try installing the desired driver module manually using `insmod`.
Confirm that it was successful by running `lsmod` and checking that the driver (either `kraken` or `kraken_x62`) is listed.

Now run
```Shell
sudo dmesg
```
Near the bottom you should see `usbcore: registered new interface driver $DRIVER` or a similar message.
If your cooler is connected, then directly above this line you should see `$DRIVER 1-7:1.0: Kraken connected` or similar.
If you see both messages, there should be a directory for your cooler device's attributes in `/sys/bus/usb/drivers/$DRIVER`, e.g. `/sys/bus/usb/drivers/$DRIVER/1-7:1.0`.

If you don't see any such messages in `dmesg` then something went wrong within the driver.
If you see a long, scary error message from the kernel (stacktrace, registry dump, etc.), the driver crashed and your kernel is in an invalid state; you should restart your computer before doing anything else (also consider doing any further testing of the driver in a virtual machine so you won't have to restart after each crash).

If you see the `registered new interface driver` message but not the `Kraken connected` message, it is probably one of three possibilities:
* The cooler is not connected properly to the motherboard: check if it's connected properly / try reconnecting it.
* The cooler is not supported by the driver: see [CONTRIBUTING.md](CONTRIBUTING.md).
* Another kernel USB module is already connected to the cooler, so the driver cannot connect to it: see the following section.

## Another module already connected

A common issue with `kraken_x62` is the device not connecting after installation, usually caused by module `usbhid` being already connected to the cooler.
To fix it, create a file `/etc/modprobe.d/usbhid-kraken-ignore.conf` with the contents
```
# 1e71:170e is the NZXT Kraken X*2 coolers
# 0x4 is HID_QUIRK_IGNORE
options usbhid quirks=0x1e71:0x170e:0x4
```
Then reload the `usbhid` module
```Shell
sudo modprobe -r usbhid && sudo modprobe usbhid
```
(You should be careful unloading USB modules as it may make your USB devices (keyboard etc.) unresponsive; therefore it's best to type all the commands out on a single line before executing them, like above.)
Also update initramfs to keep the configuration across reboots
```Shell
sudo update-initramfs -u
```

Finally reload the driver with
```Shell
sudo rmmod kraken_x62; sudo insmod kraken_x62.ko
```
or
```Shell
sudo modprobe -r kraken_x62; sudo modprobe kraken_x62
```
depending on how it was installed.
