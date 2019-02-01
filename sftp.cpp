#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <curl/curl.h>

#define SFTP_UPLOAD_OVERWRITE 0x01
#define SFTP_UPLOAD_RESUME 0x02

#define SFTP_ERROR -1
#define SFTP_ERROR_FAILED_TO_RESUME -2
#define SFTP_ERROR_FAILED_TO_READ_SOURCE -3


struct FtpFile {
  const char *filename;
  FILE *stream;
};

#define SFTP_MAX_REMOTE_ADDR 500
static char _remoteAddr[ SFTP_MAX_REMOTE_ADDR+1];

static int _sftpErrorCode = 0;


static size_t writefunc(void *buffer, size_t size, size_t nmemb, void *stream)
{
  struct FtpFile *out = (struct FtpFile *)stream;
  if(out && !out->stream) {
    /* open file for writing */
    out->stream = fopen(out->filename, "wb");
    if(!out->stream)
      return -1; /* failure, can't open file to write */
  }
  return fwrite(buffer, size, nmemb, out->stream);
}


/* read data to upload */
static size_t readfunc(void *ptr, size_t size, size_t nmemb, void *stream)
{
	FILE *f = (FILE *)stream;
	size_t n;

	if( ferror(f) ) {
		return CURL_READFUNC_ABORT;
	}

	n = fread(ptr, size, nmemb, f) * size;

	return n;
}


static int createRemoteAddr( char *fileName, char *directory, char *server, char *user, char *password ) 
{ 
  if( strlen(server) + strlen(user) + strlen(password) + strlen(directory) + strlen(fileName) + 7 >= SFTP_MAX_REMOTE_ADDR ) {
    return -1;
  }

  strcpy( _remoteAddr, "sftp://");
  strcat( _remoteAddr, user );
  strcat( _remoteAddr, ":" );
  strcat( _remoteAddr, password );
  strcat( _remoteAddr, "@" );
  strcat( _remoteAddr, server );
  strcat( _remoteAddr, directory );
  strcat( _remoteAddr, "/" );
  strcat( _remoteAddr, fileName );
  return 0;
}


/*
 * sftpGetRemoteFileSize returns the remote file size in byte; -1 on error
 */
static curl_off_t getRemoteFileSize(const char *i_remoteFile)
{
	CURLcode result = CURLE_GOT_NOTHING;
	curl_off_t remoteFileSizeByte = -1;
	CURL *curlHandlePtr = NULL;

	curlHandlePtr = curl_easy_init();
	curl_easy_setopt(curlHandlePtr, CURLOPT_VERBOSE, 0L);
	curl_easy_setopt(curlHandlePtr, CURLOPT_URL, i_remoteFile);
	curl_easy_setopt(curlHandlePtr, CURLOPT_NOPROGRESS, 1);
	curl_easy_setopt(curlHandlePtr, CURLOPT_NOBODY, 1);
	curl_easy_setopt(curlHandlePtr, CURLOPT_HEADER, 1);
	curl_easy_setopt(curlHandlePtr, CURLOPT_FILETIME, 1);

	result = curl_easy_perform(curlHandlePtr);
	if(CURLE_OK == result) {
		result = curl_easy_getinfo(curlHandlePtr,
															 CURLINFO_CONTENT_LENGTH_DOWNLOAD_T,
															 &remoteFileSizeByte);
		//printf("filesize: %" CURL_FORMAT_CURL_OFF_T "\n", remoteFileSizeByte);
	}
	curl_easy_cleanup(curlHandlePtr);

	return remoteFileSizeByte;
}


static int upload(CURL *curlHandle, char *remotepath, char *localpath, int flags)
{
	FILE *f = NULL;

  curl_off_t remoteFileSizeByte;
	if( flags & SFTP_UPLOAD_RESUME ) {
		remoteFileSizeByte = getRemoteFileSize(remotepath);
		if(remoteFileSizeByte == -1 && (flags & SFTP_UPLOAD_RESUME) ) {
			_sftpErrorCode = SFTP_ERROR_FAILED_TO_RESUME;
			return -1;
    }
	} else {
		remoteFileSizeByte = 0L;
	}

	f = fopen(localpath, "rb");
	if(!f) {
		//perror(NULL);
		_sftpErrorCode = SFTP_ERROR_FAILED_TO_READ_SOURCE;
		return -1;
	}

	curl_easy_setopt(curlHandle, CURLOPT_UPLOAD, 1L);
	curl_easy_setopt(curlHandle, CURLOPT_URL, remotepath);
	curl_easy_setopt(curlHandle, CURLOPT_READFUNCTION, readfunc);
	curl_easy_setopt(curlHandle, CURLOPT_READDATA, f);

#ifdef _WIN32
	_fseeki64(f, remoteFileSizeByte, SEEK_SET);
#else
	fseek(f, (long)remoteFileSizeByte, SEEK_SET);
#endif
	curl_easy_setopt(curlHandle, CURLOPT_APPEND, 1L);
	CURLcode result = curl_easy_perform(curlHandle);

	fclose(f);

	if(result != CURLE_OK) {
		fprintf(stderr, "%s\n", curl_easy_strerror(result));
		_sftpErrorCode = SFTP_ERROR;
		return -1;
	} else {
		return 0;
	}
}


static int download( CURL *curlHandle, char *remotepath, char *dstFileName ) {

  struct FtpFile ftpfile = {
    dstFileName,
    NULL
  };

  curl_easy_setopt(curlHandle, CURLOPT_URL, remotepath );
  /* Define our callback to get called when there's data to be written */
  curl_easy_setopt(curlHandle, CURLOPT_WRITEFUNCTION, writefunc);
  /* Set a pointer to our struct to pass to the callback */
  curl_easy_setopt(curlHandle, CURLOPT_WRITEDATA, &ftpfile);
  /* Switch on full protocol/debug output */
  curl_easy_setopt(curlHandle, CURLOPT_VERBOSE, 0L);

  CURLcode result = curl_easy_perform(curlHandle);
  if(result != CURLE_OK) {
    fprintf(stderr, "%s\n", curl_easy_strerror(result));
    return -1;
  }
  return 0;
}


int sftpUpload( char *srcFileName, char *dstFileName, char *dstDirectory, char *server, char *user, char *password ) 
{ 
  if( createRemoteAddr( dstFileName, dstDirectory, server, user, password ) == -1 ) {
    return -1;
  }
  fprintf( stderr,"REMOTE ADDRESS: %s\n",_remoteAddr);

  CURL *curlHandle = curl_easy_init();
  if( curlHandle == NULL ) {
    return -1;
  }

  int status = upload( curlHandle, _remoteAddr, srcFileName, SFTP_UPLOAD_OVERWRITE );
  
  curl_easy_cleanup(curlHandle);

  return status;
} 


int sftpDownload( char *dstFileName, char *srcFileName, char *srcDirectory, char *server, char *user, char *password ) 
{ 
  if( createRemoteAddr( srcFileName, srcDirectory, server, user, password ) == -1 ) {
    return -1;
  }
  fprintf( stderr,"REMOTE ADDRESS: %s\n",_remoteAddr);

  CURL *curlHandle = curl_easy_init();
  if( curlHandle == NULL ) {
    return -1;
  }

  int status = download( curlHandle, _remoteAddr, dstFileName );

  curl_easy_cleanup(curlHandle);

  return status;  
} 


int sftpInit() {
  if( curl_global_init(CURL_GLOBAL_ALL) != 0 ) {
    return -1;
  }
  return 0;
}


void sftpClose() {
    curl_global_cleanup();    
}


const char *_server = "u38989.ssh.masterhost.ru";
const char *_user = "u38989";
const char *_password = "amitin9sti";
const char *_dstDirectory = "/home/u38989";

int main( int argc, char **argv ) {

  if( argc < 2 ) {
    fprintf( stderr, "Argument is missing!\nUsage: sftp.exe <file-name>\n");
    exit(1);
  }

  if( sftpInit() == -1 ) {
    fprintf( stderr, "Error initializing SFTP!\nExiting...\n");
    exit(1);
  }

  int status = sftpUpload( argv[1], argv[1], (char *)_dstDirectory, (char *)_server, (char *)_user, (char *)_password );
  if( status == 0 ) {
    fprintf( stderr, "Uploaded Ok!\n");
  } else {
    fprintf( stderr, "Error uploading file!\n");    
  }

  if( argc > 2 ) {
    int status = sftpDownload( argv[2], argv[2], (char *)_dstDirectory, (char *)_server, (char *)_user, (char *)_password );
    if( status == 0 ) {
      fprintf( stderr, "Downloaded Ok!\n");
    } else {
      fprintf( stderr, "Error downloading file!\n");    
    }    
  }


  sftpClose();

  return 0;
}

