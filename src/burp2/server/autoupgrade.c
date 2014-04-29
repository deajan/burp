#include "include.h"

// Return -1 on error or success, 0 to continue normally.
int autoupgrade_server(struct async **as,
	long ser_ver, long cli_ver, const char *os, struct conf *conf)
{
	int ret=-1;
	char *path=NULL;
	char *base_path=NULL;
	char *script_path=NULL;
	char *package_path=NULL;
	char *script_path_top=NULL;
	char *script_path_specific=NULL;
	struct stat stats;
	struct stat statp;

	if(!conf->autoupgrade_dir)
	{
		// Autoupgrades not turned on on the server.
		ret=0;
		(*as)->write_str(*as, CMD_GEN, "do not autoupgrade");
		goto end;
	}

	if(cli_ver>=ser_ver)
	{
		// No need to upgrade - client is same version as server,
		// or newer.
		ret=0;
		(*as)->write_str(*as, CMD_GEN, "do not autoupgrade");
		goto end;
	}

	if(!(base_path=prepend_s(conf->autoupgrade_dir, os))
	  || !(path=prepend_s(base_path, VERSION))
	  || !(script_path_top=prepend_s(base_path, "script"))
	  || !(script_path_specific=prepend_s(path, "script"))
	  || !(package_path=prepend_s(path, "package")))
	{
		(*as)->write_str(*as, CMD_GEN, "do not autoupgrade");
		goto end;
	}

	if(!stat(script_path_specific, &stats))
		script_path=script_path_specific;
	else if(!stat(script_path_top, &stats))
		script_path=script_path_top;
	else
	{
		logp("Want to autoupgrade client, but no file at:\n");
		logp("%s\n", script_path_top);
		logp("or:\n");
		logp("%s\n", script_path_specific);
		ret=0; // this is probably OK
		(*as)->write_str(*as, CMD_GEN, "do not autoupgrade");
		goto end;
	}
	if(stat(package_path, &statp))
	{
		logp("Want to autoupgrade client, but no file available at:\n");
		logp("%s\n", package_path);
		ret=0; // this is probably OK
		(*as)->write_str(*as, CMD_GEN, "do not autoupgrade");
		goto end;
	}

	if(!S_ISREG(stats.st_mode))
	{
		logp("%s is not a regular file\n", script_path);
		(*as)->write_str(*as, CMD_GEN, "do not autoupgrade");
		goto end;
	}
	if(!S_ISREG(statp.st_mode))
	{
		logp("%s is not a regular file\n", package_path);
		(*as)->write_str(*as, CMD_GEN, "do not autoupgrade");
		goto end;
	}

	if((*as)->write_str(*as, CMD_GEN, "autoupgrade ok"))
		goto end;

	if(send_a_file(*as, script_path, conf))
	{
		logp("Problem sending %s\n", script_path);
		goto end;
	}
	if(send_a_file(*as, package_path, conf))
	{
		logp("Problem sending %s\n", package_path);
		goto end;
	}
	ret=0;
	/* Clients currently exit after forking, so exit ourselves. */
	logp("Expecting client to upgrade - now exiting\n");
	async_free(as);
	exit(0);
end:
	if(path) free(path);
	if(base_path) free(base_path);
	if(script_path_specific) free(script_path_specific);
	if(script_path_top) free(script_path_top);
	if(package_path) free(package_path);
	return ret;
}