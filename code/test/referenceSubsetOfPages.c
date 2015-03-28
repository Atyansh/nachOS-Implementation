#define Dim 	44

int A[Dim];

void stuff(int index){
    int i;  
    for (i = 0; i < Dim; i++)		/* first initialize the matrices */
     {
        A[i]=i;
     }
    if(index!=i){
        stuff(index+1);
    }


}
int
main()
{
    int i;

    for (i = 0; i < Dim; i++)		/* first initialize the matrices */
     {
        A[i]=i;
     }

    Exit(1);
}

