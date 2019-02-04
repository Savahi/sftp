#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include "sftp.h"

struct FtpFile {
    char *filename;
    FILE *stream;
};

#define SFTP_MAX_REMOTE_ADDR 500
static char _remoteAddr[ SFTP_MAX_REMOTE_ADDR+1];

static long int _sftpErrorCode = 0;
static CURLcode _curlErrorCode = CURLE_OK;
// char _curlErrorBuffer[CURL_ERROR_SIZE+1];

static long int _timeOut = -1L;

static size_t writefunc(void *buffer, size_t size, size_t nmemb, void *stream)
{
    struct FtpFile *out = (struct FtpFile *)stream;
    if(out && !out->stream) {
        /* open file for writing */
        out->stream = fopen(out->filename, "wb");
        if(!out->stream)
            _sftpErrorCode = SFTP_ERROR_FAILED_TO_WRITE_DEST;
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


static int createRemotePath( char *fileName, char *directory, 
    char *server, char *user, char *password ) 
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
static curl_off_t getRemoteFileSize(char *remoteFile)
{
	curl_off_t remoteFileSizeByte = -1;
	CURL *curl = NULL;

	curl = curl_easy_init();
	curl_easy_setopt(curl, CURLOPT_VERBOSE, 0L);
	curl_easy_setopt(curl, CURLOPT_URL, remoteFile);
	curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1);
	curl_easy_setopt(curl, CURLOPT_NOBODY, 1);
	curl_easy_setopt(curl, CURLOPT_HEADER, 1);
	curl_easy_setopt(curl, CURLOPT_FILETIME, 1);
    
    if( _timeOut > 0 ) {
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, _timeOut);
    }
	_curlErrorCode = curl_easy_perform(curl);
	if(_curlErrorCode == CURLE_OK) {
		_curlErrorCode = curl_easy_getinfo( curl, CURLINFO_CONTENT_LENGTH_DOWNLOAD_T, &remoteFileSizeByte );
	}

	curl_easy_cleanup(curl);

	return remoteFileSizeByte;
}


static int upload(CURL *curl, char *remotePath, char *srcFileName, int flags)
{
	FILE *f = NULL;

    curl_off_t remoteFileSizeByte;
	if( flags & SFTP_UPLOAD_RESUME ) {
		remoteFileSizeByte = getRemoteFileSize(remotePath);
		if( remoteFileSizeByte == -1 && (flags & SFTP_UPLOAD_RESUME) ) {
			_sftpErrorCode = SFTP_ERROR_FAILED_TO_RESUME;
			return -1;
        }
	} else {
		remoteFileSizeByte = 0L;
	}

	f = fopen(srcFileName, "rb");
	if(!f) {
		_sftpErrorCode = SFTP_ERROR_FAILED_TO_READ_SOURCE;
		return -1;
	}

	curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
	curl_easy_setopt(curl, CURLOPT_URL, remotePath);
	curl_easy_setopt(curl, CURLOPT_READFUNCTION, readfunc);
	curl_easy_setopt(curl, CURLOPT_READDATA, f);

#ifdef _WIN32
	_fseeki64(f, remoteFileSizeByte, SEEK_SET);
#else
	fseek(f, (long)remoteFileSizeByte, SEEK_SET);
#endif
	curl_easy_setopt(curl, CURLOPT_APPEND, 1L);

    if( _timeOut > 0 ) {
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, _timeOut);
    }
	_curlErrorCode = curl_easy_perform(curl);

	fclose(f);

	if(_curlErrorCode != CURLE_OK) {
        _sftpErrorCode = SFTP_ERROR;
		return -1;
	} else {
		return 0;
	}
}


static int download( CURL *curl, char *remotePath, char *dstFileName ) {
    struct FtpFile ftpfile = {
        dstFileName,
        NULL
    };

    curl_easy_setopt(curl, CURLOPT_URL, remotePath );
    /* Define our callback to get called when there's data to be written */
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writefunc);
    /* Set a pointer to our struct to pass to the callback */
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &ftpfile);
    /* Switch on full protocol/debug output */
    curl_easy_setopt(curl, CURLOPT_VERBOSE, 0L);

    if( _timeOut > 0 ) {
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, _timeOut);
    }
    _curlErrorCode = curl_easy_perform(curl);
    if(_curlErrorCode != CURLE_OK) {
        return -1;
    }
    return 0;
}

// Test is a file exists at a server
int sftpTest( char *fileName,                                     // A file name to test
    char *directory,                                                            // A server directory to test in
    char *server, char *user, char *password,         // Connection requisites: server, login, password
    unsigned long int *size )
{
    _sftpErrorCode = 0;
    _curlErrorCode = CURLE_OK;

    if( createRemotePath( fileName, directory, server, user, password ) == -1 ) {
        return -1;
    }
 
    CURL *curl = curl_easy_init();
    if( curl == NULL ) {
        return -1;
    }

    curl_off_t remoteFileSizeByte = getRemoteFileSize(_remoteAddr);
    if( remoteFileSizeByte == -1 ) {
        if( _curlErrorCode == CURLE_REMOTE_FILE_NOT_FOUND ) {
            return 0;
        } else {
            return -1;
        }
    }

    if( size != NULL ) {
        *size =    remoteFileSizeByte;
    }

    return 1;
}


int sftpUpload( char *srcFileName, char *dstFileName, char *dstDirectory, 
    char *server, char *user, char *password ) 
{ 
    _sftpErrorCode = 0;
    _curlErrorCode = CURLE_OK;

    if( createRemotePath( dstFileName, dstDirectory, server, user, password ) == -1 ) {
        return -1;
    }
    //fprintf( stderr, "REMOTE ADDRESS: %s\n", _remoteAddr );

    CURL *curl = curl_easy_init();
    if( curl == NULL ) {
        return -1;
    }

    int status = upload( curl, _remoteAddr, srcFileName, SFTP_UPLOAD_OVERWRITE );
    
    curl_easy_cleanup(curl);

    return status;
} 


int sftpDownload( char *dstFileName, char *srcFileName, char *srcDirectory, 
    char *server, char *user, char *password ) 
{ 
    _sftpErrorCode=0;
    _curlErrorCode = CURLE_OK;

    if( createRemotePath( srcFileName, srcDirectory, server, user, password ) == -1 ) {
        return -1;
    }
    //fprintf( stderr, "REMOTE ADDRESS: %s\n", _remoteAddr );

    CURL *curl = curl_easy_init();
    if( curl == NULL ) {
        return -1;
    }

    int status = download( curl, _remoteAddr, dstFileName );

    curl_easy_cleanup(curl);

    return status;    
} 


void sftpSetTimeOut( unsigned long int timeOut ) {
    _sftpErrorCode = 0;
    _timeOut = timeOut;
}


int sftpInit( void ) {
    _sftpErrorCode = 0;
    _curlErrorCode = CURLE_OK;

    if( curl_global_init(CURL_GLOBAL_ALL) != 0 ) {
        return -1;
    }
    return 0;
}


void sftpClose( void ) {
    _sftpErrorCode = 0;
    _curlErrorCode = CURLE_OK;

    curl_global_cleanup();        
}


int sftpGetLastError( int *sftpErrorCode, int *curlErrorCode, char *curlErrorText ) {
    if( sftpErrorCode != NULL ) {
        *sftpErrorCode = _sftpErrorCode;
    }
    if( curlErrorCode != NULL ) {
        *curlErrorCode = _curlErrorCode;
    }
    if( curlErrorCode != NULL ) {
        curlErrorText = (char *)curl_easy_strerror(_curlErrorCode);
    }
}