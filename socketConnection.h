#ifndef INCLUDED_SOCKET_H
#define INCLUDED_SOCKET_H

#include <iostream>
#include <errno.h>
#include <sys/types.h>
#include <Winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "Ws2_32.lib")
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#define SO_RCVBUF_SIZE 10485760
#define RECVBUFLEN 1500

using namespace std;

#define perror(msg) printf(msg" :%d\n", WSAGetLastError())

class socketUtility {
    WSADATA wsaData;
    public:
        //////////////////////////////////
        SOCKET media_Sockfd;
        SOCKET fecRow_Sockfd;
        SOCKET fecCol_Sockfd;
        SOCKET output_Sockfd;
        int fdmax;
        unsigned char *sockRecvBuf;
        //////////////////////////////////

        socketUtility(const char* mediaIP , const char* mediaPort, const char* outputIP , const char* outputPort) {
            u_short m_port = atoi(mediaPort);
            u_short fecRowPort = m_port + 4;
            u_short fecColPort = m_port + 2;
            if (0 != WSAStartup(MAKEWORD(2, 1), &wsaData))
            {
                printf("Winsock init failed!\r\n");
                WSACleanup();
                exit(0);
            }
            media_Sockfd = listenSocket(mediaIP , m_port , SO_RCVBUF_SIZE);
            fecRow_Sockfd = listenSocket(mediaIP , fecRowPort , SO_RCVBUF_SIZE);
            fecCol_Sockfd = listenSocket(mediaIP , fecColPort , SO_RCVBUF_SIZE);
            output_Sockfd = sendSocket(outputIP , outputPort);

            fdmax = maximumOfThreeNum(media_Sockfd , fecRow_Sockfd , fecCol_Sockfd);

            sockRecvBuf = (unsigned char*)malloc(RECVBUFLEN * sizeof(unsigned char));


        }

        ~socketUtility() {
            if(media_Sockfd >= 0)
                closesocket(media_Sockfd);
            if(fecRow_Sockfd >= 0)
                closesocket(fecRow_Sockfd);
            if(fecCol_Sockfd >= 0)
                closesocket(fecCol_Sockfd);
            if(output_Sockfd >= 0)
                closesocket(output_Sockfd);
            if(sockRecvBuf)
                free(sockRecvBuf);
            WSACleanup();
            exit(1);
        }

        bool isMulticastAddress(const char* mediaIP) {
            string IPstr(mediaIP);
            string firstByteStr = IPstr.substr(0 , 3);
            if(stoi(firstByteStr) >= 224 && stoi(firstByteStr) <= 239) {
                return true;
            }
            else {
                return false;
            }
        }

        SOCKET listenSocket(const char* mediaIP , u_short mediaPort , int recvBufSize) {
            SOCKET sockfd;

            sockaddr_in localAddr = { 0 };
            // memset(&localAddr, 0, sizeof(localAddr));
            
            localAddr.sin_family = AF_INET;
            localAddr.sin_addr.S_un.S_addr = INADDR_ANY;
            localAddr.sin_port = htons(mediaPort);
            

            if((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
                perror("socket() SOCK_DGRAM failed");
                return -1;
            }

            int yes = 1;
            if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (char*)&yes, sizeof(int)) == -1) {
                perror("setsockopt SO_REUSEADDR failed");
                return -1;
            }

            if(bind(sockfd , (sockaddr*)&localAddr, sizeof(localAddr)) != 0) {
                perror("bind() localAddr failed");
                return -1;
            }

            int outputValue = 0;
            int outputValue_len = sizeof(outputValue);
            int defaultBuf;

            if(getsockopt(sockfd , SOL_SOCKET , SO_RCVBUF , (char*)&outputValue , &outputValue_len) != 0) {
                perror("getsockopt SO_RCVBUF failed");
                return -1;
            }
            defaultBuf = outputValue;

            outputValue = recvBufSize;
            if(setsockopt(sockfd , SOL_SOCKET , SO_RCVBUF , (char*)&outputValue , sizeof(outputValue)) != 0) {
                perror("setsockopt SO_RCVBUF failed");
                return -1;
            }
            if(getsockopt(sockfd , SOL_SOCKET , SO_RCVBUF , (char*)&outputValue , &outputValue_len) != 0) {
                perror("getsockopt SO_RCVBUF failed 2");
                return -1;
            }

            //printf("tried to set socket receive buffer from %d to %d, got %d\n", defaultBuf , recvBufSize , outputValue);

            if(isMulticastAddress(mediaIP)) {
                struct ip_mreq multicastRequest = { 0 };
                //memcpy(&multicastRequest.imr_multiaddr , &mediaAddr.sin_addr , sizeof(multicastRequest.imr_multiaddr));
                multicastRequest.imr_multiaddr.S_un.S_addr = inet_addr(mediaIP);
                multicastRequest.imr_interface.s_addr = htonl(INADDR_ANY);

                if(setsockopt(sockfd, IPPROTO_IP , IP_ADD_MEMBERSHIP , (char*)&multicastRequest , sizeof(multicastRequest)) != 0) {
                    perror("setsockopt IP_ADD_MEMBERSHIP failed");
                    return -1;
                }
            }

            return sockfd;
        }

        int multiCastConnectSocket(const char* multicastIP , const char* multicastPort) {
            int sockfd;

            int addrInfoStatus;
            if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
                perror("socket() SOCK_DGRAM failed (multiCastConnectSocket)");
                return -1;
            }

            // allow multiple sockets to use the same PORT number
            u_int reuse_port = 1;
            if (setsockopt(sockfd, IPPROTO_IP, IP_MULTICAST_LOOP, (char*)&reuse_port, sizeof(reuse_port)) < 0) {
                perror("setsockopt() failed");
                return -1;
            }

            return sockfd;
        }

        int uCastConnectSocket(const char* unicastIP , const char* unicastPort) {
            int sockfd;
            int yes = 1;


            int addrInfoStatus;

            if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
                perror("socket() failed");
                return -1;
            }

            sockaddr_in unicastAddr;
            memset(&unicastAddr, 0, sizeof(unicastAddr));
            unicastAddr.sin_family = AF_INET;
            unicastAddr.sin_addr.S_un.S_addr = inet_addr(unicastIP);
            unicastAddr.sin_port = atoi(unicastPort);
            if (connect(sockfd, (sockaddr*)&unicastAddr, sizeof(unicastAddr)) == -1) {
                perror("connect() unicastAddr failed");
                return -1;
            }

            if(setsockopt(sockfd , SOL_SOCKET , SO_REUSEADDR , (char*)&yes , sizeof(int)) == -1) {
                perror("setsockopt() SO_REUSEADDR failed");
                return -1;
            }

            return sockfd;
        }

        int sendSocket(const char* recvIP , const char* recvPort) {
            if(isMulticastAddress(recvIP))
            	return multiCastConnectSocket(recvIP, recvPort);

            return uCastConnectSocket(recvIP, recvPort);
        }

        int maximumOfThreeNum( int a , int b , int c ) {
           int max = ( a < b ) ? b : a;
           return ( ( max < c ) ? c : max );
        }
};

#endif
