//=============================================================================
// FILE:
//      input_for_duplicate_bb_2.c
//
// DESCRIPTION:
//      Sample input file for the DuplicateBB pass.
//
// License: MIT
//=============================================================================
int foo(int arg_1) { return 1; }
int  bar(int arg_2,int arg_3){
    int temp;
    temp=arg_2+arg_3;
    return temp;
}