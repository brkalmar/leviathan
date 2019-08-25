# Driver-specific attributes of `kraken_x62`

## Querying the device's serial number

Attribute `serial_no` is an immutable property of the device.
It is an alphanumeric string.
```Shell
$ cat /sys/bus/usb/drivers/kraken_x62/DEVICE/serial_no
0A1B2C3D4E5
```

## Monitoring the liquid temperature

Attribute `temp_liquid` is a read-only integer in °C.
```Shell
$ cat /sys/bus/usb/drivers/kraken_x62/DEVICE/temp_liquid
34
```

## Monitoring the fan

Attribute `fan_rpm` is a read-only integer in RPM.
For maximum expected value see device specifications.
```Shell
$ cat /sys/bus/usb/drivers/kraken_x62/DEVICE/fan_rpm
769
```

## Monitoring the pump

Attribute `pump_rpm` is a read-only integer in RPM.
For maximum expected value see device specifications.
```Shell
$ cat /sys/bus/usb/drivers/kraken_x62/DEVICE/pump_rpm
1741
```

## Setting the fan

Attribute `fan_percent` is a write-only integer in percents.
Must be within 35 – 100 %.

```Shell
$ echo '65' > /sys/bus/usb/drivers/kraken_x62/DEVICE/fan_percent
$ echo '100' > /sys/bus/usb/drivers/kraken_x62/DEVICE/fan_percent
```

## Setting the pump

Attribute `pump_percent` is a write-only integer in percents.
Must be within 50 – 100 %.

```Shell
$ echo '50' > /sys/bus/usb/drivers/kraken_x62/DEVICE/pump_percent
$ echo '78' > /sys/bus/usb/drivers/kraken_x62/DEVICE/pump_percent
```

## Setting LEDs

All LED-attributes are write-only specifications of some of the device's LEDs's behavior.
Each LED-attribute accepts the following ABNF format:
```ABNF
led-attribute = cycles SP preset SP moving SP direction SP interval SP group-size SP color-cycles
cycles        = INTEGER
preset        = "alternating" / "breathing" / "covering_marquee" / "fading" / "fixed" / "load" / "marquee" / "pulse" / "spectrum_wave" / "tai_chi" / "water_cooler"
moving        = BOOLEAN
direction     = "forward" / "backward"
interval      = "slowest" / "slower" / "normal" / "faster" / "fastest"
group-size    = INTEGER
color-cycles  = color-cycle *( SP color-cycle )
```
where
* `SP` is a SPACE character
* `INTEGER` is an integer parsed by `kstrtouint()` with base 0
* `BOOLEAN` is a boolean parsed by `kstrtobool()`
* `color-cycles` is exactly `cycles` repetitions of `color-cycle`
* `color-cycle` depends on the specific attribute

Moreover, each color is of the form:
```ABNF
color = 6HEXDIG
```

Invalid formatting, or invalid combinations of values result in an Invalid argument (EINVAL) error from the write call to the attribute.
The driver will also print a warning to `dmesg` describing the error.

### Logo LED

Attribute `led_logo` takes 1 color for the logo LED per cycle.
```ABNF
color-cycle = color
```

```Shell
$ echo '1 fixed no forward normal 3 000000' > /sys/bus/usb/drivers/kraken_x62/DEVICE/led_logo
$ echo '4 breathing no forward fastest 3 ff0080 4444ff 000000 abcdef' > /sys/bus/usb/drivers/kraken_x62/DEVICE/led_logo
$ echo '7 pulse no forward slower 3 d047a0 d0a047 47d0a0 ffffff 47a0d0 a0d047 a047d0' > /sys/bus/usb/drivers/kraken_x62/DEVICE/led_logo
```

### Ring LEDs

Attribute `leds_ring` takes 8 colors for the ring LEDs (laid out as described in the protocol) per cycle.
```ABNF
color-cycle = color 7( SP color )
```

```Shell
$ echo '1 fixed no forward normal 3 ff0000 ff8000 ffff00 80ff00 00ff00 00ff80 00ffff 0080ff' > /sys/bus/usb/drivers/kraken_x62/DEVICE/leds_ring
$ echo '2 alternating yes forward slowest 3 ff8035 ff8035 ff8035 ff8035 ff8035 ff8035 ff8035 ff8035 202020 202020 202020 202020 202020 202020 202020 202020' > /sys/bus/usb/drivers/kraken_x62/DEVICE/leds_ring
$ echo '1 marquee no forward faster 5 0000ff 00ffff 00ff00 80ff00 ffff00 ff0000 ff00ff 8000ff' > /sys/bus/usb/drivers/kraken_x62/DEVICE/leds_ring
```

### All LEDs synchronized

Attribute `leds_sync` takes 1 color for the logo LED plus 8 colors for the ring LEDs per cycle.
```ABNF
color-cycle = color 8( SP color )
```

```Shell
$ echo '3 covering_marquee no backward normal 3 79c18d 00ffff 00ffff 00ffff 00ffff 00ffff 00ffff 00ffff 00ffff ffffff ff0000 ffff00 ff0000 ffff00 ff0000 ffff00 ff0000 ffff00 646423 ff00ff ff00ff ff00ff ff00ff ff00ff ff00ff ff00ff ff00ff' > /sys/bus/usb/drivers/kraken_x62/DEVICE/leds_sync
$ echo '1 spectrum_wave no backward slower 3 000000 000000 000000 000000 000000 000000 000000 000000 000000' > /sys/bus/usb/drivers/kraken_x62/DEVICE/leds_sync
```
