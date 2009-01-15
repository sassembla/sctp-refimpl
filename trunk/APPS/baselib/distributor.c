/*	$Header: /usr/sctpCVS/APPS/baselib/distributor.c,v 1.3 2009-01-15 14:35:15 randall Exp $ */

/*
 * Copyright (C) 2002 Cisco Systems Inc,
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <distributor.h>
#include <printMessageHandler.h>

hashableHash *
create_hashableHash(int pid, const char *string, int sz)
{
	hashableHash *o;

	o = (hashableHash *) CALLOC(1, sizeof(hashableHash));
	if (o != NULL) {
		o->hash = HashedTbl_create(string, sz);
		if (o->hash != NULL) {
			o->protocolId = pid;
		} else {
			FREE(o);
			o = NULL;
		}
	}
	return (o);
}

void
destroy_hashableHash(hashableHash * o)
{
	HashedTbl_destroy(o->hash);
	FREE(o);
}



hashableDlist *
create_hashdlist(int strm)
{
	hashableDlist *o;

	o = (hashableDlist *) CALLOC(1, sizeof(hashableDlist));
	if (o != NULL) {
		o->list = dlist_create();
		if (o->list != NULL) {
			o->streamNo = (uint32_t) strm;
		} else {
			FREE(o);
			o = NULL;
		}
	}
	return (o);
}

void
destroy_hashDlist(hashableDlist * o)
{
	dlist_destroy(o->list);
	FREE(o);
}


distributor *
createDistributor()
{
	int ret, i;
	distributor *o;

	o = (distributor *) CALLOC(1, sizeof(distributor));
	if (o == NULL)
		return (NULL);
	memset(o, 0, sizeof(distributor));
	o->fdSubscribersList = dlist_create();
	if (o->fdSubscribersList == NULL) {
		/* failed to create */
		FREE(o);
		return (NULL);
	}
	o->lazyClockTickList = dlist_create();
	if (o->lazyClockTickList == NULL) {
		dlist_destroy(o->fdSubscribersList);
		FREE(o);
		return (NULL);
	}
	o->startStopList = dlist_create();
	if (o->startStopList == NULL) {
		dlist_destroy(o->fdSubscribersList);
		dlist_destroy(o->lazyClockTickList);
		FREE(o);
		return (NULL);
	}
	/* we specify 11 subscribers */
	o->fdSubscribers = HashedTbl_create("File Descriptor Subscribers", 11);
	if (o->fdSubscribers == NULL) {
		/* failed to create */
		dlist_destroy(o->fdSubscribersList);
		dlist_destroy(o->lazyClockTickList);
		dlist_destroy(o->startStopList);
		FREE(o);
		return (NULL);
	}
	o->msgSubscriberList = dlist_create();
	if (o->msgSubscriberList == NULL) {
		/* failed to create */
		HashedTbl_destroy(o->fdSubscribers);
		dlist_destroy(o->fdSubscribersList);
		dlist_destroy(o->lazyClockTickList);
		dlist_destroy(o->startStopList);
		FREE(o);
		return (NULL);
	}
	o->msgSubscribers = HashedTbl_create("Master Hash of Hashes", 11);
	if (o->msgSubscribers == NULL) {
		/* failed to create */
		dlist_destroy(o->msgSubscriberList);
		HashedTbl_destroy(o->fdSubscribers);
		dlist_destroy(o->fdSubscribersList);
		dlist_destroy(o->lazyClockTickList);
		dlist_destroy(o->startStopList);
		FREE(o);
		return (NULL);
	}
	o->msgSubZero = create_hashableHash(DIST_SCTP_PROTO_ID_DEFAULT,
	    "Master Hash of Zero PID",
	    11);
	if (o->msgSubZero == NULL) {
		/* failed to create */
		HashedTbl_destroy(o->msgSubscribers);
		dlist_destroy(o->msgSubscriberList);
		HashedTbl_destroy(o->fdSubscribers);
		dlist_destroy(o->fdSubscribersList);
		dlist_destroy(o->lazyClockTickList);
		dlist_destroy(o->startStopList);
		FREE(o);
		return (NULL);
	}
	o->msgSubZeroNoStrm = create_hashdlist(DIST_STREAM_DEFAULT);
	if (o->msgSubZeroNoStrm == NULL) {
		/* failed to create */
		destroy_hashableHash(o->msgSubZero);
		HashedTbl_destroy(o->msgSubscribers);
		dlist_destroy(o->msgSubscriberList);
		HashedTbl_destroy(o->fdSubscribers);
		dlist_destroy(o->fdSubscribersList);
		dlist_destroy(o->lazyClockTickList);
		dlist_destroy(o->startStopList);
		FREE(o);
		return (NULL);
	}
	o->numfds = DEFAULT_POLL_LIST_SZ;
	o->numused = 0;
	o->fdlist = (struct pollfd *)CALLOC(DEFAULT_POLL_LIST_SZ, sizeof(struct pollfd));
	if (o->fdlist == NULL) {
		/* failed to create */
		destroy_hashDlist(o->msgSubZeroNoStrm);
		destroy_hashableHash(o->msgSubZero);
		HashedTbl_destroy(o->msgSubscribers);
		dlist_destroy(o->msgSubscriberList);
		HashedTbl_destroy(o->fdSubscribers);
		dlist_destroy(o->fdSubscribersList);
		dlist_destroy(o->lazyClockTickList);
		dlist_destroy(o->startStopList);
		FREE(o);
		return (NULL);
	}
	/* init fd structs */
	for (i = 0; i < o->numfds; i++) {
		o->fdlist[i].fd = -1;
		o->fdlist[i].events = 0;
		o->fdlist[i].revents = 0;
	}
	o->tmp_timer_list = hlist_create("tmp timer list", DIST_TIMER_START_CNT);
	if (o->tmp_timer_list == NULL) {
		FREE(o->fdlist);
		destroy_hashDlist(o->msgSubZeroNoStrm);
		destroy_hashableHash(o->msgSubZero);
		HashedTbl_destroy(o->msgSubscribers);
		dlist_destroy(o->msgSubscriberList);
		HashedTbl_destroy(o->fdSubscribers);
		dlist_destroy(o->fdSubscribersList);
		dlist_destroy(o->lazyClockTickList);
		dlist_destroy(o->startStopList);
		FREE(o);
		return (NULL);
	}
	/*
	 * now that everything is created lets set some of the relationships
	 * up.
	 */

	/*
	 * we first insert into the msgSubZero Hash table the default stream
	 * list.
	 */
	ret = HashedTbl_enterKeyed(o->msgSubZero->hash,
	    o->msgSubZeroNoStrm->streamNo,
	    (void *)o->msgSubZeroNoStrm,
	    (void *)&o->msgSubZeroNoStrm->streamNo,
	    sizeof(o->msgSubZeroNoStrm->streamNo));
	if (ret != LIB_STATUS_GOOD) {
		/* We have a problem. bail. */
		FREE(o->fdlist);
		destroy_hashDlist(o->msgSubZeroNoStrm);
		destroy_hashableHash(o->msgSubZero);
		HashedTbl_destroy(o->msgSubscribers);
		dlist_destroy(o->msgSubscriberList);
		HashedTbl_destroy(o->fdSubscribers);
		dlist_destroy(o->fdSubscribersList);
		dlist_destroy(o->lazyClockTickList);
		dlist_destroy(o->startStopList);
		hlist_destroy(o->tmp_timer_list);
		FREE(o);
		return (NULL);
	}
	/*
	 * Now we must put the default PID hash into the master hashtable.
	 */
	ret = HashedTbl_enter(o->msgSubscribers,
	    &o->msgSubZero->protocolId,
	    (void *)o->msgSubZero,
	    sizeof(int));
	if (ret != LIB_STATUS_GOOD) {
		/* We have a problem. bail. */
		FREE(o->fdlist);
		destroy_hashDlist(o->msgSubZeroNoStrm);
		destroy_hashableHash(o->msgSubZero);
		HashedTbl_destroy(o->msgSubscribers);
		dlist_destroy(o->msgSubscriberList);
		HashedTbl_destroy(o->fdSubscribers);
		dlist_destroy(o->fdSubscribersList);
		dlist_destroy(o->lazyClockTickList);
		dlist_destroy(o->startStopList);
		hlist_destroy(o->tmp_timer_list);
		FREE(o);
		return (NULL);
	}
	o->timerhash = HashedTbl_create("Master Hash of Timers", DIST_TIMER_START_CNT);
	if (o->timerhash == NULL) {
		FREE(o->fdlist);
		destroy_hashDlist(o->msgSubZeroNoStrm);
		destroy_hashableHash(o->msgSubZero);
		HashedTbl_destroy(o->msgSubscribers);
		dlist_destroy(o->msgSubscriberList);
		HashedTbl_destroy(o->fdSubscribers);
		dlist_destroy(o->fdSubscribersList);
		dlist_destroy(o->lazyClockTickList);
		dlist_destroy(o->startStopList);
		hlist_destroy(o->tmp_timer_list);
		FREE(o);
		return (NULL);
	}

	for(i=0; i<TIMERS_NUMBER_OF_ENT; i++) {
	  /* Initialize the timer wheel */
	  LIST_INIT(&o->timers[i]);
	}

	/* set the I am not done flag */
	o->notdone = 1;
	o->no_dup_timer = 1;
	o->timer_issued_cnt = 1;
	/* setup clock tick */
	o->idleClockTick.tv_sec = DIST_IDLE_CLOCK_SEC;
	o->idleClockTick.tv_usec = DIST_IDLE_CLOCK_USEC;
	/* get the last known time setup */
	gettimeofday(&o->lastKnownTime, (struct timezone *)NULL);
	o->msgs_round_robin = 0;
	return (o);
}

void
destroyDistributor(distributor * o)
{
	timerEntry *te;
	int i;
	fdEntry *fe;
	hashableHash *hh;
	hashableDlist *he;
	msgConsumer *mc;
	auditTick *at;
	StartStop *sat;
	void *keyP;

	/*
	 * we must get each element out of the multi-keyed tables before
	 * destroying. since both hashable lists that we keep a pointer to
	 * for shortcuts in our main structure are in the individual lists
	 * and will be found that way there is no need to do a :
	 * destroy_hashDlist(o->msgSubZeroNoStrm); <or>
	 * destroy_hashableHash(o->msgSubZero);
	 */

	/* the list of timers first. */
	for (i=0; i<TIMERS_NUMBER_OF_ENT ;i++) {
	  while((te = LIST_FIRST(&o->timers[i])) != NULL) {
		LIST_REMOVE(te, next);
		FREE(te);
	  }
	}
	/* If we exited from a timer, the tmp list may have
	 * te's in it that are not on the wheel. Free them too.
	 */
 	while ((te = (timerEntry *)hlist_getNext(o->tmp_timer_list)) != NULL) {
	  FREE(te);
	}
	hlist_destroy(o->tmp_timer_list);

	/* We just nuke this list since the memory is kept in
	 * the wheel
	 */
 	HashedTbl_destroy(o->timerhash);

	
	/* now FREE audit tick list */
	dlist_reset(o->lazyClockTickList);
	while ((at = (auditTick *) dlist_getNext(o->lazyClockTickList)) != NULL) {
		FREE(at);
	}
	dlist_destroy(o->lazyClockTickList);

	/* now FREE the start/stop list */
	dlist_reset(o->startStopList);
	while ((sat = (StartStop *) dlist_getNext(o->startStopList)) != NULL) {
		FREE(sat);
	}
	dlist_destroy(o->startStopList);

	/* get rid of all the file descriptors */
	FREE(o->fdlist);
	/*
	 * here we dump the hash table since all entries are also maintained
	 * on the dlist too
	 */
	HashedTbl_destroy(o->fdSubscribers);

	/* Now go through and FREE up all on our list */
	dlist_reset(o->fdSubscribersList);
	while ((fe = (fdEntry *) dlist_getNext(o->fdSubscribersList)) != NULL) {
		FREE(fe);
	}
	dlist_destroy(o->fdSubscribersList);

	/*
	 * Now we must go through the hash table of hashableHashes. And for
	 * each hashableHash pull all the hashableLists out. Each list is
	 * just destroyed since we will cleanup the underlying msgConsumers
	 * when we destroy the global msgSubscriberList.
	 * 
	 */
	HashedTbl_rewind(o->msgSubscribers);
	while ((hh = (hashableHash *) HashedTbl_getNext(o->msgSubscribers, &keyP)) != NULL) {
		/* we have a hashable hash, clean it up */
		HashedTbl_rewind(hh->hash);
		while ((he = (hashableDlist *) HashedTbl_getNext(hh->hash, &keyP)) != NULL) {
			/*
			 * we purge since list entries are all in
			 * msgSubscribers
			 */
			destroy_hashDlist(he);
		}
		destroy_hashableHash(hh);
	}
	HashedTbl_destroy(o->msgSubscribers);

	/* Now lets go out and get the msg subscriber entries too. */
	dlist_reset(o->msgSubscriberList);
	while ((mc = (msgConsumer *) dlist_getNext(o->msgSubscriberList)) != NULL) {
		FREE(mc);
	}
	dlist_destroy(o->msgSubscriberList);
	/* ok FREE the base and we are clean */
	FREE(o);
}

static timerEntry *
dist_pull_timer(distributor * o, timerFunc f, void *a, void *b, int entry)
{
	/* find the first timer with matching function and args */
	/* and remove it from the list */
    timerEntry *pe;
	hashableTimeEnt h;
	if (o->no_dup_timer) {
		entry = 0;
	}
	memset(&h, 0, sizeof(h));
	h.action = f;
	h.arg1 = a;
	h.arg2 = b;
	h.timer_issued_cnt = entry;
	/* See if we can find the timer */
	pe = (timerEntry *) HashedTbl_lookup(o->timerhash,
										 &h,
										 sizeof(h),
										 NULL);
	if (pe) {
	    /* we found it - remove it */
	    LIST_REMOVE(pe, next);
		HashedTbl_remove(o->timerhash, (void *)&pe->ent,
						 sizeof(hashableTimeEnt), NULL);
	}
	return (pe);
}

int
dist_TimerStart(distributor * o, timerFunc f, u_long a, u_long b, void *c, void *d)
{
  int ret, listentry;
  struct timerentries *thelist;
  struct timerEntry *te = NULL, *pe, *nxt;
  int used_entry = 0;

  if (o->no_dup_timer) {
	te = dist_pull_timer(o, f, c, d, 0);
  }
  /* allocate a new timer element */
  if (te == NULL) {
	te = (timerEntry *) CALLOC(1, sizeof(timerEntry));
	if (te == NULL) {
	  DEBUG_PRINTF("dist:Can't get memory for timer entry??\n");
	  return (-1);
	}
  } else {
	/* We are re-using a timer */
	o->timer_cnt--;
  }
  memset(te, 0, sizeof(timerEntry));
  te->ent.arg1 = c;
  te->ent.arg2 = d;
  te->ent.action = f;
  if (o->no_dup_timer == 0) {
	te->ent.timer_issued_cnt = o->timer_issued_cnt;
	used_entry = te->ent.timer_issued_cnt;
	o->timer_issued_cnt++;
	if (o->timer_issued_cnt == 0x7fffffff) {
	  o->timer_issued_cnt = 1;
	}
  } else {
	used_entry = te->ent.timer_issued_cnt = 0;
  }
  HashedTbl_enter(o->timerhash, (void *)&te->ent, (void *)te, sizeof(hashableTimeEnt));
  te->tv_sec = a;
  te->tv_usec = b;
#ifdef MINIMIZE_TIME_CALLS
  te->started.tv_sec = o->lastKnownTime.tv_sec;
  te->started.tv_usec = o->lastKnownTime.tv_usec;
#else
  gettimeofday(&te->started, (struct timezone *)NULL);
  /*
   * update the time, which is used before we go back into the
   * poll()/select().
   */
  o->lastKnownTime.tv_sec = te->started.tv_sec;
  o->lastKnownTime.tv_usec = te->started.tv_usec;
#endif
  /* now setup the expire time */
  te->expireTime.tv_sec = te->started.tv_sec + a;
  te->expireTime.tv_usec = te->started.tv_usec + b;
  while (te->expireTime.tv_usec > 1000000) {
	te->expireTime.tv_sec++;
	te->expireTime.tv_usec -= 1000000;
  }
  /*
   * put in a special precaution because I am paranoid. Set bad status
   * in case my logic is wrong :/
   */
  ret = -1;
  /* carefully place it on the right list in the wheel */
	
  listentry = te->expireTime.tv_sec % TIMERS_NUMBER_OF_ENT;
  thelist = &o->timers[listentry];
  pe = LIST_FIRST(thelist);
  if (pe) {
	do {
	  if (pe->expireTime.tv_sec > te->expireTime.tv_sec) {
		/* this one expires after the new one */
		LIST_INSERT_BEFORE(pe, te, next);
		o->timer_cnt++;
		break;
	  } else if (pe->expireTime.tv_sec == te->expireTime.tv_sec) {
		/*
		 * got to check usec's since equal seconds of
		 * expiration
		 */
		if (pe->expireTime.tv_usec > te->expireTime.tv_usec) {
		  /*
		   * ok we are expiring some time sooner than
		   * this dude
		   */
		  LIST_INSERT_BEFORE(pe, te, next);
		  o->timer_cnt++;
		  break;
		}
	  }
	  nxt = LIST_NEXT(pe, next);
	  if (nxt == NULL) {
		/* Goes at the tail */
		LIST_INSERT_AFTER(pe, te, next);
		o->timer_cnt++;
		break;
	  }
	  pe = nxt;
	}while (pe);

  } else {
	/* nothing in the list */
	LIST_INSERT_HEAD(&o->timers[listentry], te, next);
	o->timer_cnt++;
  }
  if (o->soonest_timer == NULL) {
	o->soonest_timer = te;
  } else {
	/* Do we have a new low one to cache */
	if(o->soonest_timer->expireTime.tv_sec > te->expireTime.tv_sec) {
	  /* Yes */
	  o->soonest_timer = te;
	} else if (o->soonest_timer->expireTime.tv_sec == te->expireTime.tv_sec) {
	  if (o->soonest_timer->expireTime.tv_usec > te->expireTime.tv_usec) {
		/* Yes */
		o->soonest_timer = te;
	  }
	}
  }
  return (used_entry);
}

int
dist_TimerStop(distributor * o, timerFunc f, void *a, void *b, int entry)
{
	/* find the first timer with matching function and args */
	/* and remove it from the list */
	timerEntry *pe;

	pe = dist_pull_timer(o, f, a, b, entry);
	if (pe == NULL) {
		return (LIB_STATUS_BAD);
	}
	if(o->soonest_timer == pe) {
	  o->soonest_timer = NULL;
	}
	o->timer_cnt--;
	printf("timer_cnt now:%d (reduce 0) pe:%p removed\n", o->timer_cnt, pe);
	FREE(pe);
	if (o->timer_cnt < 0) {
	  printf("timer disparity - 1\n");
	  abort();
	}
	return (LIB_STATUS_GOOD);
}


int
dist_addFd(distributor * o, int fd, serviceFunc fdNotify, int mask, void *arg)
{
	fdEntry *fe;
	int i, fnd, ret;

	/* first allocate a fd service structure and init it */
	fe = (fdEntry *) CALLOC(1, sizeof(fdEntry));
	if (fe == NULL) {
		return (LIB_STATUS_BAD);
	}
	/* init it */
	fe->onTheList = 0;
	fe->fdItServices = fd;
	fe->fdNotify = fdNotify;
	fe->arg = arg;

	/* now place it in the hash table and verify its unique */
	ret = HashedTbl_enterKeyed(o->fdSubscribers,
	    fe->fdItServices,
	    (void *)fe,
	    (void *)&fe->fdItServices,
	    sizeof(int));

	if (ret != LIB_STATUS_GOOD) {
		/* no good, probably already in */
		FREE(fe);
		return (ret);
	}
	/* now add it to the linked list */
	ret = dlist_append(o->fdSubscribersList, (void *)fe);
	if (ret != LIB_STATUS_GOOD) {
		/* now we must back it out of the hash table */
		HashedTbl_removeKeyed(o->fdSubscribers,
		    fe->fdItServices,
		    (void *)&fe->fdItServices,
		    sizeof(int),
		    (void **)NULL);
		FREE(fe);
		return (ret);
	}
	/* now plop in a place in the fd select array */
	/* first do we need to grow array? */
	if ((o->numused + 1) >= o->numfds) {
		/* yes, this will push us to have none spare */
		struct pollfd *tp;

		tp = (struct pollfd *)CALLOC((o->numfds * 2), sizeof(struct pollfd));
		if (tp == NULL) {
			void *v;

			/* can't get bigger we are out of here */
			HashedTbl_removeKeyed(o->fdSubscribers,
			    fe->fdItServices,
			    (void *)&fe->fdItServices,
			    sizeof(int),
			    (void **)NULL);
			/* push pointer so next get will get the last one */
			dlist_getToTheEnd(o->fdSubscribersList);
			/* do that so we can do a getThis() */
			v = dlist_get(o->fdSubscribersList);
			/* now pull it */
			v = dlist_getThis(o->fdSubscribersList);
			FREE(fe);
			return (LIB_STATUS_BAD1);
		}
		memcpy(tp, o->fdlist, (sizeof(struct pollfd) * o->numfds));
		/* init the rest of the struct */
		for (i = o->numfds; i < (o->numfds * 2); i++) {
			tp[i].fd = -1;
			tp[i].events = 0;
			tp[i].revents = 0;
		}
		/* adjust the size */
		o->numfds *= 2;
		/* FREE old list */
		FREE(o->fdlist);
		/* place in our new one */
		o->fdlist = tp;
	}
	fnd = 0;
	/* now lets find a entry and use it */
	for (i = o->numused; i < o->numfds; i++) {
		if (o->fdlist[i].fd == -1) {
			/* found a empty spot */
			o->fdlist[i].fd = fd;
			o->fdlist[i].events = mask;
			o->numused++;
			fnd = 1;
			break;
		}
	}
	if (!fnd) {
		for (i = 0; i < o->numused; i++) {
			if (o->fdlist[i].fd == -1) {
				/* found a empty spot */
				o->fdlist[i].fd = fd;
				o->fdlist[i].events = mask;
				o->numused++;
				fnd = 1;
				break;
			}
		}
	}
	if (!fnd) {
		/* TSNH */
		void *v;

		HashedTbl_removeKeyed(o->fdSubscribers,
		    fe->fdItServices,
		    (void *)&fe->fdItServices,
		    sizeof(int),
		    (void **)NULL);
		/* push pointer so next get will get the last one */
		dlist_getToTheEnd(o->fdSubscribersList);
		/* do that so we can do a getThis() */
		v = dlist_get(o->fdSubscribersList);
		/* now pull it */
		v = dlist_getThis(o->fdSubscribersList);
		FREE(fe);
		return (LIB_STATUS_BAD);
	}
	/* now add it to the list of service functions */
	return (LIB_STATUS_GOOD);
}


int
dist_deleteFd(distributor * o, int fd)
{
	fdEntry *fe;
	int i, removed;

	/* Look up the fd, and remove */
	if (fd == -1)
		return (LIB_STATUS_BAD);

	removed = -1;
	fe = (fdEntry *) HashedTbl_removeKeyed(o->fdSubscribers,
	    fd,
	    (void *)&fd,
	    sizeof(int),
	    (void **)NULL);
	/* find it in the fdSubscribersList and remove */
	dlist_reset(o->fdSubscribersList);
	while ((fe = (fdEntry *) dlist_get(o->fdSubscribersList)) != NULL) {
		if (fe->fdItServices == fd) {
			fe = (fdEntry *) dlist_getThis(o->fdSubscribersList);
		}
	}
	/* remove it from the fdlist and subtract current used */
	for (i = 0; i < o->numfds; i++) {
		if (o->fdlist[i].fd == fd) {
			o->fdlist[i].fd = -1;
			o->fdlist[i].events = 0;
			o->fdlist[i].revents = 0;
			removed = i;
			o->numused--;
			break;
		}
	}
	/* now FREE the memory */
	if (fe != NULL) {
		FREE(fe);
	}
	/* For netbsd we need to collapse the list */
	if (removed != -1) {
		if (removed != o->numused) {
			/*
			 * if removed == o->numused we removed the last one,
			 * no action needed.
			 */
			o->fdlist[removed].fd = o->fdlist[o->numused].fd;
			o->fdlist[removed].events = o->fdlist[o->numused].events;
			o->fdlist[removed].revents = o->fdlist[o->numused].revents;
			o->fdlist[o->numused].fd = -1;
			o->fdlist[o->numused].events = 0;
			o->fdlist[o->numused].revents = 0;
		}
	}
	return (LIB_STATUS_GOOD);
}

int
dist_changeFd(distributor * o, int fd, int newmask)
{
	int i;

	for (i = 0; i < o->numfds; i++) {
		if (o->fdlist[i].fd == fd) {
			o->fdlist[i].events = newmask;
			break;
		}
	}
	return (LIB_STATUS_GOOD);
}

int
__place_mc_in_dlist(dlist_t * dl, msgConsumer * mc)
{
	msgConsumer *at;

	dlist_reset(dl);
	while ((at = (msgConsumer *) dlist_get(dl)) != NULL) {
		/*
		 * if this one has a greater precedence our new one slips in
		 * ahead of the one we are looking at on the list
		 */
		if (at->precedence > mc->precedence) {
			return (dlist_insertHere(dl, (void *)mc));
		}
	}
	/* place it at the end of the list */
	return (dlist_append(dl, (void *)mc));
}

void
__pull_from_dlist(dlist_t * dl, msgConsumer * mc)
{
	msgConsumer *at;

	dlist_reset(dl);
	while ((at = (msgConsumer *) dlist_get(dl)) != NULL) {
		if (at == mc) {
			at = (msgConsumer *) dlist_getThis(dl);
			break;
		}
	}
}

msgConsumer *
__pull_from_dlist2(dlist_t * dl, messageFunc mf, int sctp_proto, int stream_no, void *arg)
{
	msgConsumer *at, *nat;

	nat = NULL;
	dlist_reset(dl);
	while ((at = (msgConsumer *) dlist_get(dl)) != NULL) {
		if (((uint32_t) at->SCTPprotocolId == (uint32_t) sctp_proto) &&
		    (at->SCTPstreamId == stream_no) &&
		    (at->msgNotify == mf) &&
		    (at->arg == arg)) {
			nat = (msgConsumer *) dlist_getThis(dl);
			if (nat != at) {
				/* help */
				abort();
			}
			break;
		}
	}
	return (nat);
}


int
dist_msgSubscribe(distributor * o, messageFunc mf, int sctp_proto, int stream_no, int priority, void *arg)
{
	int ret;
	msgConsumer *mc;
	hashableHash *hh;
	HashedTbl *hofl;
	hashableDlist *hdl;

	/*
	 * Ok, first we look up the hashTable with the sctp_proto number If
	 * we don't find it we add a new hash table for this to the master
	 * hash table. Once we have the master hash table and our sub-hash
	 * table, we then lookup the stream_no to find the hashDlist
	 * structure. With that we add this guy to the list (after
	 * allocating a entry to pop on and fill it in). As a short cut we
	 * look to see if the two are the default values, if so dump it
	 * right in to our pre-stored list.
	 */

	/* first get a msgConsumer */
	mc = (msgConsumer *) CALLOC(1, sizeof(msgConsumer));
	if (mc == NULL) {
		/* no memory, out of here */
		return (LIB_STATUS_BAD);
	}
	mc->precedence = priority;
	mc->SCTPprotocolId = sctp_proto;
	mc->SCTPstreamId = stream_no;
	mc->msgNotify = mf;
	mc->arg = arg;

	/*
	 * now that we have our structure see if we take the short cut.
	 */
	if ((sctp_proto == DIST_SCTP_PROTO_ID_DEFAULT) &&
	    (stream_no == DIST_STREAM_DEFAULT)) {
		ret = __place_mc_in_dlist(o->msgSubZeroNoStrm->list, mc);
		if (ret != LIB_STATUS_GOOD) {
			FREE(mc);
		}
		return (ret);
	}
	/*
	 * ok if we reach here, we must put it on some other list in all of
	 * our hash tables. So much for the short cut :)
	 */
	/*
	 * first find the right hashableHash.
	 */
	hh = (hashableHash *) HashedTbl_lookup(o->msgSubscribers,
	    &sctp_proto,
	    sizeof(uint32_t),
	    (void **)NULL);
	if (hh == NULL) {
		/* no hashable hash table exists for the protocol type */
		/* we must create it */
		char string[32];

		sprintf(string, "protocolId:0x%x", sctp_proto);
		hh = create_hashableHash(sctp_proto, string, 11);
		if (hh == NULL) {
			/* failed to alloc-create */
			FREE(mc);
			return (LIB_STATUS_BAD);
		}
		/* now we must enter it in the master table */
		ret = HashedTbl_enter(o->msgSubscribers, (void *)&hh->protocolId,
		    (void *)hh,
		    sizeof(uint32_t));
		if (ret != LIB_STATUS_GOOD) {
			/* something bad is happening */
			FREE(mc);
			destroy_hashableHash(hh);
			return (LIB_STATUS_BAD);
		}
		/*
		 * ok, we now have the hasable hash in the table and hofl
		 * setup right
		 */
		hofl = hh->hash;
	} else {
		/* we found one */
		hofl = hh->hash;
	}
	/* now lets find the low level hashDlist */
	hdl = (hashableDlist *) HashedTbl_lookupKeyed(hofl,
	    stream_no,
	    &stream_no,
	    sizeof(int),
	    (void **)NULL);
	if (hdl == NULL) {
		/*
		 * it does not exist so we must build one and dump it in the
		 * hofl table.
		 */
		hdl = create_hashdlist(stream_no);
		if (hdl == NULL) {
			/*
			 * if we are in trouble, we don't kill the previous
			 * hash table we built since it may be used again
			 * when the memory problem is cleared.. of course we
			 * may not even have created it too.
			 */
			FREE(mc);
			return (LIB_STATUS_BAD);
		}
		/* now add it to our hash table */
		ret = HashedTbl_enterKeyed(hofl, hdl->streamNo, (void *)hdl, &hdl->streamNo, sizeof(hdl->streamNo));
		if (ret != LIB_STATUS_GOOD) {
			/*
			 * no way to add the dlist to the table.. we are out
			 * of here.
			 */
			FREE(mc);
			destroy_hashDlist(hdl);
			return (LIB_STATUS_BAD);
		}
		/*
		 * ok its in the table and now hdl points to the guy we need
		 * to add to.
		 */
	}
	/* now lets place it on the global list */
	ret = __place_mc_in_dlist(o->msgSubscriberList, mc);
	if (ret != LIB_STATUS_GOOD) {
		FREE(mc);
		return (ret);
	}
	/* now on the individual list */
	ret = __place_mc_in_dlist(hdl->list, mc);
	if (ret != LIB_STATUS_GOOD) {
		__pull_from_dlist(o->msgSubscriberList, mc);
		FREE(mc);
	}
	return (ret);
}

int
dist_msgUnsubscribe(distributor * o, messageFunc mf, int sctp_proto, int stream_no, void *arg)
{
	msgConsumer *mc, *mc2;
	hashableHash *hh;
	HashedTbl *hofl;
	hashableDlist *hdl;

	/* first lets pull it from the master table of whats in the dlist */
	mc2 = mc = __pull_from_dlist2(o->msgSubscriberList, mf, sctp_proto, stream_no, arg);
	hh = (hashableHash *) HashedTbl_lookup(o->msgSubscribers,
	    &sctp_proto,
	    sizeof(int),
	    (void **)NULL);
	if (hh != NULL) {
		hofl = hh->hash;
		hdl = (hashableDlist *) HashedTbl_lookupKeyed(hofl,
		    stream_no,
		    &stream_no,
		    sizeof(int),
		    (void **)NULL);
		if (hdl != NULL) {
			/* ok lets find the entry here */
			mc2 = __pull_from_dlist2(hdl->list, mf, sctp_proto, stream_no, arg);
		}
	}
	if (mc && mc2) {
		if (mc != mc2) {
			/* help! */
			abort();
		}
		/* normal case */
		FREE(mc);
	} else if (mc) {
		FREE(mc);
	} else if (mc2) {
		/* strange should have been in other list */
		FREE(mc2);
	} else {
		return (LIB_STATUS_BAD);
	}
	return (LIB_STATUS_GOOD);
}

int
dist_setDone(distributor * o)
{
	o->notdone = 0;
	return (LIB_STATUS_GOOD);
}

void
__check_time_out_list(distributor * o)
{
	/* some timeout has occured ? */
    timerEntry *te, *nxt;
	struct timerentries *curlist;
	int start, stop, i;
	/* first get the time */
	gettimeofday(&o->lastKnownTime, (struct timezone *)NULL);
	start = o->lasttimeoutchk;
	stop = o->lastKnownTime.tv_sec % TIMERS_NUMBER_OF_ENT;
	/* get each one and have a look */
	for (i=start; i<=stop; i++) {
	  curlist = &o->timers[i];
	  te = LIST_FIRST(curlist);
	  while(te != NULL) {
		nxt = LIST_NEXT(te, next);
		if ((te->expireTime.tv_sec < o->lastKnownTime.tv_sec) ||
		    ((te->expireTime.tv_sec == o->lastKnownTime.tv_sec) &&
		    (te->expireTime.tv_usec < o->lastKnownTime.tv_usec))
		    ) {
		  
		  hlist_append(o->tmp_timer_list, te);
		  LIST_REMOVE(te, next);
		  o->timer_cnt--;
		  if (o->timer_cnt < 0) {
			printf("timer disparity - 2\n");
			abort();
		  }
		} else {
			/* ok the time is beyond this one I am looking at */
			/* we should have no more after this */
			break;
		}
		te = nxt;
	  }
	}
	o->lasttimeoutchk = stop;
	/* call every timer that is up on our temp list */
	hlist_reset(o->tmp_timer_list);
	while ((te = (timerEntry *) hlist_getNext(o->tmp_timer_list)) != NULL) {
		HashedTbl_remove(o->timerhash, (void *)&te->ent, sizeof(hashableTimeEnt), NULL);
		(*te->ent.action) (te->ent.arg1, te->ent.arg2, te->ent.timer_issued_cnt);
		if(o->soonest_timer == te) {
		  o->soonest_timer = NULL;
		}
		FREE(te);
	}
}

void
__sendClockTick(distributor * o)
{
	/* distribute a clock tick to all subscribers for ticks */
	auditTick *at;

	dlist_reset(o->lazyClockTickList);
	while ((at = (auditTick *) dlist_get(o->lazyClockTickList)) != NULL) {
		(*at->tick) (at->arg);
	}
}

void
__sendStartStopEvent(distributor * o, int event)
{
	/* distribute a clock tick to all subscribers for ticks */
	StartStop *at;

	dlist_reset(o->startStopList);
	while ((at = (StartStop *) dlist_get(o->startStopList)) != NULL) {
		if (at->flags & event)
			(*at->activate) (at->arg, event);
	}
}


int
dist_startupChange(distributor * o, startStopFunc sf, void *arg, int howmask)
{
	int chnged;
	StartStop *at;

	chnged = 0;
	dlist_reset(o->startStopList);
	while ((at = (StartStop *) dlist_get(o->startStopList)) != NULL) {
		if ((sf == at->activate) && (arg == at->arg)) {
			chnged = 1;
			at->flags = howmask;
			if (howmask == DIST_DONT_CALL_ME) {
				/* pull it from the list too */
				void *v;

				v = dlist_getThis(o->startStopList);
				if (v != at) {
					printf("Serious error, TSNH!\n");
					return (LIB_STATUS_BAD3);
				} else {
					FREE(at);
				}
			}
		}
	}
	if (!chnged) {
		/* new entry */
		int ret;

		at = (StartStop *) CALLOC(1, sizeof(StartStop));
		if (at == NULL) {
			/* no memory */
			return (LIB_STATUS_BAD);
		}
		at->activate = sf;
		at->flags = howmask;
		at->arg = arg;
		ret = dlist_append(o->startStopList, (void *)at);
		if (ret != LIB_STATUS_GOOD) {
			FREE(at);
			return (LIB_STATUS_BAD);
		}
	}
	return (LIB_STATUS_GOOD);
}


timerEntry *
__dist_get_next_to_expire(distributor *o)
{
  int i;
  timerEntry *te, *lowest;
  if (o->timer_cnt == 0) {
	return (NULL);
  }
  if (o->soonest_timer)
	return (o->soonest_timer);
  /* need to find the next timer to expire */
  lowest = NULL;
  for (i=0; i<TIMERS_NUMBER_OF_ENT; i++) {
	te = LIST_FIRST(&o->timers[i]);
	if (te) {
	  /* is it the lowest? */
	  if (lowest == NULL) {
		/* First one we saw */
		lowest = te;
	  } else {
		/* Ok lets check */
		if(te->expireTime.tv_sec < lowest->tv_sec) {
		  /* A new winner */
		  lowest = te;
		}
		/* We don't check usec, since those all fall
		 * in the same bucket.
		 */
	  }
	}
  }
  /* cache it and return */
  o->soonest_timer = lowest;
  return (lowest);
}
	 
int
dist_process(distributor * o)
{
	int i, ret, errOut;
	int clockTickFlag;
	int doingMultipleReads;
	timerEntry *te;
	fdEntry *fe;
	dlist_t *list1;

	int timeout;

#ifdef USE_SELECT
	struct timeval timeoutt;
	int cnt;
	fd_set readfds, writefds, exceptfds;

#endif

	list1 = dlist_create();
	if (list1 == NULL) {
		printf("Error no memory\n");
		return (LIB_STATUS_BAD);
	}
	i = 0;
	gettimeofday(&o->lastKnownTime, (struct timezone *)NULL);
	doingMultipleReads = 0;
	__sendStartStopEvent(o, DIST_CALL_ME_ON_START);
	/* Set the point on the wheel where we start */
	o->lasttimeoutchk = o->lastKnownTime.tv_sec % TIMERS_NUMBER_OF_ENT;
	while (o->notdone) {
	    te = __dist_get_next_to_expire(o);
#ifdef USE_SELECT
		/* spin through and setup things. */
		FD_ZERO(&readfds);
		FD_ZERO(&writefds);
		FD_ZERO(&exceptfds);
		cnt = 0;
		for (i = 0; i < o->numfds; i++) {
			if (o->fdlist[i].events & POLLIN) {
				if (o->fdlist[i].fd > cnt) {
					cnt = o->fdlist[i].fd;
				}
				FD_SET(o->fdlist[i].fd, &readfds);
			}
			if (o->fdlist[i].events & POLLOUT) {
				if (o->fdlist[i].fd > cnt) {
					cnt = o->fdlist[i].fd;
				}
				FD_SET(o->fdlist[i].fd, &writefds);
			}
			cnt++;
		}
#endif
		/* prepare timeout */
#ifndef MINIMIZE_TIME_CALLS
		gettimeofday(&o->lastKnownTime, (struct timezone *)NULL);
#endif
		if (te != NULL) {
			/* yep, a timer is running use the delta */
			clockTickFlag = 0;
			if (te->expireTime.tv_sec >= o->lastKnownTime.tv_sec) {
				timeout = ((te->expireTime.tv_sec - o->lastKnownTime.tv_sec) * 1000);
				if (te->expireTime.tv_usec > o->lastKnownTime.tv_usec) {
					timeout += ((te->expireTime.tv_usec - o->lastKnownTime.tv_usec) / 1000);
				} else if (te->expireTime.tv_usec < o->lastKnownTime.tv_usec) {
					/*
					 * borrow 1 second from the upper
					 * seconds
					 */
					timeout += (((1000000 - o->lastKnownTime.tv_usec) + te->expireTime.tv_usec) / 1000);
					/*
					 * subtract out the borrowed time
					 * above
					 */
					timeout -= 1000;
				}
				/*
				 * subtract off 10ms this is due to the
				 * clock synchronization issues. What
				 * happens is the O/S will round up leaving
				 * us a drift upwards. This compensates for
				 * that drift, we only do this if we have
				 * more than 11ms to wait.
				 */
				if (timeout > 11) {
					timeout -= 10;
				}
				/* Now round up a bit to make sure we have
				 * a timeout, else wise we end up with
				 * a situation where we have a 3 ms to.
				 */
				timeout += 9;
			} else {
				/* expire time is past? */
				timeout = 0;
			}
			if (timeout <= 0) {
				/* problem, time out has occured */
				__check_time_out_list(o);
				continue;
			}
		} else {
			/* no timeout setup default */
			clockTickFlag = 1;
			timeout = (o->idleClockTick.tv_sec * 1000) + o->idleClockTick.tv_usec / 1000;
		}
#ifdef USE_SELECT
		if (doingMultipleReads) {
			/* poll for fd only */
			timeoutt.tv_sec = 0;
			timeoutt.tv_usec = 0;
		} else {
			timeoutt.tv_sec = (timeout / 1000);
			timeoutt.tv_usec = (timeout % 1000) * 1000;
		}
		ret = select(cnt, &readfds, &writefds, &exceptfds, &timeoutt);
#else
		if (doingMultipleReads) {
			/* non-blocking poll */
			ret = poll(o->fdlist, o->numused, 0);
		} else {
			ret = poll(o->fdlist, o->numused, timeout);
		}
#endif
		/* save off the error from poll/select */
		errOut = errno;
		/*
		 * we always check the timeout updating the time in the
		 * process
		 */
		__check_time_out_list(o);
		if (ret > 0) {
			/* ok we have something to do */
#ifdef USE_SELECT
			/*
			 * translate the select terminology into the poll
			 * fundamentals
			 */
			for (i = 0; i < o->numfds; i++) {
				if (o->fdlist[i].fd == -1)
					continue;
				o->fdlist[i].revents = 0;
				if (FD_ISSET(o->fdlist[i].fd, &readfds)) {
					o->fdlist[i].revents |= POLLIN;
				}
				if (FD_ISSET(i, &writefds)) {
					o->fdlist[i].revents |= POLLOUT;
				}
			}
#endif
			/*
			 * now rip through and call all the appropriate
			 * functions
			 */
			for (i = 0; i < o->numfds; i++) {
				if (o->fdlist[i].fd == -1) {
					continue;
				}
				if (o->fdlist[i].revents == 0) {
					continue;
				}
				/* got a live one */
				fe = (fdEntry *) HashedTbl_lookupKeyed(o->fdSubscribers,
				    o->fdlist[i].fd,
				    &o->fdlist[i].fd,
				    sizeof(int),
				    (void **)NULL);
				if (fe == NULL) {
					/* huh? TSNH */
					o->fdlist[i].fd = -1;
					o->numused--;
					if (i != o->numused) {
						o->fdlist[i].fd = o->fdlist[o->numused].fd;
						o->fdlist[i].events = o->fdlist[o->numused].events;
						o->fdlist[i].revents = o->fdlist[o->numused].revents;
						o->fdlist[o->numused].fd = -1;
						o->fdlist[o->numused].events = 0;
						o->fdlist[o->numused].revents = 0;
					}
					continue;
				}
				if ((*fe->fdNotify) (fe->arg, o->fdlist[i].fd, o->fdlist[i].revents) > 0) {
					if (fe->onTheList == 0) {
						if (dlist_append(list1, (void *)fe) < LIB_STATUS_GOOD) {
							printf("Can't append??\n");
						} else {
							fe->onTheList = 1;
						}
					}
				}
				o->fdlist[i].revents = 0;
			}
		} else if ((ret == 0) && (clockTickFlag) && (!doingMultipleReads)) {
			__sendClockTick(o);
		} else if ((ret == 0) && (doingMultipleReads)) {
			dlist_reset(list1);
			fe = (fdEntry *) dlist_get(list1);
			if ((*fe->fdNotify) (fe->arg, o->fdlist[i].fd, 0) <= 0) {
				void *v;

				fe->onTheList = 0;
				v = dlist_getThis(list1);
				if (v != (void *)fe) {
					printf("Help TSNH!\n");
				}
			}
		}
		if (dlist_getCnt(list1)) {
			/*
			 * if we have some on the list then we need to
			 * arrange to just poll non-blocking
			 */
			doingMultipleReads = 1;
		} else {
			doingMultipleReads = 0;
		}
	}
	__sendStartStopEvent(o, DIST_CALL_ME_ON_STOP);
	dlist_destroy(list1);
	return (LIB_STATUS_GOOD);
}

void
__distribute_msg_o_list(dlist_t * dl, messageEnvolope * msg, uint8_t round_robin)
{
	/* distribute over a list the particular message *msg */
	msgConsumer *at, *beginning;
	int seen_end = 0;

	if (round_robin == 0) {
		/* Non round-robin case */
		dlist_reset(dl);
		beginning = NULL;
		at = (msgConsumer *) dlist_get(dl);
	} else {
		/* Round robin case */
		beginning = at = (msgConsumer *) dlist_get(dl);
		if (at == NULL) {
			/* Left off at end */
			dlist_reset(dl);
			at = (msgConsumer *) dlist_get(dl);
		}
	}
	while (at != NULL) {
		(*at->msgNotify) (at->arg, msg);
		if ((msg->data == NULL) || (msg->totData == NULL)) {
			break;
		}
		at = (msgConsumer *) dlist_get(dl);
		if (at == beginning) {
			break;
		}
		if (at == NULL) {
			/*
			 * This is protection code for round-robin, in the
			 * normal case the msg consumers stay in the list.
			 * But if for some reason the orignal consumer is
			 * removed we have the potential to loop forever. We
			 * set seen_end first time, so worst case is we
			 * rotate through the list twice.
			 */
			if (seen_end) {
				break;
			}
			seen_end = 1;
			dlist_reset(dl);
		}
	}
}


void
dist_sendmessage(distributor * o, messageEnvolope * msg)
{
	hashableHash *hh;
	HashedTbl *hofl;
	hashableDlist *hdl = NULL;

	/* first lets lookup in the master table for the protocol */
	msg->distrib = (void *)o;

	hh = (hashableHash *) HashedTbl_lookup(o->msgSubscribers,
	    &msg->protocolId,
	    sizeof(int),
	    (void **)NULL);
	if (hh != NULL) {
		/* ok do we have a protocolID specific entry? - yes */
		hofl = hh->hash;
		/* is the stream there? */
		hdl = (hashableDlist *) HashedTbl_lookupKeyed(hofl,
		    msg->streamNo,
		    &msg->streamNo,
		    sizeof(msg->streamNo),
		    (void **)NULL);
		if (hdl == NULL) {
			/* no, see if the default stream is there */
			int x;

			x = DIST_STREAM_DEFAULT;
			hdl = (hashableDlist *) HashedTbl_lookupKeyed(hofl,
			    x,
			    &x,
			    sizeof(int),
			    (void **)NULL);
		}
	}
	if (hdl != NULL) {
		/* ok lets send it over this list */
		__distribute_msg_o_list(hdl->list, msg, o->msgs_round_robin);
	}
	/*
	 * if we did not consume and we were on some other list, pass it
	 * over the default list.
	 */
	if ((msg->data != NULL) && (msg->totData != NULL) && (hdl != o->msgSubZeroNoStrm)) {
		__distribute_msg_o_list(o->msgSubZeroNoStrm->list, msg, o->msgs_round_robin);
	}
	if ((msg->totData != NULL) && (msg->takeOk)) {
		FREE(msg->totData);
	}
}



int
dist_wants_audit_tick(distributor * o, auditFunc af, void *arg)
{
	auditTick *at;

	at = (auditTick *) CALLOC(1, sizeof(auditTick));
	if (at == NULL) {
		return (LIB_STATUS_BAD);
	}
	at->arg = arg;
	at->tick = af;
	return (dlist_append(o->lazyClockTickList, (void *)at));
}

int
dist_no_audit_tick(distributor * o, auditFunc af, void *arg)
{
	/* find and remove this guy */
	auditTick *at;

	dlist_reset(o->lazyClockTickList);
	while ((at = (auditTick *) dlist_get(o->lazyClockTickList)) != NULL) {
		if ((at->arg == arg) && (at->tick == af)) {
			/* got it */
			dlist_getThis(o->lazyClockTickList);
			FREE(at);
			return (LIB_STATUS_GOOD);
		}
	}
	return (LIB_STATUS_BAD);
}
void
dist_changelazyclock(distributor * o, struct timeval *tm)
{
	o->idleClockTick.tv_sec = tm->tv_sec;
	o->idleClockTick.tv_usec = tm->tv_usec;
}

int
dist_set_No_Dup_timer(distributor * o)
{
	if (o->timer_cnt) {
		return (LIB_STATUS_BAD);
	}
	o->no_dup_timer = 1;
	return (LIB_STATUS_GOOD);
}

int
dist_clr_No_Dup_timer(distributor * o)
{
	if (o->timer_cnt) {
		return (LIB_STATUS_BAD);
	}
	o->no_dup_timer = 0;
	return (LIB_STATUS_GOOD);
}
