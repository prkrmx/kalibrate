/*
 * Copyright (c) 2010, Joshua Lackey
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 
 *     *  Redistributions of source code must retain the above copyright
 *        notice, this list of conditions and the following disclaimer.
 *
 *     *  Redistributions in binary form must reproduce the above copyright
 *        notice, this list of conditions and the following disclaimer in the
 *        documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * kal
 *
 *    Two functions:
 *
 * 	1.  Calculates the frequency offset between a local GSM tower and the
 * 	    USRP clock.
 *
 *	2.  Identifies the frequency of all GSM base stations in a given band.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#else
#define PACKAGE_VERSION "custom build"
#endif /* HAVE_CONFIG_H */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#ifdef D_HOST_OSX
#include <libgen.h>
#endif /* D_HOST_OSX */
#include <string.h>
#include <sys/time.h>
#include <errno.h>

#include "usrp_source.h"
#include "fcch_detector.h"
#include "arfcn_freq.h"
#include "offset.h"
#include "c0_detect.h"
#include "version.h"

static const double GSM_RATE = 1625000.0 / 6.0;


int g_verbosity = 0;
int g_debug = 0;

void usage(char *prog) {

	printf("kalibrate v%s, Copyright (c) 2010, Joshua Lackey\n", kal_version_string);
	printf("\nUsage:\n");
	printf("\tGSM Base Station Scan:\n");
	printf("\t\t%s <-s band indicator> [options]\n", basename(prog));
	printf("\n");
	printf("\tClock Offset Calculation:\n");
	printf("\t\t%s <-f frequency | -c channel> [options]\n", basename(prog));
	printf("\n");
	printf("Where options are:\n");
	printf("\t-s\tband to scan (GSM850, GSM900, EGSM900, DCS1800, PCS1900)\n");
	printf("\t-f\tfrequency of nearby GSM base station\n");
	printf("\t-c\tchannel of nearby GSM base station\n");
	printf("\t-b\tband indicator (GSM850, GSM900, EGSM900, DCS1800, PCS1900)\n");
	printf("\t-R\tRX subdev spec\n");
	printf("\t-a\tUHD device address args(name=RX_2)\n");
	printf("\t-A\tantenna TX/RX (0) or RX2 (1), defaults to RX2\n");
	printf("\t-g\tgain as %% of range, defaults to 45%%\n");
	printf("\t-F\tFPGA master clock frequency, defaults to device default\n");
	printf("\t-x\tenable external 10MHz reference input\n");
	printf("\t-v\tverbose\n");
	printf("\t-D\tenable debug messages\n");
	printf("\t-h\thelp\n");
	exit(-1);
}


int main(int argc, char **argv) {

	char *endptr;
	int c, antenna = 1, bi = BI_NOT_DEFINED, chan = -1, bts_scan = 0;
	char *subdev = NULL;
	char *args = NULL;
	double fpga_master_clock_freq = 52e6;
	bool external_ref = false;
	float gain = 0.45;
	double freq = -1.0, fd;
	usrp_source *u;

	while((c = getopt(argc, argv, "f:c:s:b:R:a:A:g:F:xvDh?")) != EOF) {
		switch(c) {
			case 'f':
				freq = strtod(optarg, 0);
				break;

			case 'c':
				chan = strtoul(optarg, 0, 0);
				break;

			case 's':
				if((bi = str_to_bi(optarg)) == -1) {
					fprintf(stderr, "error: bad band "
					   "indicator: ``%s''\n", optarg);
					usage(argv[0]);
				}
				bts_scan = 1;
				break;

			case 'b':
				if((bi = str_to_bi(optarg)) == -1) {
					fprintf(stderr, "error: bad band "
					   "indicator: ``%s''\n", optarg);
					usage(argv[0]);
				}
				break;

			case 'R':
				errno = 0;
				subdev = optarg;
				break;

			case 'a':
				errno = 0;
				args = optarg;
				break;

			case 'A':
				if(!strcmp(optarg, "RX2")) {
					antenna = 1;
				} else if(!strcmp(optarg, "TX/RX")) {
					antenna = 0;
				} else {
					errno = 0;
					antenna = strtoul(optarg, &endptr, 0);
					if(errno || (endptr == optarg)) {
						fprintf(stderr, "error: bad "
						   "antenna: ``%s''\n",
						   optarg);
						usage(argv[0]);
					}
				}
				break;

			case 'g':
				gain = strtod(optarg, 0);
				if((gain > 1.0) && (gain <= 100.0))
					gain /= 100.0;
				if((gain < 0.0) || (1.0 < gain))
					usage(argv[0]);
				break;

			case 'F':
				fpga_master_clock_freq = atof(optarg);
				break;

			case 'x':
				external_ref = true;
				break;

			case 'v':
				g_verbosity++;
				break;

			case 'D':
				g_debug = 1;
				break;

			case 'h':
			case '?':
			default:
				usage(argv[0]);
				break;
		}

	}

	// sanity check frequency / channel
	if(bts_scan) {
		if(bi == BI_NOT_DEFINED) {
			fprintf(stderr, "error: scaning requires band\n");
			usage(argv[0]);
		}
	} else {
		if(freq < 0.0) {
			if(chan < 0) {
				fprintf(stderr, "error: must enter channel or "
				   "frequency\n");
				usage(argv[0]);
			}
			if((freq = arfcn_to_freq(chan, &bi)) < 869e6)
				usage(argv[0]);
		}
		if((freq < 869e6) || (2e9 < freq)) {
			fprintf(stderr, "error: bad frequency: %lf\n", freq);
			usage(argv[0]);
		}
		chan = freq_to_arfcn(freq, &bi);
	}

	if(g_debug) {
#ifdef D_HOST_OSX
		printf("debug: Mac OS X version\n");
#endif
		printf("debug: FPGA Master Clock Freq:\t%f\n", fpga_master_clock_freq);
		printf("debug: External Reference    :\t%s\n", external_ref? "Yes" : "No");
		printf("debug: RX Subdev Spec        :\t%s\n", subdev? subdev : "");
		printf("debug: UHD device        	 :\t%s\n", args? args : "");
		printf("debug: Antenna               :\t%s\n", antenna? "RX2" : "TX/RX");
		printf("debug: Gain                  :\t%f\n", gain);
	}

	// let the device decide on the decimation
	u = new usrp_source(GSM_RATE, fpga_master_clock_freq, external_ref);
	if(!u) {
		fprintf(stderr, "error: usrp_source\n");
		return -1;
	}
	if(u->open(subdev, args) == -1) {
		fprintf(stderr, "error: usrp_source::open\n");
		return -1;
	}
	u->set_antenna(antenna);
	if(!u->set_gain(gain)) {
		fprintf(stderr, "error: usrp_source::set_gain\n");
		return -1;
	}

	if(!bts_scan) {
		if(!u->tune(freq)) {
			fprintf(stderr, "error: usrp_source::tune\n");
			return -1;
		}

		fprintf(stderr, "%s: Calculating clock frequency offset.\n",
		   basename(argv[0]));
		fprintf(stderr, "Using %s channel %d (%.1fMHz)\n",
		   bi_to_str(bi), chan, freq / 1e6);

		return offset_detect(u, freq);
	}

	fprintf(stderr, "%s: Scanning for %s base stations.\n",
	   basename(argv[0]), bi_to_str(bi));

	return c0_detect(u, bi);
}
