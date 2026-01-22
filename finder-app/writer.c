#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <errno.h>

int main(int argc, char *argv[]) {
    
    // Open syslog
    openlog("writer", LOG_PID | LOG_CONS, LOG_USER);

    if (argc != 3) {
        syslog(LOG_ERR, "Invalid number of arguments. Usage: %s <string1> <string2>", argv[0]);
        closelog();
        return EXIT_FAILURE;
    }

    const char *writefile = argv[1];
    const char *writestr = argv[2];

    syslog(LOG_DEBUG, "Writing string %s to %s", writestr, writefile);

    FILE *file = fopen(writefile, "w");
    if (file == NULL) {
        syslog(LOG_ERR, "Failed to open file %s: %s", writefile, strerror(errno));
        closelog();
        return EXIT_FAILURE;
    }

    if (fprintf(file, "%s", writestr) < 0) {
        syslog(LOG_ERR, "Failed to write to file %s: %s", writefile, strerror(errno));
        fclose(file);
        closelog();
        return EXIT_FAILURE;
    }
    
    if (fclose(file) != 0) {
        syslog(LOG_ERR, "Failed to close file %s: %s", writefile, strerror(errno));
        closelog();
        return EXIT_FAILURE;
    }

    closelog();
    
    return EXIT_SUCCESS;
}