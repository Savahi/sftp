// A demo for sftp file transfer
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include "sftp.h"

char *_server = "u38989.ssh.masterhost.ru";
char *_user = "u38989";
char *_password = "amitin9sti";
char *_dstDirectory = "/home/u38989";

int main( int argc, char **argv ) {
  int status;
  long unsigned int size;

  if( argc < 4 ) {
    fprintf( stderr, "Argument is missing!\nUsage: sftp.exe <file-to-test-at-server> <file-to-upload> <file-to-download>\n");
    exit(1);
  }

  if( sftpInit() == -1 ) { // A must...
    fprintf( stderr, "Error initializing SFTP!\nExiting...\n");
    exit(1);
  }

  // Testing if a file exists at the server...
  status = sftpTest( argv[1], _dstDirectory, _server, _user, _password, &size );
  if( status == 1 ) {
    fprintf( stderr, "The file %s exists! (length=%lu)\n", argv[1], size );
  } else if( status == 0 ) {
    fprintf( stderr, "The file %s does not exist!\n", argv[1] );
  } else {
    int error;
    sftpGetLastError( NULL, &error, NULL ); // Getting the error code.
    if( error == CURLE_REMOTE_FILE_NOT_FOUND ) {
      fprintf( stderr, "The file not found!\n");
    } else if( error == CURLE_COULDNT_RESOLVE_HOST ) {
      fprintf( stderr, "Server not found!\n");      
    } else if( error == CURLE_REMOTE_ACCESS_DENIED ) {
      fprintf( stderr, "Access denied!\n");      
    } else {
      fprintf( stderr, "Error %d occured!\n", error );      
    }
  }

  // Uploading...
  status = sftpUpload( argv[2], argv[2], _dstDirectory, _server, _user, _password );
  if( status == 0 ) {
    fprintf( stderr, "Uploaded Ok!\n");
  } else {
    fprintf( stderr, "Error uploading file!\n");    
  }

  // Downloading
  status = sftpDownload( argv[3], argv[3], _dstDirectory, _server, _user, _password );
  if( status == 0 ) {
    fprintf( stderr, "Downloaded Ok!\n");
  } else {
    fprintf( stderr, "Error downloading file!\n");    
  }    

  sftpClose();

  return 0;
}
