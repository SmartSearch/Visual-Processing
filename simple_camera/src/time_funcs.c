/* 
* SMART FP7 - Search engine for MultimediA enviRonment generated contenT
* Webpage: http://smartfp7.eu
*
* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/.
*
* The Original Code is Copyright (c) 2012-2013 Athens Information Technology
* All Rights Reserved
*
* Contributor:
*  Nikolaos Katsarakis nkat@ait.edu.gr
*/

#include "time_funcs.h"

/* Get the number of milliseconds since 01-01-1970 (UNIX epoch) */
long long getMillis()
{
	/* Currently crude approximation using standard C lib */
	/* Could be improved by using OS-specific functions   */
	static int called=0;
	static clock_t start_clocks;
	static time_t start_seconds;
	time_t cur_seconds;
	clock_t cur_clocks;
	if (called==0)
	{
		cur_clocks=clock();
		cur_seconds = time(NULL);
		/* Busy wait till the first change in seconds */
		do
		{
			start_clocks=clock();
			start_seconds = time(NULL);
		}
		while (start_seconds==cur_seconds);
		called=1;
		return (long long)start_seconds*1000;
	}
	cur_clocks=clock();
	return 1000*(long long)start_seconds+1000*(cur_clocks - start_clocks)/CLOCKS_PER_SEC;
}

/* Convert milliseconds to string in xsd:dateTime format with UTC timezone
   i.e. YYYY-MM-DDThh:mm:ss.nnnZ 
   where YYYY: year, MM: month, DD: day,
   hh: hour, mm: minute, ss: second, nnn: millisecond
   
   parameters: 
   millis: the number to convert
   date_time: string buffer to store the result
   len: length of string buffer 
   
   returns:
   0 on success
   -1 on error */
int millis2string(long long millis, char *date_time, size_t len)
{
	time_t seconds;
	long millisec;
	time_t this_time;
	struct tm * ptm;
	int time_len;

	if (len < 25)
	{
		fprintf(stderr, "millis2string: not big enough buffer size (%d)\n", len);
		return -1;
	}

	if (millis < 0)
	{
		fprintf(stderr, "millis2string: called with negative ms value %ld\n", millis);
		return -1;
	}
	seconds = millis / 1000;
	millisec = millis % 1000;

	this_time = seconds;
	ptm = gmtime( &this_time );
	if (ptm == NULL)
	{
		fprintf(stderr, "millis2string: too large ms value %ld\n", millis);
		return -1;
	}

	time_len = strftime(date_time,len-5,"%Y-%m-%dT%H:%M:%S", ptm);
	if (time_len == 0)
	{
		fprintf(stderr, "millis2string: not big enough buffer size (%d)\n", len);
		return -1;
	}
	sprintf(&date_time[time_len],".%03dZ",millisec);
	return 0;
}