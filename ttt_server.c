#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>

struct USER {
    char username[100];
    char password[100];
    struct USER *next;
};

enum GAME_STATE {
    CREATOR_WON = -2,
    IN_PROGRESS_CREATOR_NEXT = -1,
    DRAW = 0,
    IN_PROGRESS_CHALLENGER_NEXT = 1,
    CHALLENGER_WON = 2
};

struct GAME {
    char gamename[100];
    struct USER *creator;
    struct USER *challenger;
    enum GAME_STATE state;
    char ttt[3][3];
    struct GAME *next;
};

struct USER *user_list_head = NULL;
struct GAME *game_list_head = NULL;

struct GAME *get_game(char *name) {
    for (struct GAME *game = game_list_head; game != NULL; game = game->next) {
        if (strcmp(game->gamename, name) == 0) {
            return game;
        }
    }

    return NULL;
}

void print_game_state(char *response, struct GAME *game) {
    if (game->state == CREATOR_WON) {
        sprintf(response + strlen(response), "GAME OVER: %s WON", game->creator->username);
    } else if (game->state == CHALLENGER_WON) {
        sprintf(response + strlen(response), "GAME OVER: %s WON", game->challenger->username);
    } else if (game->state == IN_PROGRESS_CREATOR_NEXT) {
        sprintf(response + strlen(response), "IN PROGRESS: %s TO MOVE NEXT AS o", game->creator->username);
    } else if (game->state == IN_PROGRESS_CHALLENGER_NEXT) {
        sprintf(response + strlen(response), "IN PROGRESS: %s TO MOVE NEXT AS x", game->challenger->username);
    } else if (game->state == DRAW) {
        sprintf(response + strlen(response), "GAME OVER: DRAW");
    }
}

void print_ttt(char *response, char ttt[3][3]) {
    sprintf(response, "%sa  %c | %c | %c \r\n", response, ttt[0][0], ttt[0][1], ttt[0][2]);
    sprintf(response, "%s  ---|---|---\r\n", response);
    sprintf(response, "%sb  %c | %c | %c \r\n", response, ttt[1][0], ttt[1][1], ttt[1][2]);
    sprintf(response, "%s  ---|---|---\r\n", response);
    sprintf(response, "%sc  %c | %c | %c \r\n", response, ttt[2][0], ttt[2][1], ttt[2][2]);
    sprintf(response, "%s\r\n", response);
    sprintf(response, "%s   %c   %c   %c\r\n", response, '1', '2', '3');
}

void print_game(char *response, struct GAME *game) {
    sprintf(response, "GAME %s BETWEEN %s AND %s.\n", game->gamename, game->creator->username, game->challenger->username);
    print_game_state(response, game);
    sprintf(response + strlen(response), "\n");
    print_ttt(response, game->ttt);
}

char check_line(struct GAME *game, char a, char b, char c) {
    if (a == 'x' && b == 'x' && c == 'x') {
        game->state = CREATOR_WON;
        return 1;
    } else if (a == 'o' && b == 'o' && c == 'o') {
        game->state = CHALLENGER_WON;
        return 1;
    }
    return 0;
}

void check_win(struct GAME *game) {
    char won =
            // Check rows
            check_line(game, game->ttt[0][0], game->ttt[0][1], game->ttt[0][2]) ||
            check_line(game, game->ttt[1][0], game->ttt[1][1], game->ttt[1][2]) ||
            check_line(game, game->ttt[2][0], game->ttt[2][1], game->ttt[2][2]) ||

            // Check columns
            check_line(game, game->ttt[0][0], game->ttt[1][0], game->ttt[2][0]) ||
            check_line(game, game->ttt[0][1], game->ttt[1][1], game->ttt[2][1]) ||
            check_line(game, game->ttt[0][2], game->ttt[1][2], game->ttt[2][2]) ||

            // Check diagonals
            check_line(game, game->ttt[0][0], game->ttt[1][1], game->ttt[2][2]) ||
            check_line(game, game->ttt[0][2], game->ttt[1][1], game->ttt[2][0]);

    // Check for a draw
    if (!won) {
        for (int row = 0; row < 3; row++) {
            for (int column = 0; column < 3; column++) {
                if (game->ttt[row][column] == ' ') {
                    return; // not a draw
                }
            }
        }

        // No win and all cells full, so it's a draw
        game->state = DRAW;
    }
}

int main(int argc, char *argv[]) {
    int socket_desc, client_sock, read_size;
    struct sockaddr_in server, client;
    char client_message[2000];

    unsigned short int port = 8888;

    if (argc > 1)
        port = (unsigned short int) atoi(argv[1]);

    //Create socket
    socket_desc = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_desc == -1) {
        printf("Could not create socket");
    }

    //Prepare the sockaddr_in structure
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(port);

    if (bind(socket_desc, (struct sockaddr *) &server, sizeof(server)) < 0) {
        perror("bind failed. Error");
        return 1;
    }

    listen(socket_desc, 3);

    printf("Game server ready on port %d.\n", port);

    while (1) {
        // Accept connection from incoming client
        int c = sizeof(struct sockaddr_in);
        client_sock = accept(socket_desc, (struct sockaddr *) &client, (socklen_t * ) & c);
        if (client_sock < 0) {
            perror("accept failed");
            return 1;
        }

        // Read client message
        char temp[200];
        memset(client_message, '\0', 200);
        int bytes_read = 0;
        while (bytes_read < 200) {
            read_size = recv(client_sock, temp, 200, 0);
            if (read_size <= 0) {
                puts("Client disconnected");
                fflush(stdout);
                close(client_sock);
                close(socket_desc);
                return 0;
            }
            memcpy(client_message + bytes_read, temp, read_size);
            bytes_read += read_size;
        }

        // Get command, username, and password
        char response[2000];
        response[0] = '\0';
        char *command = strtok(client_message, ",");
        char *username = strtok(NULL, ",");
        char *password = strtok(NULL, ",");

        if (command == NULL || username == NULL || password == NULL) {
            sprintf(response, "MUST ENTER A VALID COMMAND WITH ARGUMENTS FROM THE LIST:\n");
            sprintf(response + strlen(response), "LOGIN,USER,PASS\n");
            sprintf(response + strlen(response), "CREATE,USER,PASS,GAMENAME\n");
            sprintf(response + strlen(response), "JOIN,USER,PASS,GAMENAME,SQUARE\n");
            sprintf(response + strlen(response), "MOVE,USER,PASS,GAMENAME,SQUARE\n");
            sprintf(response + strlen(response), "LIST,USER,PASS\n");
            sprintf(response + strlen(response), "SHOW,USER,PASS,GAMENAME\n");
            write(client_sock, response, 2000);
            close(client_sock);
            continue;
        }

        struct USER *user = NULL;
        char password_correct = 0;
        for (struct USER *user2 = user_list_head; user2 != NULL; user2 = user2->next) {
            if (strcmp(user2->username, username) == 0) {
                if (strcmp(user2->password, password) == 0) {
                    password_correct = 1;
                }
                user = user2;
                break;
            }
        }

        if (user != NULL && !password_correct) {
            sprintf(response, "BAD PASSWORD");

            write(client_sock, response, 2000);
            close(client_sock);
            continue;
        }

        if (strcmp(command, "LOGIN") == 0) {
            if (user != NULL) {
                strcpy(response, "EXISTING USER LOGIN OK");
            } else {
                struct USER *new_user = malloc(sizeof(struct USER));
                strcpy(new_user->username, username);
                strcpy(new_user->password, password);
                new_user->next = user_list_head;
                user_list_head = new_user;

                strcpy(response, "NEW USER CREATED OK");
            }

            write(client_sock, response, 2000);
            close(client_sock);
            continue;
        }

        if (user == NULL) {
            strcpy(response, "USER NOT FOUND");
            write(client_sock, response, 2000);
            close(client_sock);
            continue;
        }

        if (strcmp(command, "CREATE") == 0) {
            char *game_name = strtok(NULL, ",");

            // Make sure command is valid
            if (game_name == NULL) {
                sprintf(response, "CREATE COMMAND MUST BE CALLED AS FOLLOWS:\n");
                sprintf(response + strlen(response), "CREATE,USER,PASS,GAMENAME\n");
                write(client_sock, response, 2000);
                close(client_sock);
                continue;
            }

            // Make sure game name doesn't already exist
            char game_exists = 0;
            for (struct GAME *existing_game = game_list_head; existing_game != NULL; existing_game = existing_game->next) {
                if (strcmp(existing_game->gamename, game_name) == 0) {
                    game_exists = 1;
                    break;
                }
            }
            if (game_exists) {
                sprintf(response, "GAME NAME %s ALREADY EXISTED\n", game_name);
                write(client_sock, response, 2000);
                close(client_sock);
                continue;
            }

            // Create new game and add it to the list
            struct GAME *new_game = (struct GAME *) malloc(sizeof(struct GAME));
            strcpy(new_game->gamename, game_name);
            for (int row = 0; row < 3; row++) {
                for (int col = 0; col < 3; col++) {
                    new_game->ttt[row][col] = ' ';
                }
            }

            new_game->state = IN_PROGRESS_CHALLENGER_NEXT;
            new_game->creator = user;

            new_game->next = game_list_head;
            game_list_head = new_game;

            // Write response
            sprintf(response, "GAME %s CREATED. WAITING FOR OPPONENT TO JOIN.\n", game_name);
            print_ttt(response, new_game->ttt);
        } else if (strcmp(command, "JOIN") == 0) {
            char *game_name = strtok(NULL, ",");
            char *square = strtok(NULL, ",");

            // Make sure the command is valid
            if (game_name == NULL || square == NULL) {
                sprintf(response, "JOIN COMMAND MUST BE CALLED AS FOLLOWS:\n");
                sprintf(response + strlen(response), "JOIN,USER,PASS,GAMENAME,SQUARE\n");
                write(client_sock, response, 2000);
                close(client_sock);
                continue;
            }

            // Get row and column
            // Note: Not checking strlen(square) >= 2 is safe since strtok never returns
            // empty string, so if strlen(square) == 1, square[1] = '\0' and won't cause
            // a segmentation fault or be a valid row.
            char row = square[0] - 'a';
            char column = square[1] - '1';
            if (column < 0 || column > 2) {
                sprintf(response, "INVALID MOVE %s. COL MUST BE 1-3", square);
                write(client_sock, response, 2000);
                close(client_sock);
                continue;
            }
            if (row < 0 || row > 2) {
                sprintf(response, "INVALID MOVE %s. ROW MUST BE a-z", square);
                write(client_sock, response, 2000);
                close(client_sock);
                continue;
            }

            // Get the game struct
            struct GAME *game = get_game(game_name);
            if (game == NULL) {
                sprintf(response, "GAME %s DOES NOT EXIST", game_name);
                write(client_sock, response, 2000);
                close(client_sock);
                continue;
            }

            // Make sure game doesn't have a challenger
            if (game->challenger != NULL) {
                sprintf(response, "GAME %s ALREADY HAS A CHALLENGER", game_name);
                write(client_sock, response, 2000);
                close(client_sock);
                continue;
            }

            game->challenger = user;
            game->ttt[row][column] = 'x';
            game->state = IN_PROGRESS_CREATOR_NEXT;
            print_game(response, game);
        } else if (strcmp(command, "MOVE") == 0) {
            char *game_name = strtok(NULL, ",");
            char *square = strtok(NULL, ",");

            // Make sure the command is valid
            if (game_name == NULL || square == NULL) {
                sprintf(response, "MOVE COMMAND MUST BE CALLED AS FOLLOWS:\n");
                sprintf(response + strlen(response), "MOVE,USER,PASS,GAMENAME,SQUARE\n");
                write(client_sock, response, 2000);
                close(client_sock);
                continue;
            }

            // Get row and column
            char row = square[0] - 'a';
            char column = square[1] - '1';
            if (column < 0 || column > 2) {
                sprintf(response, "INVALID MOVE %s. COL MUST BE 1-3", square);
                write(client_sock, response, 2000);
                close(client_sock);
                continue;
            }
            if (row < 0 || row > 2) {
                sprintf(response, "INVALID MOVE %s. ROW MUST BE a-z", square);
                write(client_sock, response, 2000);
                close(client_sock);
                continue;
            }

            // Get the game struct
            struct GAME *game = get_game(game_name);
            if (game == NULL) {
                sprintf(response, "GAME %s DOES NOT EXIST", game_name);
                write(client_sock, response, 2000);
                close(client_sock);
                continue;
            }

            // Make sure square is empty
            if (game->ttt[row][column] != ' ') {
                sprintf(response, "INVALID MOVE, SQUARE NOT EMPTY");
                write(client_sock, response, 2000);
                close(client_sock);
                continue;
            }

            // Try to make a move
            if (game->state == IN_PROGRESS_CREATOR_NEXT) {
                if (game->creator != user) {
                    sprintf(response, "INVALID USER. ONLY %s CAN MAKE THE NEXT MOVE AS o IN GAME %s", game->creator->username, game_name);
                    write(client_sock, response, 2000);
                    close(client_sock);
                    continue;
                }

                game->ttt[row][column] = 'o';
                game->state = IN_PROGRESS_CHALLENGER_NEXT;
                check_win(game);
                print_game(response, game);
            } else if (game->state == IN_PROGRESS_CHALLENGER_NEXT) {
                if (game->challenger != user) {
                    sprintf(response, "INVALID USER. ONLY %s CAN MAKE THE NEXT MOVE AS x IN GAME %s", game->challenger->username, game_name);
                    write(client_sock, response, 2000);
                    close(client_sock);
                    continue;
                }

                game->ttt[row][column] = 'x';
                game->state = IN_PROGRESS_CREATOR_NEXT;
                check_win(game);
                print_game(response, game);
            } else {
                sprintf(response, "CANNOT MAKE A MOVE IN COMPLETED GAME");
                write(client_sock, response, 2000);
                close(client_sock);
                continue;
            }
        } else if (strcmp(command, "LIST") == 0) {
            sprintf(response, "LIST OF GAMES:\n");
            for (struct GAME *game = game_list_head; game != NULL; game = game->next) {
                sprintf(response + strlen(response), "GAME %s: CREATED BY %s, CHALLENGED BY: %s. ", game->gamename, game->creator->username, game->challenger->username);
                print_game_state(response, game);
            }
        } else if (strcmp(command, "SHOW") == 0) {
            char *game_name = strtok(NULL, ",");
            char *square = strtok(NULL, ",");

            // Make sure the command is valid
            if (game_name == NULL) {
                sprintf(response, "SHOW COMMAND MUST BE CALLED AS FOLLOWS:\n");
                sprintf(response + strlen(response), "SHOW,USER,PASS,GAMENAME\n");
                write(client_sock, response, 2000);
                close(client_sock);
                continue;
            }

            // Get the game struct
            struct GAME *game = get_game(game_name);
            if (game == NULL) {
                sprintf(response, "GAME %s DOES NOT EXIST", game_name);
                write(client_sock, response, 2000);
                close(client_sock);
                continue;
            }

            // Print the game
            print_game(response, game);
        } else {
            sprintf(response, "COMMAND %s NOT IMPLEMENTED", command);
        }

        write(client_sock, response, 2000);
        close(client_sock);
    }

    close(socket_desc);
    return 0;
}
