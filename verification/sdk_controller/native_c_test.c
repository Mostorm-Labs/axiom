#include <axiom/native.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
int main(int argc,char** argv){
 assert(argc==2);assert(axiom_native_initialize(argv[1],strlen(argv[1]))==0);
 const char* open="{\"documentId\":\"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\",\"builtinFonts\":false}";
 char token[33]={0};assert(axiom_native_create(open,strlen(open),NULL,0,token)==0);assert(strlen(token)==32);
 const axiom_native_navigation_sample navigation={sizeof(navigation),AXIOM_NATIVE_ABI_VERSION,
   AXIOM_NATIVE_ZOOM,0,0,0,0,1.5f};
 assert(axiom_native_navigation(token,1,&navigation)==AXIOM_NATIVE_REJECTED);
 assert(axiom_native_navigation(token,1,NULL)==AXIOM_NATIVE_INVALID_ARGUMENT);
 axiom_native_buffer buffer=0;const char* read="{\"command\":\"state\"}";assert(axiom_native_call(token,read,strlen(read),NULL,0,&buffer)==0);
 size_t n=0;assert(axiom_native_buffer_copy(buffer,NULL,0,&n)==AXIOM_NATIVE_BUFFER_TOO_SMALL);char* reply=calloc(n+1,1);assert(reply);assert(axiom_native_buffer_copy(buffer,reply,n,&n)==0);assert(strstr(reply,"NOT_ATTACHED"));
 axiom_native_buffer held[127];for(unsigned i=0;i<127;++i)assert(axiom_native_call(token,read,strlen(read),NULL,0,&held[i])==0);
 axiom_native_buffer overflow=99;assert(axiom_native_call(token,read,strlen(read),NULL,0,&overflow)==AXIOM_NATIVE_REJECTED);assert(overflow==0);
 for(unsigned i=0;i<127;++i)assert(axiom_native_buffer_release(held[i])==0);
 assert(axiom_native_close(token)==0);assert(axiom_native_buffer_copy(buffer,reply,n,&n)==0);assert(axiom_native_buffer_release(buffer)==0);assert(axiom_native_buffer_release(buffer)==AXIOM_NATIVE_INVALID_ARGUMENT);free(reply);
 assert(axiom_native_close(token)==AXIOM_NATIVE_REJECTED);puts("external C ABI session/buffer lifetime PASS");
}
