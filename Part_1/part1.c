
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
int main() {
    //(1) execute ls -l
    pid_t pid = getpid();

    //(2) change to parent directory
    char *path = "..";
    chdir(path);

    //(3) read by owner
    char *outfile = "output.txt";
    int fd = open(outfile, O_RDWR|O_CREAT, 0666);
    write(fd, "Blah blah blippity blah...\n", 27);

    return 0;
}
