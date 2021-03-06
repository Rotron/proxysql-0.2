#ifndef __CLASS_MYSQL_THREAD_H
#define __CLASS_MYSQL_THREAD_H
#include "proxysql.h"
#include "cpp.h"

/*
#define MYSQL_THREAD_EPOLL_MAXEVENTS 1000
#define MIN_POLL_FDS_PER_THREAD 1024
*/


#define MIN_POLL_LEN 8
#define MIN_POLL_DELETE_RATIO  8



static unsigned int near_pow_2 (unsigned int n) {
  unsigned int i = 1;
  while (i < n) i <<= 1;
  return i ? i : n;
}

/*
// this structure tries to emulate epoll()

class ProxySQL_Poll {

	private:
	void shrink() {
		unsigned int new_size=near_pow_2(len+1);
		fds=(struct pollfd *)realloc(fds,new_size*sizeof(struct pollfd));
		myds=(MySQL_Data_Stream **)realloc(myds,new_size*sizeof(MySQL_Data_Stream *));
//		status=(unsigned char *)realloc(status,new_size*sizeof(unsigned char));
		size=new_size;
	};
	void expand(unsigned int more) {
		if ( (len+more) > size ) {
			unsigned int new_size=near_pow_2(len+more);
			fds=(struct pollfd *)realloc(fds,new_size*sizeof(struct pollfd));
			myds=(MySQL_Data_Stream **)realloc(myds,new_size*sizeof(MySQL_Data_Stream *));
//			status=(unsigned char *)realloc(status,new_size*sizeof(unsigned char));
			size=new_size;
		}
	};

	public:
	unsigned int len=0;
	unsigned int size=0;
	struct pollfd *fds=NULL;
	MySQL_Data_Stream **myds=NULL;
//	unsigned char *status=NULL;   // this should be moved within the Data Stream
	ProxySQL_Poll() {
		size=MIN_POLL_LEN;
		// preallocate MIN_POLL_LEN slots
		fds=(struct pollfd *)malloc(size*sizeof(struct pollfd));
		myds=(MySQL_Data_Stream **)malloc(size*sizeof(MySQL_Data_Stream *));
		//status=(unsigned char *)malloc(size*sizeof(unsigned char));
	};

	~ProxySQL_Poll() {
		unsigned int i;
		for (i=0;i<len;i++) {
			if (myds[i]->myds_type==MYDS_LISTENER) {
				delete myds[i];
			}
		}
		free(myds);
		free(fds);
	}

	void add(uint32_t _events, int _fd, MySQL_Data_Stream *_myds) {
		if (len==size) {
			expand(1);
		}
		myds[len]=_myds;
		fds[len].fd=_fd;
		fds[len].events=_events;
		fds[len].revents=0;
		len++;
	};

	void remove_index_fast(unsigned int i) {
		if (i != (len-1)) {
			myds[i]=myds[len-1];
			fds[i].fd=fds[len-1].fd;
			fds[i].events=fds[len-1].events;
			fds[i].revents=fds[len-1].revents;
			//status[i]=status[len-1];
		}
		len--;
		if ( ( len>MIN_POLL_LEN ) && ( size > len*MIN_POLL_DELETE_RATIO ) ) {
			shrink();
		}
	};	
};
*/


/*
class ProxySQL_Poll;
*/

class ProxySQL_Poll {

  private:
  void shrink() {
    unsigned int new_size=near_pow_2(len+1);
    fds=(struct pollfd *)realloc(fds,new_size*sizeof(struct pollfd));
    myds=(MySQL_Data_Stream **)realloc(myds,new_size*sizeof(MySQL_Data_Stream *));
//    status=(unsigned char *)realloc(status,new_size*sizeof(unsigned char));
		last_recv=(unsigned long long *)realloc(last_recv,new_size*sizeof(unsigned long long));
		last_sent=(unsigned long long *)realloc(last_sent,new_size*sizeof(unsigned long long));
    size=new_size;
  };
  void expand(unsigned int more) {
    if ( (len+more) > size ) {
      unsigned int new_size=near_pow_2(len+more);
      fds=(struct pollfd *)realloc(fds,new_size*sizeof(struct pollfd));
      myds=(MySQL_Data_Stream **)realloc(myds,new_size*sizeof(MySQL_Data_Stream *));
//      status=(unsigned char *)realloc(status,new_size*sizeof(unsigned char));
			last_recv=(unsigned long long *)realloc(last_recv,new_size*sizeof(unsigned long long));
			last_sent=(unsigned long long *)realloc(last_sent,new_size*sizeof(unsigned long long));
      size=new_size;
    }
  };

  public:
  unsigned int len;
  unsigned int size;
  struct pollfd *fds;
  MySQL_Data_Stream **myds;
	unsigned long long *last_recv;
	unsigned long long *last_sent;
//  unsigned char *status=NULL;   // this should be moved within the Data Stream
  ProxySQL_Poll() {
    size=MIN_POLL_LEN;
    // preallocate MIN_POLL_LEN slots
    fds=(struct pollfd *)malloc(size*sizeof(struct pollfd));
    myds=(MySQL_Data_Stream **)malloc(size*sizeof(MySQL_Data_Stream *));
    //status=(unsigned char *)malloc(size*sizeof(unsigned char));
		last_recv=(unsigned long long *)malloc(size*sizeof(unsigned long long));
		last_sent=(unsigned long long *)malloc(size*sizeof(unsigned long long));
  };

  ~ProxySQL_Poll() {
    unsigned int i;
    for (i=0;i<len;i++) {
      if (myds[i]->myds_type==MYDS_LISTENER) {
        delete myds[i];
      }
    }
    free(myds);
    free(fds);
		free(last_recv);
		free(last_sent);
  };



  void add(uint32_t _events, int _fd, MySQL_Data_Stream *_myds, unsigned long long sent_time) {
		//fprintf(stderr,"ProxySQL_Poll: Adding fd %d\n",_fd);
    if (len==size) {
      expand(1);
    }
		_myds->mypolls=this;
    myds[len]=_myds;
    fds[len].fd=_fd;
    fds[len].events=_events;
    fds[len].revents=0;
		_myds->poll_fds_idx=len;  // fix a serious bug
    last_recv[len]=monotonic_time();
    last_sent[len]=sent_time;
    len++;
  };

  void remove_index_fast(unsigned int i) {
		//fprintf(stderr,"ProxySQL_Poll: Removing fd %d\n",fds[i].fd);
		if ((int)i==-1) return;
		myds[i]->poll_fds_idx=-1; // this prevents further delete
    if (i != (len-1)) {
      myds[i]=myds[len-1];
      fds[i].fd=fds[len-1].fd;
      fds[i].events=fds[len-1].events;
      fds[i].revents=fds[len-1].revents;
			myds[i]->poll_fds_idx=i;  // fix a serious bug
      //status[i]=status[len-1];
    	last_recv[i]=last_recv[len-1];
    	last_sent[i]=last_sent[len-1];
    }
    len--;
    if ( ( len>MIN_POLL_LEN ) && ( size > len*MIN_POLL_DELETE_RATIO ) ) {
      shrink();
    }
  };  
};


class MySQL_Thread
{
	protected:
	int nfds;

//	ProxySQL_Poll mypolls;
	
	public:

	

	ProxySQL_Poll mypolls;
	pthread_t thread_id;
	int shutdown;
	PtrArray *mysql_sessions;

	MySQL_Thread() {};
	virtual ~MySQL_Thread() {};
	virtual bool init() {return false;};
	virtual void poll_listener_add(int fd) {};
	virtual void run() {};

	virtual const char *version() {return NULL;};
	virtual void print_version() {};


	virtual void register_session(MySQL_Session *) {};
	virtual void unregister_session(int) {};
	virtual MySQL_Session * create_new_session_and_client_data_stream(int) {return NULL;};

	virtual struct pollfd * get_pollfd(unsigned int) {return NULL;};
};

typedef MySQL_Thread * create_MySQL_Thread_t();
typedef void destroy_MySQL_Thread_t(MySQL_Thread *);




#ifndef __CLASS_STANDARD_MYSQL_THREAD_H
#define __CLASS_STANDARD_MYSQL_THREAD_H
class Standard_MySQL_Thread: public MySQL_Thread
{
	public:
//	Standard_MySQL_Thread();
//	 ~Standard_MySQL_Thread();
	MySQL_Session * create_new_session_and_client_data_stream(int);
};
#endif /* __CLASS_STANDARD_MYSQL_THREAD_H */

#endif /* __CLASS_MYSQL_THREAD_H */
