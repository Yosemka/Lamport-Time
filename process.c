#include "process.h"


void WriteToHistory(BalanceHistory * history, BalanceState * balance, timestamp_t time)
{
	balance->s_time = time;
	balance->s_balance += balance->s_balance_pending_in;
	balance->s_balance_pending_in = 0;
	history->s_history_len++;
	history->s_history[(int)history->s_history_len - 1] = *balance;
}

void FillEmptyHistoryPoints(BalanceHistory * history, BalanceState * balance, timestamp_t time)
{
	if(balance->s_balance_pending_in != 0)
	{
		balance->s_time++;
		balance->s_balance += balance->s_balance_pending_in;
		balance->s_balance_pending_in = 0;
		history->s_history_len += 1;	
		history->s_history[(int)history->s_history_len - 1] = *balance;
	}
	//1 - 5
	if(time - balance->s_time > 1)
	{	//1-4
		//printf("%d )) Common TIME = %d, BALANCE TIME = %d History len = %d\n", (int)getpid(), (int)time, (int)balance->s_time, history->s_history_len);
		for(int i = balance->s_time + 1; i <= time; i++)
		{
			balance->s_time += 1;
			balance->s_balance_pending_in = 0;
			history->s_history_len += 1;	
			history->s_history[(int)history->s_history_len - 1] = *balance;
		}
	}
}

void CreatePipes(int procAmount, FILE * file)
{
	int countPipes = 0;		//счетчик активных/созданных/используемых каналов
	int flag;
	for(int i = 0; i <= procAmount; i++)	//Всего каналов надо ([количество доч. проц.] + 1)*2
	{
		for(int j = 0; j <= procAmount; j++)
		{
			
			if(i != j)		//pipe[i][i] внутри одного процесса не нужны
			{
				//if(pipe2(pipes[i][j].field, O_NONBLOCK) == -1)	//Ошибка при создании канала
				if(pipe(pipes[i][j].field) == -1)	//Ошибка при создании канала
				{
					printf("Error on creating pipe %d -> %d. Exiting...\n", i, j);
					exit(0);
				}
				else
				{
					flag = fcntl(pipes[i][j].field[READ], F_GETFL, 0);
					fcntl(pipes[i][j].field[READ], F_SETFL, flag | O_NONBLOCK);

					flag = fcntl(pipes[i][j].field[WRITE], F_GETFL, 0);
					fcntl(pipes[i][j].field[WRITE], F_SETFL, flag | O_NONBLOCK);

					if(fprintf(file, "Pipe from %d to %d created, R: %d  W: %d\n",
						   i, j, pipes[i][j].field[READ], pipes[i][j].field[WRITE]) == 0)
					{
						printf("Error writing on \"%s\" pipe[%d][%d]", pipes_log, i, j);
					}

					//printf("Pipe from ch[%d] to ch[%d] created, R: %d  W: %d\n",
					//	   i, j, pipes[i][j].field[READ], pipes[i][j].field[WRITE]);
					
					countPipes++;	//увеличение счетчика при успешном создании канала
				}
			}
		}
	}
	//printf("%d Pipes created\n", countPipes);
}

/*
 *  text - константные значения сообщений из файла "pa1.h"
 *  file - дескриптор открытого лог-файла (сделать проверку)
 */
void WriteEventLog(const char *text, FILE *file, ...)
{	
	va_list vars;			//Хранит список аргументов после аргумента file
	va_start(vars, file);	//из библиотеки <stdarg.h>
	//Применение описано https://metanit.com/cpp/c/5.13.php

	if(vfprintf(file, text, vars) == 0)	//Неуспешная запись в лог-файл
	{
		printf ("Error writing on \"event.log\" ");
		printf(text, vars);
	}
	va_end(vars);
}

void WritePipeLog(FILE *file, int from, int to, char* type, int curPID)
{ 
	if(fprintf(file, "Pipe from %d to %d closed to %s in process %d\n", from, to, type, curPID) == 0)
	{
		printf("Error writing on \"pipes.log\" closing pipe from %d to %d to %s in process %d\n", from, to, type, curPID);
	}
}

/*
 * Проверка атрибута после симвода 'p'
 */
int CheckOptionAndGetValue(int argc, char *argv[])
{
	int option;
	int childAmount = 0;
	while((option = getopt(argc, argv, "p:")) != UNSUCCESS)	//"p:" - двоеточие говорит, что p обязателен
	{
		switch(option)	//getopt возвращает символ аргумента, а optarg хранит значение аргумента
		{				//то есть optarg количество доч. процессов
			case('p'):
			{
				//printf("p Argumnet %s\n%d\n", optarg, argc);
				if((childAmount = atoi(optarg)) == 0)
				{
					printf("Incorrect 'child process amount' value");
					return UNSUCCESS;
				}
				else if(childAmount > MAX_PROCESS_ID)	//Если число выше MAX_PROCESS_ID (15)...
				{
					printf("'child process amount' couldnt be more than %d", MAX_PROCESS_ID); 
					childAmount = MAX_PROCESS_ID;		//... то установить количество доч. проц. равным MAX_PROCESS_ID
				}
				else 
				{
					//Ошибок нет, считаем количество аргументов после -p
					if(childAmount == argc - 3)	//argv[0]=main.exe, argv[1]=-p, argv[2]=childAmount, argv[3]=баланс_1 и т.д.
					{
						for(int i = 3; i < argc; i++)
						{
							if(IsOnlyDigits(argv[i]) == SUCCESS)
								pidBalance[i - 2] = atoi(argv[i]);
							else
							{
								printf("Incorrect input number\n");
								return UNSUCCESS;
							}
							//printf("%d) %d", i-2, pidBalance[i - 2]);
						}
					}
					else
					{
						printf("Incorrect number of arguments after -p\n");
						return UNSUCCESS;
					}
				}
				break;			
			}				
			case('?'): printf("Option -p needs an argument"); return UNSUCCESS;
			default: printf("No such option"); return UNSUCCESS;
		}
	}
	return childAmount;
}

char IsOnlyDigits(char* str)
{
	for(int i = 0; i < strlen(str); i++)
	{
		if(str[i] == '0' ||
		   str[i] == '1' ||
		   str[i] == '2' ||
		   str[i] == '3' ||
		   str[i] == '4' ||
		   str[i] == '5' ||
		   str[i] == '6' ||
		   str[i] == '7' ||
		   str[i] == '8' ||
		   str[i] == '9')
			return SUCCESS;
		else
			return UNSUCCESS;
	}
	return UNSUCCESS;
}
