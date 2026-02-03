/*
 * edited by Mayuresh Pitale
 * date: 6/6/2024
 *reference: https://gemini.google.com/share/9d8b214ac63d 
 */

#include "systemcalls.h"
#include <stdlib.h>    // for system
#include <unistd.h>    // for fork, execv, dup2
#include <sys/wait.h>  // for waitpid
#include <sys/types.h>  // for pid_t
#include <fcntl.h>     // for open
/**
 * @param cmd the command to execute with system()
 * @return true if the command in @param cmd was executed
 *   successfully using the system() call, false if an error occurred,
 *   either in invocation of the system() call, or if a non-zero return
 *   value was returned by the command issued in @param cmd.
*/
bool do_system(const char *cmd){
    // Execute command
    int ret = system(cmd);
    // Check for errors
    if (ret == -1) {
        return false; // system() call failed
    }      
    // Check command exit status
    return (WIFEXITED(ret) && WEXITSTATUS(ret) == 0);
}

/**
* @param count -The numbers of variables passed to the function. The variables are command to execute.
*   followed by arguments to pass to the command
*   Since exec() does not perform path expansion, the command to execute needs
*   to be an absolute path.
* @param ... - A list of 1 or more arguments after the @param count argument.
*   The first is always the full path to the command to execute with execv()
*   The remaining arguments are a list of arguments to pass to the command in execv()
* @return true if the command @param ... with arguments @param arguments were executed successfully
*   using the execv() call, false if an error occurred, either in invocation of the
*   fork, waitpid, or execv() command, or if a non-zero return value was returned
*   by the command issued in @param arguments with the specified arguments.
*/

bool do_exec(int count, ...) {
    va_list args;
    va_start(args, count);
    char * command[count+1];
    int i;
    for(i=0; i<count; i++)
    {
        command[i] = va_arg(args, char *);
    }
    command[count] = NULL;
    // Fork a child process
    pid_t pid = fork();
    // Check for fork error
    if (pid == -1) {
        perror("fork");
        va_end(args); // Clean up variable argument list
        return false;
    } 
    // Child process
    else if (pid == 0) {
        execv(command[0], command); // Execute command
        perror("execv"); // execv failed
        _exit(1); // Exit child process
    } 
    // Parent process
    else {
        va_end(args); // Clean up variable argument list
        int status; // Variable to store child process status
        if (waitpid(pid, &status, 0) == -1) { 
            return false;// waitpid failed
        }
        return (WIFEXITED(status) && WEXITSTATUS(status) == 0); // Check child exit status
    }
}

/**
* @param outputfile - The full path to the file to write with command output.
*   This file will be closed at completion of the function call.
* All other parameters, see do_exec above
*/
bool do_exec_redirect(const char *outputfile, int count, ...){
    va_list args;
    va_start(args, count);
    char * command[count+1];
    int i;
    for(i=0; i<count; i++){
        command[i] = va_arg(args, char *);
    }
    command[count] = NULL;

    // Open the output file for writing
    int fd = open(outputfile, O_WRONLY|O_CREAT|O_TRUNC, 0644);
    // Check for open error
    if (fd < 0) { 
        perror("open"); 
        va_end(args);
        return false; // Failed to open file
    }
    // Fork a child process
    pid_t pid = fork();
    // Check for fork error
    if (pid == -1) {
        perror("fork");
        close(fd);
        va_end(args);
        return false;
    } 
    // Child process
    else if (pid == 0) {
        // Redirect stdout to the file
        if (dup2(fd, STDOUT_FILENO) < 0) {
            perror("dup2");
            _exit(1);
        }
        close(fd); 
        execv(command[0], command);// Execute command
        _exit(1);
    } 
    else {
        // Parent process
        va_end(args);
        close(fd); 
        int status;
        waitpid(pid, &status, 0); // Wait for child process
        return (WIFEXITED(status) && WEXITSTATUS(status) == 0); // Check child exit status
    }
}
