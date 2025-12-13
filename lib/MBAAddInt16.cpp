//==============================================================================
// FILE:
//    MBAAddInt16.cpp
// AUTHOR:
//    @E0N
//
// DESCRIPTION:
//    This pass performs a substitution for 8-bit integer add
//    instruction based on this Mixed Boolean-Airthmetic expression:
//      a + b == (((a ^ b) + 2 * (a & b)) * 39 + 23) * 151 + 111
//    See formula (3) in [1].
//
// USAGE:
//      $ opt -load-pass-plugin <BUILD_DIR>/lib/libMBAAdd.so `\`
//        -passes=-"mba-add" <bitcode-file>
//      The command line option is not available for the new PM
//
//  
// [1] "Defeating MBA-based Obfuscation" Ninon Eyrolles, Louis Goubin, Marion
//     Videau
//
// License: MIT
//==============================================================================