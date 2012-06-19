/** tzif-display.c                 http://libhdate.sourceforge.net
 * display contents of tzif file   (in support of hcal and hdate)
 * hcal.c  Hebrew calendar              (part of package libhdate)
 * hdate.c Hebrew date/times information(part of package libhdate)
 *
 * compile:
 *     gcc -c -Wall -Werror -g tzif-display.c
 * build:
 *     gcc -Wall tzif-display.c -o tzif-display
 * run:
 *     ./tzif-display full-pathname-to-tzif-file
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
#define _GNU_SOURCE
#define _POSIX_C_SOURCE
#include <stdio.h>		/// For printf, fopen, fclose, FILE
#include <stdlib.h>		/// For malloc, free
#include <error.h>		/// For error
#include <errno.h>		/// For errno
#include <string.h>		/// for memset, memcpy, memmem
#include <sys/stat.h>	/// for stat
#include <locale.h>		/// for setlocale
#include <time.h>       /// for ctime_r, tzset

#define TRUE -1
#define FALSE 0

/***************************************************
* definition of a tzfile header
***************************************************/
#define HEADER_LEN 44
typedef struct {
	char magicnumber[6];	  // = TZif2 or TZif\0
//	char reserved[15];		  // nulls
	long unsigned ttisgmtcnt; // number of UTC/local indicators stored in the file.
	long unsigned ttisstdcnt; // number of standard/wall indicators stored in the file.
	long unsigned leapcnt;    // number of leap seconds for which data is stored in the file.
	long unsigned timecnt;    // number of "transition times" for which data is stored in the file.
	long unsigned typecnt;    // number of "local time types" for which data is stored in the file (must not be zero).
	long unsigned charcnt;    // number of  characters  of  "timezone  abbreviation  strings" stored in the file.
	} timezonefileheader;
#define TZIF1_FIELD_SIZE 4
#define TZIF2_FIELD_SIZE 8


/***************************************************
* parse a tzfile "long format" value
***************************************************/
long parse_tz_long( const char *sourceptr, const int field_size)
{
	long retval = 0;
	char *long_ptr;

//	if (sizeof(long) < field_size)
//		printf("warning: field truncated because it is larger than a 'long' integer\n\n");

	int i,j;
	long_ptr = (char*) &retval;
	if ((field_size < sizeof(long)) && (sourceptr[0] >> 8))
	{
		for (i=sizeof(long)-1; (i>=field_size); i--) long_ptr[i] = 255; 
	}
	j = 0;
	for (i=field_size-1; (i>=0) && (j<sizeof(long)); i--)
	{
		long_ptr[j] = sourceptr[i];
		j++;
	}
	return retval;
}

/***************************************************
* read a tzheader file
***************************************************/
int read_tz_header( timezonefileheader *header,  const char *temp_buffer)
{
	const int field_size = 4;

	memcpy( header->magicnumber, &temp_buffer[0], 5 );
	header->magicnumber[5] = '\0';

	header->ttisgmtcnt = parse_tz_long(&temp_buffer[20], field_size);
	header->ttisstdcnt = parse_tz_long(&temp_buffer[24], field_size);
	header->leapcnt = parse_tz_long(&temp_buffer[28], field_size);
	header->timecnt = parse_tz_long(&temp_buffer[32], field_size);
	header->typecnt = parse_tz_long(&temp_buffer[36], field_size);
	header->charcnt = parse_tz_long(&temp_buffer[40], field_size);

	printf("Header format ID: %s\n",header->magicnumber);
	printf("number of UTC/local indicators stored in the file = %ld\n", header->ttisgmtcnt);
	printf("number of standard/wall indicators stored in the file = %ld\n",header->ttisstdcnt);
	printf("number of leap seconds for which data is stored in the file = %ld\n",header->leapcnt);
	printf("number of \"transition times\" for which data is stored in the file = %ld\n",header->timecnt);
	printf("number of \"local time types\" for which data is stored in the file (must not be zero) = %ld\n",header->typecnt);
	printf("number of  characters  of  \"timezone  abbreviation  strings\" stored in the file = %ld\n\n",header->charcnt);

	if (header->typecnt == 0)
	{
		printf("Error in file format. Zero local time types suggested.\n");
		return FALSE;
	}

	if (header->timecnt == 0)
	{
		printf("No transition times recorded in this file\n");
		return FALSE;
	}

return TRUE;
}

/***********************************************************************
* tzif2_handle
***********************************************************************/
char* tzif2_handle( timezonefileheader *tzh, const char *tzfile_buffer_ptr, size_t buffer_size )
{
	char *start_ptr;
	char magicnumber[5] = "TZif2";

	printf("handle format 2\n");

	start_ptr = memmem( tzfile_buffer_ptr, buffer_size, &magicnumber, 5 );
	if (start_ptr == NULL)
	{
		printf("error finding tzif2 header\n");
		return NULL;
	}

	printf("tzif2 header found at position %ld\n", start_ptr - tzfile_buffer_ptr);
	if ( read_tz_header( tzh, (char*) start_ptr ) == FALSE )
	{
		printf("Error reading header file version 2\n");
		return NULL;
	}

	return start_ptr + HEADER_LEN;
}


/***************************************************
* Display contents of a timezone file
***************************************************/
void display_tzfile_data(	const timezonefileheader tzh,
							const char* tzfile_buffer_ptr,
							const int field_size /// 4bytes for tzif1, 8bytes for tzif2
							)
{
	long i;
	unsigned long temp_long;
	char ctime_buffer[200]; /// arbitrarily > 26 bytes
	long gmtoff;

	 /***************************************************
	 * 
	 * Structure of the timezone files
	 * 
	 ***************************************************/
//	start with the header info
//	timezonefileheader tzh;

//	Next in the file is the following array:
///	long unsigned transition_time[timecnt]
//	in ascending order, use ctime(2) to parse
	char	*transition_time_ptr;
	char	*temp_transition_time_ptr;
		
//	Next in the file is the following array:
///	char unsigned local_time_type[timecnt]
// which of the different types of "local time" types
// described in the  file  is associated  with  the
// same-indexed transition time. These values serve
// as indices  into  an  array  of  ttinfo  structures
// which follows.
		char *local_time_type_ptr;
		char *temp_local_time_ptr;

//	Next in the file is an array of length [typecnt] of the following structure:
		typedef struct {
			long gmtoff;	// seconds to add to UTC
			char is_dst;	// whether tm_isdst should be set by  localtime(3)
			char abbrind;	// index into the array of timezone abbreviation characters
			} ttinfo_struct;
//	However, in practice, we will not implement it as a structure, in order to simplify
//	pointer arithmetic ...
		ttinfo_struct *ttinfo_data_ptr, *temp_ttinfo_data_ptr;
		char *ttinfo_read_ptr, *temp_ttinfo_read_ptr;
/*		   localtime(3) uses the first standard-time ttinfo structure in the  file
       (or simply the first ttinfo structure in the absence of a standard-time
       structure) if either tzh_timecnt is zero or the time argument  is  less
       than the first transition time recorded in the file.
*/

		
//			Next in the file is supposed to an array of timezone aabreviations;
//			However, it was not explicitly defined in tzfile(5), so I'm guessing...
//			typedef char[4]	tz_abbrev;
		char* tz_abbrev_ptr;
		char* temp_tz_abbrev_ptr;

//			Next in the file is an array of length [leapcnt] of the following structure:
		typedef struct {
			long when; // time (as returned by time(2)) at which a leap second occurs
			long amt;  // total  number  of leap seconds to be applied after the given time
			}  leapinfo_struct;
		leapinfo_struct leapinfo;
		char *leapinfo_ptr;
		char *temp_leapinfo_ptr;
// HELP! - the leapinfo struct was not tested because my test file had it as a zero value


//			Next in the file is an array of length [ttisstdcnt] of the following:
//			char wall_indicator;
		char *wall_indicator_ptr;
		char *temp_wall_indicator_ptr;
		// whether the transition times associated with local_time_types were specified as
		// standard_time  or  wall_clock_time, and are used when a timezone file is used
		// in handling POSIX-style timezone environment variables.


//			Next in the file is an array of length [ttisgmtcnt] of the following:
//			char local_indicator;
		char *local_indicator_ptr;
		char *temp_local_indicator_ptr;
		// whether the transition times associated
		// with local_time types were specified as UTC or local time, and are used
		// when  a timezone file is used in handling POSIX-style timezone environ‐
		// ment variables.

//	This concludes the contents of the file if it was encoded using version 1 of
//	the format standard.
//  man tzfile claims that Version 2 of the format standard is identical to version
//	1, but appends the following, basically a repetition but using 8-byte values
//	for each  transition  time  or  leap-second  time, and concluding with a
//	'general rule' for transition times after the final one explicitly listed in
//	the file, or in their words "After the second  header and data comes a
//	newline-enclosed, POSIX-TZ-environment-variable-style string for use in
//	handling instants after the last transition time stored in the file (with
//	nothing between the newlines if there is no POSIX representation for such
//	instants).


 /********************************************************************
 * 
 * Beginning of code for function display_tzfile_data
 * 
 ********************************************************************/

	transition_time_ptr = (char*) tzfile_buffer_ptr;
	local_time_type_ptr = (char*) tzfile_buffer_ptr + tzh.timecnt*field_size;
	ttinfo_read_ptr = local_time_type_ptr + tzh.timecnt;

	ttinfo_data_ptr = malloc( sizeof(ttinfo_struct) * tzh.typecnt);
	if (ttinfo_data_ptr == NULL)
	{
		printf("memory allocation error - ttinfo_data\ntzname = ");
		return;
	}

	/// timezone abbreviation list
	tz_abbrev_ptr = ttinfo_read_ptr + (tzh.typecnt * 6);
	printf("time zone abbreviation list\n  # abbr_ind  abbr\n");
	temp_tz_abbrev_ptr = tz_abbrev_ptr;
	for (i=0;i<tzh.typecnt;i++)
	{
		if ((temp_tz_abbrev_ptr - tz_abbrev_ptr) < tzh.charcnt)
			printf("%3ld:   %3ld    %s\n",i, (temp_tz_abbrev_ptr - tz_abbrev_ptr), temp_tz_abbrev_ptr );
		temp_tz_abbrev_ptr = strchr( temp_tz_abbrev_ptr, 0 ) + 1;
	}

	/// Print ttinfo data
	temp_ttinfo_read_ptr = ttinfo_read_ptr;
	temp_ttinfo_data_ptr = ttinfo_data_ptr;
	printf("\nttinfo data\n #  gmt_offset   HH:mm:ss  is_dst  abbr_ind\n");
	for (i=0;i<tzh.typecnt;i++)
	{
		gmtoff = parse_tz_long( temp_ttinfo_read_ptr, 4 );
		temp_ttinfo_data_ptr->gmtoff = gmtoff;
		temp_ttinfo_data_ptr->is_dst = temp_ttinfo_read_ptr[4];
		temp_ttinfo_data_ptr->abbrind = temp_ttinfo_read_ptr[5];
		printf("%2ld: %6ld       %.2ld:%.2ld:%.2ld     %d       %2d\n", i,
				gmtoff,	gmtoff/3600, gmtoff%3600/60, gmtoff%60,
				(short int) temp_ttinfo_data_ptr->is_dst, (short int) temp_ttinfo_data_ptr->abbrind );
		temp_ttinfo_read_ptr = temp_ttinfo_read_ptr + 6;
		temp_ttinfo_data_ptr = temp_ttinfo_data_ptr + 1;
	}


	/// Print transition times
	printf("\n List of transition times\n #    epoch     local_time_type  human-readable_time\n");
	temp_transition_time_ptr = (char *) transition_time_ptr + ((tzh.timecnt-1) * field_size);
	temp_local_time_ptr = (char *) local_time_type_ptr + (tzh.timecnt-1);
	for (i=tzh.timecnt-1;i>0; i--)
	{
		temp_ttinfo_data_ptr = ttinfo_data_ptr + *temp_local_time_ptr;
		temp_long = parse_tz_long( temp_transition_time_ptr, field_size );
		long temp_local_time = temp_long  + temp_ttinfo_data_ptr->gmtoff;
		if ( asctime_r( gmtime( (const time_t *) &temp_local_time), (char*) &ctime_buffer) == NULL )
		{
			error(0,errno,"error returned by ctime_r in transition time data\n");
			free(ttinfo_data_ptr);
			return;
		}
		printf("%3ld: %11ld    -- %2d ---     %s",i, temp_long, *temp_local_time_ptr, (char*) &ctime_buffer);
		temp_transition_time_ptr = temp_transition_time_ptr - field_size;
		temp_local_time_ptr = temp_local_time_ptr - 1;
	}


//	leapinfo_ptr = tz_abbrev_ptr + (tzh.typecnt*4);
	leapinfo_ptr = tz_abbrev_ptr + (tzh.charcnt);
	if (tzh.leapcnt != 0)
	{
		temp_leapinfo_ptr = leapinfo_ptr;
		printf("\nleapinfo data\n  when  human_readable  num_of_seconds\n");
		for (i=0;i<tzh.leapcnt;i++)
		{
			leapinfo.when = parse_tz_long( temp_leapinfo_ptr, field_size );
			temp_leapinfo_ptr = temp_leapinfo_ptr +field_size;
			leapinfo.amt = parse_tz_long( temp_leapinfo_ptr, field_size );
			if ( ctime_r( (const time_t *) &leapinfo.when, (char*) &ctime_buffer) == NULL )
			{
				error(0,errno,"error returned by ctime_r in leapinfo data\n");
				free(ttinfo_data_ptr);
				return;
			}
			printf("%ld: %ld        %s     %ld\n",
					i, leapinfo.when, (char*) &ctime_buffer, leapinfo.amt );
			temp_leapinfo_ptr = temp_leapinfo_ptr +field_size;
		}
	}


	wall_indicator_ptr = leapinfo_ptr + (tzh.leapcnt*field_size*2);
	if (tzh.ttisstdcnt != 0)
	{
		temp_wall_indicator_ptr = wall_indicator_ptr;
		printf("\nwall_indicator data\n");
		for (i=0;i<tzh.ttisstdcnt;i++)
		{
			printf("%ld: %x\n",i, *temp_wall_indicator_ptr);
			temp_wall_indicator_ptr = temp_wall_indicator_ptr + 1;
		}
	}


	local_indicator_ptr = wall_indicator_ptr + tzh.ttisstdcnt;
	if (tzh.ttisgmtcnt != 0)
	{
		temp_local_indicator_ptr = local_indicator_ptr;
		printf("\nlocal_indicator data\n");
		for (i=0;i<tzh.ttisgmtcnt;i++)
		{
			printf("%ld: %x\n",i, *temp_local_indicator_ptr);
			temp_local_indicator_ptr = temp_local_indicator_ptr + 1;
		}
	}

	if (field_size == TZIF2_FIELD_SIZE)
	{
		/// parse 'general rule' at end of file
		/// format should be per man(3) tzset
		char *start_line_ptr;
		start_line_ptr = strchr( local_indicator_ptr + tzh.ttisgmtcnt, 10 );
		if (start_line_ptr == NULL)
			printf("\nno \'general rule\' information found at end of file\n");
		else
		{
			char *end_line_ptr;
			end_line_ptr = strchr( start_line_ptr+1, 10 );
			if (end_line_ptr == NULL)
				printf("\nerror finding \'general rule\' info terminator symbol\n");
			else
			{
				*end_line_ptr = '\0';
				printf("\ngeneral rule (unparsed): %s\n", start_line_ptr+1 );
			}
			
		}
			
	}

	free(ttinfo_data_ptr);
}

// Find the current time interval and return information for it
// ie local time type, leap seconds
// maybe use mktime to get seconds_since_epoch and compare to data in this file


int main (int argc, char *argv[])
{
	FILE *tz_file;
	char *tzif_buffer_ptr;
	char *start_ptr;
	int   field_size;
	struct stat file_status;
	timezonefileheader tzh;

	if (argv[1] == NULL)
	{
		printf("tzif-dislpay: display contents of tzif file\n\n\
usage: tzif-display full_pathname\n\
hints: /usr/share/zoneinfo/{continent}/{city}\n\
       /etc/localtime\n\
       You will probably want to pipe output through \'less\'\n");
		exit(0);
	}


	tz_file = fopen(argv[1], "rb");
//	tz_file = fopen("/etc/localtime", "rb");
//	tz_file = fopen("/usr/share/zoneinfo/posix/US/Alaska", "rb");
//	tz_file = fopen("/usr/share/zoneinfo/posix/MST", "rb");			// no transition times in part one
//	tz_file = fopen("/usr/share/zoneinfo/posix/MST7MDT", "rb");
//	tz_file = fopen("/usr/share/zoneinfo/posix/Asia/Baghdad", "rb");
//	tz_file = fopen("/usr/share/zoneinfo/posix/Asia/Kabul", "rb");
//	tz_file = fopen("/usr/share/zoneinfo/posix/Asia/Kathmandu", "rb");
//	tz_file = fopen("/usr/share/zoneinfo/posix/Asia/Calcutta", "rb");
//	tz_file = fopen("/usr/share/zoneinfo/posix/Asia/Shanghai", "rb");
//	tz_file = fopen("/usr/share/zoneinfo/posix/Asia/Jerusalem", "rb");
//	tz_file = fopen("/usr/share/zoneinfo/posix/America/Buenos_Aires", "rb");
//	tz_file = fopen("/usr/share/zoneinfo/posix/America/Sao_Paulo", "rb");

	if (tz_file == NULL)
	{
		error(errno,0,"tz file %s not found\n", argv[1]);
		exit(errno);
	}

	if (fstat( fileno( tz_file), &file_status) != 0)
	{
		printf("error retreiving file status\n");
		exit(errno);
	}

	printf("Data for file: %s\n",argv[1]);
	printf("file size is %ld bytes\n\n", file_status.st_size);

	tzif_buffer_ptr = (char *) malloc( file_status.st_size );
	if (tzif_buffer_ptr == NULL)
	{
		printf("memory allocation error - tzif buffer\n");
		exit(errno);
	}

	if ( fread( tzif_buffer_ptr, file_status.st_size, 1, tz_file) != 1 )
	{
		printf("error reading into tzif buffer\n");
		free(tzif_buffer_ptr);
		exit(errno);
	}
	fclose(tz_file);


	if ( read_tz_header( &tzh, tzif_buffer_ptr ) == FALSE )
	{
		printf("Error reading header file version 1\n");
		exit(errno);
	}

	if (tzh.magicnumber[4] == 50 )
	{
		start_ptr = tzif2_handle( &tzh, &tzif_buffer_ptr[HEADER_LEN], file_status.st_size - HEADER_LEN);
		field_size = TZIF2_FIELD_SIZE;
	}
	else
	{
		start_ptr = &tzif_buffer_ptr[HEADER_LEN];
		field_size = TZIF1_FIELD_SIZE;
	}
	if (start_ptr != NULL ) display_tzfile_data( tzh, start_ptr, field_size );

	free(tzif_buffer_ptr);
	exit(0);
}
