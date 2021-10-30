// Basic program skeleton for a Sketch File (.sk) Viewer
#include "displayfull.h"
#include "sketch.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

// Allocate memory for a drawing state and initialise it
state *newState() {
  state *drawingState = malloc(sizeof(state));
  *drawingState = (state) {0,0,0,0,LINE,0,0,0};
  return drawingState;
}

// Release all memory associated with the drawing state
void freeState(state *s) {
  free(s);
}

// Extract an opcode from a byte (two most significant bits).
int getOpcode(byte b) {
  unsigned char checkLeftmostBit = 0x80;
  unsigned char check2ndLeftmostBit = 0x40;
  unsigned char operation = 0;
  //checks if the leftmost bit is 1 or 0
  if ((checkLeftmostBit & b) == checkLeftmostBit) operation += 2; // if this is true, leftmost bit is 1
  if ((check2ndLeftmostBit & b) == check2ndLeftmostBit) operation += 1; // if this is true, the 2nd leftmost bit is 1
  return operation;
}

// Extract an operand (-32..31) from the rightmost 6 bits of a byte.
int getOperand(byte b) {
  signed int operand = 0;
  signed char checkBits = 0x3F;
  if ((b & 0x20) == 0x20) operand = b | ~checkBits; //Condition checks if first bit is negative. If yes then 2's complement
  else operand = b & checkBits;
  return operand;
}


void assignTool(display *d, state *s, int operand){
  if (operand == NONE) s->tool = operand;
  else if (operand == LINE) s->tool = operand;
  else if (operand == BLOCK) s->tool = operand;
  else if (operand == COLOUR) colour(d,s->data);
  else if (operand == TARGETX) s->tx = s->data;
  else if (operand == TARGETY) s->ty = s->data;
  else if (operand == SHOW) show(d);
  else if (operand == PAUSE) pause(d, s->data);
  else if (operand == NEXTFRAME) s->end = true;
  s->data = 0;
}


// Execute the next byte of the command sequence.
void obey(display *d, state *s, byte op) {
  int opCode = getOpcode(op);
  int operand = getOperand(op);
  if (opCode == TOOL) assignTool(d,s,operand);
  else if (opCode == DX) s->tx += operand;
  else if(opCode == DY){
    s->ty += operand;
    if (s->tool == LINE) line(d,s->x,s->y,s->tx,s->ty);
    else if (s->tool == BLOCK) block (d,s->x,s->y, (s->tx - s->x), (s->ty - s->y));
    s->x = s->tx;
    s->y = s->ty;
  }
  else if (opCode == DATA){
    s->data = s->data << 6;
    s->data = (s->data) | (operand & 63);
  }
}

// Draw a frame of the sketch file. For basic and intermediate sketch files
// this means drawing the full sketch whenever this function is called.a
// For advanced sketch files this means drawing the current frame whenever
// this function is called.
bool processSketch(display *d, void *data, const char pressedKey) {
  char *filename = getName(d);
  FILE *in = fopen(filename,"rb");
  state *s = (state*) data;
  long counter = s->start;
  fseek(in,counter,0);
  byte b = fgetc(in);
  while ( (!feof(in)) && (s->end == false)) {
    if (getOperand(b) == NEXTFRAME) s->start = counter + 1;
    obey(d,s,b);
    b = fgetc(in);
    counter++;
  }
  if (feof(in)) s->start = 0;
  show(d);
  fclose(in);
  *s = (state) {0,0,0,0,1,s->start,0,0};
  return (pressedKey == 27);
}

// View a sketch file in a 200x200 pixel window given the filename
void view(char *filename) {
  display *d = newDisplay(filename, 200, 200);
  state *s = newState();
  run(d, s, processSketch);
  freeState(s);
  freeDisplay(d);
}

// Include a main function only if we are not testing (make sketch),
// otherwise use the main function of the test.c file (make test).
#ifndef TESTING
int main(int n, char *args[n]) {
  if (n != 2) { // return usage hint if not exactly one argument
    printf("Use ./sketch file\n");
    exit(1);
  } else view(args[1]); // otherwise view sketch file in argument
  return 0;
}
#endif
