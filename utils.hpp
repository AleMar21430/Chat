#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <unordered_map>
#include <semaphore.h>
#include <pthread.h>
#include <iostream>
#include <stdlib.h>
#include <unistd.h>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <fstream>
#include <variant>
#include <cstdlib>
#include <stdio.h>
#include <string>
#include <memory>
#include <cerrno>
#include <vector>
#include <chrono>
#include <thread>
#include <random>

using namespace std;

inline vector<string> splitString(const string& input) {
	vector<string> result;
	istringstream iss(input);
	string token;
	while (iss >> token) {
		result.push_back(token);
	}
	return result;
}

inline string strEndSpace(const vector<string>& i_tokens, const size_t& i_start) {
	return accumulate(
		i_tokens.begin() + i_start, i_tokens.end(), string(),
		[](const string& accumulator, const string& current) {
			return accumulator.empty() ? current : accumulator + " " + current;
		}
	);
}

inline bool isInactive(const clock_t& start) {
    const clock_t end = clock();
    const double elapsed_secs = double(end - start) / CLOCKS_PER_SEC;
    return elapsed_secs >= 5.0;
}