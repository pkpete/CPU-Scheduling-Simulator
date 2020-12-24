// 2020년 / 운영체제 / CPU Schedule / B511006 / 고재욱 
// Date : 2020년 10월 16일 
// Subject : 운영체제
// CPU Schedule Simulator Homework
// Student Number : B511006
// Name : 고재욱
//
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <limits.h>

#define SEED 10

// process states
#define S_IDLE 0			
#define S_READY 1
#define S_BLOCKED 2
#define S_RUNNING 3
#define S_TERMINATE 4

int NPROC, NIOREQ, QUANTUM;

struct ioDoneEvent {
	int procid;
	int doneTime;
	int len;
	struct ioDoneEvent *prev;
	struct ioDoneEvent *next;
} ioDoneEventQueue, *ioDoneEvent;

struct process {
	int id;
	int len;		// for queue
	int targetServiceTime;
	int serviceTime;
	int startTime;
	int endTime;
	char state;
	int priority;
	int saveReg0, saveReg1;
	struct process *prev;
	struct process *next;
} *procTable;

struct process idleProc;
struct process readyQueue;
struct process blockedQueue;
struct process *runningProc;

int cpuReg0, cpuReg1;
int currentTime = 0;
int *procIntArrTime, *procServTime, *ioReqIntArrTime, *ioServTime;

void compute() {
	// DO NOT CHANGE THIS FUNCTION
	cpuReg0 = cpuReg0 + runningProc->id;
	cpuReg1 = cpuReg1 + runningProc->id;
	//printf("In computer proc %d cpuReg0 %d\n",runningProc->id,cpuReg0);
}

void initProcTable() {
	int i;
	for(i=0; i < NPROC; i++) {
		procTable[i].id = i;
		procTable[i].len = 0;
		procTable[i].targetServiceTime = procServTime[i];
		procTable[i].serviceTime = 0;
		procTable[i].startTime = 0;
		procTable[i].endTime = 0;
		procTable[i].state = S_IDLE;
		procTable[i].priority = 0;
		procTable[i].saveReg0 = 0;
		procTable[i].saveReg1 = 0;
		procTable[i].prev = NULL;
		procTable[i].next = NULL;
	}
}

void procExecSim(struct process *(*scheduler)()) {
	int pid, qTime=0, cpuUseTime = 0, nproc=0, termProc = 0, nioreq=0;
	char schedule = 0, nextState = S_IDLE;
	int nextForkTime, nextIOReqTime;
	idleProc.id = -1;
	runningProc = &idleProc;
	nextForkTime = procIntArrTime[nproc];
	nextIOReqTime = ioReqIntArrTime[nioreq];
	int finishProc = 0;
	procIntArrTime[NPROC] = INT_MAX;
	ioReqIntArrTime[NIOREQ] = INT_MAX;

	while(1) {
		int fromQuantum = 0;
		int goToScheduler = 0;
		currentTime++;
		qTime++;
		runningProc->serviceTime++;
		if (runningProc != &idleProc ) cpuUseTime++;

//		printf("\n%d / processing Proc %d / servT %d / targetServT %d / cpuUseTime %d / nproc %d / nioreq %d / ioDoneEvent Length %d / nextForkTime %d / nextIOreq %d\n"
//			, currentTime, runningProc->id, runningProc->serviceTime, runningProc->targetServiceTime, cpuUseTime, nproc, nioreq, ioDoneEventQueue.len, nextForkTime, nextIOReqTime);
		
		// MUST CALL compute() Inside While loop
		compute();

		if (currentTime == nextForkTime) { 
//			printf("process %d added to readyQueue\n", nproc);
			if(readyQueue.len == 0){
				readyQueue.next = &procTable[nproc];
				procTable[nproc].prev = &readyQueue;
			}
			else{
				struct process *tmp = &readyQueue;
				while(tmp->next != NULL)
					tmp = tmp -> next;
				tmp->next = &procTable[nproc];
				procTable[nproc].prev = tmp;
			}
			procTable[nproc].next = NULL;
			procTable[nproc].state = S_READY;
			procTable[nproc].startTime = currentTime;
			readyQueue.len++;
			nextForkTime += procIntArrTime[++nproc];
			goToScheduler = 1;
		}


		int ioDoneEventQueueLen = ioDoneEventQueue.len;
		int i;
		struct ioDoneEvent *tmpIo = &ioDoneEventQueue;
		for(i = 0; i < ioDoneEventQueueLen; i++){	// IO DONE EVENT
			tmpIo = tmpIo->next;
			if(tmpIo->doneTime == currentTime){
				pid = tmpIo->procid;
//				printf("IO Done Event for pid %d \n", tmpIo->procid);
				struct process *queue = &blockedQueue;
				int lenblockedqueue = blockedQueue.len;
				int j;
				for(j = 0; j < lenblockedqueue; j++){ // ioDoneQueue에 있는 프로세스가 blockedQueue에 있으면 pop
					queue = queue->next;
					if(queue->id == pid){
						if(queue->next == NULL){
							queue->prev->next = NULL;
						}
						else{
							queue->prev->next = queue -> next;
							queue->next->prev = queue -> prev;
						}
						queue->next = NULL;
						queue->prev = NULL;
						blockedQueue.len--;
						//blocked에서 나온 큐는 readyQueue로 들어가야된다
						if(queue->state == S_BLOCKED){
							queue->state = S_READY;
							struct process *tmp = &readyQueue;
							while(tmp->next != NULL)
								tmp = tmp -> next;
							tmp->next = queue;
							queue->prev = tmp;
							queue->next = NULL;
							readyQueue.len++;
						}
//						printf("IO Done Event move proc %d to ReadyQueue\n", pid);
//						printf("BlockedQueue %d\n",blockedQueue.len);
						break;
					}
				}
				//ioDoneEvent에서 해당부분을 pop
				if(tmpIo->next != NULL){
					tmpIo->next->prev = tmpIo->prev;
					tmpIo->prev->next = tmpIo->next;
				}
				else{
					tmpIo->prev->next = NULL;
				}
				ioDoneEventQueue.len--;
//				printf("iodoneeventQueue len %d\n", ioDoneEventQueue.len);
				goToScheduler = 1;
			}
		}

		if (qTime == QUANTUM ) { /* CASE 1 : The quantum expires */
//			printf("process %d quantum expires\n", runningProc->id);
			goToScheduler = 1;
			fromQuantum = 1;
			runningProc->priority++;
		}

		if (runningProc->state == S_RUNNING && cpuUseTime == nextIOReqTime) { /* CASE 5: reqest IO operations (only when the process does not terminate) */
//			printf("Proc%d io request\n", runningProc->id);
			//runningProc가 있는 경우에만 생김
			//해당 runningProc가 io요청 -> BLOCKED or TERMINATE
			if(runningProc->serviceTime == runningProc->targetServiceTime){
				runningProc->state = S_TERMINATE;
			}
			else{
				runningProc->state = S_BLOCKED;
//				printf("Proc%d BLOCKED\n", runningProc->id);
				//BLOCKED QUEUE에 넣어야됨
				if(blockedQueue.len == 0){
					blockedQueue.next = runningProc;
					runningProc->prev = &blockedQueue;
					runningProc->next = NULL;
				}else{
					struct process *tmp = &blockedQueue;
					while(tmp->next != NULL)
						tmp = tmp->next;
					tmp->next = runningProc;
					runningProc->prev = tmp;
					runningProc->next = NULL;
				}
				if(fromQuantum == 0)
					runningProc->priority--;
				runningProc->saveReg0 = cpuReg0;
				runningProc->saveReg1 = cpuReg1;
				blockedQueue.len++;
//				printf("BlockedQueue length %d\n", blockedQueue.len);
			}
			//ioDoneEventQueue 만들기
			if(ioDoneEventQueue.len == 0){
				ioDoneEventQueue.next = &ioDoneEvent[nioreq];
				ioDoneEvent[nioreq].prev = &ioDoneEventQueue;
			}
			else{
				struct ioDoneEvent *tmp = &ioDoneEventQueue;
				while(tmp->next != NULL)
					tmp = tmp->next;
				tmp->next = &ioDoneEvent[nioreq];
				ioDoneEvent[nioreq].prev = tmp;
			}
			ioDoneEvent[nioreq].next = NULL;
			ioDoneEvent[nioreq].doneTime = currentTime + ioServTime[nioreq];
			ioDoneEvent[nioreq].procid = runningProc->id;
			ioDoneEventQueue.len++;
//			printf("ioDoneEventQueue Length %d\n", ioDoneEventQueue.len);
//			printf("DoneTime %d\n", ioDoneEvent[nioreq].doneTime);
			nextIOReqTime += ioReqIntArrTime[++nioreq];
			goToScheduler = 1;
		}

		if (runningProc->serviceTime == runningProc->targetServiceTime) { /* CASE 4 : the process job done and terminates */	
//			printf("process %d termainates. start time %d ~ end time %d\n", runningProc->id, runningProc->startTime, currentTime);
			runningProc->state = S_TERMINATE;
			runningProc->endTime = currentTime;
			runningProc->saveReg0 = cpuReg0;
			runningProc->saveReg1 = cpuReg1;
			goToScheduler = 1;
			finishProc++;
			if(finishProc == NPROC)
				break;
		}
		
		// call scheduler() if needed
		if(goToScheduler == 1){
			runningProc -> saveReg0 = cpuReg0;
			runningProc -> saveReg1 = cpuReg1;
			// 현재 프로세스가 running중이면 readyQueue 뒤에 넣기
			if(runningProc->state == S_RUNNING){
				runningProc->state = S_READY;
				struct process *tmp = &readyQueue;
				while(tmp->next != NULL)
					tmp = tmp->next;
				tmp->next = runningProc;
				runningProc->prev = tmp;
				runningProc->next = NULL;
				readyQueue.len++;
			}
//			printf("ReadyQueue length %d\n", readyQueue.len);
			scheduler();
//			printf("ReadyQueue length %d\n", readyQueue.len);
//			printf("Scheduler selects process %d\n", runningProc->id);
			qTime = 0;
		}
	} // while loop
}

//RR,SJF(Modified),SRTN,Guaranteed Scheduling(modified),Simple Feed Back Scheduling
struct process* RRschedule() {
	//가장 빠르게 들어온 큐를 runningProc로 pop
	if(readyQueue.len != 0){
		struct process *tmp = &readyQueue;
		tmp = tmp -> next;
		readyQueue.next = tmp->next;
		if(tmp->next != NULL){
			tmp->next->prev = &readyQueue;
		}	
		runningProc = tmp;
		runningProc->prev = NULL;
		runningProc->next = NULL;
		readyQueue.len--;
		runningProc->state = S_RUNNING;
		cpuReg0 = runningProc -> saveReg0;
		cpuReg1 = runningProc -> saveReg1;
		return runningProc;
	}
	else
		return runningProc = &idleProc;
}
struct process* SJFschedule() {
	//readyQueue에 있는 프로세스 중 targetServiceTime이 제일 적은것을 pop
	if(readyQueue.len!=0){
		struct process *tmp = &readyQueue;
		tmp = tmp ->next;
		struct process *search = &readyQueue;
		while(search->next != NULL){ //search로 readyQueue 끝까지 간다
			search = search -> next;
			if(search->targetServiceTime < tmp->targetServiceTime)
				tmp = search;	
		}
		if(search->targetServiceTime < tmp->targetServiceTime)
			tmp = search;
		if(tmp->next != NULL){
			tmp->prev->next = tmp->next;
			tmp->next->prev = tmp->prev;
		}
		else
			tmp->prev->next = NULL;
		runningProc = tmp;
		runningProc->prev = NULL;
		runningProc->next = NULL;
		readyQueue.len--;
		runningProc->state = S_RUNNING;
		cpuReg0 = runningProc->saveReg0;
		cpuReg1 = runningProc->saveReg1;
		return runningProc;
	}
	else
		return runningProc = &idleProc;
}
struct process* SRTNschedule() {
	//readyQueue에서 남아 있는 수행시간(targetServiceTime - serviceTime)이 가장 작은 프로세스를 pop
	if(readyQueue.len!=0){
		struct process *tmp = &readyQueue;
		tmp = tmp-> next;
		struct process *search = &readyQueue;
		while(search->next != NULL){
			search = search -> next;
			if((search->targetServiceTime) - (search->serviceTime) < (tmp -> targetServiceTime) - (tmp->serviceTime))
				tmp = search;
		}
		int searchNum = (search->targetServiceTime) - (search->serviceTime);
		int tmpNum = (tmp->targetServiceTime) - (tmp->serviceTime);
		if(searchNum < tmpNum)
			tmp = search;
		if(tmp->next != NULL){
			tmp->prev->next = tmp->next;
			tmp->next->prev = tmp->prev;
		}
		else
			tmp->prev->next = NULL;
		runningProc = tmp;
		runningProc->prev = NULL;
		runningProc->next = NULL;
		readyQueue.len--;
		runningProc->state = S_RUNNING;
		cpuReg0 = runningProc -> saveReg0;
		cpuReg1 = runningProc -> saveReg1;
		return runningProc;
	}
	else
		return runningProc = &idleProc;
}
struct process* GSschedule() {
	//Ready Queue에 있는 프로세스 중에 serviceTime/targetServiceTime 이 가장 작은걸 pop
	if(readyQueue.len!=0){
    	struct process *tmp = &readyQueue;
		tmp = tmp-> next;
        struct process *search = &readyQueue;
        while(search->next != NULL){
	   		 search = search -> next;
			 double searchNum = (double)(search->serviceTime) / (search->targetServiceTime) ;
			 double tmpNum = (double)(tmp->serviceTime) / (tmp->targetServiceTime);
 	      	 if(searchNum < tmpNum)
			    tmp = search;
       	}
	    double searchNum = (double)(search->serviceTime) / (search->targetServiceTime);
	    double tmpNum = (double)(tmp->serviceTime) / (tmp->targetServiceTime);
	    if(searchNum < tmpNum)
		    tmp = search;
	    if(tmp->next != NULL){
		    tmp->prev->next = tmp->next;
	        tmp->next->prev = tmp->prev;
        }
        else
	        tmp->prev->next = NULL;
        runningProc = tmp;
		runningProc->prev = NULL;
		runningProc->next = NULL;
		readyQueue.len--;
		runningProc->state = S_RUNNING;
		cpuReg0 = runningProc -> saveReg0;
		cpuReg1 = runningProc -> saveReg1;
		return runningProc;
	}
	else
		return runningProc = &idleProc;
}
struct process* SFSschedule() {
	//Quantum을 다 사용 못하고 io request 발생하는 프로세스 -> 우선순위 증가
	//Quantum을 다 사용하고 강제적으로 스위치 되는 경우 -> 우선순위 감소
	//우선순위가 가장 높은걸 뽑아
	 if(readyQueue.len!=0){
		 struct process *tmp = &readyQueue;
		 tmp = tmp-> next;
		 struct process *search = &readyQueue;
		 while(search->next != NULL){
			 search = search -> next;
			 if(search->priority < tmp->priority)
	 			tmp = search;
		    }
			 if(search->priority < tmp->priority)
				 tmp = search;
			 if(tmp->next != NULL){
				 tmp->prev->next = tmp->next;
				 tmp->next->prev = tmp->prev;
			 }
			 else
				 tmp->prev->next = NULL;
			 runningProc = tmp;
			 runningProc->prev = NULL;
			 runningProc->next = NULL;
			 readyQueue.len--;
			 runningProc->state = S_RUNNING;
			 cpuReg0 = runningProc -> saveReg0;
			 cpuReg1 = runningProc -> saveReg1;
			 return runningProc;
     }
	 else
		 return runningProc = &idleProc;
}
	 
void printResult() {
	// DO NOT CHANGE THIS FUNCTION
	int i;
	long totalProcIntArrTime=0,totalProcServTime=0,totalIOReqIntArrTime=0,totalIOServTime=0;
	long totalWallTime=0, totalRegValue=0;
	for(i=0; i < NPROC; i++) {
		totalWallTime += procTable[i].endTime - procTable[i].startTime;
		/*
		printf("proc %d serviceTime %d targetServiceTime %d saveReg0 %d\n",
			i,procTable[i].serviceTime,procTable[i].targetServiceTime, procTable[i].saveReg0);
		*/
		totalRegValue += procTable[i].saveReg0+procTable[i].saveReg1;
		/* printf("reg0 %d reg1 %d totalRegValue %d\n",procTable[i].saveReg0,procTable[i].saveReg1,totalRegValue);*/
	}
	for(i = 0; i < NPROC; i++ ) { 
		totalProcIntArrTime += procIntArrTime[i];
		totalProcServTime += procServTime[i];
	}
	for(i = 0; i < NIOREQ; i++ ) { 
		totalIOReqIntArrTime += ioReqIntArrTime[i];
		totalIOServTime += ioServTime[i];
	}
	
	printf("Avg Proc Inter Arrival Time : %g \tAverage Proc Service Time : %g\n", (float) totalProcIntArrTime/NPROC, (float) totalProcServTime/NPROC);
	printf("Avg IOReq Inter Arrival Time : %g \tAverage IOReq Service Time : %g\n", (float) totalIOReqIntArrTime/NIOREQ, (float) totalIOServTime/NIOREQ);
	printf("%d Process processed with %d IO requests\n", NPROC,NIOREQ);
	printf("Average Wall Clock Service Time : %g \tAverage Two Register Sum Value %g\n", (float) totalWallTime/NPROC, (float) totalRegValue/NPROC);
	
}

int main(int argc, char *argv[]) {
	// DO NOT CHANGE THIS FUNCTION
	int i;
	int totalProcServTime = 0, ioReqAvgIntArrTime;
	int SCHEDULING_METHOD, MIN_INT_ARRTIME, MAX_INT_ARRTIME, MIN_SERVTIME, MAX_SERVTIME, MIN_IO_SERVTIME, MAX_IO_SERVTIME, MIN_IOREQ_INT_ARRTIME;
	
	if (argc < 12) {
		printf("%s: SCHEDULING_METHOD NPROC NIOREQ QUANTUM MIN_INT_ARRTIME MAX_INT_ARRTIME MIN_SERVTIME MAX_SERVTIME MIN_IO_SERVTIME MAX_IO_SERVTIME MIN_IOREQ_INT_ARRTIME\n",argv[0]);
		exit(1);
	}
	
	SCHEDULING_METHOD = atoi(argv[1]);
	NPROC = atoi(argv[2]);
	NIOREQ = atoi(argv[3]);
	QUANTUM = atoi(argv[4]);
	MIN_INT_ARRTIME = atoi(argv[5]);
	MAX_INT_ARRTIME = atoi(argv[6]);
	MIN_SERVTIME = atoi(argv[7]);
	MAX_SERVTIME = atoi(argv[8]);
	MIN_IO_SERVTIME = atoi(argv[9]);
	MAX_IO_SERVTIME = atoi(argv[10]);
	MIN_IOREQ_INT_ARRTIME = atoi(argv[11]);
	
	printf("SIMULATION PARAMETERS : SCHEDULING_METHOD %d NPROC %d NIOREQ %d QUANTUM %d \n", SCHEDULING_METHOD, NPROC, NIOREQ, QUANTUM);
	printf("MIN_INT_ARRTIME %d MAX_INT_ARRTIME %d MIN_SERVTIME %d MAX_SERVTIME %d\n", MIN_INT_ARRTIME, MAX_INT_ARRTIME, MIN_SERVTIME, MAX_SERVTIME);
	printf("MIN_IO_SERVTIME %d MAX_IO_SERVTIME %d MIN_IOREQ_INT_ARRTIME %d\n", MIN_IO_SERVTIME, MAX_IO_SERVTIME, MIN_IOREQ_INT_ARRTIME);
	
	srandom(SEED);
	
	// allocate array structures
	procTable = (struct process *) malloc(sizeof(struct process) * NPROC);
	ioDoneEvent = (struct ioDoneEvent *) malloc(sizeof(struct ioDoneEvent) * NIOREQ);
	procIntArrTime = (int *) malloc(sizeof(int) * NPROC);
	procServTime = (int *) malloc(sizeof(int) * NPROC);
	ioReqIntArrTime = (int *) malloc(sizeof(int) * NIOREQ);
	ioServTime = (int *) malloc(sizeof(int) * NIOREQ);

	// initialize queues
	readyQueue.next = readyQueue.prev = &readyQueue;
	
	blockedQueue.next = blockedQueue.prev = &blockedQueue;
	ioDoneEventQueue.next = ioDoneEventQueue.prev = &ioDoneEventQueue;
	ioDoneEventQueue.doneTime = INT_MAX;
	ioDoneEventQueue.procid = -1;
	ioDoneEventQueue.len  = readyQueue.len = blockedQueue.len = 0;
	
	// generate process interarrival times
	for(i = 0; i < NPROC; i++ ) { 
		procIntArrTime[i] = random()%(MAX_INT_ARRTIME - MIN_INT_ARRTIME+1) + MIN_INT_ARRTIME;
	}
//	procIntArrTime[NPROC] = INT_MAX;
//	printf("Process Interarrival Time: \n");
//	for(i = 0; i < NPROC; i++)
//		printf("%d ", procIntArrTime[i]);
	
	
	// assign service time for each process
	for(i=0; i < NPROC; i++) {
		procServTime[i] = random()% (MAX_SERVTIME - MIN_SERVTIME + 1) + MIN_SERVTIME;
		totalProcServTime += procServTime[i];	
	}
//	printf("\nProcess Target Service Time : \n");
//	for(i = 0; i < NPROC; i++)
//		printf("%d ", procServTime[i]);
	
	ioReqAvgIntArrTime = totalProcServTime/(NIOREQ+1);
	assert(ioReqAvgIntArrTime > MIN_IOREQ_INT_ARRTIME);
	
	// generate io request interarrival time
	for(i = 0; i < NIOREQ; i++ ) { 
		ioReqIntArrTime[i] = random()%((ioReqAvgIntArrTime - MIN_IOREQ_INT_ARRTIME)*2+1) + MIN_IOREQ_INT_ARRTIME;
	}
//	ioReqIntArrTime[NIOREQ] = INT_MAX;
//	printf("\nIO Req Interarrival Time :\n");
//	for(i = 0; i < NIOREQ; i++)
//		printf("%d ", ioReqIntArrTime[i]);
	
	// generate io request service time
	for(i = 0; i < NIOREQ; i++ ) { 
		ioServTime[i] = random()%(MAX_IO_SERVTIME - MIN_IO_SERVTIME+1) + MIN_IO_SERVTIME;
	}
//	printf("\nIO Req Service Time :\n");
//	for(i = 0; i < NIOREQ; i++)
//		printf("%d ", ioServTime[i]);
//	printf("\n");
	
#ifdef DEBUG
	// printing process interarrival time and service time
	printf("Process Interarrival Time :\n");
	for(i = 0; i < NPROC; i++ ) { 
		printf("%d ",procIntArrTime[i]);
	}
	printf("\n");
	printf("Process Target Service Time :\n");
	for(i = 0; i < NPROC; i++ ) { 
		printf("%d ",procTable[i].targetServiceTime);
	}
	printf("\n");
#endif
	
	// printing io request interarrival time and io request service time
	printf("IO Req Average InterArrival Time %d\n", ioReqAvgIntArrTime);
	printf("IO Req InterArrival Time range : %d ~ %d\n",MIN_IOREQ_INT_ARRTIME,
			(ioReqAvgIntArrTime - MIN_IOREQ_INT_ARRTIME)*2+ MIN_IOREQ_INT_ARRTIME);
			
#ifdef DEBUG		
	printf("IO Req Interarrival Time :\n");
	for(i = 0; i < NIOREQ; i++ ) { 
		printf("%d ",ioReqIntArrTime[i]);
	}
	printf("\n");
	printf("IO Req Service Time :\n");
	for(i = 0; i < NIOREQ; i++ ) { 
		printf("%d ",ioServTime[i]);
	}
	printf("\n");
#endif
	
	struct process* (*schFunc)();
	switch(SCHEDULING_METHOD) {
		case 1 : schFunc = RRschedule; break;
		case 2 : schFunc = SJFschedule; break;
		case 3 : schFunc = SRTNschedule; break;
		case 4 : schFunc = GSschedule; break;
		case 5 : schFunc = SFSschedule; break;
		default : printf("ERROR : Unknown Scheduling Method\n"); exit(1);
	}
	initProcTable();
	procExecSim(schFunc);
	printResult();
}
