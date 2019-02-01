#include <libssh/libssh.h>
#include <stdlib.h>
#include <stdio.h>

int sftp_helloworld(ssh_session session);

int main()
{
	ssh_session my_ssh_session = ssh_new();
	if (my_ssh_session == NULL) {
		fprintf( stderr, "Error creating session!\nExiting...");
		exit(-1);
	}

	ssh_free(my_ssh_session);
}

/*
int sftp_helloworld(ssh_session session)
{
	sftp_session sftp;
	int rc;
	sftp = sftp_new(session);
	if (sftp == NULL)
	{
		fprintf(stderr, "Error allocating SFTP session: %s\n",
						ssh_get_error(session));
		return SSH_ERROR;
	}
	rc = sftp_init(sftp);
	if (rc != SSH_OK)
	{
		fprintf(stderr, "Error initializing SFTP session: %s.\n",
						sftp_get_error(sftp));
		sftp_free(sftp);
		return rc;
	}
	//...
	sftp_free(sftp);
	return SSH_OK;
}

*/