#include <stdio.h>    //printf
#include <string.h>    //strlen
#include <sys/socket.h>    //socket
#include <arpa/inet.h>    //inet_addr
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

char *send_message(char *address, char *port_string, char *message) {
    char *result = malloc(2000);
    
    // default to 127.0.0.1:8888, same as ttt_client
    if (strlen(address) == 0) address = "127.0.0.1";
    if (strlen(port_string) == 0) port_string = "8888";

    unsigned short int port = (unsigned short int) atoi(port_string);

    struct sockaddr_in server;
    server.sin_addr.s_addr = inet_addr(address);
    server.sin_family = AF_INET;
    server.sin_port = htons(port);

    //Create socket
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        return "Could not create socket";
    }

    //Connect to remote server
    if (connect(sock, (struct sockaddr *) &server, sizeof(server)) < 0) {
        sprintf(result, "Error connecting to %s:%i: %s", address, port, strerror(errno));
        return result;
    }

    //Send some data
    if (send(sock, message, 200, 0) < 0) {
        return "Send failed";
    }

    size_t read_size;
    int bytes_read = 0;
    char temp[2000];
    while (bytes_read < 2000) {
        read_size = recv(sock, temp, 2000, 0);
        if (read_size <= 0) {
            puts("Server disconnected");
            fflush(stdout);
            close(sock);
            return 0;
        }
        memcpy(result + bytes_read, temp, read_size);
        bytes_read += read_size;
    }
    return result;
}

char *html = "<html>\n"
             "<body>\n"
             "  <form action=\"ttt.cgi\">\n"
             "    <b>Server Address:</b> <input type=\"text\" name=\"address\" size=\"20\" value=\"%s\"><br/>\n"
             "    <b>Server Port:</b> <input type=\"text\" name=\"port\" size=\"20\" value=\"%s\"><br/>\n"
             "    <b>Username:</b> <input type=\"text\" name=\"username\" size=\"20\" value=\"%s\"><br/>\n"
             "    <b>Password:</b> <input type=\"text\" name=\"password\" size=\"20\" value=\"%s\"><br/>\n"
             "    <b>Gamename:</b> <input type=\"text\" name=\"gamename\" size=\"20\" value=\"%s\"><br/>\n"
             "    <b>Square:</b> <input type=\"text\" name=\"square\" size=\"20\" value=\"%s\"><br/>\n"
             "    <input type=\"submit\" value=\"LOGIN\" name=\"LOGIN\">\n"
             "    <input type=\"submit\" value=\"CREATE\" name=\"CREATE\">\n"
             "    <input type=\"submit\" value=\"JOIN\" name=\"JOIN\">\n"
             "    <input type=\"submit\" value=\"MOVE\" name=\"MOVE\">\n"
             "    <input type=\"submit\" value=\"LIST\" name=\"LIST\">\n"
             "    <input type=\"submit\" value=\"SHOW\" name=\"SHOW\">\n"
             "  </form>\n"
             "  <pre>%s</pre>\n"
             "</body>\n"
             "</html>\n";

int main(int argc, char *argv[]) {
    char *query_string = getenv("QUERY_STRING");

    char *address = "";
    char *port = "";

    char *command = "";
    char *username = "";
    char *password = "";
    char *game_name = "";
    char *square = "";
    for (char *s = strtok(query_string, "&"); s != NULL; s = strtok(NULL, "&")) {
        char *value = strchr(s, '=');
        if (value == NULL) {
            // missing value for some reason, skip it to avoid segmentation fault
            continue;
        }
        size_t field_size = value - s;
        value++;

        if (strncmp(s, "address", field_size) == 0) {
            address = value;
        } else if (strncmp(s, "port", field_size) == 0) {
            port = value;
        } else if (strncmp(s, "username", field_size) == 0) {
            username = value;
        } else if (strncmp(s, "password", field_size) == 0) {
            password = value;
        } else if (strncmp(s, "gamename", field_size) == 0) {
            game_name = value;
        } else if (strncmp(s, "square", field_size) == 0) {
            square = value;
        } else if (strncmp(s, "LOGIN", field_size) == 0 ||
                   strncmp(s, "CREATE", field_size) == 0 ||
                   strncmp(s, "JOIN", field_size) == 0 ||
                   strncmp(s, "MOVE", field_size) == 0 ||
                   strncmp(s, "LIST", field_size) == 0 ||
                   strncmp(s, "SHOW", field_size) == 0) {
            command = value;
        }
    }

    char *command_string = malloc(strlen(command) + 1 + strlen(username) + 1 + strlen(password) + 1 + strlen(game_name) + 1 + strlen(square) + 1);
    if (strcmp(command, "LOGIN") == 0) {
        sprintf(command_string, "%s,%s,%s", command, username, password);
    } else if (strcmp(command, "CREATE") == 0) {
        sprintf(command_string, "%s,%s,%s,%s", command, username, password, game_name);
    } else if (strcmp(command, "JOIN") == 0) {
        sprintf(command_string, "%s,%s,%s,%s,%s", command, username, password, game_name, square);
    } else if (strcmp(command, "MOVE") == 0) {
        sprintf(command_string, "%s,%s,%s,%s,%s", command, username, password, game_name, square);
    } else if (strcmp(command, "LIST") == 0) {
        sprintf(command_string, "%s,%s,%s", command, username, password);
    } else if (strcmp(command, "SHOW") == 0) {
        sprintf(command_string, "%s,%s,%s", command, username, password);
    }

    char *result = send_message(address, port, command_string);

    // Print the result
    // Note: I'm putting the result in <pre>...</pre> to avoid having
    // to replace spaces and newlines in the result
    // (see https://developer.mozilla.org/en-US/docs/Web/HTML/Element/pre)
    printf("Content-Type: text/html; charset=utf-8\n\n");
    printf(html, address, port, username, password, game_name, square, result);
    return 0;
}
