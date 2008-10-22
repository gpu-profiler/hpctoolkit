// -*-Mode: C++;-*- // technically C99
// $Id$

//********************************************************************
// file: unwind.h
//
// purpose:
//     interface to the stack-unwinding primitives. this includes
//     both the architecture-independent, and the 
//     architecture-specific primitives
//********************************************************************

#ifndef UNWIND_H
#define UNWIND_H

//********************************************************************
// system include files
//********************************************************************

#include <ucontext.h>



//********************************************************************
// local include files
//********************************************************************

#include "unwind_cursor.h"



//********************************************************************
// interface to architecture-specific operations
//********************************************************************

void *context_pc(void *context);

void  unw_init_arch(void);
void  unw_init_cursor_arch(void* context, unw_cursor_t *cursor);
int   unw_get_reg_arch(unw_cursor_t *c, int reg_id, void **reg_value);


//********************************************************************
// interface to architecture independent operations
//********************************************************************

void unw_init(void);

void unw_init_cursor(void* context, unw_cursor_t* cursor);

//---------------------------------------------------------------------
// function: unw_step
//
// purpose:
//     Given a cursor, step the cursor to the next (less deeply nested)
//     frame.  Conforms to the semantics of libunwind's unw_step.  In
//     particular, returns:
//       > 0 : successfully advanced cursor to next frame
//         0 : previous frame was the end of the unwind
//       < 0 : error condition
//---------------------------------------------------------------------
int unw_step(unw_cursor_t *c);

int unw_get_reg(unw_cursor_t *c, int reg_id, void **reg_value);

typedef void *unw_word_t;

#define UNW_REG_IP 1

#endif