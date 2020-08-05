#ifndef __IFMO_DISTRIBUTED_CLASS_PROCESS__H
#define __IFMO_DISTRIBUTED_CLASS_PROCESS__H

#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>
#include <time.h>
#include <stdarg.h>
#include <getopt.h>
#include <fcntl.h>
#include <errno.h>
#include "banking.h"
#include "pa2345.h"

#define READ 0
#define WRITE 1
#define SUCCESS 0
#define UNSUCCESS -1
#define TIMESTAMP_2020 1577836800

#define MAX(x, y) (((x) > (y)) ? (x) : (y))

typedef struct 
{
	int field[2];
}Pipes;



int 		chProcAmount;
pid_t 		childPID[MAX_PROCESS_ID];
local_id 	currentID;
Pipes 		pipes[MAX_PROCESS_ID][MAX_PROCESS_ID];
int 		pidBalance[MAX_PROCESS_ID];

//int localTime;
//int 		msgTime;
//char 		newEvent;
timestamp_t localTime;
static char* messageType[] = {"STARTED", "DONE", "ACK", "STOP", "TRANSFER", "BALANCE_HISTORY"};

//прототипы
int CheckOptionAndGetValue(int, char**);
void CreatePipes(int, FILE*);
void CreateChilds(int);
void WriteEventLog(const char *, FILE *, ...);
void WritePipeLog(FILE *, int, int, char*, int);
char IsOnlyDigits(char* str);
void WriteToHistory(BalanceHistory * history, BalanceState * balance, timestamp_t time);
void FillEmptyHistoryPoints(BalanceHistory * history, BalanceState * balance, timestamp_t time);



#endif 
