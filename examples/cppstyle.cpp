// confy-setup { line: "//-", meta_line: "//!", block_start: "/*-", block_end: "-*/", meta_block_start: "/*!", meta_block_end: "!*/" }

// Here is some code with C++-style comments.

#include <stdio.h>

/*! 
 bool $doGreet "Print greeting?" = true;
 !*/

int main(int argc, char* argv[]) {
/*! if($doGreet) {
       string $greeting "Greeting text" = "Hello world!";
 !*/
//!  template {
//-      printf("$greeting");
//!  } into {
      printf("Hello world!");
//!  }
//! }
    return 0;
}
