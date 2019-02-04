// File transfer via SSH
#ifndef __SFTP_H
#define __SFTP_H

#define SFTP_UPLOAD_OVERWRITE 0x01
#define SFTP_UPLOAD_RESUME 0x02

#define SFTP_ERROR_Ok 0
#define SFTP_ERROR -1
#define SFTP_ERROR_FAILED_TO_READ_SOURCE -2
#define SFTP_ERROR_FAILED_TO_WRITE_DEST -3
#define SFTP_ERROR_FAILED_TO_RESUME -4

// Uploads a file to a server. Returns negative value if failed, 0 if ok.
int sftpUpload( char *srcFileName,					// A file to transfer to a server
	char *dstFileName, 								// A name for the file when it is stored at the server
	char *dstDirectory, 							// A directory to transfer the file into. For Linux servers starts with '/'
  	char *server, char *user, char *password );		// Connection requisites: server, login, password

// Downloads a file from a server. Returns negative value if failed, 0 if ok.
int sftpDownload( char *dstFileName, 				// A file name to save the downloaded file under 
	char *srcFileName, 								// A file to download
	char *srcDirectory, 							// The directory to find the file at the server
  	char *server, char *user, char *password ); 	// Connection requisites: server, login, password

// Test is a file exists at a server. "1" - yes, "0" - no, "-1" - error.
int sftpTest( char *fileName, 					// A file name to test
	char *directory, 							// A server directory to test in
  	char *server, char *user, char *password,	// Connection requisites: server, login, password
  	unsigned long int *size );					// If not NULL receives the size of file in bytes

int sftpInit( void ); // Must be called before doing anything else...

void sftpClose( void ); // Must be called when all transfers are finished...

int sftpGetLastError( int *sftpErrorCode, 		// 
	int *curlErrorCode, 						//
	char *curlErrorText );						//

#endif