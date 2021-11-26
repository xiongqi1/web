#include <stdio.h>
#include <curl/curl.h>
#include "rdb_operations.h"


static size_t write_data(void *ptr, size_t size, size_t nmemb, void *stream)
{
	return nmemb;
}

int main(void)
{
  CURL *curl;
  CURLcode res;
   //rdb_open_db();

  curl = curl_easy_init();
  if(curl) {
#if 0
  	// Internal CURL progressmeter must be disabled if we provide our own callback
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1);

    curl_easy_setopt(curl, CURLOPT_USERAGENT, "libcurl-agent/1.0");
    //don't verify peer against cert
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);
    //don't verify host against cert
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0);
    //disable signals to use with threads
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1);
	curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, 60);
	curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, 60);

  	  /* Define our callback to get called when there's data to be written */
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);

    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)NULL);// we pass our 'chunk' struct to the callback function
#endif

    curl_easy_setopt(curl, CURLOPT_URL, "98.137.149.56/index.html");
    res = curl_easy_perform(curl);

    /* always cleanup */
    curl_easy_cleanup(curl);
  }
  return 0;
}
