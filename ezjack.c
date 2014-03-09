/*
EZJACK: a simple wrapper for JACK to make stuff a bit easier
Copyright (c) Ben "GreaseMonkey" Russell, 2014.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <errno.h>

#include <jack/jack.h>

#include "ezjack.h"

static volatile jack_status_t lasterr = 0;

jack_status_t ezjack_get_error(void)
{
	// FIXME: possibly not thread-safe!
	jack_status_t ret = lasterr;
	lasterr = 0;
	return ret;
}

ezjack_bundle_t *ezjack_open(const char *client_name, int inputs, int outputs, int bufsize, float freq, ezjack_portflags_t flags)
{
	int i;
	ezjack_bundle_t bun;
	char namebuf[16];

	// Open client
	jack_status_t temperr = lasterr;
	bun.client = jack_client_open(client_name, JackNoStartServer, &temperr);
	lasterr = temperr;

	if(bun.client == NULL)
		return NULL;
	
	bun.freq = freq;
	
	// Create some ports
	bun.portstack.incount = 0;
	bun.portstack.outcount = 0;

#define HELPER_OPEN_PORTS(foo, fooputs, foocount, foofmt, flags) \
	for(i = 0; i < fooputs; i++) \
	{ \
		snprintf(namebuf, 16, foofmt, i+1); \
		bun.portstack.foo[i] = jack_port_register(bun.client, namebuf, JACK_DEFAULT_AUDIO_TYPE, flags, bufsize); \
		if(bun.portstack.foo[i] == NULL) \
		{ \
			lasterr = JackFailure; \
			jack_client_close(bun.client); \
			return NULL; \
		} \
 \
		bun.portstack.foocount++; \
	}

	HELPER_OPEN_PORTS(in, inputs, incount, "in_%i", JackPortIsInput);
	HELPER_OPEN_PORTS(out, outputs, outcount, "out_%i", JackPortIsOutput);

#undef HELPER_OPEN_PORTS

	// Return our bundle
	ezjack_bundle_t *ret = malloc(sizeof(ezjack_bundle_t));
	memcpy(ret, &bun, sizeof(ezjack_bundle_t));

	return ret;
}

int ezjack_autoconnect(ezjack_bundle_t *bun)
{
	int i;

	// Find ports
	// If we can't find any physical ports, don't worry.
	// If a connection fails, don't worry either.
#define HELPER_FIND_PORTS(foo, foocount, foonames, foopattern, fooflags, footo, foofrom) \
	if(bun->portstack.foocount > 0) \
	{ \
		const char **foonames = jack_get_ports(bun->client, foopattern, JACK_DEFAULT_AUDIO_TYPE, fooflags|JackPortIsPhysical); \
		if(foonames != NULL) \
		{ \
			i = 0; \
			while(foonames[i] != NULL) \
			{ \
				jack_connect(bun->client, foofrom, footo); \
				i++; \
			} \
		} \
	} \

	HELPER_FIND_PORTS(in, incount, innames, ".*:capture_.*", JackPortIsOutput, jack_port_name(bun->portstack.in[i % bun->portstack.incount]), innames[i]);
	HELPER_FIND_PORTS(out, outcount, outnames, ".*:playback_.*", JackPortIsInput, outnames[i], jack_port_name(bun->portstack.out[i % bun->portstack.outcount]));

#undef HELPER_FIND_PORTS

	return 0;
}

void ezjack_close(ezjack_bundle_t *bun)
{
	jack_client_close(bun->client);
	free(bun);
}

int ezjack_activate(ezjack_bundle_t *bun)
{
	return jack_activate(bun->client);
}

int ezjack_deactivate(ezjack_bundle_t *bun)
{
	return jack_deactivate(bun->client);
}

