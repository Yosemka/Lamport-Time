#include "common.h"
#include "process.h"
#include "ipc.h"

pid_t pid = 1;


int main(int argc, char *argv[])
{
	//printf("I am %d parent process\n", (int)getpid());
	chProcAmount = 0;
	currentID = 0;
	childPID[PARENT_ID] = getpid();

	localTime = 0;

	if((chProcAmount = CheckOptionAndGetValue(argc, argv)) <= 0)
	{
		printf("Must specify -p option...");
		exit(1);
	}

	//Создание дескрипторов файлов для логирования pipe и events		
	FILE *fileEv, *filePipe;

	//Открытие файла для добавления логов каналов
	filePipe = fopen(pipes_log, "w");

	//Создание pipe-ов каналов, pipe[Из][Куда] 
	//например pipe[1][2] передает от доч.проц. 1 в доч.проц. 2
	CreatePipes(chProcAmount, filePipe);	

	fclose(filePipe);
	filePipe = fopen(pipes_log, "a");

	//Открытие файла для добавления логов событий
	fileEv = fopen(events_log, "a");
	
	//Создание дочерних процессов
	for(int i = 1; i <= chProcAmount; i++)
	{
		pid = fork();	//Возвращает PID дочернего процесса, если он все еще в родительском процессе		
			if(pid == -1)	//Не удалось создать дочерний процесс
			{	
				printf("Error on creating child %d. Exiting...\n", i);
				exit(0);
			}
			//Выполняется в дочернем процессе
			if(pid == 0)	//Сейчас в дочернем процессе
			{
				currentID = i;	//currentID хранит локальный id процесса
				break;			//выйти из цикла, ибо в дочернем незачем создавать процессы
			}
		//printf("Child %d was created %d\n", i, pid);
		childPID[i] = pid;
	}

	//2 дочерних -> 6 каналов, 
	for(int i = 0; i <= chProcAmount; i++)
	{
		for(int j = 0; j <= chProcAmount; j++)
		{
			if(i != currentID && i != j)
			{
				//Например, в дочернем процессе currentID = 2 
				//Закрыть pipe[0][1], pipe[1][0]  на запись
				close(pipes[i][j].field[WRITE]);
				WritePipeLog (filePipe, i, j, "WRITE", (int)getpid());
			}
			if(j != currentID && i != j)
			{
				//Например, в дочернем процессе currentID = 2 
				//Закрыть pipe[1][0], pipe[0][1]  на чтение
				close(pipes[i][j].field[READ]);
				WritePipeLog (filePipe, i, j, "READ", (int)getpid());
			}
		}
	}
	
	///В дочернем процессе
	if(pid == 0)	//Один из дочерних процессов
	{			
		Message msg;
		TransferOrder *receivedOrder;
		BalanceState balance = {pidBalance[currentID], get_lamport_time (), 0};
		BalanceHistory history = {currentID, 1, };
		history.s_history[0] = balance;
		int resultStarted = 0;
		int resultDone = 0;
		int from = 0;
		//printf("I am %d child with PID %d and balance %d\n", currentID, (int)getpid(), balance.s_balance);

		localTime++;
		Message msgStart = { {MESSAGE_MAGIC, 0, STARTED, get_lamport_time ()}, };
		sprintf(msgStart.s_payload, log_started_fmt, (int)get_lamport_time (), currentID, (int)getpid(), (int)getppid(), balance.s_balance);
		WriteEventLog(log_started_fmt, fileEv, get_lamport_time (), currentID, (int)getpid(), (int)getppid(), balance.s_balance);
		//printf(log_started_fmt, get_lamport_time (), currentID, (int)getpid(), (int)getppid(), balance.s_balance);
		printf("%s", msgStart.s_payload);
		msgStart.s_header.s_payload_len = strlen(msgStart.s_payload);
		
		//Рассылка всем процессам сообщений STARTED из текущего дочернего процесса
		if(send_multicast(&currentID, &msgStart) == UNSUCCESS)
		{
			exit(UNSUCCESS);
		}
		//Получение STARTED сообщений от дочерних процессов
		for(int i = 1; i <= chProcAmount; i++)
		{
			if(i != currentID)
				if(receive(&currentID, i, &msg) == STARTED)
				{
					resultStarted++;		//увеличение счетчика сообщений STARTED от доч. проц.
					/*newEvent = 1;
					msgTime = msg.s_header.s_local_time;*/
					localTime = MAX(localTime, msg.s_header.s_local_time);
					localTime++;
					//printf("%d: process %d received STARTED from process %d\n", get_lamport_time (), currentID, i);
				}
		}

		if(resultStarted == chProcAmount - 1)
		{
			printf(log_received_all_started_fmt, get_lamport_time (), currentID);
			WriteEventLog(log_received_all_started_fmt, fileEv, get_lamport_time (), currentID);
		}
			while(1)
			{
				from = receive_any (&currentID, &msg);
				if(from == UNSUCCESS)
				{
					exit(UNSUCCESS);
				}
				else
				{
					localTime = MAX(localTime, msg.s_header.s_local_time);
					localTime++;
					if(msg.s_header.s_type == TRANSFER)
					{
						receivedOrder = (TransferOrder*)msg.s_payload;

						if(receivedOrder->s_src == currentID)
						{	//текущий процесс является отправителем

							//printf("%d: process %d get TRANSFER from process 0\n", (int)get_lamport_time (), currentID);
							
							FillEmptyHistoryPoints(&history, &balance, get_lamport_time ());
							
							//printf("Child %d transfer %d to %d in time %d\n", currentID, receivedOrder->s_amount, receivedOrder->s_dst, (int)get_lamport_time ());
							
							localTime++;
							WriteEventLog(log_transfer_out_fmt, fileEv, get_lamport_time (), currentID, receivedOrder->s_amount, receivedOrder->s_dst);
							printf(log_transfer_out_fmt, get_lamport_time (), currentID, receivedOrder->s_amount, receivedOrder->s_dst);

							msg.s_header.s_local_time = get_lamport_time ();

							send(&currentID, receivedOrder->s_dst, &msg);

							balance.s_time = get_lamport_time ();
							balance.s_balance_pending_in = -receivedOrder->s_amount;
							history.s_history_len += 1;
							history.s_history[(int)history.s_history_len - 1] = balance;
							//WriteToHistory(&history, &balance, get_lamport_time () + 1);

						}
						else if(receivedOrder->s_dst == currentID)
						{	//текущий процесс является получателем перевода

							//printf("Child %d receive %d from %d in time %d (msgTime = %d)\n", currentID, receivedOrder->s_amount, receivedOrder->s_src, (int)get_lamport_time (), (int)msg.s_header.s_local_time);

							FillEmptyHistoryPoints(&history, &balance, msg.s_header.s_local_time - 1);
							
							balance.s_time = msg.s_header.s_local_time;
							balance.s_balance_pending_in = receivedOrder->s_amount;
							history.s_history_len += 1;
							history.s_history[(int)history.s_history_len - 1] = balance;
							
							WriteToHistory(&history, &balance, get_lamport_time ());
							localTime++;
							WriteEventLog(log_transfer_in_fmt, fileEv, get_lamport_time (), currentID, receivedOrder->s_amount, receivedOrder->s_src);
							printf(log_transfer_in_fmt, get_lamport_time (), currentID, receivedOrder->s_amount, receivedOrder->s_src);

							//localTime++;
							Message ackMsg = {{MESSAGE_MAGIC, 0, ACK, get_lamport_time ()}, };
							send(&currentID, PARENT_ID, &ackMsg);
							//printf("%d: process %d send ACK to process 0\n", get_lamport_time (), currentID);
						}
					}
					else if(msg.s_header.s_type == STOP)
					{
						if(balance.s_balance_pending_in != 0)
						{
							balance.s_time++;
							balance.s_balance += balance.s_balance_pending_in;
							balance.s_balance_pending_in = 0;
							history.s_history_len += 1;	
							history.s_history[(int)history.s_history_len - 1] = balance;
						}
						//получено сообщени СТОП от родителя
						//printf("%d: process %d received STOP from process 0\n", get_lamport_time (), currentID);
						

						localTime++;
						
						Message doneMsg = {{MESSAGE_MAGIC, 0, DONE, get_lamport_time ()}, };
						sprintf(doneMsg.s_payload, log_done_fmt, (int)get_lamport_time (), currentID, balance.s_balance);
						WriteEventLog(log_done_fmt, fileEv, get_lamport_time (), currentID, balance.s_balance);
						//printf(log_done_fmt, get_lamport_time (), currentID, balance.s_balance);
						printf("%s", doneMsg.s_payload);
						doneMsg.s_header.s_payload_len = strlen(doneMsg.s_payload);
						
						send_multicast (&currentID, &doneMsg);

						printf("%s", doneMsg.s_payload);
						
						//"Доч. проц. заверши "полезную" работу" в лог-файл	
						
						break;
					}
				}
			}//while(1)

			//Получение DONE сообщений от дочерних процессов
			while(1)
			{
				from = receive_any (&currentID, &msg);
				if(from == UNSUCCESS)
				{
					exit(UNSUCCESS);
				}
				else
				{
					localTime = MAX(localTime, msg.s_header.s_local_time);
					localTime++;
					if(msg.s_header.s_type == DONE)
					{
						resultDone++;		//увеличение счетчика сообщений DONE от доч. проц.
						if(resultDone == chProcAmount - 1)
						{
							WriteEventLog(log_received_all_done_fmt, fileEv, get_lamport_time (), currentID);
							printf(log_received_all_done_fmt, get_lamport_time (), currentID);

							localTime++;
							Message historyMsg = {{MESSAGE_MAGIC, 
								sizeof(BalanceState) * history.s_history_len + 
								sizeof(history.s_id) + 
								sizeof(history.s_history_len),
								BALANCE_HISTORY, get_lamport_time ()}, };

							memmove(&historyMsg.s_payload, &history, sizeof(BalanceState) * history.s_history_len + 
									sizeof(history.s_id) + sizeof(history.s_history_len));

							/*printf("\n--------------CHILD %d---------------\n\tHistory Len %ds\n", currentID, history.s_history_len);
							for(int i = 0; i < history.s_history_len; i++)
							{
								printf("Balance = %d, time = %d\n", history.s_history[i].s_balance, history.s_history[i].s_time);
							}
							printf("--------------CHILD %d---------------\n\n", currentID);*/

							send (&currentID, PARENT_ID, &historyMsg);
							exit(SUCCESS);	//завершение текущего доч. проц. с кодом 0 (SUCCESS)
						}
					}
				}
			}
	}
	else
	if(PARENT_ID == currentID)	//Родительский процесс, currentID = 0
	{
		//printf("Waiting child process ending...\n");
		AllHistory allHistory;

		int countStarted = 0;
		int countDone = 0;
		int countHistory = 0;
		Message msg;
		
		//Получение STARTED сообщений от дочерних процессов
		for(int i = 1; i <= chProcAmount; i++)
		{
			if(receive(&currentID, i, &msg) == STARTED)
			{
				countStarted++;		//увеличение счетчика сообщений STARTED от доч. проц.
				localTime = MAX(localTime, msg.s_header.s_local_time);
				localTime++;
				//printf("%d: process 0 received STARTED from process %d\n", get_lamport_time (), i);
			}
		}
		
		//Проверка получения STARTED от всех дочерних процессов
		if(countStarted == chProcAmount)
		{
			WriteEventLog(log_received_all_started_fmt, fileEv, get_lamport_time(), currentID);
			printf(log_received_all_started_fmt, get_lamport_time(), currentID);
			
			//void bank_robbery(void * parent_data, local_id max_id)
			//printf("Starting bank robbery\n");
			bank_robbery(&currentID, chProcAmount);

			localTime++;
			Message stopMsg = {{MESSAGE_MAGIC, 0, STOP, get_lamport_time ()}, };
			send_multicast (&currentID, &stopMsg);
			//printf("%d: process 0 sending STOP messages to childes\n", get_lamport_time ());

			for(int i = 1; i <= chProcAmount; i++)
			{
				if(receive(&currentID, i, &msg) == DONE)
				{
					countDone++;		//увеличение счетчика сообщений BALANCE_HISTORY от доч. проц.
					localTime = MAX(localTime, msg.s_header.s_local_time);
					localTime++;
					//printf("%d: process 0 received %s from process %d\n", get_lamport_time (), messageType[msg.s_header.s_type], i);
				}
			}
			//Проверка получения DONE от всех дочерних процессов
			if(countDone == chProcAmount)
			{
				WriteEventLog(log_received_all_done_fmt, fileEv, get_lamport_time(), currentID);
				printf(log_received_all_done_fmt, get_lamport_time (), currentID);

				//printf("Getting Balance History from childes\n");
				//Получение BALANCE_HISTORY сообщений от дочерних процессов
				allHistory.s_history_len = chProcAmount;
				int maxHistory = get_lamport_time ();
				for(int i = 1; i <= chProcAmount; i++)
				{
					if(receive(&currentID, i, &msg) == BALANCE_HISTORY)
					{
						localTime = MAX(localTime, msg.s_header.s_local_time);
						localTime++;
						countHistory++;		//увеличение счетчика сообщений BALANCE_HISTORY от доч. проц.
						//printf("%d: process 0 received %s from process %d\n", get_lamport_time (), messageType[msg.s_header.s_type], i);
						memcpy(&allHistory.s_history[i - 1], msg.s_payload, msg.s_header.s_payload_len);
						//printf("GET HISTORY %d with len %d\n", allHistory.s_history[i - 1].s_id, allHistory.s_history[i - 1].s_history_len);
						/*if(maxHistory < allHistory.s_history[i - 1].s_history_len)
							maxHistory = allHistory.s_history[i - 1].s_history_len;*/
					}
				}
				//printf("MAX HISTORY LEN = %d\n", maxHistory);
				//Если перед завершением программы со счетом не было никаких действий, то нужно скорректировать историю
				//то есть, чтобы не было пустых строк завершенного раньше всех процесса
				//делается в родительском, так как ребенок не знает время завершения всех остальных процессов
				for(int i = 0; i < chProcAmount; i++)
				{
					//if(allHistory.s_history[i].s_history_len < maxHistory -1)
					//{
						BalanceState tmpBalance = allHistory.s_history[i].s_history[allHistory.s_history[i].s_history_len - 1];
						for(int j = tmpBalance.s_time + 1; j <= maxHistory; j++)
						{
							tmpBalance.s_time += 1;
							tmpBalance.s_balance_pending_in = 0;
							allHistory.s_history[i].s_history_len += 1;	
							allHistory.s_history[i].s_history[allHistory.s_history[i].s_history_len - 1] = tmpBalance;
						}
					//}
				}
				
				if(countHistory == chProcAmount)
				{
					//printf("PARENT received all %s messages\n", messageType[msg.s_header.s_type]);
					//Запись в лог-файл об окончании работы родительского процесса
					//WriteEventLog(log_done_fmt, fileEv, get_lamport_time (), currentID, 0);
					//printf(log_done_fmt, get_lamport_time (), currentID, 0);

					fclose (filePipe);
					fclose(fileEv);

					while(wait(NULL))
					{
						if(errno == ECHILD)
							break;
					}

					print_history (&allHistory);
					/*for(int i = 1; i <= chProcAmount; i++)
					{			
						if(waitpid(childPID[i], NULL, 0) == -1)	//Ожидание окончания всех дочерних процессов
						{
							printf("Error waiting child %d\n", i);
							exit(EXIT_FAILURE);
						}
						else
						{	if(kill(childPID[i], SIGKILL))	//
							{
								//printf("Child %i ended\n", i);
							}
						}
					}*/
					return SUCCESS;
				}
			}
		}
		return UNSUCCESS;
	}
}


timestamp_t get_lamport_time()
{
	/*for(int i = 0; i < MAX_PROCESS_ID; i++)
	{
		if(msgTime[i] > msgTime[i + 1])
			
	}*/
	/*if(newEvent > 0)
	{
		localTime = localTime > msgTime ? localTime + 1 : msgTime + 1;
		newEvent = 0;
	}*/
		
	return localTime;
}
