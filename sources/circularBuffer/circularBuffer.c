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
#include <sys/mman.h>
#include <semaphore.h>

#define SIZE 2000000

#define SEM_NOT_EMPTY "/sem_not_empty"
#define SEM_NOT_FULL "/sem_not_full"
#define MUTEX "/mutex"

#define SHM_ID "/SHM"

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

// function to check 'sem_open' call
void check_semaphore(sem_t *);

int main(int argc, char *argv[])
{

	FILE *logfile_a = fopen("./logfile.txt", "a");
	time_t actualtime;
	time(&actualtime);
	fprintf(logfile_a, "[%d] Executable: %s- Content: father process spawned(consumer) - Time: %s\n", getpid(), argv[0], ctime(&actualtime));
	fflush(logfile_a);

	if (argc < 5)
	{
		fprintf(stderr, "usage %s bufferSize circular_bufferSize fifoname_time_prod fifoname_time_cons\n", argv[0]);
		exit(0);
	}

	double time_in_double;
	struct timespec Time;

	// the actual size of the buffer is given as a parameter
	int buffer_size = atoi(argv[1]);
	// the actual size of the circular buffer is given as a parameter
	int circular_buffer_size = atoi(argv[2]);

	// Initialization, opening and error check of the named semaphores
	sem_t *not_empty = sem_open(SEM_NOT_EMPTY, O_CREAT, S_IRUSR | S_IWUSR, 0);
	check_semaphore(not_empty);
	sem_t *not_full = sem_open(SEM_NOT_FULL, O_CREAT, S_IRUSR | S_IWUSR, buffer_size - 1);
	check_semaphore(not_full);
	sem_t *mutex = sem_open(MUTEX, O_CREAT, S_IRUSR | S_IWUSR, 1);
	check_semaphore(mutex);

	// Integer value that'll be used to send the time to the master process
	int fd_fifo_time;
	// File descriptor of the shared memory
	int fd_shm;

	pid_t pid = fork();
	CHECK(pid);

	/**********CHILD PROCESS (producer)**************/

	if (pid == 0)
	{
		time(&actualtime);
		fprintf(logfile_a, "[%d] Executable: %s- Content: child process forked (producer) - Time: %s\n", getpid(), argv[0], ctime(&actualtime));
		fflush(logfile_a);
		// get shared memory file descriptor (NOT a file)
		CHECK(fd_shm = shm_open(SHM_ID, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR));

		// extend shared memory object as by default it's initialized with size 0
		CHECK(ftruncate(fd_shm, circular_buffer_size * sizeof(int)));
		// mmap() creates a new mapping in the virtual address space of the calling process.
		// The second argument specifies the length of the mapping (which must be greater than 0).
		// 'PROT_WRITE' indicates that the pages may be written
		int *buffer = mmap(NULL, circular_buffer_size * sizeof(int), PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, fd_shm, 0);

		if (buffer == MAP_FAILED)
		{
			printf("Mapping Failed\n");
			fflush(stdout);
			return -1;
		}
		// Initialize the index of the circular buffer (for writing)
		int in = 0;
		// Initialize the index of producer_arr array
		int counter = 0;
		// Initialization of the array that will be filled with random data
		int producer_arr[SIZE];
		// if the actual buffer size is smaller than SIZE, then let's fill the buffer with that size
		(buffer_size < SIZE) ? fillBuffer(producer_arr, buffer_size) : fillBuffer(producer_arr, SIZE);

		// take the current time (i.e. when it starts to write data on the circular buffer)
		clock_gettime(CLOCK_REALTIME, &Time);

		time(&actualtime);
		fprintf(logfile_a, "[%d] Executable: %s- Content: starts writing(producer) - Time: %s\n", getpid(), argv[0], ctime(&actualtime));
		fflush(logfile_a);

		// convert the current time into a double
		time_in_double = 1000000000 * (Time.tv_sec) + (Time.tv_nsec);

		// open the named fifo used for sending the current time (write-only), the name is passed as an argument
		CHECK(fd_fifo_time = open(argv[3], O_WRONLY));
		// write into the fifo the time (that will be read by master process)
		CHECK(write(fd_fifo_time, &time_in_double, sizeof(double)));
		// close the named fifo (for the time)
		CHECK(close(fd_fifo_time));

		// writing data: the circular buffer is filled with the elements of producer_arr array,
		// the semaphores manage the memory access from the producer and the consumer (for more info, see Shared Memory Theory).
		while (counter < buffer_size)
		{
			sem_wait(not_full);
			sem_wait(mutex);
			// if bufSize is greater than SIZE, the buffer will be overwritten, starting from the first element.
			buffer[in] = producer_arr[counter++ % SIZE];
			in = (in + 1) % circular_buffer_size;
			sem_post(mutex);
			sem_post(not_empty);
		}

		time(&actualtime);
		fprintf(logfile_a, "[%d] Executable: %s- Content: finishes writing(producer) - Time: %s\n", getpid(), argv[0], ctime(&actualtime));
		fflush(logfile_a);

		// The munmap() system call deletes the mappings for the specified address range
		//(in this case, as the first parameter of 'mmap()' is NULL, the map address is chosen by the kernel)
		CHECK(munmap(buffer, circular_buffer_size));

		// shm_open cleanup
		// CHECK(fd_shm = shm_unlink(SHM_ID));

		// Close all the named semaphores
		CHECK(sem_close(not_full));
		CHECK(sem_close(not_empty));
		CHECK(sem_close(mutex));
	}

	/**********FATHER PROCESS (consumer)**************/

	else
	{
		// get shared memory file descriptor (NOT a file)
		fd_shm = shm_open(SHM_ID, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
		if (fd_shm == -1)
		{
			perror("open");
			return -1;
		}
		// extend shared memory object as by default it's initialized with size 0
		CHECK(ftruncate(fd_shm, circular_buffer_size * sizeof(int)));

		// Same as before, but in this case 'PROT_READ' indicates that the pages may be read
		int *buffer = mmap(NULL, circular_buffer_size * sizeof(int), PROT_READ, MAP_SHARED | MAP_ANONYMOUS, fd_shm, 0);

		if (buffer == MAP_FAILED)
		{
			printf("Mapping Failed\n");
			fflush(stdout);
			return -1;
		}
		printf(BHWHT "Father PID (consumer): [%d]\n" RESET, getpid());
		printf(BHWHT "Child PID (producer): [%d]\n" RESET, pid);

		// Initialize the index of the circular buffer (for reading)
		int out = 0;

		// Initialize the index of consumer_arr array
		int counter = 0;
		// declare the variable that'll be used for the loading bar
		int count_percent = 1;
		// Initialization of the array that will contain data read from circular buffer
		int consumer_arr[SIZE];

		time(&actualtime);
		fprintf(logfile_a, "[%d] Executable: %s- Content: starts reading(consumer) - Time: %s\n", getpid(), argv[0], ctime(&actualtime));
		fflush(logfile_a);

		// reading data: each element of consumer_arr array will be filled with data continously stored in the circular buffer,
		// the semaphores manage the memory access from the producer and the consumer (for more info, see Shared Memory Theory).
		while (counter < buffer_size)
		{
			sem_wait(not_empty);
			sem_wait(mutex);
			consumer_arr[counter++ % SIZE] = buffer[out];

			out = (out + 1) % circular_buffer_size;
			sem_post(mutex);
			sem_post(not_full);

			// loading bar (NOTE: this will increase the time of the execution, you should comment
			// it to get a more reliable result)
			if (counter % (buffer_size / 100) == 0)
			{
				printf("\r" BHWHT "%d out of 100" RESET, count_percent++);
				fflush(stdout);
			}
		}
		// take the current time (i.e. when it finishes to read data on the circular buffer)
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

		time(&actualtime);
		fprintf(logfile_a, "[%d] Executable: %s- Content: time variables sent to master - Time: %s\n", getpid(), argv[0], ctime(&actualtime));
		fflush(logfile_a);

		// The munmap() system call deletes the mappings for the specified address range
		//(in this case, as the first parameter of 'mmap()' is NULL, the map address is chosen by the kernel)
		CHECK(munmap(buffer, buffer_size));
		// shm_open cleanup
		CHECK(fd_shm = shm_unlink(SHM_ID));

		// Close all the named semaphores
		CHECK(sem_close(not_full));
		CHECK(sem_close(not_empty));
		CHECK(sem_close(mutex));

		// remove all the named semaphores identified by name
		CHECK(sem_unlink(SEM_NOT_EMPTY));
		CHECK(sem_unlink(SEM_NOT_FULL));
		CHECK(sem_unlink(MUTEX));
	}

	return 0;
}

// This function is just an error check for 'sem_open()' function
void check_semaphore(sem_t *sem)
{
	if (sem == SEM_FAILED)
	{
		printf("Mapping Failed\n");
		fflush(stdout);
		exit(-1);
	}
}

// Function to fill the given buffer buf with random integers, the size of the array is given by bufSize
void fillBuffer(int buf[], int bufSize)
{
	for (int i = 0; i < bufSize; i++)
	{
		buf[i] = rand() % 9;
	}
}
