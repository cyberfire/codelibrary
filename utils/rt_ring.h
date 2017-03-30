#ifndef __REALTIME_RING_H__
#define __REALTIME_RING_H__

#include <pthread.h>
#include <stdint.h>

/* 
  a queue for real time applications to deliver data between threads:
  too old data will be dropped if not inside the window after the write header

 */


/* the whole buffer length */
#define RT_RING_BUF_LEN 64

/* at most valid items after the writer */
#define RT_RING_HIS_LEN 4


typedef void (*release_data_t)(void *);

struct rt_ring_entry
{
	void * data;
	pthread_spinlock_t lock;
};

struct rt_ring
{
	uint64_t write_count;
	pthread_spinlock_t writer_lock;
	release_data_t release_data;
	struct rt_ring_entry entry[RT_RING_BUF_LEN];

};


static inline void rt_ring_init(struct rt_ring * p_ring, release_data_t cb)
{
	int i;

	p_ring->write_count=-1; 

	pthread_spin_init(&p_ring->writer_lock,PTHREAD_PROCESS_PRIVATE);

	p_ring->release_data=cb;

	for(i=0;i<RT_RING_BUF_LEN;i++)
	{
		p_ring->entry[i].data=NULL;
		pthread_spin_init(&p_ring->entry[i].lock,PTHREAD_PROCESS_PRIVATE);
	}

}

static inline void rt_ring_release(struct rt_ring * p_ring)
{
	int i;

	struct rt_ring_entry * p_entry;

	for(i=0;i<RT_RING_BUF_LEN;i++)
	{
		p_entry=&p_ring->entry[i];

		/* free resource if any */
		if(p_entry->data)
			p_ring->release_data(p_entry->data);    	 

	}
}   

static inline void rt_ring_insert(struct rt_ring * p_ring, void * p_data)
{
	uint64_t widx;
	struct rt_ring_entry * p_entry;
       void * old_data;

	pthread_spin_lock(&p_ring->writer_lock);
	p_ring->write_count++;
	pthread_spin_unlock(&p_ring->writer_lock);


	widx=p_ring->write_count%RT_RING_BUF_LEN; 	   
	p_entry=&p_ring->entry[widx];
  
        /* it is pointer swap operation in face */

	pthread_spin_lock(&p_entry->lock);

	old_data=p_entry->data;
	p_entry->data=p_data;

	pthread_spin_unlock(&p_entry->lock);

        if(old_data)
	    p_ring->release_data(old_data);    	 

}


static inline void * rt_ring_retrieval(struct rt_ring * p_ring)
{
	int i=RT_RING_HIS_LEN-1;
	struct rt_ring_entry * p_entry;
	uint64_t cur_ts,ridx;
	void * p_data=NULL;

	while(i>=0)
	{
		cur_ts=p_ring->write_count-i;
		ridx=cur_ts%RT_RING_BUF_LEN;
		p_entry=&p_ring->entry[ridx];

		pthread_spin_lock(&p_entry->lock);
		p_data=p_entry->data;
  		p_entry->data=NULL; 

		pthread_spin_unlock(&p_entry->lock);

                if(p_data)
                    break;

		i--;

	}
     	 
	return p_data;      	   
} 


#endif
