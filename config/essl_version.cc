#include <stdio.h>
#include <essl.h>

int main()
{
    int v = iessl();
    int version      = int( v / 1000000 );
    int release      = int( (v % 1000000) / 10000 );
    int modification = int( (v % 10000) / 100 );
    int ptf          = v % 100;
    
    printf( "ESSL_VERSION=%d.%d.%d.%d\n",
            version, release, modification, ptf );
    return 0;
}