#include "unity.h"
#include <stdbool.h>
#include <stdlib.h>
#include "../../examples/autotest-validate/autotest-validate.h"
#include "../../assignment-autotest/test/assignment1/username-from-conf-file.h"

/**
* This function should:
*   1) Call the my_username() function in Test_assignment_validate.c to get your hard coded username.
*   2) Obtain the value returned from function malloc_username_from_conf_file() in username-from-conf-file.h within
*       the assignment autotest submodule at assignment-autotest/test/assignment1/
*   3) Use unity assertion TEST_ASSERT_EQUAL_STRING_MESSAGE the two strings are equal.  See
*       the [unity assertion reference](https://github.com/ThrowTheSwitch/Unity/blob/master/docs/UnityAssertionsReference.md)
*/

const char *my_username(void);
void test_validate_my_username()
{
const char *username;
    char *conf_username;
    //Get the hard-coded username provided by the autotest function
    username = my_username();

    // Read the username from the configuration txt file 
    conf_username = malloc_username_from_conf_file();

    // Verify the two usernames match and report a helpful message on failure
    TEST_ASSERT_EQUAL_STRING_MESSAGE(username, conf_username, "Username in /conf/username.txt does not match my_username()");

    // Free memory allocated by malloc_username_from_conf_file()
    free(conf_username);
}
