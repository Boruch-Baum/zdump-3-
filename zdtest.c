 /** zdtest.c                            http://libhdate.sourceforge.net
 *   zdtest - test zdump function
 *
 * compile: (presumes zdump3.h in current directory)
 *     gcc -c -I./ -Wall -Werror -g zdtest.c
 * build: (presumes zdump3 built in current directory)
 *     gcc -I./ -L./ -Wall zdtest.c -o zdtest -lzdump3
 * run: (option 1)
 *     export LD_LIBRARY_PATH=.:$LD_LIBRARY_PATH
 *     ./zdtest zonespec start_time end_time
 * run: (option 2)
 *     env LD_LIBRARY_PATH=.:$LD_LIBRARY_PATH ./zdtest zonespec start_time end_time
 *
 * Copyright:  2012 (c) Boruch Baum <zdump@gmx.com>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <stdio.h>    /// for printf
#include <stdlib.h>   /// for setenv
#include <string.h>   /// for strcat
#include <time.h>     /// for ctime
#include <zdump3.h>   /// for zdump

/// usage ./zdump3 continent/city $(date --date='1970-01-01 00:00:00' +%s) $(date --date='1970-01-01 00:00:00' +%s)

int main (int argc, char *argv[])
{
	int num_entries = 0;
	void* data = NULL;
	char lazy_tz[1000];
	int i;

	zdumpinfo* zd = NULL;
	if (argc !=4)
	{
		printf("\
zdtest: test zdump function\n\
usage: ./zdtest continent/city time_t time_t\n\
 or    ./zdtest continent/city \\\n\
             $(date --date='1970-01-01 00:00:00' +%%s) \\\n\
             $(date --date='1980-01-01 00:00:00' +%%s)\n");
		exit(0);
	}
	zdump(argv[1], atol(argv[2]), atol(argv[3]), &num_entries, &data);
	printf("number of entries found = %d\n", num_entries);
	zd = data;
	lazy_tz[0] = ':';
	strncat(&lazy_tz[1],argv[1],999	);
	setenv("TZ",&lazy_tz[0],1);
	printf("for zone: %s, for time_t %ld to %ld\n",argv[1],atol(argv[2]), atol(argv[3]));
	printf( "num:   time_t      utc_offset  save_secs abbr  - local time (derived) -\n");
	for (i=0; i<num_entries; i++)
	{
		time_t local = zd->start + zd->utc_offset;
		printf( "%2d: %11ld %10d %10d    %-6s %s", i, zd->start, zd->utc_offset, zd->save_secs, zd->abbr, ctime(&local) );
		zd = zd + 1;
	}
	free(data);
	exit(0);
}
