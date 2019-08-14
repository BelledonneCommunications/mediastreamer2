
/*
 * Copyright (c) 2010-2019 Belledonne Communications SARL.
 *
 * This file is part of mediastreamer2.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */


#if defined(_WIN32) || defined(_WIN32_WCE)
#include "ortp-config-win32.h"
#elif HAVE_CONFIG_H
#include "ortp-config.h"
#endif
#include "ortp/logging.h"
#include "ortp/port.h"
#include "ortp/str_utils.h"
#include "utils.h"

#if	defined(_WIN32) && !defined(_WIN32_WCE)
#include <process.h>
#endif

#ifdef HAVE_SYS_SHM_H
#include <sys/shm.h>
#endif

static void *ortp_libc_malloc(size_t sz){
	return malloc(sz);
}

static void *ortp_libc_realloc(void *ptr, size_t sz){
	return realloc(ptr,sz);
}

static void ortp_libc_free(void*ptr){
	free(ptr);
}

static bool_t allocator_used=FALSE;

static OrtpMemoryFunctions ortp_allocator={
	ortp_libc_malloc,
	ortp_libc_realloc,
	ortp_libc_free
};

void ortp_set_memory_functions(OrtpMemoryFunctions *functions){
	if (allocator_used){
		ortp_fatal("ortp_set_memory_functions() must be called before "
		"first use of ortp_malloc or ortp_realloc");
		return;
	}
	ortp_allocator=*functions;
}

void* ortp_malloc(size_t sz){
	allocator_used=TRUE;
	return ortp_allocator.malloc_fun(sz);
}

void* ortp_realloc(void *ptr, size_t sz){
	allocator_used=TRUE;
	return ortp_allocator.realloc_fun(ptr,sz);
}

void ortp_free(void* ptr){
	ortp_allocator.free_fun(ptr);
}

void * ortp_malloc0(size_t size){
	void *ptr=ortp_malloc(size);
	memset(ptr,0,size);
	return ptr;
}

char * ortp_strdup(const char *tmp){
	size_t sz;
	char *ret;
	if (tmp==NULL)
	  return NULL;
	sz=strlen(tmp)+1;
	ret=(char*)ortp_malloc(sz);
	strcpy(ret,tmp);
	ret[sz-1]='\0';
	return ret;
}

/*
 * this method is an utility method that calls fnctl() on UNIX or
 * ioctlsocket on Win32.
 * int retrun the result of the system method
 */
int set_non_blocking_socket (ortp_socket_t sock)
{


#if	!defined(_WIN32) && !defined(_WIN32_WCE)
	return fcntl (sock, F_SETFL, O_NONBLOCK);
#else
	unsigned long nonBlock = 1;
	return ioctlsocket(sock, FIONBIO , &nonBlock);
#endif
}


/*
 * this method is an utility method that calls close() on UNIX or
 * closesocket on Win32.
 * int retrun the result of the system method
 */
int close_socket(ortp_socket_t sock){
#if	!defined(_WIN32) && !defined(_WIN32_WCE)
	return close (sock);
#else
	return closesocket(sock);
#endif
}

#if defined (_WIN32_WCE) || defined(_MSC_VER)
int ortp_file_exist(const char *pathname) {
	FILE* fd;
	if (pathname==NULL) return -1;
	fd=fopen(pathname,"r");
	if (fd==NULL) {
		return -1;
	} else {
		fclose(fd);
		return 0;
	}
}
#else
int ortp_file_exist(const char *pathname) {
	return access(pathname,F_OK);
}
#endif /*_WIN32_WCE*/

#if	!defined(_WIN32) && !defined(_WIN32_WCE)
	/* Use UNIX inet_aton method */
#else
	int inet_aton (const char * cp, struct in_addr * addr)
	{
		unsigned long retval;

		retval = inet_addr (cp);

		if (retval == INADDR_NONE)
		{
			return -1;
		}
		else
		{
			addr->S_un.S_addr = retval;
			return 1;
		}
	}
#endif

char *ortp_strndup(const char *str,int n){
	int min=MIN((int)strlen(str),n)+1;
	char *ret=(char*)ortp_malloc(min);
	strncpy(ret,str,min);
	ret[min-1]='\0';
	return ret;
}

#if	!defined(_WIN32) && !defined(_WIN32_WCE)
int __ortp_thread_join(ortp_thread_t thread, void **ptr){
	int err=pthread_join(thread,ptr);
	if (err!=0) {
		ortp_error("pthread_join error: %s",strerror(err));
	}
	return err;
}

int __ortp_thread_create(pthread_t *thread, pthread_attr_t *attr, void * (*routine)(void*), void *arg){
	pthread_attr_t my_attr;
	pthread_attr_init(&my_attr);
	if (attr)
		my_attr = *attr;
#ifdef ORTP_DEFAULT_THREAD_STACK_SIZE
	if (ORTP_DEFAULT_THREAD_STACK_SIZE!=0)
		pthread_attr_setstacksize(&my_attr, ORTP_DEFAULT_THREAD_STACK_SIZE);
#endif
	return pthread_create(thread, &my_attr, routine, arg);
}

#endif
#if	defined(_WIN32) || defined(_WIN32_WCE)

int WIN_mutex_init(ortp_mutex_t *mutex, void *attr)
{
	*mutex=CreateMutex(NULL, FALSE, NULL);
	return 0;
}

int WIN_mutex_lock(ortp_mutex_t * hMutex)
{
	WaitForSingleObject(*hMutex, INFINITE); /* == WAIT_TIMEOUT; */
	return 0;
}

int WIN_mutex_unlock(ortp_mutex_t * hMutex)
{
	ReleaseMutex(*hMutex);
	return 0;
}

int WIN_mutex_destroy(ortp_mutex_t * hMutex)
{
	CloseHandle(*hMutex);
	return 0;
}

typedef struct thread_param{
	void * (*func)(void *);
	void * arg;
}thread_param_t;

static unsigned WINAPI thread_starter(void *data){
	thread_param_t *params=(thread_param_t*)data;
	void *ret=params->func(params->arg);
	ortp_free(data);
	return (DWORD)ret;
}

#if defined _WIN32_WCE
#    define _beginthreadex	CreateThread
#    define	_endthreadex	ExitThread
#endif

int WIN_thread_create(ortp_thread_t *th, void *attr, void * (*func)(void *), void *data)
{
    thread_param_t *params=ortp_new(thread_param_t,1);
    params->func=func;
    params->arg=data;
	*th=(HANDLE)_beginthreadex( NULL, 0, thread_starter, params, 0, NULL);
	return 0;
}

int WIN_thread_join(ortp_thread_t thread_h, void **unused)
{
	if (thread_h!=NULL)
	{
		WaitForSingleObject(thread_h, INFINITE);
		CloseHandle(thread_h);
	}
	return 0;
}

int WIN_cond_init(ortp_cond_t *cond, void *attr)
{
	*cond=CreateEvent(NULL, FALSE, FALSE, NULL);
	return 0;
}

int WIN_cond_wait(ortp_cond_t* hCond, ortp_mutex_t * hMutex)
{
	//gulp: this is not very atomic ! bug here ?
	WIN_mutex_unlock(hMutex);
	WaitForSingleObject(*hCond, INFINITE);
	WIN_mutex_lock(hMutex);
	return 0;
}

int WIN_cond_signal(ortp_cond_t * hCond)
{
	SetEvent(*hCond);
	return 0;
}

int WIN_cond_broadcast(ortp_cond_t * hCond)
{
	WIN_cond_signal(hCond);
	return 0;
}

int WIN_cond_destroy(ortp_cond_t * hCond)
{
	CloseHandle(*hCond);
	return 0;
}


#if defined(_WIN32_WCE)
#include <time.h>

const char * ortp_strerror(DWORD value) {
	static TCHAR msgBuf[256];
	FormatMessage(
			FORMAT_MESSAGE_FROM_SYSTEM |
			FORMAT_MESSAGE_IGNORE_INSERTS,
			NULL,
			value,
			0, // Default language
			(LPTSTR) &msgBuf,
			0,
			NULL
	);
	return (const char *)msgBuf;
}

int
gettimeofday (struct timeval *tv, void *tz)
{
  DWORD timemillis = GetTickCount();
  tv->tv_sec  = timemillis/1000;
  tv->tv_usec = (timemillis - (tv->tv_sec*1000)) * 1000;
  return 0;
}

#else

int gettimeofday (struct timeval *tv, void* tz)
{
	union
	{
		__int64 ns100; /*time since 1 Jan 1601 in 100ns units */
		FILETIME fileTime;
	} now;

	GetSystemTimeAsFileTime (&now.fileTime);
	tv->tv_usec = (long) ((now.ns100 / 10LL) % 1000000LL);
	tv->tv_sec = (long) ((now.ns100 - 116444736000000000LL) / 10000000LL);
	return (0);
}

#endif

const char *getWinSocketError(int error)
{
	static char buf[80];

	switch (error)
	{
		case WSANOTINITIALISED: return "Windows sockets not initialized : call WSAStartup";
		case WSAEADDRINUSE:		return "Local Address already in use";
		case WSAEADDRNOTAVAIL:	return "The specified address is not a valid address for this machine";
		case WSAEINVAL:			return "The socket is already bound to an address.";
		case WSAENOBUFS:		return "Not enough buffers available, too many connections.";
		case WSAENOTSOCK:		return "The descriptor is not a socket.";
		case WSAECONNRESET:		return "Connection reset by peer";

		default :
			sprintf(buf, "Error code : %d", error);
			return buf;
		break;
	}

	return buf;
}

#ifdef _WORKAROUND_MINGW32_BUGS
char * WSAAPI gai_strerror(int errnum){
	 return (char*)getWinSocketError(errnum);
}
#endif

#endif

#ifndef _WIN32

#include <sys/socket.h>
#include <netdb.h>
#include <sys/un.h>
#include <sys/stat.h>

static char *make_pipe_name(const char *name){
	return ortp_strdup_printf("/tmp/%s",name);
}

/* portable named pipes */
ortp_socket_t ortp_server_pipe_create(const char *name){
	struct sockaddr_un sa;
	char *pipename=make_pipe_name(name);
	ortp_socket_t sock;
	sock=socket(AF_UNIX,SOCK_STREAM,0);
	sa.sun_family=AF_UNIX;
	strncpy(sa.sun_path,pipename,sizeof(sa.sun_path)-1);
	unlink(pipename);/*in case we didn't finished properly previous time */
	ortp_free(pipename);
	fchmod(sock,S_IRUSR|S_IWUSR);
	if (bind(sock,(struct sockaddr*)&sa,sizeof(sa))!=0){
		ortp_error("Failed to bind command unix socket: %s",strerror(errno));
		return -1;
	}
	listen(sock,1);
	return sock;
}

ortp_socket_t ortp_server_pipe_accept_client(ortp_socket_t server){
	struct sockaddr_un su;
	socklen_t ssize=sizeof(su);
	ortp_socket_t client_sock=accept(server,(struct sockaddr*)&su,&ssize);
	return client_sock;
}

int ortp_server_pipe_close_client(ortp_socket_t client){
	return close(client);
}

int ortp_server_pipe_close(ortp_socket_t spipe){
	struct sockaddr_un sa;
	socklen_t len=sizeof(sa);
	int err;
	/*this is to retrieve the name of the pipe, in order to unlink the file*/
	err=getsockname(spipe,(struct sockaddr*)&sa,&len);
	if (err==0){
		unlink(sa.sun_path);
	}else ortp_error("getsockname(): %s",strerror(errno));
	return close(spipe);
}

ortp_socket_t ortp_client_pipe_connect(const char *name){
	struct sockaddr_un sa;
	char *pipename=make_pipe_name(name);
	ortp_socket_t sock=socket(AF_UNIX,SOCK_STREAM,0);
	sa.sun_family=AF_UNIX;
	strncpy(sa.sun_path,pipename,sizeof(sa.sun_path)-1);
	ortp_free(pipename);
	if (connect(sock,(struct sockaddr*)&sa,sizeof(sa))!=0){
		close(sock);
		return -1;
	}
	return sock;
}

int ortp_pipe_read(ortp_socket_t p, uint8_t *buf, int len){
	return read(p,buf,len);
}

int ortp_pipe_write(ortp_socket_t p, const uint8_t *buf, int len){
	return write(p,buf,len);
}

int ortp_client_pipe_close(ortp_socket_t sock){
	return close(sock);
}

#ifdef HAVE_SYS_SHM_H

void *ortp_shm_open(unsigned int keyid, int size, int create){
	key_t key=keyid;
	void *mem;
	int fd=shmget(key,size,create ? (IPC_CREAT | 0666) : 0666);
	if (fd==-1){
		printf("shmget failed: %s\n",strerror(errno));
		return NULL;
	}
	mem=shmat(fd,NULL,0);
	if (mem==(void*)-1){
		printf("shmat() failed: %s", strerror(errno));
		return NULL;
	}
	return mem;
}

void ortp_shm_close(void *mem){
	shmdt(mem);
}

#endif

#elif defined(_WIN32) && !defined(_WIN32_WCE)

static char *make_pipe_name(const char *name){
	return ortp_strdup_printf("\\\\.\\pipe\\%s",name);
}

static HANDLE event=NULL;

/* portable named pipes */
ortp_pipe_t ortp_server_pipe_create(const char *name){
	ortp_pipe_t h;
	char *pipename=make_pipe_name(name);
	h=CreateNamedPipe(pipename,PIPE_ACCESS_DUPLEX|FILE_FLAG_OVERLAPPED,PIPE_TYPE_MESSAGE|PIPE_WAIT,1,
						32768,32768,0,NULL);
	ortp_free(pipename);
	if (h==INVALID_HANDLE_VALUE){
		ortp_error("Fail to create named pipe %s",pipename);
	}
	if (event==NULL) event=CreateEvent(NULL,TRUE,FALSE,NULL);
	return h;
}


/*this function is a bit complex because we need to wakeup someday
even if nobody connects to the pipe.
ortp_server_pipe_close() makes this function to exit.
*/
ortp_pipe_t ortp_server_pipe_accept_client(ortp_pipe_t server){
	OVERLAPPED ol;
	DWORD undef;
	HANDLE handles[2];
	memset(&ol,0,sizeof(ol));
	ol.hEvent=CreateEvent(NULL,TRUE,FALSE,NULL);
	ConnectNamedPipe(server,&ol);
	handles[0]=ol.hEvent;
	handles[1]=event;
	WaitForMultipleObjects(2,handles,FALSE,INFINITE);
	if (GetOverlappedResult(server,&ol,&undef,FALSE)){
		CloseHandle(ol.hEvent);
		return server;
	}
	CloseHandle(ol.hEvent);
	return INVALID_HANDLE_VALUE;
}

int ortp_server_pipe_close_client(ortp_pipe_t server){
	return DisconnectNamedPipe(server)==TRUE ? 0 : -1;
}

int ortp_server_pipe_close(ortp_pipe_t spipe){
	SetEvent(event);
	//CancelIoEx(spipe,NULL); /*vista only*/
	return CloseHandle(spipe);
}

ortp_pipe_t ortp_client_pipe_connect(const char *name){
	char *pipename=make_pipe_name(name);
	ortp_pipe_t hpipe = CreateFile(
         pipename,   // pipe name
         GENERIC_READ |  // read and write access
         GENERIC_WRITE,
         0,              // no sharing
         NULL,           // default security attributes
         OPEN_EXISTING,  // opens existing pipe
         0,              // default attributes
         NULL);          // no template file
	ortp_free(pipename);
	return hpipe;
}

int ortp_pipe_read(ortp_pipe_t p, uint8_t *buf, int len){
	DWORD ret=0;
	if (ReadFile(p,buf,len,&ret,NULL))
		return ret;
	/*ortp_error("Could not read from pipe: %s",strerror(GetLastError()));*/
	return -1;
}

int ortp_pipe_write(ortp_pipe_t p, const uint8_t *buf, int len){
	DWORD ret=0;
	if (WriteFile(p,buf,len,&ret,NULL))
		return ret;
	/*ortp_error("Could not write to pipe: %s",strerror(GetLastError()));*/
	return -1;
}


int ortp_client_pipe_close(ortp_pipe_t sock){
	return CloseHandle(sock);
}


typedef struct MapInfo{
	HANDLE h;
	void *mem;
}MapInfo;

static OList *maplist=NULL;

void *ortp_shm_open(unsigned int keyid, int size, int create){
	HANDLE h;
	char name[64];
	void *buf;

	snprintf(name,sizeof(name),"%x",keyid);
	if (create){
		h = CreateFileMapping(
			INVALID_HANDLE_VALUE,    // use paging file
			NULL,                    // default security
			PAGE_READWRITE,          // read/write access
			0,                       // maximum object size (high-order DWORD)
			size,                // maximum object size (low-order DWORD)
			name);                 // name of mapping object
	}else{
		h = OpenFileMapping(
			FILE_MAP_ALL_ACCESS,   // read/write access
			FALSE,                 // do not inherit the name
			name);               // name of mapping object
	}
	if (h==(HANDLE)-1) {
		ortp_error("Fail to open file mapping (create=%i)",create);
		return NULL;
	}
	buf = (LPTSTR) MapViewOfFile(h, // handle to map object
		FILE_MAP_ALL_ACCESS,  // read/write permission
		0,
		0,
		size);
	if (buf!=NULL){
		MapInfo *i=(MapInfo*)ortp_new(MapInfo,1);
		i->h=h;
		i->mem=buf;
		maplist=o_list_append(maplist,i);
	}else{
		CloseHandle(h);
		ortp_error("MapViewOfFile failed");
	}
	return buf;
}

void ortp_shm_close(void *mem){
	OList *elem;
	for(elem=maplist;elem!=NULL;elem=elem->next){
		MapInfo *i=(MapInfo*)elem->data;
		if (i->mem==mem){
			CloseHandle(i->h);
			UnmapViewOfFile(mem);
			ortp_free(i);
			maplist=o_list_remove_link(maplist,elem);
			return;
		}
	}
	ortp_error("No shared memory at %p was found.",mem);
}


#endif


#ifdef __MACH__
#include <sys/types.h>
#include <sys/timeb.h>
#endif

void ortp_get_cur_time(ortpTimeSpec *ret){
#if defined(_WIN32_WCE) || defined(_WIN32)
	DWORD timemillis;
#	if defined(_WIN32_WCE)
	timemillis=GetTickCount();
#	else
	timemillis=timeGetTime();
#	endif
	ret->tv_sec=timemillis/1000;
	ret->tv_nsec=(timemillis%1000)*1000000LL;
#elif defined(__MACH__) && defined(__GNUC__) && (__GNUC__ >= 3)
	struct timeval tv;
	gettimeofday(&tv, NULL);
	ret->tv_sec=tv.tv_sec;
	ret->tv_nsec=tv.tv_usec*1000LL;
#elif defined(__MACH__)
	struct timeb time_val;

	ftime (&time_val);
	ret->tv_sec = time_val.time;
	ret->tv_nsec = time_val.millitm * 1000000LL;
#else
	struct timespec ts;
	if (clock_gettime(CLOCK_MONOTONIC,&ts)<0){
		ortp_fatal("clock_gettime() doesn't work: %s",strerror(errno));
	}
	ret->tv_sec=ts.tv_sec;
	ret->tv_nsec=ts.tv_nsec;
#endif
}
