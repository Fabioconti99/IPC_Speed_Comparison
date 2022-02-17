#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>

#define SIZE 200000

#define RESET "\033[0m"
#define BHWHT "\e[1;97m"
// Function: CHECK(X)
// This function writes on the shell whenever a system call returns any kind of error.
// The function will print the name of the file and the line at which it found the error.
// It will end the check exiting with code 1.
#define CHECK(X) (                                             \
    {                                                          \
      int __val = (X);                                         \
      (__val == -1 ? (                                         \
                         {                                     \
                           fprintf(stderr, "ERROR ("__FILE__   \
                                           ":%d) -- %s\n",     \
                                   __LINE__, strerror(errno)); \
                           exit(EXIT_FAILURE);                 \
                           -1;                                 \
                         })                                    \
                   : __val);                                   \
    })

// function to fill with random data the buffer that will be sent
void fillBuffer(int[], int);

int main(int argc, char *argv[])
{
  FILE *logfile_a = fopen("./logfile.txt", "a");
  time_t actualtime;
  time(&actualtime);
  fprintf(logfile_a, "[%d] Executable: %s- Content: father process spawned(consumer) - Time: %s\n", getpid(), argv[0], ctime(&actualtime));
  fflush(logfile_a);
  if (argc < 5)
  {
    fprintf(stderr, "usage %s fifo_name bufferSize fifoname_time_prod fifoname_time_cons\n", argv[0]);
    exit(0);
  }
  double time_in_double;
  struct timespec Time;

  // the actual size of the buffer is given as a parameter
  int bufSize = atoi(argv[2]);
  // declare the buffer of SIZE elements
  int buffer[SIZE];
  int fd_fifo_comm, fd_fifo_time;

  pid_t pid = fork();
  CHECK(pid);

  /**********CHILD PROCESS (producer)**************/
  if (pid == 0)
  {
    time(&actualtime);
    fprintf(logfile_a, "[%d] Executable: %s - Content: child process forked (producer) - Time: %s\n", getpid(), argv[0], ctime(&actualtime));
    fflush(logfile_a);
    // if the actual buffer size is smaller than SIZE, then let's fill the buffer with that size
    (bufSize < SIZE) ? fillBuffer(buffer, bufSize) : fillBuffer(buffer, SIZE);
    // open the named fifo used for communication (write-only), the name is passed as an argument
    CHECK(fd_fifo_comm = open(argv[1], O_WRONLY));

    time(&actualtime);
    fprintf(logfile_a, "[%d] Executable: %s- Content: starts writing(producer) - Time: %s\n", getpid(), argv[0], ctime(&actualtime));
    fflush(logfile_a);
    // take the current time (i.e. when it starts to write data on the fifo)
    clock_gettime(CLOCK_REALTIME, &Time);

    // writing data
    for (int i = 0; i < bufSize - 1; i++)
    {
      // if bufSize is greater than SIZE, the buffer will be overwritten, starting from the first element.
      write(fd_fifo_comm, &buffer[i % SIZE], sizeof(int));
    }
    // // close the named fifo (for communication)
    // CHECK(close(fd_fifo_comm));
    time(&actualtime);
    fprintf(logfile_a, "[%d] Executable: %s- Content: finishes writing(producer) - Time: %s\n", getpid(), argv[0], ctime(&actualtime));
    fflush(logfile_a);
    // convert the current time into a double
    time_in_double = 1000000000 * (Time.tv_sec) + (Time.tv_nsec);
    // open the named fifo used for sending the current time (write-only), the name is passed as an argument
    CHECK(fd_fifo_time = open(argv[3], O_WRONLY));
    // write into the fifo the time (that will be read by master process)
    CHECK(write(fd_fifo_time, &time_in_double, sizeof(double)));
    // close the named fifo (for the time)
    CHECK(close(fd_fifo_time));
  }

  /**********FATHER PROCESS (consumer)**************/
  else
  {
    printf(BHWHT "Father PID (consumer): [%d]\n" RESET, getpid());
    printf(BHWHT "Child PID (producer): [%d]\n" RESET, pid);
    // open the named fifo used for communication (read-only), the name is passed as an argument

    CHECK(fd_fifo_comm = open(argv[1], O_RDONLY));
    // declare the variable that'll be used for the loading bar
    int count_percent = 1;
    time(&actualtime);
    fprintf(logfile_a, "[%d] Executable: %s- Content: starts reading(consumer) - Time: %s\n", getpid(), argv[0], ctime(&actualtime));
    fflush(logfile_a);
    // reading data
    for (int i = 0; i < bufSize; i++)
    {
      read(fd_fifo_comm, &buffer[(i) % SIZE], sizeof(int));
      // loading bar (NOTE: this will increase the time of the execution, you should comment
      // it to get a more reliable result)
      if (i % (bufSize / 100) == 0)
      {
        printf("\r" BHWHT "%d out of 100" RESET, count_percent++);
        fflush(stdout);
      }
    }
    // close the named fifo (for communication)
    CHECK(close(fd_fifo_comm));
    // take the current time (i.e. when it finishes to read data on the fifo)
    clock_gettime(CLOCK_REALTIME, &Time);
    time(&actualtime);
    fprintf(logfile_a, "[%d] Executable: %s- Content: finishes reading(consumer) - Time: %s\n", getpid(), argv[0], ctime(&actualtime));
    fflush(logfile_a);

    // convert the current time into a double
    time_in_double = 1000000000 * (Time.tv_sec) + (Time.tv_nsec);
    // open the named fifo used for sending the current time (write-only), the name is passed as an argument
    CHECK(fd_fifo_time = open(argv[4], O_WRONLY));
    // write into the fifo the time (that will be read by master process)
    CHECK(write(fd_fifo_time, &time_in_double, sizeof(double)));
    // close the named fifo (for the time)
    CHECK(close(fd_fifo_time));
  }

  time(&actualtime);
  fprintf(logfile_a, "[%d] Executable: %s- Content: time variables sent to master - Time: %s\n", getpid(), argv[0], ctime(&actualtime));
  fflush(logfile_a);
  return 0;
}

// function to fill the given buffer buf with random integers, the size of the array is given by bufSize
void fillBuffer(int buf[], int bufSize)
{
  for (int i = 0; i < bufSize; i++)
  {
    buf[i] = rand() % 9;
  }
}