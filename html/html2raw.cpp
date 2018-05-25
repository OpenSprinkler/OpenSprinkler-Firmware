#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LIST_FNAME  "list.txt"
#define H_FNAME     "../htmls.h"

void file_error(const char* name) {
  printf("Can't open file %s\n", name);
}

void html2raw(const char*, const char*, FILE*);

int main(int argc, char* argv[])
{
  printf("--------------------------------------\n");
  printf("Convert all .html files in this folder\n");
  printf("to C++ raw strings and save them in the\n");
  printf("parent folder as htmls.h\n");
  printf("-----------------------------------------\n");

  char command[100];
  sprintf(command, "ls *.html > %s", LIST_FNAME);
  system(command);
  FILE *lp = fopen(LIST_FNAME, "rb");
  if(!lp) {file_error(LIST_FNAME); return 0;}

  FILE *hp = fopen(H_FNAME, "wb");
  if(!hp) {file_error(H_FNAME); return 0;}

  char hfname[100];
  char hsname[100];
  int nfiles = 0;
  while(!feof(lp)) {
    hfname[0]=0;
    fgets(hfname, sizeof(hfname), lp);
    char *ext = strstr(hfname, ".html");
    if(!ext) break;
    *ext=0;
    strcpy(hsname, hfname);
    strcat(hfname, ".html");
    strcat(hsname, "_html");
    printf("%s", hfname);
    printf("\n");
    html2raw(hfname, hsname, hp);
    nfiles++;
  }
  printf("%d files processed.\n", nfiles);
  fclose(hp);
  fclose(lp);
}

char in[10000];
char out[10000];

void html2raw(const char *hfname, const char *hsname, FILE *hp) {
  FILE *fp = fopen(hfname, "rb");
  if(!fp) { file_error(hfname); return; }

  int size;
  char c;
  int i;
  fprintf(hp, "const char %s[] PROGMEM = R\"(", hsname);
  while(!feof(fp)) {
    in[0]=0;
  	fgets(in, sizeof(in), fp);
  	size = strlen(in);
    if(size==0) break;
  	c = in[size-1];
  	if(c=='\r' || c=='\n') size--;
  	c = in[size-1];
  	if(c=='\r' || c=='\n') size--;
  	char *outp = out;
    bool isEmpty = true;
  	for(i=0;i<size;i++) {
  	  switch(in[i]) {
  	  case ' ':
  	  case '\t':
  	  case '\r':
  	  case '\n':  // remove starting empty chars
  	  case '\0':
  	    if(!isEmpty)
  	      *outp++ = in[i];
  	    break;
  		default:
  		  *outp++ = in[i];
  		  isEmpty = false;
  		  break;
  	  } 
  	}
  	if(size) {
    	*outp++ = '\r';
    	*outp++ = '\n';
    	*outp++ = 0;
    	fprintf(hp, "%s", out);
    }
  }
  char *outp = out;
  *outp++ = ')';
  *outp++ = '\"';
  *outp++ = ';';
	*outp++ = '\r';
 	*outp++ = '\n';
 	*outp++ = 0;
 	fprintf(hp, "%s", out);  
  fclose(fp);
}
