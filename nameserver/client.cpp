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

int main(int argc, char *argv[]) {
	if (argc != 2) {
		std::cout << "Error: Usage is ./client <port_number>\n";
		return 1;
	}

	int portNum = atoi(argv[1]);
	int serversd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (serversd == -1) {
		std::cout << "Error creating server socket\n";
		exit(1);
	}

	// Set up a connection to the server
	struct sockaddr_in server;
	server.sin_family = AF_INET;
	server.sin_port = htons((u_short) portNum);
	struct hostent *sp = gethostbyname("localhost");
	memcpy(&server.sin_addr, sp->h_addr, sp->h_length);
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
	string headerEncode = header2.encode(header2);
	int headerlen = htonl(headerEncode.size());
	send(serversd, (char *)&headerlen, sizeof(int), 0);
	send(serversd, headerEncode.c_str(), headerEncode.size(), 0);
	DNSQuestion question;
	strcpy(question.QNAME, "video.cse.umich.edu");
	question.QCLASS = 1;
	question.QTYPE = 1;
	string questionEncode = question.encode(question);
	int questionlen = htonl(questionEncode.size());
	send(serversd, (char *)&questionlen, sizeof(int), 0);
	send(serversd, questionEncode.c_str(), questionEncode.size(), 0);


//	char buf[] = "video.cse.umich.edu";
//	int buflen = htonl(sizeof(buf));
//	int bytesSent = send(serversd, &buflen, sizeof(buflen), 0);
//	if (bytesSent <= 0) {
//		std::cout << "Error sending stuff to server" << std::endl;
//	}
//	bytesSent = send(serversd, buf, sizeof(buf), 0);
//	if (bytesSent <= 0) {
//		std::cout << "Error sending stuff to server" << std::endl;
//	}

//	int headerlen;
	recv(serversd, &headerlen, sizeof(int), MSG_WAITALL);
	cout << "len " << headerlen << endl;
	headerlen = ntohl(headerlen);
	cout << "header len " << headerlen << endl;
	vector<char> headerStr;
	headerStr.resize(headerlen);
	//recv(serversd, &(headerStr[0]), headerlen, MSG_WAITALL);
	recv(serversd, headerStr.data(), headerlen, MSG_WAITALL);

	//string headerString;
	//headerString.append(headerStr.cbegin(), headerStr.cend());
	string headerString(headerStr.begin(), headerStr.end());

	cout << "string " << headerString <<endl;
	DNSHeader header;
	header = DNSHeader::decode(headerString);
	cout << header.AA << endl;
	cout << header.RD << endl;

	if (header.RCODE != '3') {
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
		cout << "ip " << record.RDATA << endl;
	}

	cout << "AFTER DECODE: " << header.RCODE << endl;

	close(serversd);
	return 0;
}