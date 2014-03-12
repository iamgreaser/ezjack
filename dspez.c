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
#include <stdarg.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <errno.h>

#include <jack/jack.h>

#include <ezjack.h>

#include <sys/syscall.h>
#include <sys/soundcard.h>
#include <unistd.h>
#include <fcntl.h>

#define DUPLEX_READ 1
#define DUPLEX_WRITE 2

int fakefd = -1;
int fakefd2 = -1;
int fddir = 0;
volatile int fdcount = 0; // TODO: proper thread safety
ezjack_format_t sfmt = EZJackFormatU8;
ezjack_bundle_t *sbun = NULL;
int sneedsupdate = 1;
int schns = 1;
int sfreq = 8000;
int sbufsz = 2048; // TODO: allow setting of this

static int convmode(int mode)
{
	switch(mode)
	{
		//case O_RDONLY: return DUPLEX_READ;
		//case O_WRONLY: return DUPLEX_WRITE;
		default:
			return DUPLEX_READ | DUPLEX_WRITE;
	}
}

int open(const char *path, int flags, ...)
{
	va_list ap;
	va_start(ap, flags);
	int mode = va_arg(ap, int);
	va_end(ap);
	
	// TODO: handle this properly
	//printf("path \"%s\"\n", path);
	if(strstr(path, "/dev/dsp") == path)
	{
		if(fakefd != -1)
		{
			// XXX: we just throw the fd back out there
			fddir |= convmode(flags & O_ACCMODE);
			fdcount++;
			sneedsupdate = 1;
			return fakefd;
			// TODO: scream when someone uses O_NONBLOCK (for now - and then support it later)
		} else {
			fddir = convmode(flags & O_ACCMODE);
			fdcount++;
			// TODO: scream when someone uses O_NONBLOCK (for now - and then support it later)

			int fds[2];
			int ret = pipe(fds);

			if(ret == 0)
			{
				fakefd2 = fds[0];
				fakefd = ret = fds[1];
				sneedsupdate = 1;
			}

			return ret;
		}
	} else {
		return syscall(SYS_open, path, flags, mode);
	}
}

int close(int fd)
{
	if(fakefd != -1 && fd == fakefd)
	{
		fdcount--;
		if(fdcount == 0)
		{
			int r1 = syscall(SYS_close, fakefd2);
			int r2 = syscall(SYS_close, fakefd);

			fakefd2 = -1;
			fakefd = -1;

			if(sbun != NULL)
			{
				ezjack_close(sbun);
				sbun = NULL;
			}
		}

		return 0;
	} else {
		return syscall(SYS_close, fd);
	}
}

int ioctl(int fd, unsigned long request, ...)
{
	va_list ap;
	va_start(ap, request);
	char *argp = va_arg(ap, char *);
	va_end(ap);

	if(fakefd != -1 && fd == fakefd)
	{
		switch(request)
		{
			case SNDCTL_DSP_GETFMTS:
				*(int *)argp = AFMT_U8 | AFMT_S8 | AFMT_S16_NE | AFMT_U16_NE;
				return 0;

			case SNDCTL_DSP_SETFMT:
				switch(*(int *)argp)
				{
					case AFMT_U8:
						sfmt = EZJackFormatU8;
						return 0;
					case AFMT_S8:
						sfmt = EZJackFormatS8;
						return 0;
					case AFMT_S16_NE:
						sfmt = EZJackFormatS16Native;
						return 0;
					case AFMT_U16_NE:
						sfmt = EZJackFormatU16Native;
						return 0;
				}

				errno = EINVAL;
				return -1;

			case SNDCTL_DSP_CHANNELS:
				if(*(int *)argp < 1 || *(int *)argp > EZJACK_PORTSTACK_MAX)
				{
					errno = EINVAL;
					return -1;
				}

				schns = *(int *)argp;
				sneedsupdate = 1;
				return 0;

			case SNDCTL_DSP_SPEED:
				if(*(int *)argp < 1)
				{
					errno = EINVAL;
					return -1;
				}

				sfreq = (float)*(int *)argp;
				sneedsupdate = 1;
				return 0;

			case SNDCTL_DSP_SETFRAGMENT:
				// TODO: actually heed this hint
				return 0;

			case SNDCTL_DSP_GETISPACE:
				// TODO: create an API for this
				if((fddir & DUPLEX_READ) == 0)
				{
					errno = EINVAL;
					return -1;
				}

				((audio_buf_info *)argp)->bytes = jack_ringbuffer_read_space(sbun->portstack.inrb[0]);
				((audio_buf_info *)argp)->fragments = 0;
				((audio_buf_info *)argp)->fragstotal = 1;
				((audio_buf_info *)argp)->fragsize = sbufsz;
				return 0;

			case SNDCTL_DSP_GETOSPACE:
				// TODO: create an API for this
				if((fddir & DUPLEX_WRITE) == 0)
				{
					errno = EINVAL;
					return -1;
				}

				((audio_buf_info *)argp)->bytes = jack_ringbuffer_write_space(sbun->portstack.outrb[0]);
				((audio_buf_info *)argp)->fragments = 0;
				((audio_buf_info *)argp)->fragstotal = 1;
				((audio_buf_info *)argp)->fragsize = sbufsz;
				return 0;

			case SNDCTL_DSP_SETDUPLEX:
				// deprecated, does nothing
				return 0;

			default:
				errno = EINVAL;
				return -1;
		}
	} else {
		return syscall(SYS_ioctl, fd, request, argp);
	}
}

static int dsp_try_reopen(void)
{
	if(sneedsupdate)
	{
		if(sbun != NULL)
			ezjack_close(sbun);

		printf("opening again %i %i %i %i\n", schns, fddir, sbufsz, sfreq);
		sbun = ezjack_open("devdsp_emu",
			(((fddir & DUPLEX_READ) != 0) ? schns : 0),
			(((fddir & DUPLEX_WRITE) != 0) ? schns : 0),
			sbufsz, (float)sfreq, 0);
		ezjack_activate(sbun);
		ezjack_autoconnect(sbun);

		sneedsupdate = 0;
	}

	return 0;
}

ssize_t write(int fd, const void *buf, size_t nbytes)
{
	if(fakefd != -1 && fd == fakefd)
	{
		if((fddir & DUPLEX_WRITE) == 0)
		{
			errno = EBADF;
			return -1;
		}

		dsp_try_reopen();

		return ezjack_write(sbun, buf, (int)nbytes, sfmt);
	} else {
		return syscall(SYS_write, fd, buf, nbytes);
	}
}


ssize_t read(int fd, void *buf, size_t nbytes)
{
	if(fakefd != -1 && fd == fakefd)
	{
		if((fddir & DUPLEX_READ) == 0)
		{
			errno = EBADF;
			return -1;
		}

		dsp_try_reopen();

		return ezjack_read(sbun, buf, (int)nbytes, sfmt);
	} else {
		return syscall(SYS_read, fd, buf, nbytes);
	}
}

