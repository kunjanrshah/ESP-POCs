#ifndef UTILS_H_
#define UTILS_H

// Function to set the kth bit of n
int setBit(int n, int k)
{
    return (n | (1 << (k - 1)));
}

// Function to return kth bit on n
int getBit(int n, int k)
{
    return ((n >> k) & 1);
}

// Function to clear the kth bit of n
int clearBit(int n, int k)
{
    return (n & (~(1 << (k - 1))));
}

 // Convert String to Integer array
int *convert(char *c)
{
    int len = strlen(c), i;
    int *a = (int *)malloc(len * sizeof(int));
    for (i = 0; i < len; i++)
        a[i] = c[i] - 48;
    return a;
}

// Function to toggle the kth bit of n
int toggleBit(int,int);

// function to convert Hexadecimal to Binary Number
char *HexToBin(char *);

// function to convert Binary to Hexadecimal Number
char *BinToHex(char *);


char *JSON_Types(int);

long decimalToBinary(int);

char *decimal_to_binary(int);


#endif