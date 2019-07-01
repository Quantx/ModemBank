This directory contains all the needed configurations files needed for ModemBank to run.

The `modems.csv` file contains a comma seperated list of modems to initialize.

Format is as follows:
```
/dev/ttyS0,300
/dev/ttyS1,2400,AT&F1&R2%7=60
...
/dev/ttyS10,19200
```
Where the first entry points to a valid TTY and the second valid baud rate.
Everything in the optional third entry (starting after the second comma) is sent to the modem as an initialization string.
The contents of this string are treated literally and is automatically appended by a carrige return character.
The master reset command `ATZ` is sent to the modem prior to the initialization.
