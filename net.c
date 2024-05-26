#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/socket.h>

#define ERR   -1

bool testFunc(int testVal, int errVal, char *msg) {
    if(testVal != errVal) {
        return false;
    } else if(testVal == errVal) {
        fprintf(stdout, msg);
        return true;
    }
}

int main(int argc, char **argv) {
    int z;
    int s[2];
    char cp[64];
    char msg[80] = {0};
    char buf[80];

    snprintf(msg, sizeof(msg), "%s: socketpair(AF_LOCAL, SOCK_STREAM, 0, s)\n", strerror(errno));
    if(testFunc(z = socketpair(AF_LOCAL, SOCK_STREAM, 0, s), ERR, msg) ) {
        return 1;
    }

    snprintf(cp, sizeof(cp), "hello");
    snprintf(msg, sizeof(msg), "%s: write(%d, \"%s\", %d)\n", strerror(errno), s[1], cp, strlen(cp));
    
    if(testFunc(z = write(s[1],cp,6), ERR, msg)) return 2;
    printf("Wrote message '%s' on s[1]\n",cp);

    snprintf(msg, sizeof(msg), "%s: read(%d,buf,%d)\n", strerror(errno), s[0], sizeof(buf));

    if(testFunc(z = read(s[0],buf, sizeof(buf)), ERR, msg)) return 3;
    buf[z] = 0;
    printf("Received message '%s' from socket s[0]\n", buf);

    snprintf(cp, sizeof(cp), "go away!");
    snprintf(msg, sizeof(msg), "%s: write(%d,\"%s\",%d)\n", strerror(errno), s[0], cp,strlen(cp));

    if(testFunc(z = write(s[0], cp, 8), ERR, msg)) return 4;
    printf("wrote message '%s' on s[0]\n", cp);
    snprintf(msg, sizeof(msg), "%s: read(%d,buf,%d)\n", strerror(errno),s[1],sizeof buf);

    if(testFunc(z = read(s[1], buf, sizeof buf), ERR, msg)) return 3;
    buf[z] = 0;
    printf("recieved message '%s' from socket s[1]\n", buf);

    close(s[0]);
    close(s[1]);

    puts("done");

    return 0;
}