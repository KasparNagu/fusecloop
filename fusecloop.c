/*
 * File:    fusecloop.c
 * Purpose: cloop for FUSE.
 * Copyright (c) 2007 Vitaly "_Vi" Shukela
 * Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>
 *
 * Contact Email: public_vi@tut.by
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 */

/*
    fusecloop: Knoppix's Compressed Loopback for FUSE
    Based on code of cloop's extract_compressed_fs.c and FUSE examples.

    gcc -Wall `pkg-config fuse --cflags --libs` -lz cloopreader.c fusecloop.c -o fusecloop
    This file depends on:
	cloopreader.c
	common_header.h
	debug.h
	cloopreader.h
*/
#define FUSE_USE_VERSION 26
#define _XOPEN_SOURCE 500

#include <fuse.h>
#include <time.h>
#include <stdio.h>  // for fprintf
#include <malloc.h> // for malloc

const char* version=
#include "strver"
;

#include "debug.h"

#include "common_header.h"
#include "cloopreader.h"


struct stat stb;

struct cloop_data cd;

static int fusecloop_getattr(const char *path, struct stat *stbuf)
{
    bfuncinfo("path=%s",path);

    if(strcmp(path, "/") != 0)
        return -ENOENT;
	
	
    memcpy(stbuf,&stb,sizeof stb);
    stbuf->st_mode&=~0222;
    stbuf->st_size = cd.blocksize * cd.numblocks;
    /*
    stbuf->st_mode = S_IFREG | 0444;
    stbuf->st_nlink = 1;
    stbuf->st_uid = getuid();
    stbuf->st_gid = getgid();
    stbuf->st_size = fnamelen;
    stbuf->st_blocks = 0;
    stbuf->st_atime = stbuf->st_mtime = stbuf->st_ctime = time(NULL);
    */    

    return 0;
}

static int fusecloop_truncate(const char *path, off_t size)
{
    (void) size;

    bfuncinfo("path=%s size=0x%lx",path,(ulong)size);

    if(strcmp(path, "/") != 0)
        return -ENOENT;

    return -EPERM;
}

static int fusecloop_open(const char *path, struct fuse_file_info *fi)
{
    (void) fi;
    bfuncinfo("path=%s",path);


    if(strcmp(path, "/") != 0)
        return -ENOENT;

    return 0;
}

int busy=0;

static int fusecloop_read(const char *path, char *buf, size_t size,
                     off_t offset, struct fuse_file_info *fi)
{
    if(busy){
	/* This is just for debugging (checking if we need reentrancy)
	 * It seems we don't when using direct_io flag and do if not.
	 */
	bprintf("***FAILURE***\n");
    }
    busy=1;
    int res;
    bfuncinfo("path=%s buf=0x%lx size=0x%lx offset=0x%lx sizeof(off_t)=%d",
	    path,(ulong)buf,(ulong)size,(ulong)offset,sizeof(off_t));

    if(strcmp(path, "/") != 0)
        return -ENOENT;

    if(fi->flags & O_WRONLY || fi->flags & O_RDWR)
    	return -EPERM;

    
    res = cloop_read(&cd,offset,buf,size);/*pread(fd, buf, size, offset)*/;
    if (res == -1)
        res = -errno;

    bprintf("returning %d from fusecloop_read\n",res);
    busy=0;

    return res;
    
}


static int fusecloop_write(const char *path, const char *buf, size_t size,
                     off_t offset, struct fuse_file_info *fi)
{
    bfuncinfo("path=%s buf=0x%lx size=0x%lx offset=0x%lx",
	    path,(ulong)buf,(ulong)size,(ulong)offset);
    (void) buf;
    (void) offset;
    (void) fi;

    if(strcmp(path, "/") != 0)
        return -ENOENT;

    return -EPERM;
}

static int fusecloop_utimens(const char *path, const struct timespec ts[2]){
    bfuncinfo("path=%s",path);

    if(strcmp(path, "/") != 0)
        return -ENOENT;

    bprintf("Silently ignoring utimens\n");

    return 0;
}

static struct fuse_operations fusecloop_oper = {
    .getattr	= fusecloop_getattr,
    .truncate	= fusecloop_truncate,
    .open	= fusecloop_open,
    .read	= fusecloop_read,
    .write	= fusecloop_write,   
    .utimens    = fusecloop_utimens,
};

int main(int argc, char *argv[])
{
    int fd,ret,i;
    char* argv2[argc-1+2];  // File name removed, "-o nonempty,direct_io" added

    if(argc<3){
	fprintf(stderr,"fusecloop version %s alpha. Coded by _Vi. GPL.\n",
		version);
	fprintf(stderr,"Usage: %s file_or_device mountpoint_file\n",argv[0]);
	return 1;
    }
    dbg_init(argc,argv);
    bfuncinfo("");

    OP(lstat(argv[1],&stb));

    fd=OP(open(argv[1],O_RDONLY));

    cloop_init(&cd, fd);

    argv2[0]=argv[0];
    for(i=2;i<argc;++i)argv2[i-1]=argv[i];
    argv2[argc-1]="-o";
    argv2[argc+0]="nonempty,direct_io";
    argv2[argc+1]=0;

    ret=fuse_main(argc-1+2, argv2, &fusecloop_oper, NULL);

    OP(close(fd));

    return ret;
}