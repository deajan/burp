#include "../../burp.h"
#include "../../alloc.h"
#include "../../attribs.h"
#include "../../bfile.h"
#include "../../cntr.h"
#include "../../log.h"
#include "../../sbuf.h"
#include "../extrameta.h"

static char *meta_buffer=NULL;
static size_t meta_buffer_len=0;
static char *mp=NULL;

static void rabin_close_file_extrameta(struct sbuf *sb)
{
	free_w(&meta_buffer);
	meta_buffer_len=0;
	mp=NULL;
	sb->protocol2->bfd.mode=BF_CLOSED;
}

// Return -1 for error, 0 for could not get data, 1 for success.
static int rabin_open_file_extrameta(struct sbuf *sb, struct asfd *asfd,
	struct cntr *cntr)
{
	// Load all of the metadata into a buffer.
	rabin_close_file_extrameta(sb);
	if(get_extrameta(asfd, NULL,
		sb, &meta_buffer, &meta_buffer_len, cntr))
			return -1;
	if(!meta_buffer)
		return 0;
	mp=meta_buffer;
	sb->protocol2->bfd.mode=BF_READ;
	return 1;
}

ssize_t rabin_read_extrameta(struct sbuf *sb, char *buf, size_t bufsize)
{
	// Place bufsize of the meta buffer contents into buf.
	size_t to_read=meta_buffer_len;
	if(!meta_buffer_len)
		return 0;
	if(bufsize<meta_buffer_len)
		to_read=bufsize;
	memcpy(buf, meta_buffer, to_read);
	meta_buffer_len-=to_read;
	return (ssize_t)to_read;
}

// Return -1 for error, 0 for could not open file, 1 for success.
int rabin_open_file(struct sbuf *sb, struct asfd *asfd, struct cntr *cntr,
        struct conf **confs)
{
	BFILE *bfd=&sb->protocol2->bfd;
#ifdef HAVE_WIN32
	if(win32_lstat(sb->path.buf, &sb->statp, &sb->winattr))
#else
	if(lstat(sb->path.buf, &sb->statp))
#endif
	{
		// This file is no longer available.
		logw(asfd, cntr, "%s has vanished\n", sb->path.buf);
		return 0;
	}
	sb->compression=get_int(confs[OPT_COMPRESSION]);
	// Encryption not yet implemented in protocol2.
	//sb->protocol2->encryption=conf->protocol2->encryption_password?1:0;
	if(attribs_encode(sb)) return -1;
	if(sbuf_is_metadata(sb))
		return rabin_open_file_extrameta(sb, asfd, cntr);

	if(bfd->open_for_send(bfd, asfd,
		sb->path.buf, sb->winattr,
		get_int(confs[OPT_ATIME]), cntr, PROTO_2))
	{
		logw(asfd, get_cntr(confs),
			"Could not open %s\n", sb->path.buf);
		return 0;
	}
	return 1;
}

void rabin_close_file(struct sbuf *sb, struct asfd *asfd)
{
	BFILE *bfd;
	if(sbuf_is_metadata(sb))
	{
		rabin_close_file_extrameta(sb);
		return;
	}
	bfd=&sb->protocol2->bfd;
	bfd->close(bfd, asfd);
}

ssize_t rabin_read(struct sbuf *sb, char *buf, size_t bufsize)
{
	BFILE *bfd;
	if(sbuf_is_metadata(sb))
		return rabin_read_extrameta(sb, buf, bufsize);
	bfd=&sb->protocol2->bfd;
	return (ssize_t)bfd->read(bfd, buf, bufsize);
}