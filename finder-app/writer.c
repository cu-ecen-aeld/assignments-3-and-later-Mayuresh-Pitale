/* 
* Author: Mayuresh Pitale
* Date: 2023-10-05
* Reference: https://gemini.google.com/share/35faf02b45e3
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <errno.h>


int main(int argc, char *argv[]) {
    // Open syslog
    openlog("writer", LOG_PID | LOG_CONS, LOG_USER);
    // Check for correct number of arguments
    if (argc != 3) {
        syslog(LOG_ERR, "Invalid number of arguments. Usage: %s <string1> <string2>", argv[0]);
        closelog();
        return EXIT_FAILURE;
    }
    // Extract arguments
    const char *writefile = argv[1];
    const char *writestr = argv[2];

    // Log the write operation
    syslog(LOG_DEBUG, "Writing string %s to %s", writestr, writefile);

    // Open the file for writing
    FILE *file = fopen(writefile, "w");
    if (file == NULL) {
        syslog(LOG_ERR, "Failed to open file %s: %s", writefile, strerror(errno));
        closelog();
        return EXIT_FAILURE;
    }
    // Write the string to the file
    if (fprintf(file, "%s", writestr) < 0) {
        syslog(LOG_ERR, "Failed to write to file %s: %s", writefile, strerror(errno));
        fclose(file);
        closelog();
        return EXIT_FAILURE;
    }
    // Close the file
    if (fclose(file) != 0) {
        syslog(LOG_ERR, "Failed to close file %s: %s", writefile, strerror(errno));
        closelog();
        return EXIT_FAILURE;
    }

    closelog();
    
    return EXIT_SUCCESS;
}