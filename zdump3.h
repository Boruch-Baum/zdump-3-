/** zdump3.h            http://libhdate.sourceforge.net
 * Decode and dump information from a zonedata (TZif) timezone file.
 * The file is expected to have been compiled using the glibc 'zic'
 * command.
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
 
/// zdumpinfo - an element of the array to return
#define MAX_TZ_ABBR_SIZE   10  /// safe
typedef struct {
	time_t start;    	/// seconds from epoch
	int		utc_offset;	/// in seconds.
	int		save_secs;	/// (0 if not dst)
	char	abbr[MAX_TZ_ABBR_SIZE]; /// terminate with '\0'
	} zdumpinfo;


extern int
zdump(                   /// returns 0 on success, -1 on failure
    char* tzname,        /// fully-qualified time-zone name (eg. Asia/Baku)
                         ///    if NULL, use current system timezone
    const time_t start,  /// seconds from epoch to be scanned
    const time_t end,    /// seconds from epoch to be scanned
    int* num_entries,    /// upon successful return, int will contain number
                         ///    of dst transitions + 1, for the interval
                         ///    'start' to 'end'. The first entry will
                         ///    always be the tz state at time_t start.
                         ///    Returns 0 on failure.
    void** return_data   /// upon successful return, will contain a pointer
                         ///    to a malloc()ed space of 'num_entries' of
                         ///    'tzinfo' data, as described below, sorted
                         ///    in ascending chronological order.
                         ///    Returns NULL on failure.
          );


#define ZD_SUCCESS  0
#define ZD_FAILURE -1
#define ZD_BAD_VALUES  5001 /** time_t start > time_t end */
#define ZD_DIR_PATH    5002 /** path to zonetab directory not found */
#define ZD_FOPEN       5003 /** file not found */
#define ZD_FREAD       5004 /** unable to read file */
#define ZD_MALLOC      5005 /** memory allocation error */
#define ZD_TZIF_HEADER 5006 /** unable to parse tzif header */
