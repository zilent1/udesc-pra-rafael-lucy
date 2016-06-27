#include <stdio.h>
#include <unistd.h>
#include <curl/curl.h>
#include <dirent.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <sys/stat.h>

#define CFISH_USE_SHORT_NAMES
#define LUCY_USE_SHORT_NAMES
#include "Clownfish/String.h"
#include "Lucy/Simple.h"
#include "Lucy/Document/Doc.h"

const char path_to_index[] = "lucy-idx";
const char uscon_source[]  = "lucy-sites/";
char *outFile;

Doc* S_parse_file(const char *filename, const char *url);
unsigned char S_ends_with(char *str, const char *ext);

size_t write_callback(char *ptr, size_t size, size_t nmemb, void *userdata); 
unsigned char curl_fetch(char *url);
void lucy_index(const char *url);
long strpos(char *haystack, char *needle);
off_t fsize(const char *filename);

int main(int argc, char **argv) {
    int i;
    if (argc < 2) {
        printf("Usage: %s http://www.example.com\n",argv[0]);
        return 0;
    }
    outFile = (char*) malloc(sizeof(char) * 255);
    outFile[0] = 0;
    strcat(outFile,uscon_source);
    for (i=7;i<strlen(argv[1]);i++) {
	if (argv[1][i] == '.') {
            outFile[strlen(outFile)] = '_';
        } else if (argv[1][i] == '/') {
            outFile[strlen(outFile)] = '-';
        } else {
            outFile[strlen(outFile)] = argv[1][i];
        }
    }
    strcat(outFile,".htm");
    printf("Fetching %s\n",argv[1]);
    curl_fetch(argv[1]);
    printf("Indexing...\n");
    lucy_index(argv[1]);
    free(outFile);

    return 0;
}

long strpos(char *haystack, char *needle) {
   char *p = strstr(haystack, needle);
   if (p)
      return p - haystack;
   return -1;
}

unsigned char curl_fetch(char *url) {
    CURL *curl;
    CURLcode res;
    curl = curl_easy_init();
    if(curl) {
	unlink(outFile);
        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
        res = curl_easy_perform(curl);
        if(res != CURLE_OK) {
            fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
        }
        curl_easy_cleanup(curl);
        return 1;
    } else {
        fprintf(stderr, "curl_easy_init() failed!\n");
        return 0;
    }

}

size_t write_callback(char *ptr, size_t size, size_t nmemb, void *userdata) {
    FILE *fp;
    size_t realsize = size * nmemb;
    
    printf("Writing to [%s]\n",outFile);
    fp = fopen(outFile,"a+");
    fwrite(ptr, size, nmemb, fp);
    fclose(fp);
    
    return realsize;
}

void lucy_index(const char *url) {
    lucy_bootstrap_parcel();

    String *folder   = Str_newf("%s", path_to_index);
    String *language = Str_newf("pt");
    Simple *lucy     = Simple_new((Obj*)folder, language);

    printf("Parsing: %s\n",outFile);
    Doc *doc = S_parse_file(outFile, url);
    Simple_Add_Doc(lucy, doc); // ta-da!

    DECREF(doc);
    DECREF(lucy);
    DECREF(language);
    DECREF(folder);
}

off_t fsize(const char *filename) {
    struct stat st; 

    if (stat(filename, &st) == 0)
        return st.st_size;

    return -1; 
}

Doc* S_parse_file(const char *filename, const char *url) {
    size_t bytes = strlen(uscon_source) + 1 + strlen(filename) + 1;
    char *path = (char*)malloc(bytes);
    long i;
    off_t fileSize = fsize(filename);
    path[0] = '\0';
    strcat(path, filename);

    FILE *stream = fopen(path, "r");
    if (stream == NULL) {
        perror(path);
        exit(1);
    }

    char *title = (char*) malloc(sizeof(char) * 512);
    title[0] = 0;
    char *bodytext = malloc((sizeof(char) * fileSize) + 2) ;
    size_t bytesRead = fread(bodytext, sizeof(char), (sizeof(char) * fileSize) + 1, stream);
    bodytext[bytesRead] = '\0';
    fclose(stream);

    long titleStart = strpos(bodytext,"<title>");
    long titleEnd = strpos(bodytext,"</title>");
    for (i=7;i<titleEnd-titleStart;i++) {
        title[i-7] = bodytext[titleStart+i];
        title[i-6] = 0;
    }
/*
    printf("Title: {%s}\n",title);
    printf("Body: {%s}\n",bodytext);
*/
    Doc *doc = Doc_new(NULL, 0);

    {
        String *field = Str_newf("title");
        String *value = Str_new_from_utf8(title, strlen(title));
        Doc_Store(doc, field, (Obj*)value);
        DECREF(field);
        DECREF(value);
    }

    {
        String *field = Str_newf("content");
        String *value = Str_new_from_utf8(bodytext, strlen(bodytext));
        Doc_Store(doc, field, (Obj*)value);
        DECREF(field);
        DECREF(value);
    }

    {
        String *field = Str_newf("url");
        String *value = Str_new_from_utf8(url, strlen(url));
        Doc_Store(doc, field, (Obj*)value);
        DECREF(field);
        DECREF(value);
    }

    free(bodytext);
    free(title);
    free(path);
    return doc;
}

unsigned char S_ends_with(char *str, const char *ext) {
    char *dot = strrchr(str, '.');
    if (dot && !strcmp(dot, ext)) {
        return 1;
    }
    return 0;
}

