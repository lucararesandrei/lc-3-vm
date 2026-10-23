#include <stdio.h>
#include <stdint.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/select.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/termios.h>
#include <sys/mman.h>

#define MEMORY_MAX (1 << 16)
uint16_t memory[MEMORY_MAX];

enum {
  R_R0 = 0,
  R_R1,
  R_R2,
  R_R3,
  R_R4,
  R_R5,
  R_R6,
  R_R7,
  R_PC,
  R_COND,
  R_COUNT
};
uint16_t reg[R_COUNT];

enum {
  FL_POS = 1 << 0,
  FL_ZRO = 1 << 1,
  FL_NEG = 1 << 2,
};

enum {
  OP_BR = 0,  // branch
  OP_ADD,     // add
  OP_LD,      // load
  OP_ST,      // store
  OP_JSR,     // jump register
  OP_AND,     // bitwise AND
  OP_LDR,     // load register
  OP_STR,     // store register
  OP_RTI,     // unused
  OP_NOT,     // bitwise NOT
  OP_LDI,     // load indirect
  OP_STI,     // store indirect
  OP_JMP,     // jump
  OP_RES,     // reserved (unused)
  OP_LEA,     // load effective address
  OP_TRAP     // execute trap
};

enum {
  TRAP_GETC = 0x20, // get char, not echoed onto the terminal
  TRAP_OUT = 0x21, // print char
  TRAP_PUTS = 0x22, // print string
  TRAP_IN = 0x23, // get char, echoed onto the terminal
  TRAP_PUTSP = 0x24, // print byte string
  TRAP_HALT = 0x25
};

static inline uint16_t sign_extend( uint16_t x, int bit_count ) {
  return ( (x >> (bit_count -1)) & 1 ) ? 
         ( x | (0xFFFF << bit_count) ) :
         x;
}

static inline void update_flags( uint16_t r ) {
  if ( reg[r] == 0 ) 
    reg[R_COND] = FL_ZRO;
  else if ( reg[r] >> 15 )
    reg[R_COND] = FL_NEG;
  else
    reg[R_COND] = FL_POS;
}

static inline uint16_t swap16( uint16_t x ) {
  return (x << 8) | (x >> 8);
}

int read_image_file( FILE *file ) {
  uint16_t origin;
  if ( fread( &origin, sizeof( origin ), 1, file ) != 1 )
    return 0;

  origin = swap16( origin );

  size_t max_read = MEMORY_MAX - origin;
  uint16_t *p = memory + origin;
  size_t read = fread( p, sizeof( uint16_t ), max_read, file );

  while ( read-- > 0 ) {
    *p = swap16( *p );
    p++;
  }

  return ferror( file ) == 0;
}

int read_image( const char *image_path ) {
  FILE *file = fopen( image_path, "rb" );
  
  if ( !file )
    return 0;

  if ( !read_image_file( file ) ) {
    fclose( file );
    return 0;
  }
  fclose( file );

  return 1;
}

enum {
  MR_KBSR = 0xFE00, // status
  MR_KBDR = 0xFE02 // data
};

static inline uint16_t check_key(void);

static inline void mem_write( uint16_t address, uint16_t val ) {
  memory[address] = val;
}

uint16_t mem_read( uint16_t address ) {
  if ( address == MR_KBSR ) {
    if ( check_key() ) {
      memory[MR_KBSR] = (1 << 15);
      memory[MR_KBDR] = getchar();
    } else {
      memory[MR_KBSR] = 0;
    }
  }

  return memory[address];
}

struct termios original_tio;
int input_buffering_enabled;

static inline void disable_input_buffering(void) {
  if ( tcgetattr( STDIN_FILENO, &original_tio ) != 0 )
    return;

  struct termios new_tio = original_tio;
  new_tio.c_lflag &= ~ICANON & ~ECHO;
  if ( tcsetattr( STDIN_FILENO, TCSANOW, &new_tio ) == 0 )
    input_buffering_enabled = 1;
}

static inline void restore_input_buffering(void) {
  if ( input_buffering_enabled ) {
    tcsetattr( STDIN_FILENO, TCSANOW, &original_tio );
    input_buffering_enabled = 0;
  }
}

static inline uint16_t check_key(void) {
  fd_set readfds;
  FD_ZERO( &readfds );
  FD_SET( STDIN_FILENO, &readfds );

  struct timeval timeout;
  timeout.tv_sec = 0;
  timeout.tv_usec = 0;

  return select( 1, &readfds, NULL, NULL, &timeout ) != 0;
}

static inline void handle_interrupt( int signal ) {
  (void)signal;
  restore_input_buffering();
  printf( "\n" );
  exit( -2 );
}

int main( int argc, const char* argv[] ) {

  int i, running;
  uint16_t instr, op;
  uint16_t r0, r1, imm_flag, pc_offset, cond_flag, long_flag, offset;
  uint16_t *c;
  char char1, char2;

  // load arguments
  if ( argc < 2 ) {
    printf( "lc3 [image-file1] ...\n" );
    exit( 2 );
  }

  for ( i = 1; i < argc; i++ ) {
    if ( !read_image( argv[i] ) ) {
      printf( "failed to load image: %s\n", argv[i] );
      exit( 1 );
    }
  }

  // setup
  signal( SIGINT, handle_interrupt );
  disable_input_buffering();

  reg[R_COND] = FL_ZRO;

  enum { PC_START = 0x3000 };
  reg[R_PC] = PC_START;

  running = 1;
  while ( running ) {

    op = ( instr = mem_read( reg[R_PC]++ ) ) >> 12;

    switch ( op ) {

      case OP_ADD:

        r0 = (instr >> 9) & 0x7;
        r1 = (instr >> 6) & 0x7;
        imm_flag = (instr >> 5) & 0x1;

        if ( imm_flag ) 
          reg[r0] = reg[r1] + ( sign_extend( (instr & 0x1F), 5 ) );
        else 
          reg[r0] = reg[r1] + reg[ (instr & 0x7) ];

        update_flags( r0 );

        break;

      case OP_AND:
        
        r0 = (instr >> 9) & 0x7;
        r1 = (instr >> 6) & 0x7;
        imm_flag = (instr >> 5) & 0x1;

        if ( imm_flag )
          reg[r0] = reg[r1] & ( sign_extend( (instr & 0x1F), 5 ) );
        else
          reg[r0] = reg[r1] & reg[ (instr & 0x7) ];

        update_flags( r0 );

        break;

      case OP_NOT:

        r0 = (instr >> 9) & 0x7;
        r1 = (instr >> 6) & 0x7;

        reg[r0] = ~reg[r1];
        
        update_flags( r0 );

        break;

      case OP_BR:

        pc_offset = sign_extend( (instr & 0x1FF), 9 );
        cond_flag = (instr >> 9) & 0x7;

        if ( cond_flag & reg[R_COND] )
          reg[R_PC] += pc_offset;

        break;

      case OP_JMP:

        r1 = (instr >> 6) & 0x7;
        
        reg[R_PC] = reg[r1];

        break;

      case OP_JSR:

        long_flag = (instr >> 11) & 1;

        reg[R_R7] = reg[R_PC];

        if ( long_flag ) 
          reg[R_PC] += sign_extend( (instr & 0x7FF), 11 );
        else
          reg[R_PC] = reg[ (instr >> 6) & 0x7 ];

        break;

      case OP_LD:

        r0 = (instr >> 9) & 0x7;
        pc_offset = sign_extend( (instr & 0x1FF), 9 );

        reg[r0] = mem_read( reg[R_PC] + pc_offset );
        
        update_flags( r0 );

        break;

      case OP_LDI:

        r0 = (instr >> 9) & 0x7;
        pc_offset = sign_extend( (instr & 0x1FF), 9 );

        reg[r0] = mem_read( mem_read( reg[R_PC] + pc_offset ) );

        update_flags( r0 );

        break;

      case OP_LDR:

        r0 = (instr >> 9) & 0x7;
        r1 = (instr >> 6) & 0x7;
        offset = sign_extend( (instr & 0x3F), 6 );

        reg[r0] = mem_read( reg[r1] + offset );

        update_flags( r0 );

        break;

      case OP_LEA:

        r0 = (instr >> 9) & 0x7;
        pc_offset = sign_extend( (instr & 0x1FF), 9 );

        reg[r0] = reg[R_PC] + pc_offset;

        update_flags( r0 );

        break;

      case OP_ST:

        r0 = (instr >> 9) & 0x7;
        pc_offset = sign_extend( (instr & 0x1FF), 9 );

        mem_write( reg[R_PC] + pc_offset, reg[r0] );

        break;

      case OP_STI:

        r0 = (instr >> 9) & 0x7;
        pc_offset = sign_extend( (instr & 0x1FF), 9 );

        mem_write( mem_read( reg[R_PC] + pc_offset ), reg[r0] );

        break;

      case OP_STR:

        r0 = (instr >> 9) & 0x7;
        r1 = (instr >> 6) & 0x7;
        offset = sign_extend( (instr & 0x3F), 6 );

        mem_write( reg[r1] + offset, reg[r0] );

        break;

      case OP_TRAP:

        reg[R_R7] = reg[R_PC];

        switch ( instr & 0xFF ) {

          case TRAP_GETC:

            reg[R_R0] = (uint16_t)getchar();
            
            update_flags( R_R0 );

            break;

          case TRAP_OUT:

            putc( (char)reg[R_R0], stdout );

            fflush( stdout );

            break;

          case TRAP_PUTS:

            c = memory + reg[R_R0];

            while ( *c ) {
              putc( (char)*c, stdout );
              c++;
            }

            fflush( stdout );

            break;

          case TRAP_IN:

            printf( "Enter a character: " );

            char1 = (char)getchar();
            putc( char1, stdout );
            
            fflush( stdout );

            reg[R_R0] = (uint16_t)char1;

            update_flags( R_R0 );

            break;

          case TRAP_PUTSP:

            c = memory + reg[R_R0];

            while ( *c ) {
              char1 = (char)((*c) & 0xFF);
              putc( char1, stdout );

              char2 = (char)(*c >> 8);
              if ( char2 )
                putc( char2, stdout );
              
              c++;
            }

            fflush( stdout );

            break;

          case TRAP_HALT:

            puts( "HALT" );

            fflush( stdout );

            running = 0;

            break;

        }

        break;

      case OP_RES:
        
        abort();

        break;

      case OP_RTI:

        abort();

        break;

      default:

        abort();

        break;

    }

  }

  // shutdown
  restore_input_buffering();

  return 0;

}
