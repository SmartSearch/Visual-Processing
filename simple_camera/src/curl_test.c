/***************************************************************************
*
* author: Nikos Katsarakis nkat_at_ait_dot_edu_dot_gr
*
* This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY
* KIND, either express or implied.
*
***************************************************************************/ 


#include "curl_helper.h"
#include "time_funcs.h"

/* Set to 1 to print messages for successfull data inserts, 2 to print all debug info */
int errorlevel=0;

int createMessage(char *buff, double intensity, double frame_difference)
{
	const char direct_interface_sample[]="{\n"
		"\t\"_id\" : \"%s\",\n"
		"\t\"timestamp\" : %lld,\n"
		"\t\"data\" : {\n"
		"\t\t\"time\" : \"%s\",\n"
		"\t\t\"camera\" : {\n"
		"\t\t\t\"@ID\" : \"my_webcam\",\n"
		"\t\t\t\"mean_intensity\" : %.4llf\n"
		"\t\t},\n"
		"\t\t\"visual_processing\" : {\n"
		"\t\t\t\"@ID\" : \"my_webcam_processing\",\n"
		"\t\t\t\"frame_difference\" : %.4llf\n"
		"\t\t}\n"
		"\t}\n"
		"}\n";

	char _id[100]=""; /* Document ID */
	long long timestamp; /* Timestamp for data */

	timestamp = getMillis();
	
	/* Set the document ID to the xml dateTime string */
	millis2string(timestamp, _id, 100);
	/* Also set the time attribute to the xml dateTime string */
	return sprintf(buff,direct_interface_sample,_id,timestamp,_id,intensity,frame_difference);
}

int main(void)
{
	double intensity=134;
	double frame_difference=0.19;

	/* Memory storage for the data to send */
	char tmpdata[5000];
	int datalen;

	int i;

	if (curl_init("http://dusk.ait.gr/couchdb/camerafeed")<0)
	{
		fprintf(stderr,"Could not initialise curl");
		return -1;
	}

	curl_set_debug_level(errorlevel);

	for (i=0;i<1000;i++)
	{
		datalen = createMessage(tmpdata,intensity,frame_difference);
		if (errorlevel==0)
		{
			/* Print progress */
			printf("\rSending message %3d/1000", i);
		}
		else if (errorlevel>1) printf("len:%d, data:%s\n", datalen, tmpdata);

		if (curl_send(tmpdata, datalen)!=CURLE_OK)
		{
			int tmp = curl_show_error();
			/* Stop at the first bad formed request */
			if (tmp == HTTP_BAD_REQUEST)
			{
				printf("\nData was: \n%s\n", tmpdata);
				if (i==307) printf("Stopping at message 307 is intended behaviour\n", tmpdata);
				break;
			}
		}
		else
			curl_show_warning();

		frame_difference*=10;
		intensity*=10;
	}

	curl_cleanup();
	
	printf("Press enter to exit\n");
	getchar();
	return 0;
}
