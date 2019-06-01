#include <finger.h>

int main(void)
{
    set_device("/dev/ttyardu0");

    char message [] = "a\n";
    for (size_t i = 0; i < 10; i++)
    {
       write_to_device(message, strlen(message)* sizeof(char));
    }
    
    
    return 0;
}