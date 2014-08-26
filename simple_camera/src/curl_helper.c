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


/* This file uses libcurl which comes with the following notice: 
*
** COPYRIGHT AND PERMISSION NOTICE
** 
** Copyright (c) 1996 - 2012, Daniel Stenberg, <daniel@haxx.se>.
** 
** All rights reserved.
** 
** Permission to use, copy, modify, and distribute this software for any purpose
** with or without fee is hereby granted, provided that the above copyright
** notice and this permission notice appear in all copies.
** 
** THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
** IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
** FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT OF THIRD PARTY RIGHTS. IN
** NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
** DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
** OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE
** OR OTHER DEALINGS IN THE SOFTWARE.
** 
** Except as contained in this notice, the name of a copyright holder shall not
** be used in advertising or otherwise to promote the sale, use or other dealings
** in this Software without prior written authorization of the copyright holder.
** 
*
* This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY
* KIND, either express or implied.
*
***************************************************************************/ 

#include "curl_helper.h"

/* Curl handle */
CURL *curl_session = NULL;

/* Header list */
struct curl_slist *header = NULL;

/* Debug level */
int curl_debug_level = 0;

/* store result code from curl functions */
CURLcode res;

/* Store the response code from http */
long http_response_code = 0;

/* This dummy function can be used if we want to avoid printing the received data */
size_t dummy_write(char *ptr, size_t size, size_t nmemb, void *userdata)
{
	return size * nmemb;
}

int curl_init(char *url)
{
	/* Check that we haven't initialised already */
	if(curl_session != NULL)
	{
		fprintf(stderr, "curl_init() has been called before, ignoring\n");
		return -1;
	}

	/* In windows, this will init the winsock stuff */ 
	res = curl_global_init(CURL_GLOBAL_DEFAULT);
	/* Check for errors */ 
	if(res != CURLE_OK) 
	{
		fprintf(stderr, "curl_global_init() failed: %s\n",
			curl_easy_strerror(res));
		return -1;
	}

	/* get a curl handle */ 
	curl_session = curl_easy_init();

	if(curl_session == NULL)
	{
		fprintf(stderr, "curl_easy_init() failed\n");
		curl_global_cleanup();
		return -1;
	}

	/* We are now ready to perform operations on the curl handle */

	/* Set the URL for the POST request */ 
	curl_easy_setopt(curl_session, CURLOPT_URL, url);

	/* Set content-type to json */
	header = curl_slist_append(header, "Content-Type: application/json");
	curl_easy_setopt(curl_session, CURLOPT_HTTPHEADER, header);

	/* Set to fail if the HTTP request returns error */
	curl_easy_setopt(curl_session, CURLOPT_FAILONERROR, 1L);

	return 0;
}

/* Sets the verbosity of the output */
int curl_set_debug_level(int debug_level)
{
	/* Check that curl session is initialised */
	if (curl_session==NULL)
	{
		fprintf(stderr, "CURL has not been initialised, call curl_init() first\n");
		return -1;
	}
	curl_debug_level=debug_level;
	if (curl_debug_level>1)
	{
		/* get verbose debug output please */ 
		curl_easy_setopt(curl_session, CURLOPT_VERBOSE, 1L);
	}
	else if (curl_debug_level==0)
	{
		/* avoid printing HTTP response */
		curl_easy_setopt(curl_session, CURLOPT_WRITEFUNCTION, &dummy_write);
	}
	return 0;
}



int curl_send(char * data, int len)
{
	/* Check that curl session is initialised */
	if (curl_session==NULL)
	{
		fprintf(stderr, "CURL has not been initialised, call curl_init() first\n");
		return -1;
	}

	/* Configure the data to POST */
	curl_easy_setopt(curl_session, CURLOPT_POSTFIELDS, data);

	/* Set the request data size, since we know it beforehand */
	curl_easy_setopt(curl_session, CURLOPT_POSTFIELDSIZE, len);

	/* Perform the request, res will get the return code */ 
	res = curl_easy_perform(curl_session);

	/* Also get the HTTP Response code */
	curl_easy_getinfo(curl_session, CURLINFO_RESPONSE_CODE, &http_response_code);
	return res;
}

/* Should be called if curl_send() returns error to return detailed error code */ 
int curl_show_error()
{
	const char* error = curl_easy_strerror(res);

	/* If an HTTP error has occurred, return the http code*/
	if (strstr(error,"HTTP")!=NULL)
	{
		if (http_response_code == HTTP_CONFLICT)
		{
			fprintf(stderr,"Data already exists\n");
		}
		else if (http_response_code == HTTP_NOT_FOUND)
		{
			fprintf(stderr, "Address not found\n", http_response_code);
		}
		else if (http_response_code == HTTP_BAD_REQUEST)
		{
			fprintf(stderr, "Bad request (HTTP error %ld)\n", http_response_code);
		}
		else
		{
			fprintf(stderr,"Unknown HTTP response code %ld\n", http_response_code);
		}
		return http_response_code;
	}
	else /* Some other error occured, return the curl error code */
	{
		fprintf(stderr, "curl_easy_perform() failed: %s\n", error);
		return res;
	}
}

/* Can be called if curl_send() returns CURLE_OK to show any warnings */ 
void curl_show_warning()
{
	if (http_response_code == HTTP_CREATED)
	{
		/* Don't print all the time the success */
		if (curl_debug_level>0) printf("Data inserted successfully\n");
	}
	else
	{
		fprintf(stderr, "Operation was successful but response code is unknown %ld\n", http_response_code);
	}
}


void curl_cleanup()
{
	/* Check that curl session is initialised */
	if (curl_session==NULL)
	{
		fprintf(stderr, "CURL has not been initialised, call curl_init() first\n");
		return;
	}
	/* Free the header list*/
	curl_slist_free_all(header);

	/* always cleanup */ 
	curl_easy_cleanup(curl_session);
	curl_session = NULL;
	curl_global_cleanup();
}
