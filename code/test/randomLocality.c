#define Dim  1000

#include "syscall.h"
int A[Dim];
int random[11];
int random2[11];
main()
{
    int i;
    int j;
   random[0] =  694;
   random[1] =  692;
   random[2] =  462;
   random[3] =  129;
   random[4] =  910;
   random[5] =  732;
   random[6] =  363;
   random[7] =  777;
   random[8] =  395;
   random[9] =  326;
   random[10] = 680;
   random2[0] =421;   
   random2[1] =60;
   random2[2] =720;
   random2[3] =590;
   random2[4] =590;
   random2[5] =812;
   random2[6] =648;
   random2[7] =245;
   random2[8] =594;
   random2[9] =941;
   random2[10]=492;
        

    for (i = 0; i < Dim; i++)		/* first initialize the matrices */
    {
        A[i]=i;
    }
    for (i = 0; i < 11; i++)		/* first initialize the matrices */
    {
        A[random[i]] = i;
    }

    for (i = 0; i < 11; i++)		/* first initialize the matrices */
    {
        A[random2[i]]=i ;
    }    

    Exit(1);
}

