About Kalibrate
===============

Kalibrate is a GSM signal detector and clock frequency offset calculator.
Clock accuracy and frequency offset is an important aspect of GSM and cellular
networks. Out-of-specification clock accuracy causes a number of network issues
such as inconsistent network detection by the handset, handover failure, and
poor data and voice call performance.

Original version of kalibrate (for use with USRP1):
  * http://ttsou.github.com/kalibrate/kal-v0.4.1.tar.bz2

Universal Hardware Driver (UHD):
  * http://uhd.ettus.com

Release Notes

The USRP2/N200/N210 is clocked at 100MHz and does not output fractional sample rates.
The hardware sample rate warning can be safely ignored.

For USRP1, the original, non-UHD version of kalibrate is recommended.

Build
=====

```
$ autoreconf -i
$ ./configure
$ make -j $nproc
$ sudo make install
```

Examples
========

USRP2 with internal reference:

```
$ sudo kal -c 523 -b DCS
[INFO] [UHD] linux; GNU C++ version 5.4.0 20160609; Boost_105800; UHD_3.13.1.HEAD-0-gbbce3e45
[INFO] [USRP2] Opening a USRP2/N-Series device...
[INFO] [USRP2] Current recv frame size: 1472 bytes
[INFO] [USRP2] Current send frame size: 1472 bytes
kal: Calculating clock frequency offset.
Using DCS-1800 channel 523 (1807.4MHz)
average		[min, max]	(range, stddev)
+ 862Hz	[850, 872]	(23, 6.271877)
overruns: 0
not found: 0
average absolute error: 0.477 ppm
```

USRP2 with external 10MHz reference - Agilent E4438C (OCXO):

```
$ ./kal -f 1941.6e6 -x
linux; GNU C++ version 4.4.4 20100630 (Red Hat 4.4.4-10); Boost_104100; UHD_20101116.195923.c5043c6

Current recv sock buff size: 50000000 bytes

Warning:
    The hardware does not support the requested RX sample rate:
    Target sample rate: 0.270833 MSps
    Actual sample rate: 0.271739 MSps

kal: Calculating clock frequency offset.
Using PCS-1900 channel 569 (1941.6MHz)
average         [min, max]      (range, stddev)
+  13Hz         [-32, 86]       (118, 34.811478)
overruns: 0
not found: 0
```

Authors
=======

Kalibrate was originally written by Joshua Lackey <jl@thre.at> in 2010.
Subsequent UHD device support and other changes were added by
Tom Tsou <tom@tsou.cc>.
