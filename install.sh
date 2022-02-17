# Checking if the user entered a path name when running the bash file.
# If no additional pathname is entered, the bash script will exit echoing an error message.

if [ $# -eq 0 ]
then
echo "Usage: source ./install.sh <pathname> ";
exit;
fi
 
 
# If the file already exists installation of the source package will begin.
# Otherwise, the script will ask the user if he wants to create the package and install the source inside of it. 
if [ ! -d $1 ]
then
echo "Error: Directory $1 DOES NOT exist.";
while true; do
read -p "Do you want to create $1 directory? [Y/n] " yn
case $yn in
[Yy]* ) mkdir $1; break;;
[Nn]* ) exit;;
* ) "Please, answer y for yes or n for no.";;
esac
done
fi


 
echo "Begin program installation on $1 ... ";
 

# Unzipping of the source file and moveing all the needed files inside the newly created directory. 
unzip sources.zip -d $1;

 
 
echo "Program installed on $1";
echo "Begin sources' compilation ...";

# Compiling of all the scripts and moveing the executables all to the same directory.
 
gcc $1/master/master.c -o $1/ms;
gcc $1/unnamedPipe/unnamedPipe.c -o $1/up;
gcc $1/namedPipe/namedPipe.c -o $1/np;
gcc $1/circularBuffer/circularBuffer.c -o $1/cb -pthread -lrt;
gcc $1/socket/socket.c -o $1/sock;


echo "You can run the program in $1 with ./run.sh or either get some info about the precessing by running ./help.sh";

# Setting the folder's name as a global variable.

export X=$1;

