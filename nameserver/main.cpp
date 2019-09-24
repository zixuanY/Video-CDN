#include <iostream>
#include <arpa/inet.h>
#include <string.h>
#include <sys/select.h>
#include <algorithm>
#include <cassert>
#include <stdlib.h>
#include <string>
#include <cstring>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include "unistd.h"
#include "ctime"
#include <vector>
#include <fstream>
#include "DNSHeader.h"
#include "DNSQuestion.h"
#include "DNSRecord.h"
#include <unordered_map>
#include <unordered_set>
#include <stdint.h>
#include <limits.h>

using namespace std;

struct servers {
	vector<string> IPs; // vector for round robin
	// the following are used for geography_based
	vector<vector<int>> matrix; // matrix for the cost
	unordered_map<int, pair<string, string>> elements; // number -> type, ip
};

string logFile(const string &clientIP, const string &queryName, const string &responseIP);
string roundRobin(const servers &data, int &ip);
string geography(servers &data, const string &ip);
int process(servers &data, const string &log, int port, bool rr);

int main(int argc, char *argv[]) {
	if (argc != 5) {
		cerr << "Wrong usage: ./nameserver <log> <port> <geography_based> <servers>\n";
		return 1;
	}
	string log = argv[1];
	ofstream oFile;
	oFile.open(log.c_str());
	oFile.close();
	int port = atoi(argv[2]);
	int geography_based = atoi(argv[3]);
	string serversFile = argv[4];

	servers serverData;

	if (geography_based) {
		ifstream iFile;
		iFile.open(serversFile.c_str());
		string tmp;
		int nodes;
		iFile >> tmp >> nodes;
		int number;
		string ip;
		for (int i = 0; i < nodes; ++i) {
			iFile >> number >> tmp >> ip;
			serverData.elements[number] = make_pair(tmp, ip);
		}

		int links;
		iFile >> tmp >> links;
		vector<vector<int>> m(nodes, vector<int>(nodes, INT_MAX));
		for (int i = 0; i < links; ++i) {
			int p1, p2, cost;
			iFile >> p1 >> p2 >> cost;
			m[p1][p2] = cost;
			m[p2][p1] = cost;
		}
		serverData.matrix = move(m);
		iFile.close();
		// the distance based scheme
		process(serverData, log, port, false);
	} else {
		ifstream iFile;
		iFile.open(serversFile.c_str());
		string IP;
		while (iFile >> IP) {
			serverData.IPs.push_back(IP);
		}
		iFile.close();
		// round-robin load balancing scheme
		process(serverData, log, port, true);
	}
	return 0;
}

string logFile(const string &clientIP, const string &queryName, const string &responseIP) {
	return clientIP + " " + queryName + " " + responseIP + "\n";
}

string roundRobin(const servers &data, int &ip) {
	string ans = data.IPs[ip % data.IPs.size()];
	ip++;
	return ans;
}

struct geoStruct {
	bool visited;
	int cost;
	int pred;
};

string geography(servers &data, const string &ip) {
	string ans;
	vector<geoStruct> visitCost;
	geoStruct g{false, INT_MAX, -1};
	for (int i = 0; i < data.elements.size(); ++i) {
		visitCost.push_back(g);
	}

	bool find = false;
	for (auto &x: data.elements) {
		if (x.second.second == ip) {
			find = true;
			visitCost[x.first].visited = true;
			visitCost[x.first].cost = 0;
			visitCost[x.first].pred = 0;
			for (int i = 0; i < visitCost.size(); ++i) {
				if (data.matrix[x.first][i] < INT_MAX) {
					int cur = data.matrix[x.first][i];
					if (visitCost[i].cost > cur) {
						visitCost[i].cost = cur;
						visitCost[i].pred = x.first;
					}
				}
			}
			break;
		}
	}
	if (!find) return "nothing";

	while (true) {
		int position = -1;
		int smallestCost = INT_MAX;
		for (int i = 0; i < visitCost.size(); ++i) {
			if (!visitCost[i].visited && visitCost[i].cost < smallestCost) {
				smallestCost = visitCost[i].cost;
				position = i;
			}
		}
		visitCost[position].visited = true;
		if (data.elements[position].first == "SERVER") {
			ans = data.elements[position].second;
			break;
		}
		for (int i = 0; i < visitCost.size(); ++i) {
			if (data.matrix[position][i] < INT_MAX) {
				int cur = data.matrix[position][i] + visitCost[position].cost;
				if (visitCost[i].cost > cur) {
					visitCost[i].cost = cur;
					visitCost[i].pred = position;
				}
			}
		}
	}
	return ans;
}

int process(servers &data, const string &log, int port, bool rr) {
	ofstream oFile;
	int ip = 0; // cursor for the IP array
	string queryName = "video.cse.umich.edu";

	// Bind server to sd and set up listen server
	int sd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	struct sockaddr_in self, client;
	int clientlen = sizeof(client);
	self.sin_family = AF_INET;
	self.sin_addr.s_addr = INADDR_ANY;
	self.sin_port = htons((u_short) port);
	bind(sd, (struct sockaddr *) &self, sizeof(self));

	int err = listen(sd, 10);
	if (err == -1) {
		cout << "Error setting up listen queue\n";
		return 1;
	}

	// Set of file descriptors to listen to
	fd_set readSet;
	// Keep track of each file descriptor accepted
	vector<int> fds;
	int selflen = sizeof(self);

//	string clientIP;
	while (true) {
		// Set up the readSet
		FD_ZERO(&readSet);
		FD_SET(sd, &readSet);
		for (int i = 0; i < (int) fds.size(); ++i) {
			FD_SET(fds[i], &readSet);
		}


		int maxfd = 0;
		if (fds.size() > 0) {
			maxfd = *max_element(fds.begin(), fds.end());
		}
		maxfd = max(maxfd, sd);

		// maxfd + 1 is important
		int err = select(maxfd + 1, &readSet, NULL, NULL, NULL);
		assert(err != -1);

		if (FD_ISSET(sd, &readSet)) {
			int clientsd = accept(sd, (struct sockaddr *)&self, (socklen_t *)&selflen);
			if (clientsd == -1) {
				cout << "Error on accept" << endl;
			} else {
				fds.push_back(clientsd);
				if (getpeername(clientsd, (struct sockaddr*)&client, (socklen_t *)&clientlen) == -1) {
					cout << "Get peername failed!" << endl;
					cout << strerror(errno) << endl;
					exit(1);
				}
//				clientIP = inet_ntoa(client.sin_addr);
			}
		}

		for (int i = 0; i < (int) fds.size(); ++i) {
			if (FD_ISSET(fds[i], &readSet)) {
				//  receive a 4-byte "integer" (remember to convert from Network Byte Order) as the length of the expected std::string
				int headerlen;
				int bytesRecvd = recv(fds[i], &headerlen, sizeof(int), MSG_WAITALL);
				if (bytesRecvd < 0) {
					cout << "Error recving bytes" << endl;
					cout << strerror(errno) << endl;
					exit(1);
				} else if (bytesRecvd == 0) {
					cout << "Connection closed" << endl;
					fds.erase(fds.begin() + i);
					--i;
					continue;
				}
				headerlen = ntohl(headerlen);
				vector<char> headerStr(headerlen);
				bytesRecvd = recv(fds[i], &(headerStr[0]), headerlen, MSG_WAITALL);
				if (bytesRecvd < 0) {
					cout << "Error recving bytes" << endl;
					cout << strerror(errno) << endl;
					exit(1);
				} else if (bytesRecvd == 0) {
					cout << "Connection closed" << endl;
					fds.erase(fds.begin() + i);
					--i;
					continue;
				}
				string headerString;
				headerString.append(headerStr.cbegin(), headerStr.cend());
				DNSHeader header;
				header = header.decode(headerString);

				int lengthHost;
				bytesRecvd = recv(fds[i], &lengthHost, sizeof(int), MSG_WAITALL);
				if (bytesRecvd < 0) {
					cout << "Error recving bytes" << endl;
					cout << strerror(errno) << endl;
					exit(1);
				} else if (bytesRecvd == 0) {
					cout << "Connection closed" << endl;
					fds.erase(fds.begin() + i);
					--i;
					continue;
				}
				int length = ntohl(lengthHost);

				// receive the content for a std::string
				vector<char> buf(length);
				bytesRecvd = recv(fds[i], &(buf[0]), length, MSG_WAITALL);
				if (bytesRecvd < 0) {
					cout << "Error recving bytes" << endl;
					cout << strerror(errno) << endl;
					exit(1);
				} else if (bytesRecvd == 0) {
					cout << "Connection closed" << endl;
					fds.erase(fds.begin() + i);
					--i;
					continue;
				}

				string hostRecvd;
				hostRecvd.append(buf.cbegin(), buf.cend());
				DNSQuestion question;
				question = question.decode(hostRecvd);
				string hostRequire = question.QNAME;

				// encode the IP
				DNSHeader header2;
				header2.ID = 0;
				header2.QR = 0;
				header2.OPCODE = 0;
				header2.TC = 0;
				header2.QDCOUNT = 0;
				header2.ANCOUNT = 0;
				header2.AA = 1;
				header2.RD = 0;
				header2.RA = 0;
				header2.Z = 0;
				header2.NSCOUNT = 0;
				header2.ARCOUNT = 0;
				header2.RCODE = 0;
				if (hostRequire != queryName) header2.RCODE = 3;
				string headerEncode = DNSHeader::encode(header2);
				int size = headerEncode.length();
				headerlen = htonl(size);
				send(fds[i], (char *)&headerlen, sizeof(int), 0);
				send(fds[i], headerEncode.c_str(), size, 0);

				string hostIP;
				if (rr) hostIP = roundRobin(data, ip);
				else hostIP = geography(data, inet_ntoa(client.sin_addr));
				if (hostIP == "nothing") {
					cout << "The client ip is invalid\n";
					continue;
				}
				cout << hostIP << endl;

				DNSRecord record;
				record.TYPE = 1;
				record.CLASS = 1;
				record.TTL = 0;
				strcpy(record.RDATA, hostIP.c_str());
				string recordEncode = record.encode(record);
				int recordlen = htonl(recordEncode.size());
				send(fds[i], (char *)&recordlen, sizeof(int), 0);
				send(fds[i], recordEncode.c_str(), recordEncode.size(), 0);
				string resp = logFile(inet_ntoa(client.sin_addr), queryName, hostIP);
				oFile.open(log.c_str(), ios::app);
				if (resp != "") {
					oFile << resp;
				}
				oFile.close();
			}
		}
	}
	return 0;
}
