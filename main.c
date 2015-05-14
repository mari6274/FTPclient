/*
 * main.c
 *
 *  Created on: 18-04-2015
 *      Author: mario
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netdb.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <ctype.h>

char * host = "";
char * user = "";
char * pass = "";

int client_socket_desc, client_socket_desc_data, server_socket_desc_data;
struct sockaddr_in server_addr;

char * str_replace ( const char *string, const char *substr, const char *replacement ){
	char *tok = NULL;
	char *newstr = NULL;
	char *oldstr = NULL;
	/* if either substr or replacement is NULL, duplicate string a let caller handle it */
	if ( substr == NULL || replacement == NULL ) return strdup (string);
	newstr = strdup (string);
	while ( (tok = strstr ( newstr, substr ))){
		oldstr = newstr;
		newstr = malloc ( strlen ( oldstr ) - strlen ( substr ) + strlen ( replacement ) + 1 );
		/*failed to alloc mem, free old string and return NULL */
		if ( newstr == NULL ){
			free (oldstr);
			return NULL;
		}
		memcpy ( newstr, oldstr, tok - oldstr );
		memcpy ( newstr + (tok - oldstr), replacement, strlen ( replacement ) );
		memcpy ( newstr + (tok - oldstr) + strlen( replacement ), tok + strlen ( substr ), strlen ( oldstr ) - strlen ( substr ) - ( tok - oldstr ) );
    	memset ( newstr + strlen ( oldstr ) - strlen ( substr ) + strlen ( replacement ) , 0, 1 );
    	free (oldstr);
  	}
  	return newstr;
}

void send_command(char * message);
char * receive_from_server();
void put_from_server();
void put_data_from_server();

char * get_line(int with_crlf) {
	size_t line_size = 1000;
	char * line = NULL;
	int size = getline(&line, &line_size, stdin);
	if (with_crlf) {
		line[size-1] = '\0';
		line = strcat(line, "\r\n\0");
	} else {
		line[size-1] = '\0';
	}
	return line;
}

void ask_connection() {

	puts("Do you want to enter conection parameters? (yes / no - default)");
	char * answer = get_line(0);
	while (strcmp(answer, "yes") != 0 && strcmp(answer, "no") != 0) {
		puts("Wrong answer! Type \"yes\" or \"no\". Do you want to enter connection parameters? (yes / no - default)");
		answer = get_line(0);
	}

	if (strcmp(answer, "yes") == 0) {
		puts("Get ftp host:");
		host = get_line(0);
		puts("Get username:");
		user = get_line(0);
		puts("Get password");
		pass = get_line(0);
	}

}

void init_client_socket() {
	client_socket_desc = socket(AF_INET, SOCK_STREAM, 0);
	client_socket_desc_data = socket(AF_INET, SOCK_STREAM, 0);

	if (client_socket_desc == -1 || client_socket_desc_data == -1)
	{
		puts("Could not create a socket");
		exit(0);
	}
}

void connect_to_server() {
	struct in_addr addr;
	if (inet_aton(host, &addr) == 0) {
		struct in_addr **addr_list = (struct in_addr **) gethostbyname(host)->h_addr_list;
		if (addr_list == NULL) {
			puts("Could not resolved host name");
			exit(0);
		}
		addr = *addr_list[0];
	}


	server_addr.sin_addr = addr;
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons( IPPORT_FTP );

	if (connect(client_socket_desc , (struct sockaddr *)&server_addr , sizeof(server_addr)) < 0)
	{
		puts("connect error");
		exit(0);
	}

	puts("Connected");

	put_from_server();
}

void login() {
	char c1[100] = "USER ";
	char c2[100] = "PASS ";
	strcat(c1, user);
	strcat(c2, pass);
	strcat(c1, "\r\n");
	strcat(c2, "\r\n");

	send_command(c1);
	put_from_server();
	send_command(c2);
	put_from_server();
}

char * get_port_command(int port, char * addr) {
	int port1 = port/256;
	int port2 = port%256;
	addr = str_replace(addr, ".", ",");
	char port1c[7];
	char port2c[7];
	sprintf(port1c, "%d", port1);
	sprintf(port2c, "%d", port2);

	char * port_message = malloc( sizeof(char) * 26 );
	strcat(port_message, "PORT ");
	strcat(port_message, addr);
	strcat(port_message, ",");
	strcat(port_message, port1c);
	strcat(port_message, ",");
	strcat(port_message, port2c);
	strcat(port_message, "\r\n\0");
	return port_message;
}

void active_data_connection() {
	puts("Waiting for establish active data connection...");
	struct sockaddr_in client_addr;
	int length = sizeof(client_addr);
	getsockname(client_socket_desc, (struct sockaddr *) &client_addr,(socklen_t*) &length);
	int data_port = ntohs(client_addr.sin_port)+1;
	char * ip = inet_ntoa(client_addr.sin_addr);

	client_addr.sin_addr.s_addr = INADDR_ANY;
	client_addr.sin_family = AF_INET;
	client_addr.sin_port = htons(data_port);

	send_command(get_port_command(data_port, ip));

	if (bind(client_socket_desc_data, (struct sockaddr*) &client_addr, sizeof(client_addr)) < 0) {
		puts("bind failed");
		exit(0);
	}
	if (listen(client_socket_desc_data, 1) < 0) {
		puts("listen failed");
		exit(0);
	}
	if ((server_socket_desc_data = accept(client_socket_desc_data, (struct sockaddr*) &client_addr, (socklen_t*)&length)) < 0) {
		puts("accept failed");
		exit(0);
	}

	puts("Active data connection established");
}

int parse_port(char * in) {
	int i = 0;
	int points = 0;
	char * port1 = malloc( sizeof(char) * 4);
	char * port2 = malloc( sizeof(char) * 4);
	port1[0] = '\0';
	port2[0] = '\0';
	while (in[i] != ')') {
		if (in[i] == ',') {
			++points;
		} else if (points == 4) {
			port1[strlen(port1)] = in[i];
			port1[strlen(port1)+1] = '\0';
		} else if (points == 5) {
			port2[strlen(port2)] = in[i];
			port2[strlen(port2)+1] = '\0';
		}
		i++;
	}
	int p1,p2;
	sscanf(port1, "%d", &p1);
	sscanf(port2, "%d", &p2);
	return p1*256+p2;
}

void passive_data_connection() {
	puts("Waiting for establish passive data connection...");
	send_command("PASV\r\n");
	char * s = receive_from_server();
	int a = parse_port(s);
	server_addr.sin_port = htons( a );
	fcntl(client_socket_desc, F_SETFL, fcntl(client_socket_desc, F_GETFL)^O_NONBLOCK);
}

void data_connect() {
	if (connect(client_socket_desc_data , (struct sockaddr *)&server_addr , sizeof(server_addr)) < 0)
	{
		puts("connect error");
		exit(0);
	}

	puts("Passive data connection established");
}

char * receive_from_server() {
	char * reply_buffer = malloc( sizeof(char) * 2000 );
	int ret_recv = recv(client_socket_desc, reply_buffer, 2000-1, 0);

	if (ret_recv > 0) {
		fcntl(client_socket_desc, F_SETFL, fcntl(client_socket_desc, F_GETFL)|O_NONBLOCK);
		reply_buffer[ret_recv] = '\0';
	} else {
		reply_buffer = NULL;
		fcntl(client_socket_desc, F_SETFL, fcntl(client_socket_desc, F_GETFL)^O_NONBLOCK);
	}

	return reply_buffer;
}

char * receive_data_from_server() {
	char * reply_buffer = malloc( sizeof(char) * 2000 );
	int ret_recv = recv(client_socket_desc_data, reply_buffer, 2000-1, 0);

	if (ret_recv > 0) {
		fcntl(client_socket_desc_data, F_SETFL, fcntl(client_socket_desc_data, F_GETFL)|O_NONBLOCK);
		reply_buffer[ret_recv] = '\0';
	} else {
		reply_buffer = NULL;
		fcntl(client_socket_desc_data, F_SETFL, fcntl(client_socket_desc_data, F_GETFL)^O_NONBLOCK);
	}

	return reply_buffer;
}

void put_from_server() {
	char * result;
	while ((result = receive_from_server()) != NULL) {
		printf("%s", result);
		fflush(stdin);
	}
	printf("\n");
}

void put_data_from_server() {
	char * result;
	while ((result = receive_data_from_server()) != NULL) {
		printf("%s", result);
		fflush(stdin);
	}
	printf("\n");
}

void prompt() {
	while (1) {
		char * message = get_line(1);
		send_command(message);
	}
}

void send_command(char * message) {
	int ret_send = send(client_socket_desc, message, strlen(message), 0);
	if (ret_send < 0) {
		puts("send failed");
		exit(0);
	}
}

int main(int argc, char **argv) {
	puts("==============FTP CLIENT==============");
	ask_connection();
	init_client_socket();
	connect_to_server();
	login();
//	active_data_connection();
	passive_data_connection();

	while (1) {
		printf("\nprompt >>> ");
		char * command = get_line(1);
		int i;
		for (i = 0; i < strlen(command); ++i) {
			command[i] = tolower(command[i]);
		}
		send_command(command);

		if (strcmp(command, "list\r\n") == 0) {
			put_data_from_server();
			put_from_server();
		} else {
			put_from_server();
		}
	}

	return 0;
}

