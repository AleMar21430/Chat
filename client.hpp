#include "utils.hpp"

#include "proyecto.pb.h"

struct ThreadArgs {
    int* socket;
    string username;
};

void *get_in_addr(struct sockaddr *sa);
void *Thread_Listener(void *args);

void changeStatus(const vector<string>& data);
void sendPrivateMsg(const vector<string>& data);
void sendPublicMsg(const vector<string>& data);
void getUserInfo(const vector<string>& data);
void getAllUserInfo(const vector<string>& data);