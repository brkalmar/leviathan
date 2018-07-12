# Driver-specific attributes of `kraken_x62`

## Querying the device's serial number

Attribute `serial_no` is an immutable property of the device.
It is an alphanumeric string.
```Shell
$ cat /sys/bus/usb/drivers/kraken_x62/$DEVICE/serial_no
0A1B2C3D4E5
```

## Monitoring the liquid temperature

Attribute `temp_liquid` is a read-only integer in °C.
```Shell
$ cat /sys/bus/usb/drivers/kraken_x62/$DEVICE/temp_liquid
34
```

## Monitoring the fan

Attribute `fan_rpm` is a read-only integer in RPM.
For maximum expected value see device specifications.
```Shell
$ cat /sys/bus/usb/drivers/kraken_x62/$DEVICE/fan_rpm
769
```

## Monitoring the pump

Attribute `pump_rpm` is a read-only integer in RPM.
For maximum expected value see device specifications.
```Shell
$ cat /sys/bus/usb/drivers/kraken_x62/$DEVICE/pump_rpm
1741
```

## Setting the fan

Attribute `fan_percent` is a write-only integer in percents.
Must be within 35 – 100 %.

```Shell
$ echo '65' > /sys/bus/usb/drivers/kraken_x62/$DEVICE/fan_percent
$ echo '100' > /sys/bus/usb/drivers/kraken_x62/$DEVICE/fan_percent
```

## Setting the pump

Attribute `pump_percent` is a write-only integer in percents.
Must be within 50 – 100 %.

```Shell
$ echo '50' > /sys/bus/usb/drivers/kraken_x62/$DEVICE/pump_percent
$ echo '78' > /sys/bus/usb/drivers/kraken_x62/$DEVICE/pump_percent
```

## Setting LEDs

All LED-attributes are write-only specifications of some of the device's LEDs's behavior.
They accept the following format:
```ABNF
led-attribute = "off" / cycles

cycles = nr-cycles 1*WSP preset 1*WSP moving 1*WSP direction 1*WSP interval 1*WSP group-size 1*WSP COLORS

nr-cycles = INTEGER
preset = "alternating" / "breathing" / "covering_marquee" / "fading" / "fixed" / "load" / "marquee" / "pulse" / "spectrum_wave" / "tai_chi" / "water_cooler"
moving = "*" / BOOLEAN
direction = "*" / "forward" / "backward" / "*"
interval = "*" / "slowest" / "slower" / "normal" / "faster" / "fastest"
group-size = "*" / INTEGER
```
where
* `INTEGER` is an integer parsed by `kstrtouint()` with base 0
* `BOOLEAN` is a boolean parsed by `kstrtobool()`
* `COLORS` is `nr-cycles` repetitions of `colors-cycle`
* the definition of `colors-cycle` depends on the specific attribute

`off` turns the LEDs off entirely.
`*` directs the driver to use the default for that value.

Moreover, each color is of the form:
```ABNF
color = 6HEXDIG
```

### Logo LED

Attribute `led_logo` takes 1 color for the logo LED per cycle.
```ABNF
colors-cycle = color
```

```Shell
$ echo 'off' > /sys/bus/usb/drivers/kraken_x62/$DEVICE/led_logo
$ echo '4 breathing * * * * ff0080 4444ff 000000 abcdef' > /sys/bus/usb/drivers/kraken_x62/$DEVICE/led_logo
$ echo '7 pulse * * slower * d047a0 d0a047 47d0a0 ffffff 47a0d0 a0d047 a047d0' > /sys/bus/usb/drivers/kraken_x62/$DEVICE/led_logo
```

### Ring LEDs

Attribute `leds_ring` takes 8 colors for the ring LEDs (laid out as described in the protocol) per cycle.
```ABNF
colors-cycle = 8color
```

```Shell
$ echo '1 fixed * * * * ff0000 ff8000 ffff00 80ff00 00ff00 00ff80 00ffff 0080ff' > /sys/bus/usb/drivers/kraken_x62/$DEVICE/leds_ring
$ echo '2 alternating yes * slowest * ff8035 ff8035 ff8035 ff8035 ff8035 ff8035 ff8035 ff8035 202020 202020 202020 202020 202020 202020 202020 202020' > /sys/bus/usb/drivers/kraken_x62/$DEVICE/leds_ring
$ echo '1 marquee * forward faster 5 0000ff 00ffff 00ff00 80ff00 ffff00 ff0000 ff00ff 8000ff' > /sys/bus/usb/drivers/kraken_x62/$DEVICE/leds_ring
```

### All LEDs synchronized

Attribute `leds_sync` takes 1 color for the logo LED plus 8 colors for the ring LEDs per cycle.
```ABNF
colors-cycle = color 8color
```

```Shell
$ echo '3 covering_marquee * backward normal * 79c18d 00ffff 00ffff 00ffff 00ffff 00ffff 00ffff 00ffff 00ffff ffffff ff0000 ffff00 ff0000 ffff00 ff0000 ffff00 ff0000 ffff00 646423 ff00ff ff00ff ff00ff ff00ff ff00ff ff00ff ff00ff ff00ff' > /sys/bus/usb/drivers/kraken_x62/$DEVICE/leds_sync
$ echo '1 spectrum_wave * backward slower * 000000 000000 000000 000000 000000 000000 000000 000000 000000' > /sys/bus/usb/drivers/kraken_x62/$DEVICE/leds_sync
```
