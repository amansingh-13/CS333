#include "simplefs-ops.h"

int arr[8];

int main()
{
    char str[] = "!-----------------------64 Bytes of Data-----------------------!";
    simplefs_formatDisk();

    for (int i = 0; i < 100; i++)
    {
        char digit1, digit2;
        char fName[MAX_NAME_STRLEN];
        fName[MAX_NAME_STRLEN-1] = '\0';
        strcpy(fName + 2, "_.txt");
	if(i > 7){
		digit1 = (i-8)/10 + '0';
		digit2 = (i-8)%10 + '0';
		fName[0] = digit1;
		fName[1] = digit2;
		simplefs_close(arr[i%8]);
		simplefs_delete(fName);
	}
	digit1 = i/10 + '0';
	digit2 = i%10 + '0';
	fName[0] = digit1;
	fName[1] = digit2;
        printf("Creating file %d_.txt: %d\n", i, simplefs_create(fName));
	arr[i%8] = simplefs_open(fName);
        printf("Opening file %d_.txt: %d\n", i, arr[i%8]);
        printf("Writing file %d_.txt: %d\n", i, simplefs_write(arr[i%8], "hello..", 8));
	simplefs_seek(arr[i%8], 7);
        printf("Writing file %d_.txt: %d\n", i, simplefs_write(arr[i%8], str, 64));
    }
    simplefs_dump();
}
