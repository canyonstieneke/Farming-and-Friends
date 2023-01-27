#include <stdio.h>
#include <stdlib.h>

#include <unistd.h>

#include <mysql/mysql.h>

#include <string.h>

#include <sys/socket.h>
#include <sys/types.h>

#include <netinet/in.h>

#include <openssl/ssl.h>
#include <openssl/err.h>

#include <arpa/inet.h>

#include <signal.h>

#include <pthread.h>

#include <time.h>

//#include <concord/discord.h>
//#include <concord/log.h>


static char *host = "localhost";
static char *user = "fafbankapp";
static char *pass = "gV_m3V^2GT=!mFED";
static char *dbname = "farming_and_friends";

MYSQL *conn;

unsigned int port = 3306;
static char *unix_socket = NULL;
unsigned int flag = 0;


static char code_playerID_not_found[] = "-1";
static char code_player_passwd_wrong[] = "-1";


int portSSL = 19918;

char *getDate()
{
	char *date;
	time_t t = time(NULL);
	struct tm tm = *localtime(&t);
	sprintf(date, "%d-%02d-%02d", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday);
	
	return date;
}

int create_socket(int serverPort) // Code used from https://wiki.openssl.org/index.php/Simple_TLS_Server
{
	int s;
	struct sockaddr_in addr;

	addr.sin_family = AF_INET;
	addr.sin_port = htons(serverPort);
	addr.sin_addr.s_addr = htonl(INADDR_ANY);

	s = socket(AF_INET, SOCK_STREAM, 0);
	if (s < 0)
	{
		perror("Unable to create socket");
		exit(EXIT_FAILURE);
	}

	if (bind(s, (struct sockaddr*)&addr, sizeof(addr)) < 0)
	{
		perror("Unable to bind");
		exit(EXIT_FAILURE);
	}

	if (listen(s, 1) < 0)
	{
		perror("Unable to listen");
		exit(EXIT_FAILURE);
	}


	return s;

}


SSL_CTX *create_context() // Code used from https://wiki.openssl.org/index.php/Simple_TLS_Server
{
	const SSL_METHOD *method;
	SSL_CTX *ctx;

	method = TLS_server_method();

	ctx = SSL_CTX_new(method);
	if (!ctx)
	{
		perror("Unable to create SSL context");
		ERR_print_errors_fp(stderr);
		exit(EXIT_FAILURE);
	}

	return ctx;


}

void configure_context(SSL_CTX *ctx) // Code used from https://wiki.openssl.org/index.php/Simple_TLS_Server
{
	/* Set the key and cert */
	if (SSL_CTX_use_certificate_file(ctx, "cert.pem", SSL_FILETYPE_PEM) <= 0)
	{
		ERR_print_errors_fp(stderr);
		exit(EXIT_FAILURE);
	}
	
	if (SSL_CTX_use_PrivateKey_file(ctx, "key.pem", SSL_FILETYPE_PEM) <= 0 )
	{
		ERR_print_errors_fp(stderr);
		exit(EXIT_FAILURE);
	}
}


char *randStrGen(int randLen)
{
	srand(time(0));
	char special_characters[] = "!@#$%^&*()_+-=[]{}\\|;:'\",.<>/?";
	int random_number = rand() % (sizeof(special_characters) - 1);
	char random_char = special_characters[random_number];
	
	char *randStr = malloc(sizeof(char) * (randLen + 1));
	memset(randStr, 0, sizeof(randStr)); // Initialize the array with null characters
	
	int random_number_or_ascii;
	int i = 0; // Initialize i to 0
	while (i < randLen)
	{
		random_number_or_ascii = rand() % 3;
		if (random_number_or_ascii == 0)
		{
			randStr[i] = 'a' + (rand() % 26);
		}
		else if (random_number_or_ascii == 1)
		{
			randStr[i] = 'A' + (rand() % 26);
		}
		else if (random_number_or_ascii == 2)
		{
			randStr[i] = '0' + (rand() % 26);
		}
		i++; // increment i
	}
	return randStr;
}

int tableCheck(char tableName[20])
{
	int success; // If = 1, table not found, if = 2, table found!
	
	char queryCMD[100] = "SHOW TABLES LIKE '";
	strncat(queryCMD, tableName, 50);
	strncat(queryCMD, "';", 50);
	
	/* set up variable sql query ^^^ */
	
	if (mysql_query(conn, queryCMD))
	{
		fprintf(stderr, "\nError: %s [%d]\n", mysql_error(conn), mysql_errno(conn));
	}
	
	/* execute query ^^^ */
	
	MYSQL_RES *result = mysql_store_result(conn);
	
	if (result == NULL)
	{
		printf(" STOPPING!");
		fprintf(stderr, "\nError: %s [%d]\n", mysql_error(conn), mysql_errno(conn));
		exit(1);
	}
	
	int numFields = mysql_num_fields(result);
	
	MYSQL_ROW row;
	
	while ((row = mysql_fetch_row(result)))
	{
		
		for (int i = 0; i < numFields; i++)
		{
			//printf("%s ", row[i]);
			if (strncmp(row[i], tableName, 50) == 0)
			{
				success = 0;
			}
			else
			{
				success = 1;
			}
		}
	}
	
	mysql_free_result(result);
	
	/* Check query contents to see if table exist ^^^ */
	
	return success;
}

int createTable(char tableName[20], char firstCol[20], char firstColDatatype[10])
{
	printf("Starting Creation of table '%s' now\n", tableName);
	int success; // If = 1, table not found, if = 2, table found!
	
	char queryCMD[100] = "CREATE TABLE ";
	strncat(queryCMD, tableName, 50);
	strncat(queryCMD, " ( ", 50);
	strncat(queryCMD, firstCol, 50);
	strncat(queryCMD, " ", 50);
	strncat(queryCMD, firstColDatatype, 50);
	strncat(queryCMD, " );", 50);
	
	/* form the mysql query string from the variables ^^^ */
	
	if (mysql_query(conn, queryCMD))
	{
		fprintf(stderr, "\nError: %s [%d]\n", mysql_error(conn), mysql_errno(conn));
		success = 1;
	}
	else
	{
		success = 0;
	}
	
	/* run query ^^^ */
	
	
	
	return success;
}

int addCol(char tableName[20], char prevCol[20], char firstCol[20], char firstColDatatype[10])
{
	printf("Adding column '%s' to table '%s' now\n", firstCol, tableName);
	int success; // If = 1, table not found, if = 2, table found!
	
	char queryCMD[100] = "ALTER TABLE ";
	strncat(queryCMD, tableName, 50);
	strncat(queryCMD, " ADD COLUMN ", 50);
	strncat(queryCMD, firstCol, 50);
	strncat(queryCMD, " ", 50);
	strncat(queryCMD, firstColDatatype, 50);
	strncat(queryCMD, " AFTER ", 50);
	strncat(queryCMD, prevCol, 50);
	strncat(queryCMD, ";", 50);
	
	/* form the mysql query string from the variables ^^^ */

	if (mysql_query(conn, queryCMD))
	{
		fprintf(stderr, "\nError: %s [%d]\n", mysql_error(conn), mysql_errno(conn));
		success = 1;
	}
	else
	{
		success = 0;
	}
	
	/* run query ^^^ */
	
	
	
	return success;
}

int checkPlayerID(char userid[10])
{
	int success; // If = 1, userid not found, if = 2, userid found!
	char queryCMD[100] = "SELECT * FROM Players WHERE playerID = '";
	strncat(queryCMD, userid, 10);
	strncat(queryCMD, "';", 3);
	if (mysql_query(conn, queryCMD))
	{
		fprintf(stderr, "\nError: %s [%d]\n", mysql_error(conn), mysql_errno(conn));
	}
	
	MYSQL_RES *result = mysql_store_result(conn);
	if (result == NULL)
	{
		fprintf(stderr, "\nError: %s [%d]\n", mysql_error(conn), mysql_errno(conn));
	}
	
	MYSQL_ROW row;
	char rowChar[100];
	row = mysql_fetch_row(result);
	if (row != NULL)
	{
		sprintf(rowChar, "%s", row[0]);
			
		if (strncmp(rowChar, "0", 10) != 0)
		{
			success = 0;
		}
		else
		{
			success = 1;
		}
	}
	else
	{
		success = 1;
	}
	
	mysql_free_result(result);
	
	
	return success;
}

int checkPlayerPasswd(char playerid[10], char playerpasswd[25])
{
	int success; // If = 1, password not correct, if = 2, password is correct!
	char queryCMD[100] = "SELECT Password FROM Players WHERE playerID = '";
	strncat(queryCMD, playerid, 10);
	strncat(queryCMD, "';", 3);
	
	if (mysql_query(conn, queryCMD))
	{
		fprintf(stderr, "\nError: %s [%d]\n", mysql_error(conn), mysql_errno(conn));
	}
	
	MYSQL_RES *result = mysql_store_result(conn);
	if (result == NULL)
	{
		fprintf(stderr, "\nError: %s [%d]\n", mysql_error(conn), mysql_errno(conn));
	}
	
	MYSQL_ROW row;
	char rowChar[100];
	row = mysql_fetch_row(result);
	if (row != NULL)
	{
		sprintf(rowChar, "%s", row[0]);
			
		if (strncmp(rowChar, playerpasswd, 10) == 0)
		{
			success = 0;
		}
		else
		{
			success = 1;
		}
	}
	else
	{
		success = 1;
	}
	
	mysql_free_result(result);
	
	return success;
}

int walletBal(char playerid[10])
{
	int bal;
	int ret;
	char queryCMD[100];
	do
	{
		ret = 0;
		
		sprintf(queryCMD, "SELECT Wallet FROM Players WHERE playerID = '%s';", playerid);
	
		if (mysql_query(conn, queryCMD))
		{
			fprintf(stderr, "\nError: %s [%d]\n", mysql_error(conn), mysql_errno(conn));
		}
	
		MYSQL_RES *result = mysql_store_result(conn);
	
		if (result == NULL)
		{
			ret = 1;
			
			sprintf(queryCMD, "UPDATE Players SET Wallet = 0 WHERE playerID = '%s';", playerid);
			
			if (mysql_query(conn, queryCMD))
			{
				fprintf(stderr, "\nError: %s [%d]\n", mysql_error(conn), mysql_errno(conn));
			}
			
			mysql_free_result(result);
		}
		else
		{
			MYSQL_ROW row;
			row = mysql_fetch_row(result);
			
			bal = atoi(row[0]);
			
			mysql_free_result(result);
		}
	}
	while (ret == 1);
	
	return bal;
}

int walletIncrease(char playerid[10], int amt)
{
	int suc;
	char queryCMD[100];
	
	int balBef = walletBal(playerid);
	int balAft = balBef + amt;
	
	sprintf(queryCMD, "UPDATE Players SET Wallet = %d WHERE playerID = '%s';", balAft, playerid);
	if (mysql_query(conn, queryCMD))
	{
		fprintf(stderr, "\nError: %s [%d]\n", mysql_error(conn), mysql_errno(conn));
		suc = 1;
	}
	else
	{
		suc = 0;
	}
	
	return suc;
}

int walletDecrease(char playerid[10], int amt)
{
	int suc;
	char queryCMD[100];
	
	int balBef = walletBal(playerid);
	int balAft = balBef - amt;
	
	sprintf(queryCMD, "UPDATE Players SET Wallet = %d WHERE playerID = '%s';", balAft, playerid);
	if (mysql_query(conn, queryCMD))
	{
		fprintf(stderr, "\nError: %s [%d]\n", mysql_error(conn), mysql_errno(conn));
		suc = 1;
	}
	else
	{
		suc = 0;
	}
	
	return suc;
}

int payment(char sender[10], char sentto[10], int amount)
{
	int suc;
	
	if (walletIncrease(sentto, amount) == 0)
	{
		suc = 0;
		if (walletDecrease(sender, amount) != 0)
		{
			suc = 1;
		}
	}
	else
	{
		suc = 1;
	}
	
	return suc;
}

char *incomingInvoices(char playerid[10])
{
	char queryCMD[100];
	sprintf(queryCMD, "SELECT * FROM Invoices WHERE Sent_to = '%s'", playerid);
	char *buffer = malloc(1024);
	
	if (mysql_query(conn, queryCMD))
	{
		fprintf(stderr, "\nError: %s [%d]\n", mysql_error(conn), mysql_errno(conn));
	}
	
	MYSQL_RES *result = mysql_store_result(conn);
	
	if (result != NULL)
	{
		int num_fields = mysql_num_fields(result);
		
		MYSQL_ROW row;
		
		while ((row = mysql_fetch_row(result)))
		{
			for (int i = 0; i <= num_fields; i++)
			{
				strcat(buffer, row[i] ? row[i] : "NULL");
				strcat(buffer, " ");
			}
			
			strcat(buffer, "\n");
		}
	}
	
	mysql_free_result(result);
	
	return buffer;
}

int payInvoice(char invoiceID[20])
{
	
}

int sendInvoice(char sender[10], char sentTo[10], int amt)
{
	int suc;
	char queryCMD[100];
	char insertCMD[150];
	char randomID[20];
	int randIDExist = 1;
	while (randIDExist == 1)
	{
		strcpy(randomID, randStrGen(20));
		sprintf(queryCMD, "SELECT InvoiceID FROM Invoices WHERE InvoiceID = '%s'", randomID);
		if (mysql_real_query(conn, queryCMD, strlen(queryCMD)) == 0)
		{
			MYSQL_RES *result = mysql_use_result(conn);
			if (result != NULL)
			{
				MYSQL_ROW row;
				row = mysql_fetch_row(result);
				if (row != NULL && strncmp(row[0], randomID, 20) == 0)
				{
					randIDExist = 1;
				}
				else
				{
					randIDExist = 0;
				}
			}
			mysql_free_result(result);
		}
	}
	
	char *date = getDate();
	
	sprintf(insertCMD,
	"INSERT INTO `Invoices`(InvoiceID, Sent_by, Sent_to, Amount, Date) VALUES (\"%s\", \"%s\", \"%s\", %d, \"%s\");",
	randomID, sender, sentTo, amt, date);
	
	if (mysql_query(conn, insertCMD))
	{
		fprintf(stderr, "\nError: %s [%d]\n", mysql_error(conn), mysql_errno(conn));
		suc = 1;
	}
	else
	{
		suc = 0;
	}
	
	
	return suc;
}

void * connection(void* p_client)
{
	int client = *((int*)p_client);
	free(p_client);
	char playerid[256];
	char playerpasswd[256];
	char clientCMD[256];
	char buffer[256];
	char serverMessage[1024];
	char helpMessage[] = "List of avalible commands:\n";
	int i = 0;
	int sc = 0;
	char argArray[10][20];
	int res;
	int suc;
	char queryCMD[256];
	
	SSL_CTX *ctx;
	SSL *ssl;
	ctx = create_context();
	configure_context(ctx);
	
	ssl = SSL_new(ctx);
	SSL_set_fd(ssl, client);
	
	printf("Client Connecting..\n");

	if (SSL_accept(ssl) <= 0) 
	{
		ERR_print_errors_fp(stderr);
		fprintf(stderr, "Client Tried Connecting, Connectiong Refused..\n");
	}
	else
	{
		SSL_read(ssl, buffer, 256);
		sscanf(buffer, "%s %s %[a-zA-Z0-9 ]", playerid, playerpasswd, clientCMD);
		
		printf("\nUSERNAME: %s\nPASSWORD: %s\nCOMMAND: %s\n", playerid, playerpasswd, clientCMD);
		
		if (checkPlayerID(playerid) == 0)
		{
			if (checkPlayerPasswd(playerid, playerpasswd) == 0)
			{
				while (i <= strlen(clientCMD))
				{
					if (clientCMD[i] == ' ')
					{
						if (clientCMD[i + 1] != ' ' || clientCMD[i + 1] != '\0')
						{
							sc++;
						}
						else if (clientCMD[i + 1] == ' ')
						{
							clientCMD[i] = '\0'; 
						}
					}
					
					i++;
				}
				printf("I made it here");
				switch (sc)
				{
					case 0:
						sscanf(clientCMD, "%s", argArray[0]);
						break;
					case 1:
						sscanf(clientCMD, "%s %s", argArray[0], argArray[1]);
						break;
					case 2:
						sscanf(clientCMD, "%s %s %s", argArray[0], argArray[1], argArray[2]);
						break;
					case 3:
						sscanf(clientCMD, "%s %s %s %s", argArray[0], argArray[1], argArray[2], argArray[3]);
						break;
					case 4:
						sscanf(clientCMD, "%s %s %s %s %s", argArray[0], argArray[1], argArray[2], argArray[3], argArray[4]);
						break;
					case 5:
						sscanf(clientCMD, "%s %s %s %s %s %s", argArray[0], argArray[1], argArray[2], argArray[3], argArray[4], argArray[5]);
						break;
					case 6:
						sscanf(clientCMD, "%s %s %s %s %s %s %s", argArray[0], argArray[1], argArray[2], argArray[3], argArray[4], argArray[5], argArray[6]);
						break;
					case 7:
						sscanf(clientCMD, "%s %s %s %s %s %s %s %s", argArray[0], argArray[1], argArray[2], argArray[3],
						argArray[4], argArray[5], argArray[6], argArray[7]);
						break;
					case 8:
						sscanf(clientCMD, "%s %s %s %s %s %s %s %s %s", argArray[0], argArray[1], argArray[2], argArray[3],
						argArray[4], argArray[5], argArray[6], argArray[7], argArray[8]);
						break;
					case 9:
						sscanf(clientCMD, "%s %s %s %s %s %s %s %s %s %s", argArray[0], argArray[1], 
						argArray[2], argArray[3], argArray[4], argArray[5], argArray[6], argArray[7], argArray[8], argArray[9]);
						break;
				}
				if (strcmp(argArray[0], "invoice") == 0)
				{
					if (strcmp(argArray[1], "incoming") == 0)
					{
						SSL_write(ssl, incomingInvoices(playerid), 1024);
					}
					else if (strcmp(argArray[1], "send") == 0)
					{
						res = sendInvoice(playerid, argArray[2], atoi(argArray[3]));
						if (res == 0)
						{
							SSL_write(ssl, "Invoice Sent", 15);
						}
						else
						{
							SSL_write(ssl, "Failed to send Invoice", 25);
						}
					}
					else if (strcmp(argArray[1], "pay") == 0)
					{
						res = payInvoice(argArray[2]);
						if (res == 0)
						{
							SSL_write(ssl, "Payment Sent", 20);
						}
						else
						{
							SSL_write(ssl, "Failed to send Payment", 30);
						}
					}
					else
					{
						SSL_write(ssl, "Wrong Syntax, Try 'send <Invoice Receiver> <Amount>' or 'pay <Payment Receiver> <Amount>'", 256);
					}
				}
				else if (strcmp(argArray[0], "pay") == 0)
				{
					res = payment(playerid, argArray[1], atoi(argArray[2]));
					if (res == 0)
					{
						SSL_write(ssl, "Payment Sent", 20);
					}
					else
					{
						SSL_write(ssl, "Failed to send Payment", 30);
					}
				}
				else if (strcmp(argArray[0], "help") == 0)
				{
					SSL_write(ssl, helpMessage, 1024);
				}
				else if (strcmp(argArray[0], "bal") == 0)
				{
					sprintf(serverMessage, "%d", walletBal(playerid));
					SSL_write(ssl, serverMessage, 150);
				}
				else
				{
					SSL_write(ssl, "Command Not Found. Try 'help' for a list of commands!", 100);
				}
			}
			else
			{
				SSL_write(ssl, code_player_passwd_wrong, strlen(code_player_passwd_wrong));
			}
		}
		else
		{
			SSL_write(ssl, code_playerID_not_found, strlen(code_playerID_not_found));
		}
	}
	
	SSL_shutdown(ssl);
	SSL_CTX_free(ctx);
	SSL_free(ssl);
	close(client);
	pthread_exit(NULL);
}

void handleConnect()
{

	int sock;
	
	/* Ignore broken pipe signals */
	signal(SIGPIPE, SIG_IGN);
	
	sock = create_socket(portSSL);
	
	/* Handle connections */
	while(1)
	{
		printf("Waiting for client..\n");
		struct sockaddr_in addr;
		unsigned int len = sizeof(addr);
		int client = accept(sock, (struct sockaddr*)&addr, &len);
		if (client < 0)
		{
			perror("Unable to accept client..\n");
		}
		else
		{
			sleep(1);
			printf("Client Accepted!\n");
			pthread_t t;
			int* p_client = malloc(sizeof(int));
			*p_client = client; 
			pthread_create(&t, NULL, connection, p_client);
		}
	}

	close(sock);
	
	printf("Closed SSL Server Socket..\n");
}

int main()
{
	
	conn = mysql_init(NULL);
	
	if (!(mysql_real_connect(conn, host, user, pass, dbname, port, unix_socket, flag)))
	{
		fprintf(stderr, "Error: %s [%d]\n", mysql_error(conn), mysql_errno(conn));
		exit(1);
	}
	else
	{
		printf("Connected to database.\n");
	}
	
	/* perform connection ^^^ */
	
	
	printf("Performing Table Checks\n");
	
	if (tableCheck("Players") == 0)
	{
		printf("Table 'Players' Found in Database\n");
	}
	else
	{
		printf("Table 'Players' Not Found in Database\n");
		createTable("Players", "playerID", "VARCHAR(10)");
		sleep(1);
		addCol("Players", "playerID", "Player_Name", "VARCHAR(25)");
		sleep(1);
		addCol("Players", "Player_Name", "Password", "VARCHAR(25)");
		sleep(1);
		addCol("Players", "Password", "Wallet", "INT");
	}

	if (tableCheck("Invoices") == 0)
	{
		printf("Table 'Invoices' Found in Database\n");
	}
	else
	{
		printf("Table 'Invoices' Not Found in Database\n");
		createTable("Invoices", "InvoiceID", "VARCHAR(20)");
		sleep(1);
		addCol("Invoices", "InvoiceID", "Sent_by", "VARCHAR(10)");
		sleep(1);
		addCol("Invoices", "Sent_by", "Sent_to", "VARCHAR(10)");
		sleep(1);
		addCol("Invoices", "Sent_to", "Amount", "INT");
		sleep(1);
		addCol("Invoices", "Amount", "Date", "VARCHAR(20)");
		sleep(1);
		addCol("Invoices",  "Date", "Paid_Status", "VARCHAR(6)");
	}
	
	if (tableCheck("Payments") == 0)
	{
		printf("Table 'Payments' Found in Database\n");
	}
	else
	{
		printf("Table 'Payments' Not Found in Database\n");
		createTable("Payments", "paymentID", "VARCHAR(20)");
		sleep(1);
		addCol("Payments", "paymentID", "pay_by", "VARCHAR(10)");
		sleep(1);
		addCol("Payments", "pay_by", "pay_to", "VARCHAR(10)");
		sleep(1);
		addCol("Payments", "pay_to", "Amount", "INT");	
		sleep(1);
		addCol("Payments", "Amount", "Date", "VARCHAR(20)");	
	}
	
	/* perform table check ^^^ */
	
	handleConnect();

	mysql_close(conn);
	printf("Mysql Closed..\n");

	printf("\n");
	
	return 0;
}
