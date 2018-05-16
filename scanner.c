#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include "scanner.h"

static void *allocateMsg(size_t size,char *where);
static void *reallocateMsg(void *s,size_t size,char *where);
static void skipWhiteSpace(FILE *);

char *readToken(FILE *fp) {
    int ch,index;
    char *buffer;
    int size = 80;

    skipWhiteSpace(fp);

    ch = fgetc(fp);
    if (ch == EOF) return 0;

    buffer = allocateMsg(size,"readToken");

    index = 0;
    while (!isspace(ch) && ch != ',') {
        if (ch == EOF) break;
        if (index > size - 2) {
            ++size;
            buffer = reallocateMsg(buffer,size,"readToken");
		}
        buffer[index] = ch;
        ++index;
        ch = fgetc(fp);
	}

    if(ch != ',')
        ungetc(ch,fp);

    if (index > 0)
        clearerr(fp);

    buffer[index] = '\0';

    return buffer;
}

static void skipWhiteSpace(FILE *fp) {
    int ch;

    //Read characters until non-whitespace is found
    while ((ch = fgetc(fp)) != EOF && isspace(ch))
        continue;

    if (ch != EOF) ungetc(ch,fp);
}


void *allocateMsg(size_t size,char *where) {
    char *s;
    s = malloc(size);
    if (s == 0) {
        fprintf(stderr,"%s: could not allocate string, out of memory\n",
            where);
        exit(3);
	}

    return s;
}

static void *reallocateMsg(void *s,size_t size,char *where) {
    char *t;
    t = realloc(s,size);
    if (t == 0) {
        fprintf(stderr,"%s: could not reallocate string, out of memory\n",
            where);
        exit(3);
	}

    return t;
}
