#include "server.hpp"

unordered_map<string, User*> connected_clients;

int main(int argc, char const* argv[]){
	GOOGLE_PROTOBUF_VERIFY_VERSION;

	if (argc != 2) {
		cerr << "Args: <server port>" << endl;
		return EXIT_FAILURE;
	}

	long port = strtol(argv[1], NULL, 10);
	sockaddr_in server, connection;
	socklen_t connection_size;
	int socket_node, client_socket_node;
	char connection_address[INET_ADDRSTRLEN];
	server.sin_family = AF_INET;
	server.sin_port = htons(port);
	server.sin_addr.s_addr = INADDR_ANY;
	memset(server.sin_zero, 0, sizeof server.sin_zero);
	socket_node = socket(AF_INET, SOCK_STREAM, 0);
	if (socket_node == -1) {
		cerr << "ERROR creating socket" << endl;
		return EXIT_FAILURE;
	}

	if (bind(socket_node, (struct sockaddr *)&server, sizeof(server)) == -1) {
		close(socket_node);
		cerr << "ERROR binding IP to socket" << endl;
		return EXIT_FAILURE;
	}

	if (listen(socket_node, 5) == -1) {
		close(socket_node);
		cerr << "ERROR listening on port " << port << endl;
		return EXIT_FAILURE;
	}

	cout << "SUCCESS listening on port " << port << endl;
	
	while (true) {
		connection_size = sizeof(connection);
		client_socket_node = accept(socket_node, (struct sockaddr *)&connection, &connection_size);

		if (client_socket_node == -1) {
			cerr << "ERROR accepting incomming socket connection" << endl;
			continue;
		}

		struct User client;
		client.socket_node = client_socket_node;
		inet_ntop(AF_INET, &(connection.sin_addr), client.ip_address, INET_ADDRSTRLEN);
		pthread_t thread_id;
		pthread_attr_t attrs;
		pthread_attr_init(&attrs);
		pthread_create(&thread_id, &attrs, Worker_Thread, (void *)&client);

	}
	google::protobuf::ShutdownProtobufLibrary();
	return 0;
}

void SendErrorResponse(int socket_node, string errorMsg) {
	string message_stream;
	chat::ServerResponse *server_error = new chat::ServerResponse();
	server_error->set_option(1);
	server_error->set_code(500);
	server_error->set_servermessage(errorMsg);
	server_error->SerializeToString(&message_stream);
	char message_buffer[message_stream.size() + 1];
	strcpy(message_buffer, message_stream.c_str());
	send(socket_node, message_buffer, sizeof(message_buffer), 0);
}

void *Worker_Thread(void *params) {
	struct User user;
	struct User *client_params = (struct User *)params;
	int socket_node = client_params->socket_node;
	char message_buffer[8192];
	
	string message_stream;
	chat::ClientPetition *client_request = new chat::ClientPetition();
	chat::ServerResponse *server_response = new chat::ServerResponse();
	
	while (true) {
		server_response->Clear();

		if (recv(socket_node, message_buffer, 8192, 0) < 1) {
			if (recv(socket_node, message_buffer, 8192, 0) == 0) {
				cout << "@" << user.username << " logged out" << endl;
				connected_clients.erase(user.username);
			}
			break;
		}
		client_request->ParseFromString(message_buffer);
		switch (client_request->option()) {
			case 1: { // Registro de Usuarios 
				if (connected_clients.count (client_request->registration().username()) > 0) {
					cout << "ERROR Username @" << client_request->registration().username() << "already exists" << endl;
					SendErrorResponse(socket_node, "ERROR Username already exists");
					break;
				}
				server_response->set_option(1);
				server_response->set_servermessage("SUCCESS registering user");
				server_response->set_code(200);
				server_response->SerializeToString(&message_stream);
				strcpy(message_buffer, message_stream.c_str());
				send(socket_node, message_buffer, message_stream.size() + 1, 0);
				cout << "SUCCESS connecting User @" << client_request->registration().username() << " | IP " << client_request->registration().ip() << " | Socket " << socket_node << endl;
				user.username = client_request->registration().username();
				user.socket_node = socket_node;
				user.status = "activo";
				user.last_active_time = chrono::steady_clock::now();
				strcpy(user.ip_address, client_params->ip_address);
				connected_clients[user.username] = &user;
				break;
			}
			case 2: { // Usuarios Conectados
				try {
					connected_clients[user.username]->last_active_time = chrono::steady_clock::now();
					chat::ConnectedUsersResponse *connected_user_response = new chat::ConnectedUsersResponse();
					for(pair<const string, User*> client : connected_clients){
						const chrono::_V2::steady_clock::time_point end = chrono::steady_clock::now();
						const double elapsed_secs = chrono::duration_cast<std::chrono::duration<double>>(end - client.second->last_active_time).count();
						if (elapsed_secs >= 5.0) {
							client.second->status = "inactivo";
							cout << "Timeout de @" << client.second->username << "por inactividad" << endl;
						} else {
							cout << "Delta de actividad de @" << client.second->username << ": " << elapsed_secs << endl;
						}
						chat::UserInfo *user_info = new chat::UserInfo();
						user_info->set_username(client.second->username);
						user_info->set_status(client.second->status);
						user_info->set_ip(client.second->ip_address);
						connected_user_response->add_connectedusers()->CopyFrom(*user_info);
					}
					server_response->set_servermessage("SUCCESS receiving info of all connected users");
					server_response->set_allocated_connectedusers(connected_user_response);
					server_response->set_option(2);
					server_response->set_code(200);
					server_response->SerializeToString(&message_stream);
					strcpy(message_buffer, message_stream.c_str());
					send(socket_node, message_buffer, message_stream.size() + 1, 0);
					cout << "SUCCESS providing info of all connected users for @" << user.username << endl;
				}
				catch (const exception& e) {
					server_response->set_servermessage("ERROR retrieving info of all connected users");
					server_response->set_code(500);
					server_response->SerializeToString(&message_stream);
					strcpy(message_buffer, message_stream.c_str());
					send(socket_node, message_buffer, message_stream.size() + 1, 0);
					cout << "ERROR providing info of all connected users for @" << user.username << endl;
				}
				break;
			}
			case 3: { // Cambio de Estado
				try {
					connected_clients[user.username]->status = client_request->change().status();
					chat::ChangeStatus *change_status = new chat::ChangeStatus();
					change_status->set_username(client_request->change().username());
					change_status->set_status(client_request->change().status());
					server_response->set_allocated_change(change_status);
					server_response->set_servermessage("SUCCESS changing status to " + client_request->change().status());
					server_response->set_code(200);
					server_response->set_option(3);
					server_response->SerializeToString(&message_stream);
					strcpy(message_buffer, message_stream.c_str());
					send(socket_node, message_buffer, message_stream.size() + 1, 0);
					cout << "SUCCESS changing status for @" << client_request->change().username() << " to " << client_request->change().status() << endl;
				}
				catch (const exception& e) {
					server_response->set_servermessage("ERROR changing status to " + client_request->change().status());
					server_response->set_code(500);
					server_response->SerializeToString(&message_stream);
					strcpy(message_buffer, message_stream.c_str());
					send(socket_node, message_buffer, message_stream.size() + 1, 0);
					cout << "ERROR changing status for @" << client_request->change().username() << " to " << client_request->change().status() << endl;
				}
				break;
			}
			case 4:{ // Mensajes
				if (client_request->messagecommunication().recipient() == "everyone") {
					try {
						for (pair<const string, User*> client : connected_clients){
							if (client.first==client_request->messagecommunication().sender()) {
								client.second->last_active_time = chrono::steady_clock::now();
								chat::MessageCommunication *message_communication = new chat::MessageCommunication();
								message_communication->set_sender(user.username);
								message_communication->set_recipient("everyone");
								message_communication->set_message(client_request->messagecommunication().message());
								server_response->set_allocated_messagecommunication(message_communication);
								server_response->set_servermessage("SUCCESS sending a global message");
								server_response->set_code(200);
								server_response->set_option(4);
								server_response->SerializeToString(&message_stream);
								strcpy(message_buffer, message_stream.c_str());
								send(socket_node, message_buffer, message_stream.size() + 1, 0);
							}
							else {
								chat::MessageCommunication *message_communication = new chat::MessageCommunication();
								message_communication->set_sender(user.username);
								message_communication->set_recipient("everyone");
								message_communication->set_message(client_request->messagecommunication().message());
								server_response->set_allocated_messagecommunication(message_communication);
								server_response->set_servermessage("SUCCESS retreiving a global message from @" + user.username);
								server_response->set_code(200);
								server_response->set_option(4);
								server_response->SerializeToString(&message_stream);
								strcpy(message_buffer, message_stream.c_str());
								send(client.second->socket_node, message_buffer, message_stream.size() + 1, 0);
							}
						}
						cout << "SUCCESS sending a global message from @" << client_request->messagecommunication().sender() << " [ "  << client_request->messagecommunication().message() << " ]" << endl;
					}
					catch (const exception& e) {
						server_response->set_servermessage("ERROR sending a global message");
						server_response->set_code(500);
						server_response->SerializeToString(&message_stream);
						strcpy(message_buffer, message_stream.c_str());
						send(socket_node, message_buffer, message_stream.size() + 1, 0);
						cout << "ERROR sending a global message from @" << client_request->messagecommunication().sender() << " [ "  << client_request->messagecommunication().message() << " ]" << endl;
					}
				}
				else {
					if (connected_clients.count(client_request->messagecommunication().recipient()) > 0) {
						try {
							chat::MessageCommunication *message_communication = new chat::MessageCommunication();
							message_communication->set_sender(user.username);
							message_communication->set_recipient(client_request->messagecommunication().recipient());
							message_communication->set_message(client_request->messagecommunication().message());
							server_response->set_allocated_messagecommunication(message_communication);
							server_response->set_servermessage("SUCCESS receiving a private message from @" + user.username);
							server_response->set_code(200);
							server_response->set_option(4);
							server_response->SerializeToString(&message_stream);
							strcpy(message_buffer, message_stream.c_str());
							send(connected_clients[client_request->messagecommunication().recipient()]->socket_node, message_buffer, message_stream.size() + 1, 0);
							server_response->Clear();

							connected_clients[user.username]->last_active_time = chrono::steady_clock::now();
							chat::MessageCommunication *response_message = new chat::MessageCommunication();
							response_message->set_sender(user.username);
							response_message->set_recipient(client_request->messagecommunication().recipient());
							response_message->set_message(client_request->messagecommunication().message());
							server_response->set_allocated_messagecommunication(response_message);
							server_response->set_servermessage("SUCCESS sending a private message to @" + client_request->messagecommunication().recipient());
							server_response->set_code(200);
							server_response->set_option(4);
							server_response->SerializeToString(&message_stream);
							strcpy(message_buffer, message_stream.c_str());
							send(socket_node, message_buffer, message_stream.size() + 1, 0);
							cout << "SUCCESS sending a private message from @" << client_request->messagecommunication().sender() << " to @" << client_request->messagecommunication().recipient() << " [ "  << client_request->messagecommunication().message() << " ]" << endl;
						}
						catch (const exception& e) {
							server_response->set_servermessage("ERROR sending message to @" + client_request->messagecommunication().recipient());
							server_response->set_code(500);
							server_response->SerializeToString(&message_stream);
							strcpy(message_buffer, message_stream.c_str());
							send(socket_node, message_buffer, message_stream.size() + 1, 0);
							cout << "ERROR sending a private message from @" << client_request->messagecommunication().sender() << " to @" << client_request->messagecommunication().recipient() << " [ "  << client_request->messagecommunication().message() << " ]" << endl;
						}
					}
					else {
						server_response->set_servermessage("ERROR sending a message to an unexisting user: @" + client_request->messagecommunication().recipient());
						server_response->set_code(500);
						server_response->SerializeToString(&message_stream);
						strcpy(message_buffer, message_stream.c_str());
						send(socket_node, message_buffer, message_stream.size() + 1, 0);
						cout << "ERROR sending a private message from @" << client_request->messagecommunication().sender() << " to an unexisting user @" << client_request->messagecommunication().recipient() << endl;
					}
				}
				break;
			}
			case 5: { // Info Usuario Particular
				if (client_request->users().user() == "everyone") {
					try {
						connected_clients[user.username]->last_active_time = chrono::steady_clock::now();
						chat::ConnectedUsersResponse *connected_user_response = new chat::ConnectedUsersResponse();
						for( pair<const string, User*> client : connected_clients) {
						const chrono::_V2::steady_clock::time_point end = chrono::steady_clock::now();
						const double elapsed_secs = chrono::duration_cast<std::chrono::duration<double>>(end - client.second->last_active_time).count();
							if (elapsed_secs >= 5.0) {
								client.second->status = "inactivo";
								cout << "Timeout de @" << client.second->username << " por inactividad" << endl;
							} else {
								cout << "Delta de actividad de @" << client.second->username << ": " << elapsed_secs << endl;
							}
							chat::UserInfo *user_info = new chat::UserInfo();
							user_info->set_username(client.second->username);
							user_info->set_status(client.second->status);
							user_info->set_ip(client.second->ip_address);
							connected_user_response->add_connectedusers()->CopyFrom(*user_info);
						}
						server_response->set_servermessage("SUCCESS retreiving info of all users");
						server_response->set_allocated_connectedusers(connected_user_response);
						server_response->set_option(2);
						server_response->set_code(200);
						server_response->SerializeToString(&message_stream);
						strcpy(message_buffer, message_stream.c_str());
						send(socket_node, message_buffer, message_stream.size() + 1, 0);
						cout << "SUCCESS providing info of all users for @" << user.username << endl;
					}
					catch (const exception& e) {
						server_response->set_servermessage("ERROR retrieving info of all connected clients");
						server_response->set_code(500);
						server_response->SerializeToString(&message_stream);
						strcpy(message_buffer, message_stream.c_str());
						send(socket_node, message_buffer, message_stream.size() + 1, 0);
						cout << "ERROR providing info of all connected clients for @" << user.username << endl;
					}
				}
				else {
					try {
						chat::UserInfo *user_info = new chat::UserInfo();
						connected_clients[user.username]->last_active_time = chrono::steady_clock::now();
						const chrono::_V2::steady_clock::time_point end = chrono::steady_clock::now();
						const double elapsed_secs = chrono::duration_cast<std::chrono::duration<double>>(end - connected_clients[client_request->users().user()]->last_active_time).count();
						if (elapsed_secs >= 5.0) {
							connected_clients[client_request->users().user()]->status = "inactivo";
							cout << "Timeout de @" << connected_clients[client_request->users().user()]->username << " por inactividad" << endl;
						} else {
							cout << "Delta de actividad de @" << client_request->users().user() << ": " << elapsed_secs << endl;
						}
						user_info->set_username(connected_clients[client_request->users().user()]->username);
						user_info->set_status(connected_clients[client_request->users().user()]->status);
						user_info->set_ip(connected_clients[client_request->users().user()]->ip_address);
						server_response->set_allocated_userinforesponse(user_info);
						server_response->set_servermessage("SUCCESS providing info of @" + client_request->users().user());
						server_response->set_code(200);
						server_response->set_option(5);
						server_response->SerializeToString(&message_stream);
						strcpy(message_buffer, message_stream.c_str());
						send(socket_node, message_buffer, message_stream.size() + 1, 0);
						cout << "SUCCESS providing info of @" << client_request->users().user() << " for @" << user.username << endl;
					}
					catch (const exception& e) {
						server_response->set_servermessage("ERROR retrieving info of @" + client_request->users().user());
						server_response->set_code(500);
						server_response->SerializeToString(&message_stream);
						strcpy(message_buffer, message_stream.c_str());
						send(socket_node, message_buffer, message_stream.size() + 1, 0);
						cout << "ERROR providing info of " << client_request->users().user() << " for @" << user.username << endl;
					}
				}
				break;
			}
			default: {
				server_response->set_servermessage("ERROR sent an unkown command");
				server_response->set_code(500);
				server_response->SerializeToString(&message_stream);
				strcpy(message_buffer, message_stream.c_str());
				send(socket_node, message_buffer, message_stream.size() + 1, 0);
				cout << "ERROR @" << user.username << " sent an unknown command" << endl;
				break;
			}
		}
	}
	return params;
}