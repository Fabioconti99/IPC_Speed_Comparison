/* A simple server in the internet domain using TCP
   The port number is passed as an argument
   This version runs forever, forking off a separate
   process for each connection
*/
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdlib.h>
#include <strings.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <signal.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>

// Definition of the size of the Producer and Consumer memory array.
#define SIZE 200000

// Definition of colors for prints.
#define RESET "\033[0m"
#define BHWHT "\e[1;97m"

// Function: CHECK(X)
// This function writes on the shell whenever a system call returns any kind of error.
// The function will print the name of the file and the line at which it found the error.
// It will end the check exiting with code 1.
#define CHECK(X) (                                                 \
    {                                                              \
        int __val = (X);                                           \
        (__val == -1 ? (                                           \
                           {                                       \
                               fprintf(stderr, "ERROR ("__FILE__   \
                                               ":%d) -- %s\n",     \
                                       __LINE__, strerror(errno)); \
                               exit(EXIT_FAILURE);                 \
                               -1;                                 \
                           })                                      \
                     : __val);                                     \
    })

// Function: dostaff(__,__,__)
// There is a separate instance of this function for each connection.  It handles all
// communication once a connnection has been established.

void dostuff(int, int, const char *, const char *); /* function prototype */

// Function: error(__)
// This function is called when a system call fails. It displays a message about the error on stderr and then
// aborts the program.
void error(char *);

// Function: fillBuffer(__,__)
// this function fills a buffer vith a fixed size of integers
void fillBuffer(int[], int);

// Variables for saving the time instants in both Producer anc Confoumer
// in order to obtain the value of the time spent for transfering data.
double time_in_double;
struct timespec Time;

// Initialization of the memory buffer needed for sharing memory between
// processes.
int buffer[SIZE];

FILE *logfile_a;
time_t actualtime;

int main(int argc, char *argv[])
{
    logfile_a = fopen("./logfile.txt", "a");

    time(&actualtime);
    fprintf(logfile_a, "[%d] Executable: %s- Content: father process spawned(consumer) - Time: %s\n", getpid(), argv[0], ctime(&actualtime));
    fflush(logfile_a);
    // portno stores the port number on which the server accepts connections.
    int portno = atoi(argv[2]);

    // Size of the amount of memory to transfer choosen by the user.
    int bufSize = atoi(argv[3]);

    // sockfd is a file descriptor, i.e. array subscripts into the file descriptor table . This
    // variable stores the value returned by the socket system call.
    int sockfd;

    /*A sockaddr_in is a structure containing an internet address. This structure is defined in <netinet/in.h>.
    Here is the definition:
    struct sockaddr_in {
            short   sin_family;
            u_short sin_port;
            struct  in_addr sin_addr;
            char    sin_zero[8];
    };
    An in_addr structure, defined in the same header file, contains only one field, a unsigned long called
    s_addr. The variable serv_addr will contain the address of the server.*/
    struct sockaddr_in serv_addr;

    // The user needs to pass in the port number on which the server will accept connections as an argument. This
    // code displays an error message if the user fails to do this.
    if (argc < 6)
    {
        fprintf(stderr, "usage %s hostname port bufferSize fifoname_time_prod fifoname_time_cons\n", argv[0]);
        exit(0);
    }

    // Forking the procss to distnguish a Producer( Child process (server)) and a
    // Consumer (Parent process (Client))
    pid_t pid = fork();
    CHECK(pid);
    // Child process (server)
    if (pid == 0)
    {
        time(&actualtime);
        fprintf(logfile_a, "[%d] Executable: %s- Content: child process forked (producer) - Time: %s\n", getpid(), argv[0], ctime(&actualtime));
        fflush(logfile_a);
        // sockfd is a file descriptor, i.e. array subscripts into the file descriptor table . This
        // variable stores the value returned by the socket system call.
        int newsockfd;

        // clilen stores the size of the address of the client. This is needed for the accept system call.
        socklen_t clilen;

        /*A sockaddr_in is a structure containing an internet address. This structure is defined in <netinet/in.h>.
        Here is the definition:
        struct sockaddr_in {
                short   sin_family;
                u_short sin_port;
                struct  in_addr sin_addr;
                char    sin_zero[8];
        };
        An in_addr structure, defined in the same header file, contains only one field, a unsigned long called
        s_addr. The variable serv_addr will contain the address of the server, and cli_addr will contain the
        address of the client which connects to the server.*/
        struct sockaddr_in cli_addr;

        // The socket() system call creates a new socket. It takes three arguments. The first is the address domain of
        // the socket.The second argument is the type of socket. The third argument is the protocol.
        sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd < 0)
            error("ERROR opening socket");

        // Setting the some option to make the user capable of using the same
        // port number multiple times in a row.
        int option = 1;
        CHECK(setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option)));

        // The function bzero() sets all values in a buffer to zero. It takes two arguments, the first is a pointer to the
        // buffer and the second is the size of the buffer. Thus, this line initializes serv_addr to zeros.
        bzero((char *)&serv_addr, sizeof(serv_addr));

        // The variable serv_addr is a structure of type struct sockaddr_in. This structure has four fields. The first
        // field is short sin_family, which contains a code for the address family. It should always be set to the
        // symbolic constant AF_INET.
        serv_addr.sin_family = AF_INET;

        // The third field of sockaddr_in is a structure of type struct in_addr which contains only a single field
        // unsigned long s_addr. This field contains the IP address of the host. For server code, this will always be
        // the IP address of the machine on which the server is running, and there is a symbolic constant INADDR_ANY
        // which gets this address.
        serv_addr.sin_addr.s_addr = INADDR_ANY;

        // The second field of serv_addr is unsigned short sin_port , which contain the port number. However,
        // instead of simply copying the port number to this field, it is necessary to convert this to network byte order
        // using the function htons() which converts a port number in host byte order to a port number in network byte
        // order.
        serv_addr.sin_port = htons(portno);

        /*The bind() system call binds a socket to an address, in this case the address of the current host and port
        number on which the server will run. It takes three arguments, the socket file descriptor, the address to which
        is bound, and the size of the address to which it is bound. The second argument is a pointer to a structure of
        type sockaddr, but what is passed in is a structure of type sockaddr_in, and so this must be cast to the
        correct type. This can fail for a number of reasons, the most obvious being that this socket is already in use
        on this machine.*/
        if (bind(sockfd, (struct sockaddr *)&serv_addr,
                 sizeof(serv_addr)) < 0)
            error("ERROR on binding");

        /*The listen system call allows the process to listen on the socket for connections. The first argument is the
        socket file descriptor, and the second is the size of the backlog queue, i.e., the number of connections that
        can be waiting while the process is handling a particular connection. This should be set to 5, the maximum
        size permitted by most systems. If the first argument is a valid socket, this call cannot fail, and so the code
        doesn't check for errors. */
        CHECK(listen(sockfd, 5));

        /*The accept() system call causes the process to block until a client connects to the server. Thus, it wakes up
        the process when a connection from a client has been successfully established. It returns a new file
        descriptor, and all communication on this connection should be done using the new file descriptor. The
        second argument is a reference pointer to the address of the client on the other end of the connection, and the
        third argument is the size of this structure.*/
        CHECK(clilen = sizeof(cli_addr));
        newsockfd = accept(sockfd,
                           (struct sockaddr *)&cli_addr, &clilen);
        if (newsockfd < 0)
            error("ERROR on accept");

        // Retrieving the name of the fifo the producer has to right its time to.
        const char *fifo_name_time = argv[4];

        // Calling the function to fill an array of a certain amout of data and sending it to the consumer.
        dostuff(newsockfd, bufSize, fifo_name_time, argv[0]);

        // Closing the accept() and socket() FDs
        CHECK(close(newsockfd));
        CHECK(close(sockfd));
    }
    else
    {
        printf(BHWHT "Father PID (consumer): [%d]\n" RESET, getpid());
        printf(BHWHT "Child PID (producer): [%d]\n" RESET, pid);
        struct sockaddr_in serv_addr;

        // The variable server is a pointer to a structure of type hostent
        struct hostent *server;

        // All of this code is the same as that in the server.
        sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd < 0)
            error("ERROR opening socket");

        // The function gethostbyname() takes the argv(1) as an argument (name of the local-host) and
        // returns a pointer to a hostent containing information about that host.
        server = gethostbyname(argv[1]);

        // This code sets the fields in serv_addr. Much of it is the same as in the server.
        bzero((char *)&serv_addr, sizeof(serv_addr));
        serv_addr.sin_family = AF_INET;
        bcopy((char *)server->h_addr,
              (char *)&serv_addr.sin_addr.s_addr,
              server->h_length);
        serv_addr.sin_port = htons(portno);

        usleep(2000);

        // The connect function is called by the client to establish a connection to the server. It takes three arguments,
        // the socket file descriptor, the address of the host to which it wants to connect (including the port number),
        // and the size of this address. This function returns 0 on success and -1 if it fails.
        if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
            error("ERROR connecting");

        // Reading the data Arriving form the producer and filling the local buffer.
        bzero(buffer, SIZE);
        int count_percent = 1;
        time(&actualtime);
        fprintf(logfile_a, "[%d] Executable: %s- Content: starts reading(consumer) - Time: %s\n", getpid(), argv[0], ctime(&actualtime));
        fflush(logfile_a);
        for (int i = 0; i < bufSize; i++)
        {
            CHECK(read(sockfd, &buffer[(i) % SIZE], sizeof(int)));
            if (i % (bufSize / 100) == 0)
            {
                printf("\r" BHWHT "%d out of 100" RESET, count_percent++);
                fflush(stdout);
            }
        }
        // Getting the time value at the end of the transfering process and sending it
        // to the master process.
        CHECK(clock_gettime(CLOCK_REALTIME, &Time));
        time(&actualtime);
        fprintf(logfile_a, "[%d] Executable: %s- Content: finishes reading(consumer) - Time: %s\n", getpid(), argv[0], ctime(&actualtime));
        fflush(logfile_a);
        time_in_double = 1000000000 * (Time.tv_sec) + (Time.tv_nsec);
        int fd_fifo_time = open(argv[5], O_WRONLY);
        CHECK(write(fd_fifo_time, &time_in_double, sizeof(double)));
        CHECK(close(fd_fifo_time));
        time(&actualtime);
        fprintf(logfile_a, "[%d] Executable: %s- Content: time variables sent to master - Time: %s\n", getpid(), argv[0], ctime(&actualtime));
        fflush(logfile_a);
    }
    return 0;
}

/******** DOSTUFF() *********************
 There is a separate instance of this function
 for each connection.  It handles all communication
 once a connnection has been established.
 *****************************************/
void dostuff(int sock, int bufSize, const char *fifo_name, const char *executable)
{

    bzero(buffer, SIZE);
    (bufSize < SIZE) ? fillBuffer(buffer, bufSize) : fillBuffer(buffer, SIZE);
    time(&actualtime);
    fprintf(logfile_a, "[%d] Executable: %s- Content: starts writing(producer) - Time: %s\n", getpid(), executable, ctime(&actualtime));
    fflush(logfile_a);
    // get the current time
    CHECK(clock_gettime(CLOCK_REALTIME, &Time));
    time_in_double = 1000000000 * (Time.tv_sec) + (Time.tv_nsec);

    for (int i = 0; i < bufSize; i++)
    {
        CHECK(write(sock, &buffer[(i) % SIZE], sizeof(int)));
    }
    time(&actualtime);
    fprintf(logfile_a, "[%d] Executable: %s- Content: finishes writing(producer) - Time: %s\n", getpid(), executable, ctime(&actualtime));
    fflush(logfile_a);
    int fd_fifo_time;
    CHECK(fd_fifo_time = open(fifo_name, O_WRONLY));
    CHECK(write(fd_fifo_time, &time_in_double, sizeof(double)));
    CHECK(close(fd_fifo_time));
    time(&actualtime);
    fprintf(logfile_a, "[%d] Executable: %s- Content: time variables sent to master - Time: %s\n", getpid(), executable, ctime(&actualtime));
    fflush(logfile_a);
}

// Function: fillBuffer(__,__)
// this function fills a buffer vith a fixed size of integers
void fillBuffer(int buf[], int bufSize)
{
    for (int i = 0; i < bufSize; i++)
    {
        buf[i] = rand() % 9;
    }
}

// Function: error(__)
// This function is called when a system call fails. It displays a message about the error on stderr and then
// aborts the program.
void error(char *msg)
{
    perror(msg);
    exit(1);
}