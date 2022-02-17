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

#define FIFO_TIME_P "/tmp/my_time_p"
#define FIFO_TIME_C "/tmp/my_time_c"

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

// Function: spawn(__,__)
// This function forks the program and, on the child process, calls the function execvp()
// with the parameters passed as arguments of spawn().
int spawn(const char *, char **);

// Function: create_fifo(__)
// This function uses the mkfifo syscall to make create a named pipe.
void create_fifo(const char *);

// Function: printExecutionTime(__, __)
// This function collects the time values recorded by the Producer and Consumer processes
// and prints out their values and the difference between the final and initial time of the data transfer.
void printExecutionTime(double, double);

// ANSI Colors for Console prints.
const char *red = "\033[0;31m";
const char *bhred = "\e[1;91m";
const char *green = "\033[1;32m";
const char *yellow = "\033[1;33m";
const char *cyan = "\033[0;36m";
const char *magenta = "\033[0;35m";
const char *bhgreen = "\e[1;92m";
const char *bhyellow = "\033[1;93m";
const char *bhblue = "\e[1;94m";
const char *bhmagenta = "\e[1;95m";
const char *bhcyan = "\e[1;96m";
const char *bhwhite = "\e[1;97m";
const char *reset = "\033[0m";

double bufSize_d;
float circular_buffer_size = 0.0;
int portno = 0;
int main(int argc, char *argv[])
{
  time_t actualtime;

  // Opening of the logfile.txt.
  FILE *logfile_w = fopen("./logfile.txt", "w");
  FILE *logfile_a = fopen("./logfile.txt", "a");
  time(&actualtime);
  fprintf(logfile_a, "[%d] Executable: %s - Content: Main Process Started- Time: %s\n", getpid(), argv[0], ctime(&actualtime));
  fflush(logfile_a);

  // Check for correct number of arguments.
  if (argc != 1)
  {
    fprintf(stderr, "usage:%s <filename>\n", argv[0]);
    exit(-1);
  }

  // Initialization of the value containing the amount of data the user wants to transfer
  // between processes (in [MB]).
  int bufSize;

  // Initialization of array of char needed to transfer the data about the Buffer Size between processes.
  char bufSize_s[80];

  // Array containing the encoding of the 4 modalities of data transfering:
  // Un-Named Pipes [1],
  // Named Pipes [2],
  // Sockets [3],
  // Shared memory With Circular-Buffer [4].
  int mode[4] = {1, 2, 3, 4};

  // Initialization of the variable that will later be used to choose the data transfer modality.
  int input_mode = 0;

  // Initialization of the pids needed to run any of the data transfering processes.
  pid_t pid_socket, pid_cb, pid_np, pid_up;

  // Initialization of the variables needed to collect the time values recorded by the transfering processes.
  double timeprod, timecons;

  // Initialization of the file descriptors needed to transfer the time values between the child processes and the master.
  int fd_fifo_p, fd_fifo_c;

  // Initialization of the file-descriptor array needed for the use of U-Named Pipes.
  int fd_up[2];

  // PathName of the special file needed for transfering data via Named Pipes.
  char *named_pipe = "/tmp/named_pipe";

  // Print of the Master PID value.
  printf("\nPID master [%d]\n", getpid());
  fflush(stdout);

  // Beginning of the loop that will keep the program rolling until the user decides to quit.
  while (1)
  {
    // Print needed to tell the user what to do in order to run the program.
    printf("\n\n%sInsert buffer size (in MB) smaller than 100MB ([-1] to quit):%s \n", bhblue, reset);
    fflush(stdout);

    // This DO-WHILE LOOP will spin until a correct input is given.
    do
    {

      // Retrieve of the value given by the user.
      scanf("%d", &bufSize);

      // If the value is equal to '-1' the program will stop executing and it will print
      // an info message.
      if (bufSize == -1)
      {
        printf("program stopped by the user\n");
        fflush(stdout);
        time(&actualtime);
        fprintf(logfile_a, "[%d] Executable: %s- Content: User pressed '-1' - Time: %s\n", getpid(), argv[0], ctime(&actualtime));
        fflush(logfile_a);
        return 0;
      }
      time(&actualtime);
      fprintf(logfile_a, "[%d] Executable: %s - Content: input bufSize= %d- Time: %s\n", getpid(), argv[0], bufSize, ctime(&actualtime));
      fflush(logfile_a);
      // If the Input is either out of the given bounds or it is not an accepted char value
      // the program will print an error message.
      if (bufSize > 100 || bufSize <= 0)
      {

        printf("%sWrong value! try again:%s\n", bhred, reset);
        fflush(stdout);
        while (getchar() != '\n')
          ;
      }
    } while (bufSize > 100 || bufSize <= 0);

    // Knowing that a single integer value weights around 4 bytes, the program will multiply the memory value
    // retrieved with 250000 to make it match with the array's size.
    bufSize = bufSize * 250000;
    bufSize_d = bufSize;
    // Translating the buffer size into a string variable to pass it to the other processes thanks to the execvp()
    // function contained in the spawn() function.
    sprintf(bufSize_s, "%d", bufSize);

    printf("\n%sWhich data sharing mode do you want to use? Press:%s \n", bhblue, reset);
    printf("%s[1]: Un-named pipes%s\n%s[2]: Named pipes%s\n%s[3]: Sockets%s\n%s[4]: Shared memory with circular buffer%s\n%s[-1] to quit\n%s", bhyellow, reset, bhgreen, reset, bhmagenta, reset, bhcyan, reset, bhred, reset);

    // A sking the user for acorrect input, otherwise printing en error message.
    do
    {

      scanf("%d", &input_mode);

      // '-1' to close the program.
      if (input_mode == -1)
      {
        time(&actualtime);
        fprintf(logfile_a, "[%d] Executable: %s- Content: User pressed '-1' - Time: %s\n", getpid(), argv[0], ctime(&actualtime));
        fflush(logfile_a);
        printf("program stopped by the user\n");
        fflush(stdout);
        return 0;
      }

      if (input_mode != 1 && input_mode != 2 && input_mode != 3 && input_mode != 4)
      {
        printf("%sWrong key! try again:%s\n", bhred, reset);
        fflush(stdout);
        while (getchar() != '\n')
          ;
      }
    } while (input_mode != 1 && input_mode != 2 && input_mode != 3 && input_mode != 4);
    time(&actualtime);
    fprintf(logfile_a, "[%d] Executable: %s- Content: input_mode= %d - Time: %s\n", getpid(), argv[0], input_mode, ctime(&actualtime));
    fflush(logfile_a);
    // Creation of the fifos needed to transfer the time values of the consumer and producer process.
    create_fifo(FIFO_TIME_P);
    create_fifo(FIFO_TIME_C);

    // Division into the four modalities of data transfering.
    // If the user chooses any of the following input modes, the program will execute the corrisponding process
    // that will measure the time spent on transferring data.

    // INPUT MODE 1: Un-Name Pipes
    if (input_mode == 1)
    {
      printf("\n%s[1]: Un-named pipes%s\n", bhyellow, reset);
      printf("%s-----------------------%s\n", bhyellow, reset);
      printf("%sSending data...\n%s", bhwhite, reset);
      fflush(stdout);

      // Arrays of char to retrive the FDs of the Un-Named Piepe.
      char fd_1[80];
      char fd_2[80];

      // Checking for any errors regarding the creation of the pipe.
      if (pipe(fd_up) == -1)
      {
        perror("Error creating unnamed fifo\n");
        exit(1);
      }

      // Translating the two FDs into strings to pass them via execvp() funciton.
      sprintf(fd_1, "%d", fd_up[1]);
      sprintf(fd_2, "%d", fd_up[0]);

      // Initialization of the array of data needed for using the execvp() function
      // contained in the spawn() function.
      char *arg_list[] = {"./up", fd_1, fd_2, bufSize_s, FIFO_TIME_P, FIFO_TIME_C, NULL};

      // Spawning of the other processes.
      pid_up = spawn("./up", arg_list);
      time(&actualtime);
      fprintf(logfile_a, "[%d] Executable: %s - Content: process ./up spawned - Time: %s\n", getpid(), argv[0], ctime(&actualtime));
      fflush(logfile_a);
    }

    // INPUT MODE 2: Named Pipe.
    if (input_mode == 2)
    {
      printf("\n%s[2]: Named pipes:%s\n", bhgreen, reset);
      printf("\n%s-----------------------%s\n", bhgreen, reset);
      printf("%sSending data...\n%s", bhwhite, reset);
      fflush(stdout);

      // Creating the fifo channel needed for transfering data via Named Pipe.
      create_fifo(named_pipe);

      // Initialization of the array of data needed for using the execvp() function
      // contained in the spawn() function.
      char *arg_list[] = {"./np", named_pipe, bufSize_s, FIFO_TIME_P, FIFO_TIME_C, NULL};

      // Spawning the process needed for transfering data using the mode choose by the user.
      pid_np = spawn("./np", arg_list);
      time(&actualtime);
      fprintf(logfile_a, "[%d] Executable: %s- Content: process ./np spawned - Time: %s\n", getpid(), argv[0], ctime(&actualtime));
      fflush(logfile_a);
    }

    // INPUT MODE 3: Sockets.
    if (input_mode == 3)
    {
      printf("\n%s[3]: Sockets:%s\n", bhmagenta, reset);
      fflush(stdout);

      // Asking the user for a port number until the input is accepted.
      // According to the theory, the port numbers within the choosen bound should be good to
      // work with.

      do
      {
        printf("\n%sChoose a port number (from 1024 to 49151): \n%s", bhmagenta, reset);
        scanf("%d", &portno);
        if (portno <= 1024 || portno > 49151)
        {
          printf("%sTry again!%s\n", bhred, reset);
          fflush(stdout);
          while (getchar() != '\n')
            ;
        }

      } while (portno <= 1024 || portno > 49151);
      time(&actualtime);
      fprintf(logfile_a, "[%d] Executable: %s- Content: portno= %d - Time: %s\n", getpid(), argv[0], portno, ctime(&actualtime));
      fflush(logfile_a);

      // Translating the value of the port number into an string to trasfer it via execvp()
      char portno_s[80];
      sprintf(portno_s, "%d", portno);

      // Initialization of the array of data needed for using the execvp() function
      // contained in the spawn() function.
      char *arg_list[] = {"./sock", "127.0.0.1", portno_s, bufSize_s, FIFO_TIME_P, FIFO_TIME_C, (char *)NULL};

      // Spawning the process needed for transfering data using the mode choose by the user.
      pid_socket = spawn("./sock", arg_list);
      time(&actualtime);
      fprintf(logfile_a, "[%d] Executable: %s - Content: process ./sock spawned - Time: %s\n", getpid(), argv[0], ctime(&actualtime));
      fflush(logfile_a);
      printf("\n%s-----------------------%s\n", bhmagenta, reset);
      printf("%sSending data...\n%s", bhwhite, reset);
      fflush(stdout);
    }

    // INPUT MODE 4: Shared memory with circular buffer.
    if (input_mode == 4)
    {

      // Asking the user to insert a size for the circular buffer.
      // float circular_buffer_size = 0.0;
      char circular_buffer_size_s[80];
      printf("\n%sHow big should the circular-buffer memory be [KB]? (0.1-10):%s \n", bhcyan, reset);
      do
      {
        scanf("%f", &circular_buffer_size);
        if (circular_buffer_size <= 0.1 || circular_buffer_size > 10)
        {
          printf("%sWrong mem! try again:%s\n", bhred, reset);
          fflush(stdout);
          while (getchar() != '\n')
            ;
        }
      } while (circular_buffer_size <= 0.1 || circular_buffer_size > 10);
      time(&actualtime);
      fprintf(logfile_a, "[%d] Executable: %s- Content: process circular_buffer_size= %.1f - Time: %s\n", getpid(), argv[0], circular_buffer_size, ctime(&actualtime));
      fflush(logfile_a);

      // Knowing that a single integer value weights around 4 bytes, the program will multiply the memory value
      // retrieved with 250 to make it match with the array's size.
      circular_buffer_size = circular_buffer_size * 250;

      // Putting the data into string in order to pass it as an argument of the execvp() function.
      sprintf(circular_buffer_size_s, "%f", circular_buffer_size);

      printf("\n%s[4]: Shared memory with circular buffer:%s\n", bhcyan, reset);
      printf("\n%s-----------------------%s\n", bhcyan, reset);
      printf("%sSending data...\n%s", bhwhite, reset);
      fflush(stdout);

      // Initialization of the array of data needed for using the execvp() function
      // contained in the spawn() function.
      char *arg_list[] = {"./cb", bufSize_s, circular_buffer_size_s, FIFO_TIME_P, FIFO_TIME_C, (char *)NULL};

      // Spawning the process needed for transfering data using the mode choose by the user.
      pid_cb = spawn("./cb", arg_list);
      fflush(stdout);
      time(&actualtime);
      fprintf(logfile_a, "[%d] Executable: %s- Content: process ./cb spawned - Time: %s\n", getpid(), argv[0], ctime(&actualtime));
      fflush(logfile_a);
    }

    // Opening the fifo for retriving the time values of the any of the data transfering processes.
    CHECK(fd_fifo_p = open(FIFO_TIME_P, O_RDONLY));
    CHECK(fd_fifo_c = open(FIFO_TIME_C, O_RDONLY));

    // Waiting for the first child process to die.
    int status, pid;
    CHECK(pid = wait(&status));
    time(&actualtime);
    fprintf(logfile_a, "[%d] Executable: %s - Content: process [%d] terminated - Time: %s\n", getpid(), argv[0], pid, ctime(&actualtime));
    fflush(logfile_a);

    // Checking if the child process died correctly.
    if (WIFEXITED(status))
    {
      CHECK(WEXITSTATUS(status));
    }

    // Reading the time values coming from the processes.
    CHECK(read(fd_fifo_p, &timeprod, sizeof(double)));
    CHECK(read(fd_fifo_c, &timecons, sizeof(double)));

    // printing execution time and the single times retrieved.
    printExecutionTime(timecons, timeprod);

    timecons = 0;
    timeprod = 0;
    // Based on the chosen data tranfering modality the ending of the cycle will be different.
    switch (input_mode)
    {
      // Un-Named Piepe.
    case 1:

      // Closing the FDs of the Un-Named Pipe.
      CHECK(close(fd_up[0]));
      CHECK(close(fd_up[1]));
      time(&actualtime);
      fprintf(logfile_a, "[%d] Executable: %s- Content: closing file descriptors (./up) - Time: %s\n", getpid(), argv[0], ctime(&actualtime));
      fflush(logfile_a);
      printf("%s-----------------------%s\n", bhyellow, reset);
      fflush(stdout);
      break;

      // Named Pipe.
    case 2:

      // Deleting the fifo channel used for tranfering data via Named Pipe.
      CHECK(unlink(named_pipe));
      time(&actualtime);
      fprintf(logfile_a, "[%d] Executable: %s - Content: unlinking named pipe  (./np) - Time: %s\n", getpid(), argv[0], ctime(&actualtime));
      fflush(logfile_a);
      printf("%s-----------------------%s\n", bhgreen, reset);
      fflush(stdout);
      break;

      // Sockets.
    case 3:
      printf("%s-----------------------%s\n", bhmagenta, reset);
      fflush(stdout);
      break;

      // Shared memory with circular buffer.
    case 4:
      printf("%s-----------------------%s\n", bhcyan, reset);
      fflush(stdout);
      break;
    }

    // Unlinking the fifo used for retreaving time values.
    CHECK(unlink(FIFO_TIME_P));
    CHECK(unlink(FIFO_TIME_C));
    time(&actualtime);
    fprintf(logfile_a, "[%d] Executable: %s- Content: execution completed - Time: %s\n", getpid(), argv[0], ctime(&actualtime));
    fflush(logfile_a);
  }
  return 0;
}

// Function: spawn(__,__)
// This function forks the program and, on the child process, calls the function execvp()
// with the parameters passed as arguments of spawn().
int spawn(const char *program, char **arg_list)
{
  // Forking the process and executing the exacvp() function.
  pid_t child_pid = fork();
  CHECK(child_pid);
  if (child_pid != 0)
    return child_pid;

  else
  {
    execvp(program, arg_list);
    // Check for errors of execvp.
    perror("exec failed");
    return 1;
  }
}

// Function: create_fifo(__)
// This function uses the mkfifo syscall to make create a named pipe.
void create_fifo(const char *name)
{
  // automatically checks for errors.
  if (mkfifo(name, 0666) == -1)
  {

    // Checking outomatically for errors.
    if (errno != EEXIST)
    {
      perror("Error creating named fifo\n");
      exit(1);
    }
  }
}

// Function: printExecutionTime(__, __)
// This function collects the time values recorded by the Producer and Consumer processes
// and prints out their values and the difference between the final and initial time of the data transfer.

void printExecutionTime(double t_cons, double t_prod)
{

  // Calculating the time difference.
  double timediff = t_cons - t_prod;

  // And printing all the data recived out.
  t_prod = t_prod / 1000000000;
  t_cons = t_cons / 1000000000;
  printf("!\n%sData sent correctly!%s\n", bhwhite, reset);
  printf("%sInitial (Epoch) time is: %lf[s] %s\n", bhwhite, t_prod, reset);
  printf("%sFinal (Epoch) time  is: %lf[s] %s\n", bhwhite, t_cons, reset);

  timediff = timediff / 1000000000;
  printf("%sBuffer size: %.0lf [Mb]%s\n", bhwhite, bufSize_d / 250000, reset);
  if (circular_buffer_size)
  {
    printf("%sCircular buffer size: %.1lf [Kb]%s\n", bhwhite, circular_buffer_size / 250, reset);
    circular_buffer_size = 0.0;
  }
  if (portno)
  {
    printf("%sPort Number: %d %s\n", bhwhite, portno, reset);
    portno = 0;
  }
  printf("%sExecution time: %.4lf[s] %s\n", bhyellow, timediff, reset);
  printf("%sSpeed: %.4lf[kb/s] %s\n", bhyellow, bufSize_d / (timediff * 1000), reset);
  fflush(stdout);
}
