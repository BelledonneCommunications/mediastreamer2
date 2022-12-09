/*
 * Copyright (c) 2010-2022 Belledonne Communications SARL.
 *
 * This file is part of mediastreamer2 
 * (see https://gitlab.linphone.org/BC/public/mediastreamer2).
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "mediastreamer2/msasync.h"
#include "asyncrw.h"
#include "mediastreamer2/msqueue.h"


struct _MSAsyncReader{
	MSWorkerThread *wth;
	ms_mutex_t mutex;
	MSBufferizer buf;
	bctbx_vfs_file_t *fp;
	int ntasks_pending;
	size_t blocksize;
	off_t seekoff;
	int moving;
	bool_t eof;
};

static void async_reader_fill(void *data);

MSAsyncReader *ms_async_reader_new(bctbx_vfs_file_t *fp){
	MSAsyncReader *obj = ms_new0(MSAsyncReader,1);
	ms_mutex_init(&obj->mutex, NULL);
	ms_bufferizer_init(&obj->buf);
	obj->fp = fp;
	obj->wth = ms_worker_thread_new("MSAsyncReader");
#ifndef WIN32
	obj->blocksize = getpagesize();
#else
	obj->blocksize = 4096;
#endif
	/*immediately start filling the reader */
	obj->ntasks_pending++;
	ms_worker_thread_add_task(obj->wth, async_reader_fill, obj);
	return obj;
}

void ms_async_reader_destroy(MSAsyncReader *obj){
	ms_worker_thread_destroy(obj->wth, FALSE);
	ms_mutex_destroy(&obj->mutex);
	ms_bufferizer_flush(&obj->buf);
	ms_free(obj);
}

static void async_reader_fill(void *data){
	MSAsyncReader *obj = (MSAsyncReader*) data;
	mblk_t *m = allocb(obj->blocksize, 0);

	int err = (int)bctbx_file_read2(obj->fp, m->b_wptr, obj->blocksize);
	ms_mutex_lock(&obj->mutex);
	if (err >= 0){
		if (err > 0){
			m->b_wptr += err;
			ms_bufferizer_put(&obj->buf, m);
		}else freemsg(m);
		if ((size_t)err < obj->blocksize){
			obj->eof = TRUE;
		}
	}else if (err == BCTBX_VFS_ERROR){
		ms_error("async_reader_fill(): %s", strerror(errno));
		obj->eof = TRUE; /*what to do then ?*/
	}
	obj->ntasks_pending--;
	ms_mutex_unlock(&obj->mutex);
}

int ms_async_reader_read(MSAsyncReader *obj, uint8_t *buf, size_t size){
	int err;
	size_t avail;
	
	ms_mutex_lock(&obj->mutex);
	if (obj->moving){
		err = -BCTBX_EWOULDBLOCK;
		goto end;
	}
	avail = ms_bufferizer_get_avail(&obj->buf);
	if (avail < size && obj->ntasks_pending){
		err = -BCTBX_EWOULDBLOCK;
		goto end;
	}
	/*eventually ask to fill the bufferizer*/
	if (obj->ntasks_pending == 0){
		if (avail < obj->blocksize){
			obj->ntasks_pending++;
			ms_worker_thread_add_task(obj->wth, async_reader_fill, obj);
		}
	}
	/*and finally return the datas*/
	err = (int)ms_bufferizer_read(&obj->buf, buf, MIN(size, avail));
end:
	ms_mutex_unlock(&obj->mutex);
	return err;
}

static void async_reader_seek(void *data){
	MSAsyncReader *obj = (MSAsyncReader*) data;
	ms_mutex_lock(&obj->mutex);
	if (bctbx_file_seek(obj->fp, obj->seekoff, SEEK_SET) == BCTBX_VFS_ERROR){
		ms_error("async_reader_seek() seek failed : %s", strerror(errno));
	}
	obj->moving--;
	ms_bufferizer_flush(&obj->buf);
	ms_mutex_unlock(&obj->mutex);
	async_reader_fill(data);
}

void ms_async_reader_seek(MSAsyncReader *obj, off_t offset){
	ms_mutex_lock(&obj->mutex);
	obj->ntasks_pending++;
	obj->moving++;
	obj->seekoff = offset;
	ms_worker_thread_add_task(obj->wth, async_reader_seek, obj);
	ms_mutex_unlock(&obj->mutex);
}

struct _MSAsyncWriter{
	MSWorkerThread *wth;
	ms_mutex_t mutex;
	MSBufferizer buf;
	uint8_t *wbuf;
	bctbx_vfs_file_t *fp;
	size_t blocksize;
};

MSAsyncWriter *ms_async_writer_new(bctbx_vfs_file_t *fp){
	MSAsyncWriter *obj = ms_new0(MSAsyncWriter,1);
	ms_mutex_init(&obj->mutex, NULL);
	ms_bufferizer_init(&obj->buf);
	obj->fp = fp;
	obj->wth = ms_worker_thread_new("MSAsyncWriter");
#ifndef WIN32
	obj->blocksize = getpagesize();
#else
	obj->blocksize = 4096;
#endif
	obj->wbuf = ms_malloc(obj->blocksize);
	return obj;
}

static void async_writer_write(void *data){
	MSAsyncWriter *obj = (MSAsyncWriter*) data;
	size_t size;
	bool_t ok = FALSE;
	
	ms_mutex_lock(&obj->mutex);
	size = MIN(obj->blocksize, ms_bufferizer_get_avail(&obj->buf));
	if (ms_bufferizer_read(&obj->buf, obj->wbuf, size) == size){
		ok = TRUE;
	}else{
		ms_error("async_writer_write(): should not happen");
	}
	ms_mutex_unlock(&obj->mutex);
	if (ok){
		if (bctbx_file_write2(obj->fp, obj->wbuf, size) == BCTBX_VFS_ERROR){
			ms_error("async_writer_write(): %s", strerror(errno));
		}
	}
}

void ms_async_writer_destroy(MSAsyncWriter *obj){
	if (ms_bufferizer_get_avail(&obj->buf) > 0){
		/*push last samples, even if less than blocksize long */
		ms_worker_thread_add_task(obj->wth, async_writer_write, obj);
	}
	ms_worker_thread_destroy(obj->wth, TRUE);
	ms_mutex_destroy(&obj->mutex);
	ms_bufferizer_flush(&obj->buf);
	ms_free(obj->wbuf);
	ms_free(obj);
}

int ms_async_writer_write(MSAsyncWriter *obj, mblk_t *m){
	ms_mutex_lock(&obj->mutex);
	ms_bufferizer_put(&obj->buf, m);
	/*each time we have blocksize bytes in a bufferizer, push a write*/
	if (ms_bufferizer_get_avail(&obj->buf) >= obj->blocksize){
		ms_worker_thread_add_task(obj->wth, async_writer_write, obj);
	}
	ms_mutex_unlock(&obj->mutex);
	return 0;
}
