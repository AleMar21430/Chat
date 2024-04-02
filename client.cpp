#include "client.hpp"

bool connected, server_wait, main_loop;

int main(int argc, char const* argv[]) {
	int socket_fd;
	char buffer[8192];
	struct addrinfo hints, *servinfo, *port;
	int rv;
	char s[INET6_ADDRSTRLEN];
	
	if (argc != 4){
		cerr << "Args: <username> <server ip> <server port>" << endl;
		return EXIT_FAILURE;
	}

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	if ((rv = getaddrinfo(argv[2], argv[3], &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return EXIT_FAILURE;
	}

	for (port = servinfo; port != NULL; port = port->ai_next) {
		if ((socket_fd = socket(port->ai_family, port->ai_socktype,port->ai_protocol)) == -1) {
			cerr << "ERROR: socket" << endl;
			continue;
		}
			if (connect(socket_fd, port->ai_addr, port->ai_addrlen) == -1) {
			cerr << "ERROR: connecting client" << endl;
			close(socket_fd);
			continue;
		}
		break;
	}

	if (port == NULL) {
		cerr << "ERROR: failed to connect" << endl;
		return EXIT_FAILURE;
	}

	inet_ntop(port->ai_family, get_in_addr((struct sockaddr *)port->ai_addr),s, sizeof s);
	cout << "CONNECTED IP: " << s << endl;
	freeaddrinfo(servinfo);

	string message_serialized;
	chat::ClientPetition *client_request = new chat::ClientPetition();
	chat::UserRegistration *user_registration = new chat::UserRegistration();
	chat::ServerResponse *server_response = new chat::ServerResponse();

	user_registration->set_username(argv[1]);
	user_registration->set_ip(s);
	client_request->set_option(1);
	client_request->set_allocated_registration(user_registration);
	client_request->SerializeToString(&message_serialized);
	strcpy(buffer, message_serialized.c_str());
	send(socket_fd, buffer, message_serialized.size() + 1, 0);
	recv(socket_fd, buffer, 8192, 0);
	server_response->ParseFromString(buffer);

	if (server_response->code() != 200) {
		cout << server_response->servermessage()<< endl;
		return EXIT_SUCCESS;
	}

	cout << "SERVER: " << server_response->servermessage() << endl;
	connected = true;
	pthread_t thread_id;
	pthread_attr_t attrs;
	ThreadArgs thread_args = { &socket_fd, argv[1]};
	pthread_attr_init(&attrs);
	pthread_create(&thread_id, &attrs, Thread_Listener, (void *)&thread_args);
	
	main_loop = true;

	cout
	<< endl << "╔══════════════════════════════════════ HELP ══════════════════════════════════════╗"
	<< endl << "║                                                                                  ║"
	<< endl << "║   /msg  <message>                   |   Global chat                              ║"
	<< endl << "║   /msg  @<username> <message>       |   Private chat                             ║"
	<< endl << "║   /stat <STATUS>                    |   Change status                            ║"
	<< endl << "║   /info                             |   User list                                ║"
	<< endl << "║   /info @<username>                 |   User info                                ║"
	<< endl << "║   /help                             |   Help                                     ║"
	<< endl << "║   /exit                             |   Exit                                     ║"
	<< endl << "║                                                                                  ║"
	<< endl << "╠══════════════════════════════════════════════════════════════════════════════════╣"
	<< endl << "║                                                                                  ║"
	<< endl << "║ STATUS [ 1 = ACTIVO = activo | 2 = INACTIVO = inactivo | 3 = OCUPADO = ocupado ] ║"
	<< endl << "║                                                                                  ║"
	<< endl << "╚══════════════════════════════════════ HELP ══════════════════════════════════════╝" << endl;
	
	const set<string> message_command = { "/msg", "\\msg" ,".msg"};
	const set<string> status_command = { "/stat", "\\stat" ,".stat"};
	const set<string> info_command = { "/info", "\\info" , ".info"};
	const set<string> help_command = { "/h", "\\h", "/help", "\\help", ".help", ".h" };
	const set<string> exit_command = { "/e", "\\e", "/q", "\\q", "/quit", "\\quit", "/exit", "\\exit", ".e", ".q", ".exit", ".quit" };
	
	while (main_loop) {
		while (server_wait) {}

		client_request->Clear();
		string input;
		getline(cin, input);
		vector<string> command_tokens = splitString(input);

		if (message_command.count(command_tokens[0]) and command_tokens.size() > 1 and command_tokens[1][0] != '@') { // everyone
			chat::MessageCommunication *message = new chat::MessageCommunication();
			message->set_sender(argv[1]);
			message->set_recipient("everyone");
			message->set_message(strEndSpace(command_tokens, 1));
			client_request->set_option(4);
			client_request->set_allocated_messagecommunication(message);
			client_request->SerializeToString(&message_serialized);
			strcpy(buffer, message_serialized.c_str());
			send(socket_fd, buffer, message_serialized.size() + 1, 0);
			server_wait = true;
		}
		else if (message_command.count(command_tokens[0]) and command_tokens.size() > 2 and command_tokens[1][0] == '@') { // privado
			chat::MessageCommunication *message = new chat::MessageCommunication();
			message->set_sender(argv[1]);
			message->set_recipient(command_tokens[1].substr(1));
			message->set_message(strEndSpace(command_tokens, 2));
			client_request->set_option(4);
			client_request->set_allocated_messagecommunication(message);
			client_request->SerializeToString(&message_serialized);
			strcpy(buffer, message_serialized.c_str());
			send(socket_fd, buffer, message_serialized.size() + 1, 0);
			server_wait = true;
		}
		else if (status_command.count(command_tokens[0]) and command_tokens.size() > 1) {
			chat::ChangeStatus *status = new chat::ChangeStatus();
			status->set_username(argv[1]);
			if      (command_tokens[1]=="1" or command_tokens[1]=="ACTIVE"   or command_tokens[1]=="activo")
				status->set_status("activo");
			else if (command_tokens[1]=="2" or command_tokens[1]=="INACTIVE" or command_tokens[1]=="inactivo")
				status->set_status("inactivo");
			else if (command_tokens[1]=="3" or command_tokens[1]=="OCUPADO"  or command_tokens[1]=="ocupado")
				status->set_status("ocupado");
			else {
				cerr << "The value entered is invalid" << endl;
			}
			client_request->set_option(3);
			client_request->set_allocated_change(status);
			client_request->SerializeToString(&message_serialized);
			strcpy(buffer, message_serialized.c_str());
			send(socket_fd, buffer, message_serialized.size() + 1, 0);
			server_wait = true;
		}
		else if (info_command.count(command_tokens[0]) and command_tokens.size() == 2 and command_tokens[1][0] == '@') { // privado
			chat::UserRequest *inforequest = new chat::UserRequest();
			inforequest->Clear();
			inforequest->set_user(command_tokens[1].substr(1));
			client_request->set_option(5);
			client_request->set_allocated_users(inforequest);
			client_request->SerializeToString(&message_serialized);
			strcpy(buffer, message_serialized.c_str());
			send(socket_fd, buffer, message_serialized.size() + 1, 0);
			server_wait = true;
		}
		else if (info_command.count(command_tokens[0]) and command_tokens.size() == 1) { // publico
			chat::UserRequest *inforequest = new chat::UserRequest();
			inforequest->set_user("everyone");
			client_request->set_option(2);
			client_request->set_allocated_users(inforequest);
			client_request->SerializeToString(&message_serialized);
			strcpy(buffer, message_serialized.c_str());
			send(socket_fd, buffer, message_serialized.size() + 1, 0);
			server_wait = true;
		}
		else if (help_command.count(command_tokens[0])) {
		cout
		<< endl << "╔══════════════════════════════════════ HELP ══════════════════════════════════════╗"
		<< endl << "║                                                                                  ║"
		<< endl << "║   /msg  <message>                   |   Global chat                              ║"
		<< endl << "║   /msg  @<username> <message>       |   Private chat                             ║"
		<< endl << "║   /stat <STATUS>                    |   Change status                            ║"
		<< endl << "║   /info                             |   User list                                ║"
		<< endl << "║   /info @<username>                 |   User info                                ║"
		<< endl << "║   /help                             |   Help                                     ║"
		<< endl << "║   /exit                             |   Exit                                     ║"
		<< endl << "║                                                                                  ║"
		<< endl << "╠══════════════════════════════════════════════════════════════════════════════════╣"
		<< endl << "║                                                                                  ║"
		<< endl << "║ STATUS [ 1 = ACTIVO = activo | 2 = INACTIVO = inactivo | 3 = OCUPADO = ocupado ] ║"
		<< endl << "║                                                                                  ║"
		<< endl << "╚══════════════════════════════════════ HELP ══════════════════════════════════════╝" << endl;
		}
		else if (exit_command.count(command_tokens[0])) {
			cout << endl << "Bye." << endl << endl;
			main_loop = false;
		}
		else {
			cerr << "The value entered is invalid" << endl;
		}
	}
	pthread_cancel(thread_id);
	return 0;
}

void *get_in_addr(struct sockaddr *sa) {
	if (sa->sa_family == AF_INET) return &(((struct sockaddr_in *)sa)->sin_addr);
	return &(((struct sockaddr_in6 *)sa)->sin6_addr);
}

void *Thread_Listener(void *args) {
	while (true) {
		char message_buffer[8192];
		ThreadArgs* arguments = reinterpret_cast<ThreadArgs*>(args);
		string username = arguments->username;
		int *socket = (int *)arguments->socket;
		chat::ServerResponse *server_response = new chat::ServerResponse();
		int incoming_bytes = recv(*socket, message_buffer, 8192, 0);
		if (incoming_bytes == 0) {
			cout << endl << "DICONNECTED FROM SERVER" << endl;
			connected = false;
		}
		server_response->ParseFromString(message_buffer);
	
		if (server_response->code() == 200) {
			switch (server_response->option()) {
				case 1: { // register user
					if (server_response->has_servermessage())
						cout << server_response->servermessage() << endl;
					break;
				}
				case 2: { // all users info
					if (server_response->has_servermessage())
						cout << server_response->servermessage() << endl;
					for(int i = 0; i < server_response->connectedusers().connectedusers_size(); i++)
						cout << "Username [ @" << server_response->connectedusers().connectedusers(i).username() << " ] IP [ " << server_response->connectedusers().connectedusers(i).ip() << " ] Status [ " << server_response->connectedusers().connectedusers(i).status() << " ]" << endl;
					break;
				}
				case 3: { // cambio de estado
					if (server_response->has_servermessage())
						cout << server_response->servermessage() << endl;
					break;
				}
				case 4: { // message
					if (server_response->has_servermessage())
						cout << server_response->servermessage() << endl;
					if (server_response->messagecommunication().has_recipient() and server_response->messagecommunication().has_message() and server_response->messagecommunication().has_sender()) {	
						if (server_response->messagecommunication().recipient() == "everyone") {
							if (server_response->messagecommunication().sender() != username)
								cout << "(g) from @" << server_response->messagecommunication().sender() << " to @everyone: " << server_response->messagecommunication().message() << endl;
							else
								cout << "(g) from You: " << server_response->messagecommunication().message() << endl;
						}
						else {
							if (server_response->messagecommunication().sender() != username)
								cout << "(p) from @" << server_response->messagecommunication().sender() << " to You: " << server_response->messagecommunication().message() << endl;
							else if (server_response->messagecommunication().recipient() != username)
								cout << "(p) from You to @ " << server_response->messagecommunication().recipient() << ": " << server_response->messagecommunication().message() << endl;
							else
								cout << "(p) from You to Yourself: " << server_response->messagecommunication().message() << endl;
						}
					}
					break;
				}
				case 5: { // user info
					if (server_response->has_servermessage())
						cout << server_response->servermessage() << endl;
					if (server_response->has_userinforesponse())
						cout << "Username [ @" << server_response->userinforesponse().username() << " ] IP [ " << server_response->userinforesponse().ip() << " ] Status [ " << server_response->userinforesponse().status() << " ]" << endl;
					break;
				}
				default: {
					cout << "Unknown option Error" << endl;
					break;
				}
			}
		}
		else if (server_response->code() == 500)
			cout << "ERROR 500" << ": " << server_response->servermessage() << endl;
		else {
			cout << endl << "DISCONNECTED FROM SERVER" << endl;
			server_response->Clear();
			main_loop = false;
			server_wait = false;
			connected = false;
			pthread_exit(0);
		}
		server_response->Clear();
		server_wait = false;
		if (!connected) {
			main_loop = false;
			server_wait = false;
			pthread_exit(0);
		}
	}
}