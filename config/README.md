This directory contains all the needed configurations files needed for ModemBank to run.

"modems.csv" contains a comma seperated list of modems to initialize.

Format is as follows:
```
/dev/tty0,300
/dev/tty1,2400
...
/dev/tty43,19200
```
Where the first entry points to a valid TTY and the second valid baud rate.
