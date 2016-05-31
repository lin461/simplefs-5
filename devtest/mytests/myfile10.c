#include <stdio.h>
#include <fcntl.h>


void
appl10() {
    // multiple openFiles of the same file. As a consequence,
    // this also checks that
    // when a file exists in the mount directory, they should openFile it
    // and not create a new one.

    int fd;
    int retVal;
    int i;
    char commitStrBuf[512];

    for( i = 0; i < 256; i++ )
        commitStrBuf[i] = '0';

    fd = open( "testfile10", O_WRONLY|O_CREAT, S_IRUSR|S_IWUSR );
    if (fd < 0) {
        perror("open");
        return;
    }

    // write first transaction starting at offset 512
    for (i = 0; i < 50; i++) {
        lseek(fd, i * 256, SEEK_SET);
        retVal = write( fd, commitStrBuf, 256 );
        if (retVal <= 0) {
            perror("write");
            return;
        }
    }
    for (; i < 100; i++) {
        lseek(fd, i * 256, SEEK_SET);
        retVal = write( fd, commitStrBuf, 256 );
        if (retVal <= 0) {
            perror("write");
            return;
        }
    }


    retVal = close( fd );
    if (retVal < 0) {
        perror("close");
        return;
    }


    fd = open( "testfile10", O_WRONLY|O_CREAT, S_IRUSR|S_IWUSR );
    if (fd < 0) {
        perror("open");
        return;
    }

    // write second transaction starting at offset 0
    retVal =write( fd, "111111", 6 );
    if (retVal <= 0) {
        perror("write");
        return;
    }

    retVal = close( fd );
    if (retVal < 0) {
        perror("close");
        return;
    }

}

int main() {
    appl10();   
    return 1;
}
