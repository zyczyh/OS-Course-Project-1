/* resched.c  -  resched */

#include <conf.h>
#include <kernel.h>
#include <proc.h>
#include <q.h>

unsigned long currSP;	/* REAL sp of current process */
int schedtype;
extern int ctxsw(int, int, int, int);
/*-----------------------------------------------------------------------
 * resched  --  reschedule processor to highest priority ready process
 *
 * Notes:	Upon entry, currpid gives current process id.
 *		Proctab[currpid].pstate gives correct NEXT state for
 *			current process if other than PRREADY.
 *------------------------------------------------------------------------
 */
void setschedclass(int sched_class)
{
	schedtype = sched_class;
}

int getschedclass()
{
	return schedtype;
}

void updatecurrproc(register struct	pentry *currp)
{
	int priority = 0;

	priority = currp->goodness - currp->qremain;
	currp->qremain = preempt;
	currp->goodness = priority + currp->qremain;
	if(currp->qremain <= 0 || currpid == NPROC)
	{
		currp->goodness = 0;
		currp->qremain = 0;
	}
}

int findnextproc()
{
	int procid = 0;
	int gvalue = 0;
	int ansproc = 0;

	procid = q[rdyhead].qnext;
	while(procid != rdytail)
	{
		if(proctab[procid].goodness > gvalue)
		{
			gvalue = proctab[procid].goodness;
			ansproc = procid;
		}
		procid = q[procid].qnext;
	}
	return ansproc;
}

void epochupdate()
{
	int i = 0;

	for(i = 0; i < NPROC; i = i + 1)
	{
		if(proctab[i].pstate != PRFREE)
		{
			if(proctab[i].qremain == 0 || proctab[i].qremain == proctab[i].quantum)
			{
				proctab[i].quantum = proctab[i].pprio;
			}
			else
			{
				proctab[i].quantum = proctab[i].pprio + proctab[i].qremain / 2;
			}
			proctab[i].qremain = proctab[i].quantum;
			proctab[i].goodness = proctab[i].qremain + proctab[i].pprio;
		}
	}
}

int resched()
{
	register struct	pentry	*optr;	/* pointer to old process entry */
	register struct	pentry	*nptr;	/* pointer to new process entry */
	int index = 0;

	if(getschedclass() == AGESCHED)
	{
		/* increase the priority of processes in ready queue */

		index = q[rdyhead].qnext;
		while(index != rdytail)
		{
			if(index != NULLPROC)
			{
				q[index].qkey = q[index].qkey + 1;
			}
			index = q[index].qnext;
		}
		/* no switch needed if current process priority higher than next*/

		if ( ( (optr= &proctab[currpid])->pstate == PRCURR) &&
	   	(lastkey(rdytail)<optr->pprio)) {
			return(OK);
		}
	
		/* force context switch */

		if (optr->pstate == PRCURR) {
			optr->pstate = PRREADY;
			insert(currpid,rdyhead,optr->pprio);
		}

		/* remove highest priority process at end of ready list */

		nptr = &proctab[ (currpid = getlast(rdytail)) ];
		nptr->pstate = PRCURR;		/* mark it currently running	*/
		#ifdef	RTCLOCK
			preempt = QUANTUM;		/* reset preemption counter	*/
		#endif
	
		ctxsw((int)&optr->pesp, (int)optr->pirmask, (int)&nptr->pesp, (int)nptr->pirmask);
	
		/* The OLD process returns here when resumed. */
		return OK;
	}
	else if(getschedclass() == LINUXSCHED)
	{
		optr = &proctab[currpid];
		updatecurrproc(optr);
		index = findnextproc();
		if (index == 0 && (optr->pstate != PRCURR || optr->qremain == 0))
		{
			epochupdate();
			preempt = optr->qremain;
			updatecurrproc(optr);
			index = findnextproc();
		}
		if(index == 0 && (optr->pstate != PRCURR || optr->qremain == 0))
		{
			if(currpid != NULLPROC)
			{
				if(optr->pstate == PRCURR)
				{
					optr->pstate = PRREADY;
					insert(currpid,rdyhead,optr->pprio);
				}
				nptr = &proctab[NULLPROC];
				nptr->pstate = PRCURR;
				dequeue(NULLPROC);
				currpid = NULLPROC;
				#ifdef	RTCLOCK
					preempt = QUANTUM;
				#endif
				ctxsw((int)&optr->pesp, (int)optr->pirmask, (int)&nptr->pesp, (int)nptr->pirmask);
				return OK;
			}
			else
			{
				return OK;
			}
		}
		else if(optr->goodness >= proctab[index].goodness && optr->goodness > 0 && optr->pstate == PRCURR)
		{
			preempt = optr->qremain;
			return OK;
		}
		else if (proctab[index].goodness > 0 && (optr->pstate != PRCURR || optr->qremain == 0 || optr->goodness < proctab[index].goodness))
		{
			if(optr->pstate == PRCURR)
			{
				optr->pstate = PRREADY;
				insert(currpid,rdyhead,optr->pprio);
			}
			nptr = &proctab[index];
			nptr->pstate = PRCURR;
			dequeue(index);
			currpid = index;
			preempt = nptr->qremain;
			ctxsw((int)&optr->pesp, (int)optr->pirmask, (int)&nptr->pesp, (int)nptr->pirmask);
			return OK;
		}
		else
		{
			return SYSERR;
		}
	}
	else
	{
		if ( ( (optr= &proctab[currpid])->pstate == PRCURR) &&
	   (lastkey(rdytail)<optr->pprio)) {
		return(OK);
		}
	
	/* force context switch */

		if (optr->pstate == PRCURR) {
			optr->pstate = PRREADY;
			insert(currpid,rdyhead,optr->pprio);
		}

	/* remove highest priority process at end of ready list */

		nptr = &proctab[ (currpid = getlast(rdytail)) ];
		nptr->pstate = PRCURR;		/* mark it currently running	*/
		#ifdef	RTCLOCK
			preempt = QUANTUM;		/* reset preemption counter	*/
		#endif
	
		ctxsw((int)&optr->pesp, (int)optr->pirmask, (int)&nptr->pesp, (int)nptr->pirmask);
	
		/* The OLD process returns here when resumed. */
		return OK;
	}
}
