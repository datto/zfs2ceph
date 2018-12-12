/*
 * Copyright (C) 2018 Datto, Inc.
 *
 * Licensed under the GNU Lesser General Public License Version 2.1
 * Fedora-License-Identifier: LGPLv2+
 * SPDX-2.0-License-Identifier: LGPL-2.1+
 * SPDX-3.0-License-Identifier: LGPL-2.1-or-later
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */


#include "zfstypes.h"
#include "cephtypes.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

/**************************/
/****** IO FUNCTIONS ******/
/**************************/

#define read_header(pipe, drr) read_data(pipe, drr, sizeof(dmu_replay_record_t))
#define EOF_SENTINEL -1
#define MIN(x, y) (((x) < (y)) ? (x) : (y))


static int write_data(FILE *pipe, void *buf, uint64_t size) {
	int ret;
	size_t bytes;

	//write the data to the pipe
	bytes = fwrite(buf, 1, size, pipe);
	if(bytes != size){
		ret = ferror(pipe) ? errno : EIO;
		fprintf(stderr, "failed to write to pipe: %s\n", strerror(ret));
		goto error;
	}

	return 0;

error:
	fprintf(stderr, "failed to write data to pipe: %s\n", strerror(ret));
	return ret;
}

int read_data(FILE *pipe, void *buf, uint64_t size){
	int ret;
	size_t bytes;

	//read the data from the pipe, return EOF_SENTINEL to indicate EOF
	bytes = fread(buf, 1, size, pipe);
	if(bytes == 0 && feof(pipe)) return EOF_SENTINEL;
	else if(bytes != size){
		ret = ferror(pipe) ? errno : EIO;
		fprintf(stderr, "failed to read from pipe: %s\n", strerror(ret));
		goto error;
	}

	return 0;

error:
	fprintf(stderr, "failed to read data from pipe: %s\n", strerror(ret));
	return ret;
}

int read_skip(FILE *pipe, uint64_t size){
	int ret;
	uint64_t bytes_next, bytes_left = size;
	uint8_t buf[4096];

	while(bytes_left != 0){
		bytes_next = MIN(sizeof(buf), bytes_left);

		ret = read_data(pipe, buf, bytes_next);
		if(ret) goto error;

		bytes_left -= bytes_next;
	}

	return 0;

error:
	fprintf(stderr, "failed to skip ahead in pipe: %s\n", strerror(ret));
	return ret;
}

/***************************************/
/****** CEPH CONVERSION FUNCTIONS ******/
/***************************************/


static int write_start_header(FILE *pipe) {
	uint64_t length;

	length = strlen(RBD_EXPORT_BANNER);
	return write_data(pipe, RBD_EXPORT_BANNER, length);
}

static int write_snap(FILE *pipe, uint8_t tag, char *snap, uint32_t length) {
	int r;

	r = write_data(pipe, &tag, sizeof(uint8_t));
	if (r) goto out;

	r = write_data(pipe, &length, sizeof(uint32_t));
	if (r) goto out;

	r = write_data(pipe, snap, length);
	if (r) goto out;

out:
	return r;
}


static int write_fsnap(FILE *pipe, char *snap, uint32_t length) {
	return write_snap(pipe, RBD_DIFF_FROM_SNAP, snap, length);
}

static int write_tsnap(FILE *pipe, char *snap, uint32_t length) {
	return write_snap(pipe, RBD_DIFF_TO_SNAP, snap, length);
}

static int write_image_size(FILE *pipe, uint64_t size) {
	int r;
	uint8_t tag;

	tag = RBD_DIFF_IMAGE_SIZE;
	r = write_data(pipe, &tag, sizeof(uint8_t));
	if (r) {
		return r;
	}

	return write_data(pipe, &size, sizeof(uint64_t));
}

static int write_data_header(FILE *pipe, uint8_t tag, uint64_t offset, uint64_t length) {
	int r;
	r = write_data(pipe, &tag, sizeof(uint8_t));
	if (r) {
		return r;
	}
	
	r = write_data(pipe, &offset, sizeof(uint64_t));
	if (r) {
		return r;
	}

	return write_data(pipe, &length, sizeof(uint64_t));
}

static int write_block(FILE *pipe, uint64_t offset, uint64_t length, uint8_t *buf) {
	int r;

	r = write_data_header(pipe, RBD_DIFF_WRITE, offset, length);
	if (r) {
		return r;
	}

	return write_data(pipe, buf, length);
}

static int write_zeroes(FILE *pipe, uint64_t offset, uint64_t length) {
	return write_data_header(pipe, RBD_DIFF_ZERO, offset, length);
}

static int write_end_header(FILE *pipe) {
	uint32_t tag;

	tag = RBD_DIFF_END;
	return write_data(pipe, &tag, sizeof(uint32_t));
}

/***********************************/
/****** ZFS PARSING FUNCTIONS ******/
/***********************************/

static int read_next(FILE *pipe, dmu_replay_record_t *drr, uint8_t *buf){
	int ret;
	uint64_t data_len = 0;

	//read the header from the send file
	ret = read_header(pipe, drr);
	if(ret == EOF_SENTINEL) return ret;
	else if(ret){
		fprintf(stderr, "failed to read header from pipe: %s\n", strerror(ret));
		goto error;
	}

	switch(drr->drr_type){
	case DRR_BEGIN:
		break;
	case DRR_END:
		break;
	case DRR_OBJECT:
		data_len = P2ROUNDUP(drr->drr_u.drr_object.drr_bonuslen, 8);
		break;
	case DRR_FREEOBJECTS:
		break;
	case DRR_WRITE:
		data_len = drr->drr_u.drr_write.drr_length;
		break;
	case DRR_FREE:
		break;
	case DRR_WRITE_BYREF:
		break;
	case DRR_SPILL:
		data_len = drr->drr_u.drr_spill.drr_length;
		break;
	case DRR_WRITE_EMBEDDED:
		data_len = P2ROUNDUP(drr->drr_u.drr_write_embedded.drr_psize, 8);
		break;
	default:
		fprintf(stderr, "unrecognized record type encountered: %d\n", drr->drr_type);
		break;
	}

	//read any additional data into the buffer
	if(data_len > 0){
		ret = read_data(pipe, buf, data_len);
		if(ret) goto error;
	}

	return 0;

error:
	fprintf(stderr, "failed to read record from pipe: %s\n", strerror(ret));
	return ret;
}

static int zsend_convert(FILE *pipe, FILE *outfile, uint64_t image_size) {
	int ret;
	dmu_replay_record_t drr;
	uint8_t *buf = NULL;
	uint64_t offset, length, object;
	char to_snap_name[24];
	char from_snap_name[24];

	//allocate a buffer for post-header data
	buf = malloc(SPA_MAXBLOCKSIZE);
	if(!buf){
		ret = ENOMEM;
		goto error;
	}

	//read first header (should be type DRR_BEGIN)
	ret = read_header(pipe, &drr);
	if(ret) goto error;

	//confirm magic number
	if(drr.drr_u.drr_begin.drr_magic != DMU_BACKUP_MAGIC) {
		ret = EINVAL;
		fprintf(stderr, "invalid magic number from pipe\n");
		goto error;
	}

	// We only support zvol types
	if (drr.drr_u.drr_begin.drr_type != DMU_OST_ZVOL) {
		ret = EINVAL;
		fprintf(stderr, "invalid type, only zvols are supported\n");
		goto error;
	}

	//handle extra data that might be included after the DRR_BEGIN header
	if((DMU_GET_STREAM_HDRTYPE(drr.drr_u.drr_begin.drr_versioninfo) == DMU_COMPOUNDSTREAM) && drr.drr_payloadlen != 0){
		ret = read_skip(pipe, drr.drr_payloadlen);
		if(ret) goto error;
	}

	// Begin writing some ceph information headers
	ret = write_start_header(outfile);
	if (ret) goto error;

	// 0 GUID implies base send, which has no from snap
	if (drr.drr_u.drr_begin.drr_fromguid != 0) { 
		ret = snprintf(from_snap_name, 24, "%lu", drr.drr_u.drr_begin.drr_fromguid);
		if (ret < 0) {
			fprintf(stderr, "could not parse from snap guid\n");
			goto error;
		}
		
		ret = write_fsnap(outfile, from_snap_name, strlen(from_snap_name));
		if (ret) goto error;
	}

	ret = snprintf(to_snap_name, 24, "%lu", drr.drr_u.drr_begin.drr_toguid);
	if (ret < 0) {
		fprintf(stderr, "could not parse to snap guid\n");
		goto error;
	}

	ret = write_tsnap(outfile, to_snap_name, strlen(to_snap_name));
	if (ret) goto error;

	if (image_size != 0) {
		ret = write_image_size(outfile, image_size);
		if (ret) goto error;
	}

	//main processing loop
	while(!feof(pipe)){
		ret = read_next(pipe, &drr, buf);
		if(ret == EOF_SENTINEL) break;
		else if(ret) goto error;

		switch(drr.drr_type){
		//we only care about writes
		case DRR_WRITE:

			object = drr.drr_u.drr_free.drr_object;
			if (object != 1) continue;

			//get the offset and length from the header
			offset = drr.drr_u.drr_write.drr_offset;
			length = drr.drr_u.drr_write.drr_length;

			/*
			 * zfs send writes in blocks and relies on the file's bonus buffer from DRR_OBJECT
			 * to determine the actual size. This is hard to parse outside of zfs core, so we
			 * use the file size passed into us from stat instead.
			 */
			if(offset > image_size) continue;
			if(offset + length > image_size) length = image_size - offset;

			//write the zsend record to the output file
			ret = write_block(outfile, offset, length, buf);
			if(ret) goto error;

			break;
		case DRR_FREE:
			object = drr.drr_u.drr_free.drr_object;
			if (object != 1) continue;

			//get the offset and length from the header
			offset = drr.drr_u.drr_free.drr_offset;
			length = drr.drr_u.drr_free.drr_length;

			//length == DMU_OBJECT_END indicates that length should go to the end of the file
			if(offset > image_size) continue;
			if(length == DMU_OBJECT_END || offset + length > image_size) length = image_size - offset;

			//write the zsend record to the output file
			ret = write_zeroes(outfile, offset, length);
			if(ret) goto error;

			break;
		//ignore these and keep processing
		case DRR_OBJECT:
		case DRR_SPILL:
		case DRR_FREEOBJECTS:
		case DRR_END:
			break;
		/*
		 * DRR_BEGIN should never happen (we processed it above the loop).
		 * We don't currently handle DRR_WRITE_EMBEDDED or DRR_WRITE_BYREF, but we didn't
		 * ask for a dedup'ed or embedded stream when we opened the pipe so this should
		 * never happen.
		 */
		case DRR_BEGIN:
		case DRR_WRITE_BYREF:
		case DRR_WRITE_EMBEDDED:
		default:
			ret = EINVAL;
			fprintf(stderr, "unexpected record type %d encountered\n", drr.drr_type);
			goto error;
		}
	}

	free(buf);

	ret = write_end_header(outfile);
	if (ret) goto error;

	return 0;

error:
	fprintf(stderr, "parse failed: %s\n", strerror(ret));
	if(buf) free(buf);

	return ret;
}


/****************************************/
/****** Program Entry Coordination ******/
/****************************************/

static void print_usage(int exitcode){
	fprintf(stderr, "Usage:\n");
	fprintf(stderr, "\tzfs2ceph -s <image size>\n");
	fprintf(stderr, "Note:\n");
	fprintf(stderr, "\t<image_size> is specified in bytes\n");
	exit(exitcode);
}

int main(int argc, char **argv) {
	uint64_t image_size = 0;
	char c;

	while((c = getopt(argc, argv, "s:")) != -1){
		switch(c){
		case 's':
			image_size = atol(optarg);
			break;
		default:
			print_usage(EINVAL);
		}
	}

	if (image_size == 0) {
		print_usage(EINVAL);
	}

	if (isatty(fileno(stdout))) {
		fprintf(stderr, "%s does not support output to tty\n", argv[0]);
		return EINVAL;
	}

	return zsend_convert(stdin, stdout, image_size);
}

