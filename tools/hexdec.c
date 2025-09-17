#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

enum hexEnum {A = 10, B = 11, C = 12, D= 13, E = 14, F = 15 };

int is_hex(char its_a_Char) {
    if( its_a_Char == 'A' || its_a_Char == 'a' ) 
        return A;
    else if(its_a_Char == 'B' || its_a_Char == 'b' )
        return B;
    else if(its_a_Char == 'C' || its_a_Char == 'c' )
        return C;
    else if(its_a_Char == 'D' || its_a_Char == 'd' )
        return D;
    else if(its_a_Char == 'E' || its_a_Char == 'e' )
        return E;
    else if(its_a_Char == 'F' || its_a_Char == 'f' )
        return F;
    else {
            printf("error formatting %c\n", its_a_Char);
            exit(0);
    }
}

int hex2dd(char hex[]) {
    unsigned char c;
    char d;
    int dec = 0;
    int hx;
    int i = strlen(hex) - 1;
    double scale;

    for(c = hex[i], scale = 0; i >= 0; i--, scale++) {
        c = hex[i];
        int pre = (int) pow(16.0, scale);
        if( isdigit(c) ) {
            dec += (pre * (c - '0'));
        } else if( hx = (int) is_hex(c) ) {
            dec += (pre * hx);
        } else { printf("error\n"); exit(0); }
    }
    printf("the new decimal value is %d\n", dec);
}

char dec2hex(int dec) {
    unsigned char c;
    char *hex;
    int dc[];

    int div(int d) {
        return d/10;
    }

    for(int i = 0; )
}

int main() {
    char hexStr[80];
    printf("enter a hex value: ");
    fgets(hexStr, 80, stdin);
    int hex_str_len = strlen(hexStr);
    
    hexStr[hex_str_len - 1] = '\0';
    hex2dd(hexStr);
    return 0;
}