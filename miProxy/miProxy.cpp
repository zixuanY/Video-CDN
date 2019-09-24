#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <netdb.h>
#include <vector>
#include <algorithm>
#include <cassert>
#include <iostream>
#include <cstdlib>
#include <iomanip>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include <unistd.h>
#include <fstream>
#include <sstream>
#include "DNSRecord.h"
#include "DNSQuestion.h"
#include "DNSHeader.h"

using namespace std;

vector<string> available_bitrate;
ofstream myfile;

void error(string msg) {
	cerr << msg << endl;
	myfile.close();
	exit(1);
}

int getContentLength(char *buffer) {
	string str(buffer);
	size_t pos_start = str.find("Content-Length: ");
	int content_length = -1;
	if (pos_start != string::npos) {
		size_t pos_end = str.find("\r\n", pos_start);
		content_length = stoi(str.substr(pos_start + 16, pos_end - pos_start - 16));
	}
	return content_length;
}

int isEndHTTPHeader(char *buffer) {
	string str(buffer);
	size_t pos = str.find("\r\n\r\n");
	if (pos != string::npos) {
		return pos;
	}
	return -1;
}

int myRecv(char *buffer, int sockfd) {
	int content_length_s, header_length;
	int recv_byte = 0;
	size_t pos = 0;
	while (recv_byte < 8000) {
		recv_byte += read(sockfd, buffer + recv_byte, 8000 - recv_byte);
		cout << "Read inside myrecv" << endl;
		if (recv_byte < 0) error("ERROR reading from socket");
		pos = isEndHTTPHeader(buffer);
		if (pos != -1) {
			header_length = pos + 4;
			cout << "Head_length in my recv: " << header_length << endl;
			content_length_s = getContentLength(buffer);
			cout << "Content-length in myrecv: " << content_length_s << endl;
			break;
		}

	}
	while (recv_byte < content_length_s + header_length) {
		int bytesRecvd = read(sockfd, buffer + recv_byte, content_length_s + header_length - recv_byte);
		if (bytesRecvd < 0) {
			error("Error recving bytes");
			exit(1);
		}
		recv_byte += bytesRecvd;
	}
	return recv_byte;
}

string LowestAvlb(double t_cur) {
	for (int i = 1; i < available_bitrate.size(); i++) {
		if (stoi(available_bitrate[i]) * 1.5 > t_cur)
			return available_bitrate[i - 1];
	}
	return available_bitrate[available_bitrate.size() - 1];
}

vector<string> getAvailableBitrate(char *buffer) {
	size_t pos = 0;
	string str(buffer);
	vector<string> bit_vct;
	while (1) {
		pos = str.find("<media", pos);
		cout << "pos: " << pos << endl;
		if (pos != string::npos) {
			size_t bitrate_pos = str.find("bitrate", pos);
			if (bitrate_pos == string::npos)
				error("Error finding bitrate");
			size_t quote1 = str.find("\"", bitrate_pos);
			if (quote1 == string::npos)
				error("Error finding first quotation mark");
			size_t quote2 = str.find("\"", quote1 + 1);
			if (quote2 == string::npos)
				error("Error finding second quotation mark");
			string bitrate_value = str.substr(quote1 + 1, quote2 - quote1 - 1);
			bit_vct.push_back(bitrate_value);
		} else {
			break;
		}
		pos++;
	}
	return bit_vct;
}

string parseGETrequest(char *buffer) {
	string str(buffer);
	size_t pos_start = str.find("GET");
	string get_msg;
	string path;
	if (pos_start != string::npos) {
		size_t pos_end = str.find("\r\n", pos_start);
		get_msg = str.substr(pos_start + 4, pos_end - pos_start - 4);
		int path_end = get_msg.find("HTTP") - 1;
		path = get_msg.substr(0, path_end);
	}
	return path;
}

string getHostName(char *buffer) {
	string str(buffer);
	size_t pos_start = str.find("Host: ");
	string host_name;
	if (pos_start != string::npos) {
		size_t pos_end = str.find("\r\n", pos_start);
		host_name = str.substr(pos_start + 6, pos_end - pos_start - 6);
	}
	return host_name;
}

string logFile(const string &browserIP, const double &duration, const double &T_new, const double &T_curs,
               const string &proper_bitrate, const string &wwwIP,
               string chunkname) {
	stringstream ss;
	ss << browserIP << " " << duration << " " << T_new << " " << T_curs << " " << proper_bitrate
	   << " " << wwwIP << " " << chunkname << endl;
	return ss.str();
}

string getIP(const int &portNum, const string &dnsIP) {
	int serversd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (serversd == -1) {
		cout << "Error creating server socket\n";
		exit(1);
	}

	// Set up a connection to the server
	struct sockaddr_in server;
	server.sin_family = AF_INET;
	server.sin_port = htons((u_short) portNum);
	inet_aton(dnsIP.c_str(), &server.sin_addr);
	int err = connect(serversd, (sockaddr *) &server, sizeof(server));
	if (err == -1) {
		std::cout << "Error on connect\n";
		exit(1);
	}

	// Enter a loop where it constantly sends one byte to the server and gets one byte back
	DNSHeader header2;
	header2.AA = 1;
	header2.RD = 0;
	header2.RA = 0;
	header2.Z = 0;
	header2.NSCOUNT = 0;
	header2.ARCOUNT = 0;
	header2.ID = 0;
	header2.QR = 0;
	header2.OPCODE = 0;
	header2.TC = 0;
	header2.QDCOUNT = 0;
	header2.ANCOUNT = 0;
	header2.RCODE = 0;
	string headerEncode = header2.encode(header2);
	int headerlen = htonl(headerEncode.size());
	send(serversd, (char *) &headerlen, sizeof(int), 0);
	send(serversd, headerEncode.c_str(), headerEncode.size(), 0);
	DNSQuestion question;
	strcpy(question.QNAME, "video.cse.umich.edu");
	question.QCLASS = 1;
	question.QTYPE = 1;
	string questionEncode = question.encode(question);
	int questionlen = htonl(questionEncode.size());
	send(serversd, (char *) &questionlen, sizeof(int), 0);
	send(serversd, questionEncode.c_str(), questionEncode.size(), 0);

	recv(serversd, &headerlen, sizeof(int), MSG_WAITALL);
	headerlen = ntohl(headerlen);
	vector<char> headerStr;
	headerStr.resize(headerlen);
	recv(serversd, headerStr.data(), headerlen, MSG_WAITALL);
	string headerString(headerStr.begin(), headerStr.end());
	DNSHeader header;
	header = DNSHeader::decode(headerString);

	string ip;
	if (header.RCODE != 3) {
		int recordlen;
		recv(serversd, &recordlen, sizeof(int), MSG_WAITALL);
		recordlen = ntohl(recordlen);
		vector<char> recordStr(recordlen);
		recv(serversd, &(recordStr[0]), recordlen, MSG_WAITALL);
		string recordString;
		recordString.append(recordStr.cbegin(), recordStr.cend());
		// decode
		DNSRecord record;
		record = record.decode(recordString);
		ip = record.RDATA;
	}
	close(serversd);
	return ip;
}

int main(int argc, char *argv[]) {
	// parse command line parameters
	if (argc < 6 || argc > 7) {
		cerr << "Wrong usage: ./miProxy <log> <alpha> <listen-port> <dns-ip> <dns-port> [<www-ip>]\n";
		return 1;
	}
	string log = argv[1];
	float alpha = stof(argv[2]);
	int listenPort = stoi(argv[3]);
	string dnsIP = argv[4];
	int dnsPort = stoi(argv[5]);
	string wwwIP;
	if (argc == 7) wwwIP = argv[6];
	if (wwwIP == "") {
		wwwIP = getIP(dnsPort, dnsIP);
	}

	myfile.open(log, ofstream::out);
	myfile.close();

	cout << "Log file path: " << log << "\n alpha: " << alpha << "\n listenPort: "
	     << listenPort << "\n wwwIP: " << wwwIP << endl;

	/*client---->server*/
	// select() on clients, listen for browser connections on INADDR_ANY
	char buffer[500000];
	int sd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sd < 0) error("ERROR opening socket");
	struct sockaddr_in self, cli_addr;
	unsigned int clilen;
	clilen = sizeof(cli_addr);
	int opt = 1;

	if (setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, (char *) &opt,
	               sizeof(opt)) < 0) {
		error("setsockopt");
		exit(EXIT_FAILURE);
	}
	bzero((char *) &self, sizeof(self));
	self.sin_family = AF_INET;
	self.sin_addr.s_addr = INADDR_ANY;
	self.sin_port = htons(listenPort);
	if (bind(sd, (struct sockaddr *) &self, sizeof(self)) < 0)
		error("ERROR on binding");

	if (listen(sd, 10) < 0)
		error("ERROR on listening");

	// Set of file descriptors to listen to
	fd_set readSet;
	// Keep track of each file descriptor accepted
	vector<int> fds;
	vector<double> T_curs;
	vector<int> con_count;

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
		int selflen = sizeof(self);
		assert(err != -1);

		if (FD_ISSET(sd, &readSet)) {
			int clientsd = accept(sd, (struct sockaddr *) &self, (socklen_t *) &selflen);
			if (clientsd == -1) {
				cout << "Error on accept" << endl;
			} else {
				fds.push_back(clientsd);
				con_count.push_back(0);
				T_curs.push_back(0);
				int ret = getpeername(clientsd, (struct sockaddr *) &cli_addr, &clilen);
				if (ret < 0)
					error("Error getting peername");
			}
		}

		for (int i = 0; i < (int) fds.size(); ++i) {
			if (FD_ISSET(fds[i], &readSet)) {
				// receive request from client
				int recv_byte_c = 0;
				string host_name = "";
				string path = "";
				int content_length_c = -1;
				int header_length_c;
				bool hasBody = false;
				int filetype = -1; //0 for big_buck_bunny.f4m, 1 for SegX-FragX
				double T_new = 0;
				struct timeval start_time, end_time;
				bzero(buffer, 500000);

				// process HTTP header
				while (recv_byte_c < 8000) {
					int bytesRecvd = read(fds[i], buffer + recv_byte_c, 8000 - recv_byte_c);
					if (bytesRecvd < 0) {
						error("Error recving bytes");
						exit(1);
					} else if (bytesRecvd == 0) {
						close(fds[i]);
						fds.erase(fds.begin() + i);
						con_count.erase(con_count.begin() + i);
						T_curs.erase(T_curs.begin() + i);
						cout << "Connection closed" << endl;
						--i;
						continue;
					}
					recv_byte_c += bytesRecvd;

					// find \r\n\r\n end of header
					int pos_head = isEndHTTPHeader(buffer);
					if (pos_head != -1) {
						// check whether this request has a message body
						header_length_c = pos_head + 4;
						cout << "header_length_c:" << header_length_c << endl;
						content_length_c = getContentLength(buffer);
						cout << "Content-length_c: " << content_length_c << endl;
						if (content_length_c != -1) {
							hasBody = true;
						}
						// to see whether it is a GET request
						// if yes, parse and get file path
						path = parseGETrequest(buffer);
						cout << path << endl;
						if (path != "/") {
							// parse and get hostname
							host_name = getHostName(buffer);
							if (host_name == "") {
								error("Error getting host name");
							}
							filetype = -1;
							if ((path.find("big_buck_bunny.f4m")) != string::npos) {
								filetype = 0;
							}
							if ((path.find("Seg")) != string::npos) {
								filetype = 1;
								gettimeofday(&start_time, NULL);
							}
						}
						break; // reach the end of header, break
					} else {
						error("HTTP request error!");
					}
				}
				if (hasBody) {
					// read the entire body into buffer
					while (recv_byte_c < content_length_c + header_length_c) {
						int bytesRecvd = read(fds[i], buffer + recv_byte_c,
						                      content_length_c + header_length_c - recv_byte_c);
						if (bytesRecvd < 0) {
							error("Error recving bytes");
							exit(1);
						} else if (bytesRecvd == 0) {
							close(fds[i]);
							fds.erase(fds.begin() + i);
							con_count.erase(con_count.begin() + i);
							T_curs.erase(T_curs.begin() + i);
							cout << "Connection closed" << endl;
							--i;
							continue;
						}
						recv_byte_c += bytesRecvd;
					}
				}
				cout << "Receive from client: " << buffer << endl << endl;

				// create connection with server
				int n;
				int portno = 80;
				int recv_byte_s = 0;
				struct sockaddr_in serv_addr;
				int sockfd = socket(AF_INET, SOCK_STREAM, 0);
				if (sockfd < 0)
					error("ERROR opening socket");

				bzero((char *) &serv_addr, sizeof(serv_addr));
				serv_addr.sin_family = AF_INET;
				inet_aton(wwwIP.c_str(), &serv_addr.sin_addr);
				serv_addr.sin_port = htons(portno);

				if (connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
					error("ERROR connecting");
				cout << "Connect to server" << endl;
				cout << "filetype: " << filetype << endl;
				if (filetype == 0) { // client requests .f4m file
					// send .f4m request to server
					n = write(sockfd, buffer, recv_byte_c);
					cout << "Send to server .f4m: " << buffer << endl << endl;
					if (n < 0) error("ERROR writing to socket");
					string temp_str(buffer);

					// wait for server's response and recv
					bzero(buffer, recv_byte_c);
					myRecv(buffer, sockfd);
					cout << "Receive from server .f4m: " << buffer << endl << endl;

					// parse .f4m and store bitrate choice as global var
					available_bitrate = getAvailableBitrate(buffer);

					// send nolist.f4m request to server
					size_t f4m_pos = temp_str.find(".f4m");
					cout << "f4m_pos: " << f4m_pos << endl;

					temp_str = temp_str.substr(0, f4m_pos) + "_nolist" + temp_str.substr(f4m_pos);
					cout << "pass" << endl;
					strcpy(buffer, temp_str.c_str());
					n = write(sockfd, buffer, temp_str.length()); //??? check length ???
					if (n < 0) error("ERROR writing to socket");
					cout << "Send to server nolist.f4m: " << buffer << endl << endl;

					// wait for server's response
					bzero(buffer, temp_str.length() + 1);
					recv_byte_s = myRecv(buffer, sockfd);

					// forward nolist.f4m to client
					n = write(fds[i], buffer, recv_byte_s);
					if (n < 0) error("ERROR writing to socket");
					cout << "Send to client nolist.f4m: " << buffer << endl << endl;
				} else if (filetype == 1) { // client requests video chunk
					string temp_str(buffer);
					size_t vod_pos = temp_str.find("vod");
					size_t seg_pos = temp_str.find("Seg");

					string proper_bitrate = "";
					if (con_count[i] == 0) { // first time connection for chunk request, use the lowest bitrate
						proper_bitrate = available_bitrate[0];
						con_count[i]++;
					} else { // choose optimizing bitrate
						proper_bitrate = LowestAvlb(T_curs[i]);
					}
					string chunkname = proper_bitrate + path.substr(path.find("Seg"));
					temp_str = temp_str.substr(0, vod_pos + 4) + proper_bitrate + temp_str.substr(seg_pos);
					strcpy(buffer, temp_str.c_str());

					n = write(sockfd, buffer, temp_str.length());
					if (n < 0) error("ERROR writing to socket");
					cout << "Send to server chunk request: " << buffer << endl << endl;

					bzero(buffer, temp_str.length());
					recv_byte_s = myRecv(buffer, sockfd);
					cout << "Receive " << recv_byte_s << " bytes from server" << endl;
					cout << "Receive from server chunk request: " << buffer << endl << endl;

					gettimeofday(&end_time, NULL);
					cout << "start_usec: " << start_time.tv_usec << endl;
					cout << "end_usec: " << end_time.tv_usec << endl;
					double duration = (end_time.tv_sec + end_time.tv_usec * 0.000001 - start_time.tv_sec -
					                   start_time.tv_usec * 0.000001); // in second
					T_new = 8.0 * recv_byte_s / duration / 1000.0;
					n = write(fds[i], buffer, recv_byte_s);
					if (n < 0) error("ERROR writing to socket");
					cout << "Send to client chunk request: " << buffer << endl << endl;

					// calculate throughput and update
					T_curs[i] = (1 - alpha) * T_curs[i] + alpha * T_new;

					myfile.open(log.c_str(), ofstream::app);
					string oneLog = logFile(inet_ntoa(cli_addr.sin_addr), duration, T_new, T_curs[i], proper_bitrate, wwwIP, chunkname);
					myfile << oneLog;
					myfile.close();
				} else {
					n = write(sockfd, buffer, recv_byte_c);
					cout << "-1 write to server: " << buffer << endl << endl;
					if (n < 0) error("ERROR writing to socket");
					//cout<<buffer<<endl;
					bzero(buffer, recv_byte_c);
					recv_byte_s = myRecv(buffer, sockfd);
					cout << "-1 recv_byte_s: " << recv_byte_s << endl;
					cout << "-1 receive back from server: " << buffer << endl << endl;
					n = write(fds[i], buffer, recv_byte_s);
					cout << "-1 send to client: " << buffer << endl << endl;
					if (n < 0) error("ERROR writing to socket");
				}
			}
		}
	}
	return 0;
}
