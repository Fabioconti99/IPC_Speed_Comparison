
############################ ASSIGNMENT INFOS #############################

HOW TO RUN the project: 

To run the project you should first launch the INSTALL shell script by running the following command on the command window:

$ source ./install.sh <path_name>
$ ./run.sh

NOTE:
If you ever close the shell you are launching the simulation from, remember to run the first command again before running the program with the following one. 


SUMMARY: 

This project is a collection of 4 concurrent programs plus a 'master' process dedicate manage their activation. 
The aim of the assignment is to measure the speed when sending and receiving data between a producer and a consumer using 4 different methods of data-transferring. 
The master process works as a UI that will let the user set some parameters regarding the test he wants to try.


UI interface: 

-1- The user will first be asked the amount of data to transfer from the producer process to another. The maximum amout of data that can be transfered from the producer process to the consumer process is 100 MB.

-2- The second step will be selecting one of the four possible data-transferring models by typing either:
	
	-[1] to use the UN-NAMED PIPES model
	-[2] to use the NAMED PIPES model
	-[3] to use the SOCKETS model
	-[4] to use the SHARES MEMORY WITH CIRCULAR BUFFER model

If the user types in [-1], the program will close all the comunication channels and processes currently running and it will end the simulation.

if the user either chooses th third or the fourth model the interface will ask some addtional infos about the data transfering process.

-3- additional info

	-3.1- If the THIRD [3] option is choosen, the master proces will ask the user to choose a port number from 1240 and 49000. If this port is by chance already in use the server portion of the socket comunication structure will later tell you this and exit.
	-3.2- If the FOURTH [4] option is choosen, the master proces will ask the user to choose a circular buffer size from  0.1 up to 10 KB in order to share data through the use of shared user-memory. 


GENERAL description of the processes' behavior:

Once called, the transferring data programs will fork themselfs to become either a producer or a consumer process. For every data transferring modality the producer will start filling up an array of data in order to contain the right amount corresponding to the user's choice. An array of the same dimension will be created in the receiver portion of the process. This array will be filled in with the data collected in the producer's one using the choosen data-transferring protocol. 
The program will keep track of the time spent to transferring data from one process to another and it will report the following values:
 
	- The absolute time of the beginning of the data transfer,
	- The absolute time of the ending of the data transfer,
	- The difference between the two (i.e. the execution time),
	- The trasfer speed.

	

	

	
