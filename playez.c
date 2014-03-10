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

#include <ezjack.h>

#include <unistd.h>
#include <fcntl.h>

#define USE_CALLBACK

static int infp = -1;

#ifdef USE_CALLBACK
static volatile int killme = 0;
static int16_t cb_buf[1024*2];

int playez_callback(int nframes_in, int nframes_out, ezjack_bundle_t *bun)
{
	while(nframes_out > 0)
	{
		int len = (int)nframes_out;
		if(len > 1024)
			len = 1024;

		// Note, not realtime so THIS IS A BAD IDEA
		if(read(infp, (void *)cb_buf, len*4) == 0)
			killme = 1;

		ezjack_write(bun, cb_buf, len*4, EZJackFormatS16LE);

		nframes_out -= len;
	}

	return 0;
}
#endif

int main(int argc, char *argv[])
{
	infp = STDIN_FILENO;
	
	if(argc > 1)
		infp = open(argv[1], O_RDONLY);

	ezjack_bundle_t *bun = ezjack_open("ezjack_test", 0, 2, 4096, 44100.0f, 0);
	printf("returned %p (%i)\n", bun, ezjack_get_error());

	if(bun != NULL)
	{
		ezjack_activate(bun);
		printf("autoconnect returned %i\n", ezjack_autoconnect(bun));

#ifdef USE_CALLBACK
		printf("using callback API\n");
		ezjack_set_callback(playez_callback);
		while(!killme) sleep(1);
#else
		printf("streaming directly\n");
		for(;;)
		{
			char buf[1024];
			if(read(infp, buf, 1024) == 0)
				break;
			ezjack_write(bun, buf, 1024, EZJackFormatS16LE);
		}

		sleep(1);
#endif

		ezjack_close(bun);
	}

	return 0;
}

