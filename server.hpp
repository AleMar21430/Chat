#include "utils.hpp"

#include "proyecto.pb.h"

struct User {
	char ip_address[INET_ADDRSTRLEN];
	int socket_node;
	string username;
	string status;
	clock_t last_active_time;
};

void SendErrorResponse(int socketFd, string errorMsg);
void *Worker_Thread(void *params);