#include <winsock2.h>
#include <Windows.h>
#include <Winreg.h>
#include <ws2tcpip.h>
#include <stdio.h>
#include <string.h>

#ifndef UNICODE
#define UNICODE
#endif
#define WIN32_LEAN_AND_MEAN

#pragma comment(lib, "ws2_32.lib")

#define DEFAULT_BUFLEN 1024
#define DEFAULT_PORT 37773

int main() {
    int MAXRECV = DEFAULT_BUFLEN;
    WSADATA wsa;
    SOCKET master, new_socket, client_socket[5], s;
    struct sockaddr_in server, address;
    int max_clients = 5, activity, addrlen, i, message_recieved;
    char * message = "The machine is rebooting...";

    // Set of socket descriptors
    fd_set readfds;

    char * buffer;
    // 1 extra for null character, string termination
    buffer = (char * ) malloc((DEFAULT_BUFLEN + 1) * sizeof(char));

    for (i = 0; i < 5; i++) {
        client_socket[i] = 0;
    }

    if (WSAStartup(MAKEWORD(2, 2), & wsa) != 0) {
        exit(EXIT_FAILURE);
    }

    // Create a socket
    if ((master = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET) {
        exit(EXIT_FAILURE);
    }

    // Prepare the sockaddr_in structure
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(DEFAULT_PORT);

    // Bind
    if (bind(master, (struct sockaddr * ) & server, sizeof(server)) == SOCKET_ERROR) {
        exit(EXIT_FAILURE);
    }

    // Listen to incoming connections
    listen(master, 3);

    //Accept and incoming connection
    addrlen = sizeof(struct sockaddr_in);

    while (TRUE) {
        // Clear the socket fd set
        FD_ZERO( & readfds);

        // Add master socket to fd set
        FD_SET(master, & readfds);

        // Add child sockets to fd set
        for (i = 0; i < max_clients; i++) {
            s = client_socket[i];
            if (s > 0) {
                FD_SET(s, & readfds);
            }
        }

        // Wait for an activity on any of the sockets.
        // Timeout is NULL so wait indefinitely
        activity = select(0, & readfds, NULL, NULL, NULL);
        if (activity == SOCKET_ERROR) {
            exit(EXIT_FAILURE);
        }

        // If something happened on the master socket then its an incoming connection
        if (FD_ISSET(master, & readfds)) {
            if ((new_socket = accept(master, (struct sockaddr * ) & address, (int * ) & addrlen)) < 0) {
                exit(EXIT_FAILURE);
            }

            // Inform about a socket number
            printf("New connection , socket fd is %d , ip is : %s , port : %d \n", new_socket, inet_ntoa(address.sin_addr), ntohs(address.sin_port));

            // Send new connection greeting message
            char greeting[] = "Hello, remote rebooter\n";
            send(new_socket, greeting, strlen(greeting), 0);

            // Add new socket to array of sockets
            for (i = 0; i < max_clients; i++) {
                if (client_socket[i] == 0) {
                    client_socket[i] = new_socket;
                    break;
                }
            }
        }

        // Else its some IO operation on some other socket
        for (i = 0; i < max_clients; i++) {
            s = client_socket[i];

            // If client presend in read sockets             
            if (FD_ISSET(s, & readfds)) {
                // Get details of the client
                getpeername(s, (struct sockaddr * ) & address, (int * ) & addrlen);

                // Check if it was for closing , and also read the incoming message
                // Recv does not place a null terminator at the end of the string (whilst printf %s assumes there is one).
                message_recieved = recv(s, buffer, MAXRECV, 0);

                if (message_recieved == SOCKET_ERROR) {
                    int error_code = WSAGetLastError();
                    if (error_code == WSAECONNRESET) {
                        // If somebody disconnected print their details
                        printf("Host disconnected unexpectedly , ip %s , port %d \n", inet_ntoa(address.sin_addr), ntohs(address.sin_port));

                        // Close the socket and mark as 0 in list for reuse
                        closesocket(s);
                        client_socket[i] = 0;
                    } else {
                        printf("recv failed with error code : %d", error_code);
                    }
                }
                if (message_recieved == 0) {
                    // Somebody disconnected , get his details and print
                    printf("Host disconnected , ip %s , port %d \n", inet_ntoa(address.sin_addr), ntohs(address.sin_port));
                    // Close the socket and mark as 0 in list for reuse
                    closesocket(s);
                    client_socket[i] = 0;
                }
                // Reboot if "reboot" getting or echo back the message that came in
                else {
                    // Add null character for using in printf/puts or other string handling functions
                    buffer[message_recieved] = '\0';
                    printf("%s:%d - %s \n", inet_ntoa(address.sin_addr), ntohs(address.sin_port), buffer);
                    char str1[6] = "reboot";
                    printf("comparing \"%s\" and \"%s\" is %d \n", "reboot", buffer, strcmp(str1, buffer));
                    if ((strcmp(str1, buffer)) > 0) {
                        // System reboot now
                        InitiateSystemShutdownA("127.0.0.1", "Remote reboot", 0, 1, 1);
                        send(s, buffer, "REBOOT", 0);
                    } else {
                        send(s, buffer, message_recieved, 0);
                    }
                }
            }
        }
    }
    closesocket(s);
    WSACleanup();
    return 0;
}