/* Internal header file for editline library.
 *
 * Copyright (c) 1992, 1993  Simmule Turner and Rich Salz
 * All rights reserved.
 *
 * This software is not subject to any license of the American Telephone
 * and Telegraph Company or of the Regents of the University of California.
 *
 * Permission is granted to anyone to use this software for any purpose on
 * any computer system, and to alter it and redistribute it freely, subject
 * to the following restrictions:
 * 1. The authors are not responsible for the consequences of use of this
 *    software, no matter how awful, even if they arise from flaws in it.
 * 2. The origin of this software must not be misrepresented, either by
 *    explicit claim or by omission.  Since few users ever read sources,
 *    credits must appear in the documentation.
 * 3. Altered versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.  Since few users
 *    ever read sources, credits must appear in the documentation.
 * 4. This notice may not be removed or altered.
 */

#ifndef EDITLINE_PRIVATE_H_
#define EDITLINE_PRIVATE_H_

#include "config.h"
#include <stdio.h>
#if HAVE_MALLOC_H
#include <malloc.h>
#endif
#if HAVE_STDLIB_H
#include <stdlib.h>
#endif
#if HAVE_STRING_H
#include <string.h>
#endif
#if HAVE_DIRENT_H
#include <dirent.h>
#endif
#if HAVE_SIGNAL_H
#include <signal.h>
#endif
#if SYS_UNIX
#include "unix.h"
#endif
#if SYS_OS9
#include "os9.h"
#endif
/* The following two are for TIOCGWINSZ */
#if HAVE_TERMIOS_H
# include <termios.h>
#endif
#if GWINSZ_IN_SYS_IOCTL
# include <sys/ioctl.h>
#endif

#define MEM_INC         64
#define SCREEN_INC      256

/* From The Practice of Programming, by Kernighan and Pike */
#ifndef NELEMS
#define NELEMS(array) (sizeof(array) / sizeof(array[0]))
#endif

/*
**  Variables and routines internal to this package.
*/
extern int	rl_eof;
extern int	rl_erase;
extern int	rl_intr;
extern int	rl_kill;
extern int	rl_quit;
#if CONFIG_SIGSTOP
extern int	rl_susp;
#endif
void  rl_ttyset(int Reset);
void  rl_add_slash(char *path, char *p);
char *rl_complete(char *token, int *match);
int   rl_list_possib(char *token, char ***av);

#if !defined(HAVE_STDLIB_H) || (HAVE_STDLIB_H == 0)
extern char	*getenv(const char *name);
extern char	*malloc(size_t size);
extern char	*realloc(void *ptr, size_t size);
extern char	*memcpy(void *dest, const void *src, size_t n);
extern char	*strcat(char *dest, const char *src);
extern char	*strchr(const char *s, int c);
extern char	*strrchr(const char *s, int c);
extern char	*strcpy(char *dest, const char *src);
extern char	*strdup(const char *s);
extern int	strcmp(const char *s1, const char *s2);
extern int	strlen(const char *s);
extern int	strncmp(const char *s1, const char *s2, size_t n);
#endif/* !HAVE_STDLIB_H */

#if !defined(HAVE_STRDUP) || (HAVE_STRDUP == 0)
extern char	*strdup(const char *s);
#endif

#include "editline.h"
#endif  /* EDITLINE_PRIVATE_H_ */
