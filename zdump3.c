 /** zdump3.c                           http://libhdate.sourceforge.net
 *   zdump3() - return timezone data for a requested interval
 *
 * compile:
 *  gcc -c -Wall -Werror -fPIC zdump3.c
 * build:
 *  gcc -shared -o libzdump3.so zdump3.o
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
 
#define _GNU_SOURCE     /// feature_test_macro - for memmem
#define _POSIX_C_SOURCE 1
#include <time.h>		/// for time, ctime
#include <stdlib.h>		/// for getenv
#include <unistd.h>		/// for getcwd, fstat
#include <stdio.h>		/// for fopen, fileno
#include <sys/types.h>	/// for fstat
#include <sys/stat.h>	/// for fstat
#include <unistd.h>		/// for fstat
#include <string.h> 	/// for memcpy

#define ZD_SUCCESS  0
#define ZD_FAILURE -1
#define ZD_BAD_VALUES  5001 /** time_t start > time_t end */
#define ZD_DIR_PATH    5002 /** path to zonetab directory not found */
#define ZD_FOPEN       5003 /** file not found */
#define ZD_FREAD       5004 /** unable to read file */
#define ZD_MALLOC      5005 /** memory allocation error */
#define ZD_TZIF_HEADER 5006 /** unable to parse tzif header */

#define NUMERIC "-123456789"
#define TIMERIC "-123456789:"
#define NOTABBR "-123456789:,"

 
/// zdumpinfo - an element of the array to return
#define MAX_TZ_ABBR_SIZE   10  /// safe
typedef struct {
	time_t	start;    	/// seconds from epoch
	int		utc_offset;	/// in seconds
	int		save_secs;	/// (0 if not dst)
	char	abbr[MAX_TZ_ABBR_SIZE]; /// terminate with '\0'
	} zdumpinfo;

/// timezonefileheader - exists in one or two parts of a tzif file
/// refer to 'man 5 tzfile' for structure of the TZif file
#define HEADER_LEN 44
typedef struct {
	char magicnumber[6];	  /// = TZif2 or TZif\0
///	char reserved[15];		  /// nulls
	long unsigned ttisgmtcnt; /// number of UTC/local indicators stored in the file.
	long unsigned ttisstdcnt; /// number of standard/wall indicators stored in the file.
	long unsigned leapcnt;    /// number of leap seconds for which data is stored in the file.
	long unsigned timecnt;    /// number of "transition times" for which data is stored in the file.
	long unsigned typecnt;    /// number of "local time types" for which data is stored in the file (must not be zero).
	long unsigned charcnt;    /// number of  characters  of  "timezone  abbreviation  strings" stored in the file.
	} timezonefileheader;
#define TZIF1_FIELD_SIZE 4
#define TZIF2_FIELD_SIZE 8
static char *local_time_type_ptr, *ttinfo_ptr, *tz_abbrev_ptr;
#define SIZE_OF_TTINFO 6


/// posix rule details
typedef struct {
	char type[2];
	char abbr[2][MAX_TZ_ABBR_SIZE];
	int j[2];
	int m[2];
	int w[2];
	int d[2];
	int offset[2];
	int start_time[2];
	int hour[2];
	int min[2];
	int sec[2];
	int has_dst;
	int save_secs[2];
	} rule_detail;
#define STD 0
#define DST 1
#define DEFAULT_START_TIME 2 * 60 * 60
#define DEFAULT_START_HOUR 2
#define DEFAULT_START_MIN  0
#define DEFAULT_START_SEC  0


#define BUFFER_INCREMENT 3  /// safe for a single year
void* perform_a_realloc(void *old_buffer, size_t *buff_size)
{
	void *new_ret;
	*buff_size = *buff_size + (sizeof(zdumpinfo) * BUFFER_INCREMENT);
	new_ret = realloc(old_buffer, *buff_size);
	if (new_ret == NULL) free(old_buffer);
	/// an alternative would be that if num_entries > 0
	/// return with what we've ZD_SUCCESSfully read
	return new_ret;
}

signed long flip_tz_long( const char *sourceptr, const unsigned int field_size)
{
	signed long retval = 0;
	char *long_ptr;
	unsigned int i,j;
	long_ptr = (char*) &retval;
	j = 0;
	if ((field_size < sizeof(long)) && (sourceptr[0] >> 8))
	{
		for (i=sizeof(long)-1; (i>=field_size); i--) long_ptr[i] = '\x0ff';
	}
	for (i=field_size; i && (j<sizeof(long)); i--, j++)
	{
		long_ptr[j] = sourceptr[i-1];
	}
	return retval;
}

void j_to_md ( int j, int* m, int* d)
{
	/// For the TZ rule,   j = 1 - 365. no leap yeaers recognized
	/// For the tm struct, j = 0 -365, leap years ARE recognized
	/// This function parses the TZ POSIX rule into terms compatible
	/// with the tm struct: m = 0 - 11, d = 1 -31
/**jan*/if (j<32)         { *m = 0; *d = j; }
/**feb*/else if (j < 60)  { *m = 1; *d = j-31; }
/**mar*/else if (j < 91)  { *m = 2; *d = j-60; }
/**apr*/else if (j < 121) { *m = 3; *d = j-91 ; }
/**may*/else if (j < 152) { *m = 4; *d = j-121; }
/**jun*/else if (j < 182) { *m = 5; *d = j-152; }
/**jul*/else if (j < 213) { *m = 6; *d = j-182; }
/**aug*/else if (j < 244) { *m = 7; *d = j-213; }
/**sep*/else if (j < 274) { *m = 8; *d = j-244; }
/**oct*/else if (j < 305) { *m = 9; *d = j-274; }
/**nov*/else if (j < 335) { *m = 10; *d = j-305; }
/**dec*/else              { *m = 11; *d = j-335; }
	return;
}

int read_tz_header( timezonefileheader *header,  const char *temp_buffer)
{
	const int field_size = 4;
	memcpy( header->magicnumber, &temp_buffer[0], 5 );
	header->magicnumber[5] = '\0';
	header->ttisgmtcnt = flip_tz_long(&temp_buffer[20], field_size);
	header->ttisstdcnt = flip_tz_long(&temp_buffer[24], field_size);
	header->leapcnt = flip_tz_long(&temp_buffer[28], field_size);
	header->timecnt = flip_tz_long(&temp_buffer[32], field_size);
	header->typecnt = flip_tz_long(&temp_buffer[36], field_size);
	header->charcnt = flip_tz_long(&temp_buffer[40], field_size);
	if ( (header->typecnt == 0) || (header->timecnt == 0) ) return 0;
	return 1;
}

void add_a_rule_entry( const time_t start, void* return_data, int* num_entries,
                       const int utc_offset, const int save_secs, const char* abbr)
{
	zdumpinfo* zd;
	zd = (zdumpinfo*) return_data + *num_entries;
	zd->start = start;
	zd->utc_offset = utc_offset;
	zd->save_secs = save_secs;
	strncpy( zd->abbr, abbr, MAX_TZ_ABBR_SIZE);
	*num_entries = *num_entries + 1;
}

void add_a_tzif_entry( const int i, const time_t start, void* return_data, int* num_entries )
{
	char* endptr;
	int local_time_offset = local_time_type_ptr[i] * SIZE_OF_TTINFO;
	char* tzabbr;
	zdumpinfo* zd;
	int prior_local_time_offset;

	zd = (zdumpinfo*) return_data + *num_entries;
	zd->start = start;
	zd->utc_offset = (int) flip_tz_long( ttinfo_ptr + local_time_offset, 4);
	zd->save_secs = 0;
	if ((i != 0) && (ttinfo_ptr[ local_time_offset + 4 ]))
	{
		prior_local_time_offset = local_time_type_ptr[i-1] * SIZE_OF_TTINFO;
		zd->save_secs = abs( zd->utc_offset
						- ((int) flip_tz_long( ttinfo_ptr + prior_local_time_offset, 4)) );
	}
	tzabbr = &tz_abbrev_ptr[ (int) ttinfo_ptr[ local_time_offset + 5 ] ];
	endptr = mempcpy( zd->abbr, tzabbr, strlen(tzabbr) );
	memset( endptr, '\0', 1);
	*num_entries = *num_entries + 1;
}

int get_time( char *strptr, int *hour, int *min, int *sec )
{
	int fields_found = 0;
	int sign;

	fields_found = sscanf(strptr, "%i:%i:%i", hour, min, sec);
	switch (fields_found)
	{
	case 0: *hour = DEFAULT_START_HOUR;
	case 1: *min = DEFAULT_START_MIN;
	case 2: *sec = DEFAULT_START_SEC;
	}
	sign = *hour < 0 ? -1 : 1;
	return (*hour * 3600) + (sign * ((*min * 60) + *sec));
}

char* rule_julian( int i, char* next, rule_detail* p_rule )
{
	char julian[4];
	int  len = 0;
	

	next++;
	p_rule->type[i] = 'J';
	len = strspn(next, NUMERIC);
	if (len > 3) return NULL;
	memcpy(&julian, next, len);
	memset(&julian[len],'\0',1);
	p_rule->j[i] = atoi( (char*) &julian);
	j_to_md(p_rule->j[i], &p_rule->m[i], &p_rule->d[i]);
	next += len;
	if (*next == '/') p_rule->start_time[i] = get_time(next+1, 
									&p_rule->hour[i], &p_rule->min[i], &p_rule->sec[i]);
	else
	{
		p_rule->start_time[i] = DEFAULT_START_TIME;
		p_rule->hour[i] = DEFAULT_START_HOUR;
		p_rule->min[i] = DEFAULT_START_MIN;
		p_rule->sec[i] = DEFAULT_START_SEC;
	}
	return strchr( next, ',' );
}

char* rule_mwd( int i, char* next, rule_detail* p_rule )
{
	char *next_time  = NULL;
	p_rule->type[i] = 'M';
	next++;
	// maybe error-check for 3 values read in sscanf?
	sscanf(next, "%i.%i.%i", &p_rule->m[i], &p_rule->w[i], &p_rule->d[i]);
	next_time = strchr( next, '/' );
	next      = strchr( next, ',' );
	if ((next_time != NULL) && ((next == NULL) || (next_time < next)))
	{
		next_time++;
		p_rule->start_time[i] = get_time(next_time, &p_rule->hour[i], &p_rule->min[i], &p_rule->sec[i]);
	}
	else
	{
		p_rule->start_time[i] = DEFAULT_START_TIME;
		p_rule->hour[i] = DEFAULT_START_HOUR;
		p_rule->min[i] = DEFAULT_START_MIN;
		p_rule->sec[i] = DEFAULT_START_SEC;
	}
	return next;
}

int rule_decode( const char* tzif, const size_t tzif_size, rule_detail* p_rule )
{
	char *rule_string = NULL;
	int  rule_len;
	char *next = NULL;
	int  len = 0;
	int  offset_hour, offset_min, offset_sec;

	// consider error checking and bounds checking
	// don't depend on it being a valid file
	if ( tzif[tzif_size] == '\x0a' ) return ZD_FAILURE;
	rule_string = memrchr( tzif, '\x0a', tzif_size);
	if (rule_string == NULL) return ZD_FAILURE;
	rule_string++;
	next = rule_string;
	setenv("TZ",rule_string,1);
	rule_len = strlen(rule_string);
	if (rule_len == 0) return ZD_FAILURE;
	memset(p_rule,'\0',sizeof(rule_detail));
	len = strcspn(rule_string, NUMERIC);
	memcpy(p_rule->abbr[STD], rule_string, len);
	next += len;
	len = strspn(next, TIMERIC);
	p_rule->offset[STD] = get_time( next, &offset_hour, &offset_min, &offset_sec );
	next +=  len;
	if ( next != (rule_string + rule_len -1) )
	{
		p_rule->has_dst = 1;
		len = strcspn(next, NOTABBR);
		memcpy(p_rule->abbr[DST], next, len);
		next += len;
		len = strspn(next, TIMERIC);
		if (len == 0) p_rule->offset[DST] = p_rule->offset[STD] + 3600;
		else p_rule->offset[DST] = get_time( next, &offset_hour, &offset_min, &offset_sec );
		next += len;
		p_rule->save_secs[STD] = abs(p_rule->offset[DST] - p_rule->offset[STD]);
		next++;
		if (*next == 'J') next = rule_julian(STD, next, p_rule);
		else next = rule_mwd(STD, next, p_rule);
		if (*next == ',')
		{
			next++;
			if (*next == 'J') next = rule_julian(DST, next, p_rule);
			else next = rule_mwd(DST, next, p_rule);
		}
	}
/** DEBUG
	printf("rule details:\n\
%c %6s j=%3d m=%2d w=%d d=%2d offset=%6d start_time=%6d h=%2d m=%2d s=%2d has_dst=%d save_secs=%d\n\
%c %6s j=%3d m=%2d w=%d d=%2d offset=%6d start_time=%6d h=%2d m=%2d s=%2d has_dst=%d save_secs=%d\n",
(int) p_rule->type[0], (char*) &p_rule->abbr[0], p_rule->j[0], p_rule->m[0], p_rule->w[0], p_rule->d[0],
p_rule->offset[0], p_rule->start_time[0], p_rule->hour[0], p_rule->min[0], p_rule->sec[0], p_rule->has_dst, p_rule->save_secs[0],
(int) p_rule->type[1], (char*) &p_rule->abbr[1], p_rule->j[1], p_rule->m[1], p_rule->w[1], p_rule->d[1],
p_rule->offset[1], p_rule->start_time[1], p_rule->hour[1], p_rule->min[1], p_rule->sec[1], p_rule->has_dst, p_rule->save_secs[1]
);
exit(0);
*/
	return ZD_SUCCESS;
}

void init_tm_struct( struct tm *tmx )
{
		tmx->tm_sec = 0;
		tmx->tm_min = 0;
		tmx->tm_hour = 0;
		tmx->tm_mday = 0;
		tmx->tm_mon = 0;
		tmx->tm_year = 0;
		tmx->tm_wday = 0;
		tmx->tm_yday = 0;
		tmx->tm_isdst = 0;
}

int utc_seconds( const struct tm *tmx )
{
	return tmx->tm_sec + (tmx->tm_min * 60) + (tmx->tm_hour * 3600);
}

int rule_dump( const char* tzif, const size_t tzif_size,
					  const time_t start, const time_t end, time_t current,
					  int* num_entries, void** return_data, size_t *ret_buff_size)
{
	rule_detail p_rule;
	time_t prior;
	struct tm tm_local, tm_gmt, tm_prior;
	int utc_offset;
	int save_secs;
	zdumpinfo* zd;
	int i;

// Should I make this a little simpler by using the glibc version
// of struct tm which has additional fields:
//  long tm_gmtoff;           /* Seconds east of UTC */
//  const char *tm_zone;      /* Timezone abbreviation */
// returned by gmtime_r localtime_r
	if (rule_decode( tzif, tzif_size, &p_rule ) == ZD_FAILURE) return ZD_FAILURE;
	init_tm_struct(&tm_local);
	init_tm_struct(&tm_gmt);
	if (current < start)
	{
		// We really should try to be totally independant of
		// these two functions localtime_r() and gmtime_r()
		localtime_r(&start, &tm_local);
		gmtime_r(&start, &tm_gmt);
		utc_offset = utc_seconds(&tm_local) - utc_seconds(&tm_gmt);
		// FIXME - account for possibility of initial transition being
		// from standard to standard or DST to DST, with different offsets
		if (tm_local.tm_isdst <= 0) save_secs = tm_local.tm_isdst;
		else
		{
			localtime_r(&current, &tm_prior);
			gmtime_r(&current, &tm_gmt);
			save_secs = abs(utc_offset
						- (utc_seconds(&tm_prior) - utc_seconds(&tm_gmt)) );
		}
		if ( !( *num_entries%BUFFER_INCREMENT) )
		{
			*return_data = perform_a_realloc(*return_data, ret_buff_size);
			if (*return_data == NULL) return ZD_FAILURE;
		}
		add_a_rule_entry( start, *return_data, num_entries, utc_offset, save_secs, tm_local.tm_zone );
		if (!p_rule.has_dst) return ZD_SUCCESS;
		memcpy( (char*) &tm_prior, (char*) &tm_local, sizeof(struct tm));
	}
	else
	{
		zd = (zdumpinfo*) *return_data + (*num_entries - 1);
		prior = zd->start;
		localtime_r(&prior, &tm_prior);
	}
	if ((!tm_prior.tm_isdst) && (!p_rule.has_dst)) return ZD_SUCCESS;
	i = tm_prior.tm_isdst > 0 ? 1 : 0;
	while (1)
	{
		tm_local.tm_hour = p_rule.hour[i];
		tm_local.tm_min  = p_rule.min[i];
		tm_local.tm_sec  = p_rule.sec[i];
		switch (p_rule.type[i])
		{
		case 'M':
			tm_local.tm_mday = 1;
			tm_local.tm_mon = p_rule.m[i] - 1;
			if ( tm_prior.tm_mon < tm_local.tm_mon ) tm_local.tm_year = tm_prior.tm_year;
			else tm_local.tm_year = tm_prior.tm_year + 1;
			current = mktime(&tm_local);
			tm_local.tm_mday = ((p_rule.w[i]-1)*7) + p_rule.d[i] + 1 + (7 - tm_local.tm_wday);
			break;
		case 'J':
			if ( tm_prior.tm_yday < p_rule.j[i] ) tm_local.tm_year = tm_prior.tm_year;
			else tm_local.tm_year = tm_prior.tm_year + 1;
			tm_local.tm_mday  = p_rule.j[i];
			tm_local.tm_mon  = 0;
			break;
		default: return ZD_FAILURE; break;
		}
		current = mktime(&tm_local);
		if (current > end) break;
		if ( !( *num_entries%BUFFER_INCREMENT) )
		{
			*return_data = perform_a_realloc(*return_data, ret_buff_size);
			if (*return_data == NULL) return ZD_FAILURE;
		}
		add_a_rule_entry( current, *return_data, num_entries, tm_local.tm_gmtoff,
							p_rule.save_secs[i], tm_local.tm_zone );
		// this next is only for the first pass
		if (!p_rule.has_dst) return ZD_SUCCESS;
		memcpy( (char*) &tm_prior, (char*) &tm_local, sizeof(struct tm));
		i = i > 0 ? 0 : 1;
	}
	return ZD_SUCCESS;
}


int zdump(               /// returns 0 on ZD_SUCCESS, -1 on ZD_FAILURE
    char* tzname,        /// fully-qualified time-zone name (eg. Asia/Baku)
                         ///    if NULL, use current system timezone
    const time_t start,  /// seconds from epoch to be scanned
    const time_t end,    /// seconds from epoch to be scanned
    int* num_entries,    /// upon ZD_SUCCESSful return, int will contain number
                         ///    of dst transitions + 1, for the interval
                         ///    'start' to 'end'. The first entry will
                         ///    always be the tz state at time_t start.
                         ///    Returns 0 on ZD_FAILURE.
    void** return_data   /// upon ZD_SUCCESSful return, will contain a pointer
                         ///    to a malloc()ed space of 'num_entries' of
                         ///    'tzinfo' data, as described below, sorted
                         ///    in ascending chronological order.
                         ///    Returns NULL on ZD_FAILURE.
          )
{
// TODO - report errors and set errno
	char* startdir = NULL;	/// current working directory on entry to this function
	char* tzdir     = NULL;	/// system base timezone directory
	char* tzdirlist[2] = { "/usr/share/zoneinfo/",	/// libc >= 5.4.6
						   "/usr/lib/zoneinfo/" };	/// libc <  5.4.6
	char* localtime_name = "localtime";
	struct stat file_status;
	char* tzif = NULL;		/// tz file copied into memory here
	timezonefileheader tzh;
	char* start_ptr;		/// point in *tzif where we start to parse
	unsigned int field_size;/// different for tzif and tzif2
	FILE *tz_file;
	int result;
	unsigned int i;
	signed long temp_long;
	size_t ret_buff_size = 0;
	char *transition_time_ptr;

	*num_entries = 0;
	if (end < start) return ZD_BAD_VALUES;
	startdir = getcwd( NULL, 0 );
	tzdir = getenv("TZDIR");
	result = ZD_DIR_PATH;
	if (tzdir != NULL) result = chdir(tzdir);
	if (result)        result = chdir(tzdirlist[0]);
	if (result)        result = chdir(tzdirlist[1]);
	if (result)        return ZD_DIR_PATH;
	result = ZD_SUCCESS;
	if (tzname == NULL) tzname = localtime_name;
	tz_file = fopen(tzname, "rb");
	if (tz_file == NULL) {result= ZD_FOPEN; goto endpoint;};
	if (fstat( fileno(tz_file), &file_status) != 0) {result= ZD_FREAD; goto endpoint;};
	tzif = malloc( file_status.st_size );
	if (tzif == NULL)  {result= ZD_MALLOC; goto endpoint;};
	if (fread( tzif, file_status.st_size, 1, tz_file) != 1 )  {result= ZD_FREAD; goto endpoint;};
	fclose(tz_file);
	if (!read_tz_header( &tzh, tzif)) {result= ZD_TZIF_HEADER; goto endpoint;};
	if (tzh.magicnumber[4] == 50 )
	{
		start_ptr = memmem( &tzif[HEADER_LEN], file_status.st_size - HEADER_LEN, "TZif2", 5 );
		if (start_ptr == NULL) {result= ZD_TZIF_HEADER; goto endpoint;};
		if (read_tz_header( &tzh, start_ptr ) == ZD_FAILURE ) {result= ZD_TZIF_HEADER; goto endpoint;};
		start_ptr = start_ptr + HEADER_LEN;
		field_size = TZIF2_FIELD_SIZE;
	}
	else
	{
		start_ptr = &tzif[HEADER_LEN];
		field_size = TZIF1_FIELD_SIZE;
	}

	transition_time_ptr = start_ptr;
	local_time_type_ptr = start_ptr + tzh.timecnt*field_size;
	ttinfo_ptr = local_time_type_ptr + tzh.timecnt;
	tz_abbrev_ptr = ttinfo_ptr + (tzh.typecnt * 6);
	// TODO - replace this incremental search with something more efficient
	for (i=0; i<tzh.timecnt; i++)
	{
		temp_long = flip_tz_long( transition_time_ptr, field_size );
		if (temp_long >= start)
		{
			*return_data = perform_a_realloc(*return_data, &ret_buff_size);
			if (*return_data == NULL) {result= ZD_MALLOC; goto endpoint;};
			add_a_tzif_entry( i>0 ? i-1: 0, (time_t) start, *return_data, num_entries );
			break;
		}
		transition_time_ptr = transition_time_ptr + field_size;
	}
	while (i < tzh.timecnt)
	{
		temp_long = flip_tz_long( transition_time_ptr, field_size );
		if (temp_long > end) break;
		if ( !( *num_entries%BUFFER_INCREMENT) ) *return_data = perform_a_realloc(*return_data, &ret_buff_size);
		add_a_tzif_entry( i, (time_t) temp_long, *return_data, num_entries );
		transition_time_ptr = transition_time_ptr + field_size;
		i++;
	}
	if ((i == tzh.timecnt) && (temp_long < end))
		result = rule_dump(tzif, file_status.st_size-2, start, end, (time_t) temp_long,
						num_entries, return_data, &ret_buff_size);
/// cleanup and exit
endpoint:
	if (tzif != NULL) free(tzif);
	chdir(startdir);
	if (!(*num_entries))
	{
		if (*return_data != NULL) free(*return_data);
		result = ZD_FAILURE;
	}
	return result;
}
