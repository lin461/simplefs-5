#include <stdio.h>
#include <fcntl.h>


void
appl8() {
    // multiple openFiles of the same file. As a consequence,
    // this also checks that
    // when a file exists in the mount directory, they should openFile it
    // and not create a new one.

    int fd;
    int retVal;
    int i;
    char commitStrBuf[512];

    for( i = 0; i < 512; i++ )
        commitStrBuf[i] = '1';

    fd = open( "testfile8", O_WRONLY|O_CREAT, S_IRUSR|S_IWUSR );
    if (fd < 0) {
        perror("open");
        return;
    }

    // write first transaction starting at offset 512
    for (i = 0; i < 50; i++) {
        lseek(fd, 512 + i * 512, SEEK_SET);
        retVal = write( fd, commitStrBuf, 512 );
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

    for( i = 0; i < 512; i++ )
        commitStrBuf[i] = '2';

    fd = open( "testfile8", O_WRONLY|O_CREAT, S_IRUSR|S_IWUSR );
    if (fd < 0) {
        perror("open");
        return;
    }

    // write second transaction starting at offset 0
    retVal =write( fd, commitStrBuf, 512 );
    if (retVal <= 0) {
        perror("write");
        return;
    }

    retVal = close( fd );
    if (retVal < 0) {
        perror("close");
        return;
    }


    for( i = 0; i < 512; i++ )
        commitStrBuf[i] = '3';

    fd = open( "testfile8" , O_WRONLY|O_CREAT, S_IRUSR|S_IWUSR);
    if (fd < 0) {
        perror("open");
        return;
    }

    // write third transaction starting at offset 50*512
    for (i = 0; i < 50; i++) {
        lseek(fd, 50 * 512 + i * 512, SEEK_SET);
        retVal = write( fd, commitStrBuf, 512 );
        if (retVal <= 0) {
            perror("write");
            return;
        }
    }
    for (; i < 100; i++) {
        lseek(fd, 50 * 512 + i * 512, SEEK_SET);
        retVal = write( fd, commitStrBuf, 512 );
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
}

int main() {
    appl8();   
    return 1;
}
