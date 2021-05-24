#include "utils.h"

// Function to set the kth bit of n
int setBit(int n, int k)
{
	return (n | (1 << (k - 1)));
}

// Function to clear the kth bit of n
int clearBit(int n, int k)
{
	return (n & (~(1 << (k - 1))));
}

// Function to toggle the kth bit of n
int toggleBit(int n, int k)
{
	return (n ^ (1 << (k - 1)));
}

char *JSON_Types(int type) {
	if (type == cJSON_Invalid) return ("cJSON_Invalid");
	if (type == cJSON_False) return ("cJSON_False");
	if (type == cJSON_True) return ("cJSON_True");
	if (type == cJSON_NULL) return ("cJSON_NULL");
	if (type == cJSON_Number) return ("cJSON_Number");
	if (type == cJSON_String) return ("cJSON_String");
	if (type == cJSON_Array) return ("cJSON_Array");
	if (type == cJSON_Object) return ("cJSON_Object");
	if (type == cJSON_Raw) return ("cJSON_Raw");
	return NULL;
}

long decimalToBinary(int decimalnum)
{
    long binarynum = 0;
    int rem, temp = 1;

    while (decimalnum!=0)
    {
        rem = decimalnum%2;
        decimalnum = decimalnum / 2;
        binarynum = binarynum + rem*temp;
        temp = temp * 10;
    }
    return binarynum;
}

char *decimal_to_binary(int n)
{
  int c, d, t;
  char *p;

  t = 0;
  p = (char*)malloc(32+1);

  if (p == NULL)
    exit(0);

  for (c = 31 ; c >= 0 ; c--)
  {
    d = n >> c;

    if (d & 1)
      *(p+t) = 1 + '0';
    else
      *(p+t) = 0 + '0';

    t++;
  }
  *(p+t) = '\0';

  return  p;
}


char *BinToHex(char *returned_str)
{
    // convert binary string to integer
    int value = (int)strtol(returned_str, NULL, 2);

    // convert integer to hex string
    static char hexString[24]; // long enough for any 32-bit value, 4-byte aligned
    sprintf(hexString, "%x", value);

    return hexString;
}

// function to convert Hexadecimal to Binary Number
char *HexToBin(char *hexdec)
{
    static char hexdec1[100] = "";
    long int i = 0;

    while (hexdec[i])
    {

        switch (hexdec[i])
        {
        case '0':
            strcat(hexdec1, "0000");
            break;
        case '1':
            strcat(hexdec1, "0001");
            break;
        case '2':
            strcat(hexdec1, "0010");
            break;
        case '3':
            strcat(hexdec1, "0011");
            break;
        case '4':
            strcat(hexdec1, "0100");
            break;
        case '5':
            strcat(hexdec1, "0101");
            break;
        case '6':
            strcat(hexdec1, "0110");
            break;
        case '7':
            strcat(hexdec1, "0111");
            break;
        case '8':
            strcat(hexdec1, "1000");
            break;
        case '9':
            strcat(hexdec1, "1001");
            break;
        case 'A':
        case 'a':
            strcat(hexdec1, "1010");
            break;
        case 'B':
        case 'b':
            strcat(hexdec1, "1011");
            break;
        case 'C':
        case 'c':
            strcat(hexdec1, "1100");
            break;
        case 'D':
        case 'd':
            strcat(hexdec1, "1101");
            break;
        case 'E':
        case 'e':
            strcat(hexdec1, "1110");
            break;
        case 'F':
        case 'f':
            strcat(hexdec1, "1111");
            break;
        default:
            printf("\nInvalid hexadecimal digit %c", hexdec[i]);
        }
        i++;
    }
    return hexdec1;
}