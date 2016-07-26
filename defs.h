#ifndef _MOVIT_DEFS_H
#define _MOVIT_DEFS_H

// Utility macros that are useful from other header files.

#ifdef __GNUC__
#define MUST_CHECK_RESULT __attribute__((warn_unused_result))
#define DOES_NOT_RETURN __attribute__((noreturn))
#else
#define MUST_CHECK_RESULT 
#define DOES_NOT_RETURN
#endif

#endif  // !defined(_MOVIT_DEFS_H)
