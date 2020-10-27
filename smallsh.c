#include<stdio.h>

/*
A struct command to make parsing each input line easier
*/
struct commandLine {
    char* command;
    char* args; // optional
    char* inFile; // optional
    char* outFile; // optional
    char* bgProcess; // optional
}



int main() 
{
    printf("Hey");

    return 0;
}

