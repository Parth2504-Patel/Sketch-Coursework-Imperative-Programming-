#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <math.h>

enum { DX = 0, DY = 1, TOOL = 2, DATA = 3};
enum { NONE = 0, LINE = 1, BLOCK = 2, COLOUR = 3, TARGETX = 4, TARGETY = 5};

typedef struct state { int x, y, tx, ty; unsigned int colour, data,tool;} state;
typedef int byte;
/*
  The colour value of the pixel from .pgm can be represented by 8 bits, r1,r2,r3,r4,r5,r6,r7,r8
  To change this value into RGBA value, the red,blue,green component needs to be the same which is equal to this the colour value of pixel.
  The opacity of RGBA needs to be 11111111 for all colour vales.
  The order of commands below are such that when the DATA command is applied, the final result is
  (r1)(r2)(r3)(r4)(r5)(r6)(r7)(r8) (r1)(r2)(r3)(r4)(r5)(r6)(r7)(r8) (r1)(r2)(r3)(r4)(r5)(r6)(r7)(r8) 11111111
  (r1) is a single bit, r1
*/
void setBlockColour(int character, FILE *out){
  unsigned char bitShiftPattern = 0xC0; //This is so when it is masked over commands, it adds the DATA opcode 11
  unsigned char dataCommand1;
  if (character != 0){
    dataCommand1 = (character >> 6) | bitShiftPattern; //This will write 110000(r1)(r2)
    fputc(dataCommand1,out);
    dataCommand1 = ((character << 2) >> 2) | bitShiftPattern;//This will write 11(r3)(r4)(r5)(r6)(r7)(r8)
    fputc(dataCommand1,out);
    dataCommand1 = (character >> 2) | bitShiftPattern;//This will write 11(r1)(r2)(r3)(r4)(r5)(r6)
    fputc(dataCommand1,out);
    dataCommand1 = ((character >> 4) | bitShiftPattern) | ((character << 6) >> 2);//This will write 11(r7)(r8)(r1)(r2)(r3)(r4)
    fputc(dataCommand1,out);
    dataCommand1 = ( ((character  << 4) >> 2) | (bitShiftPattern | 0x03) );//This will write 11(r5)(r6)(r7)(r8)11
    fputc(dataCommand1,out);
    fputc(0xFF,out); //11111111
  }
  else { // colour is 0 but still need to set the opacity to 11111111
    fputc(0xC3,out); //11000011
    fputc(0xFF,out); //11111111
  }
  fputc(0x83,out); //This command sets the colour value to value built up in s->data 10000011
}

void turnIntoCommand(int character, FILE *out, int *heightCounter, int *counter){
  unsigned char dataCommandHeight;
  unsigned char bitShiftPattern = 0xC0; //This is so when it is masked over commands, it adds the DATA opcode 11
  if (*counter == 200) {
    *counter = 0;
    *heightCounter += 1;
    fputc(0x84,out);//set TARGETX to 0, 10000100

    if (*heightCounter > 31) {
      int height = *heightCounter;
      dataCommandHeight = (height >> 6) | bitShiftPattern;
      fputc(dataCommandHeight,out);
      dataCommandHeight = (height) | bitShiftPattern;
      fputc(dataCommandHeight,out);
  }
    else {
      dataCommandHeight = *heightCounter | bitShiftPattern;
      fputc(dataCommandHeight,out);
    }
    fputc(0x85,out);// sets TARGETY to value built up in s->data by  commands above. 10000101
    fputc(0x80,out);//Set tool to NONE, 10 000000
    fputc(0x40,out); //DY 0 to set x,y to tx,ty without drawing block
    fputc(0x82,out);//Set tool to BLOCK, 10 000010
  }
  fputc(0x01,out); //this shifts the tx 1 to the right so can draw the next pixel
  *counter += 1;
  setBlockColour(character,out);
  fputc(0x41,out);//DY +1, 01 000001
  fputc(0x7F,out);//DY -1, 01 111111
}

// This function removes and adds the relevant file extension depending on what conversion occurs.
char *fileExtension(char *filename, bool skOrNot){
  int i = 0;
  int length = strlen(filename);
  char *a;
  if (skOrNot == true) length += 2;
  a = (char *)malloc(length);
  a[0] = '\0';
  while (filename[i] != '.'){
    i++;
  }
  strncat(a,filename,i);
  if (skOrNot == true) strcat(a,".pgm");
  else strcat(a,".sk");
  return a;
}

void readingPGMFile(char *filename, bool skOrNot) {
  char *outputFileName = fileExtension(filename,skOrNot);
  FILE *in = fopen(filename,"r");
  FILE *out = fopen(outputFileName ,"wb");
  int character = fgetc(in);
  int heightCounter = 0;
  int counter = 0;
  int *pointerToCounter = &counter;
  int *pointerToHeightCounter = &heightCounter;
  //This while loop skips the header of the .pgm file so the next character after is the pixel colour values
  while (character != '\n'){
    character = fgetc(in);
  }

  character = fgetc(in);
  fputc(0x82,out);//Set tool to BLOCK, 10 000010
  while (!feof(in)){
    turnIntoCommand(character,out,pointerToHeightCounter,pointerToCounter);
    character = fgetc(in);
  }
  free(outputFileName);
  fclose(in);
  fclose(out);
}


int getOpcode(byte b) {
  unsigned char checkLeftmostBit = 0x80;
  unsigned char check2ndLeftmostBit = 0x40;
  unsigned char operation = 0;
  //checks if the leftmost bit is 1 or 0
  if ((checkLeftmostBit & b) == checkLeftmostBit) operation += 2; // if this is true, leftmost bit is 1
  if ((check2ndLeftmostBit & b) == check2ndLeftmostBit) operation += 1; // if this is true, the 2nd leftmost bit is 1
  return operation;
}
int getOperand(byte b) {
  signed int operand = 0;
  signed char checkBits = 0x3F;
  if ((b & 0x20) == 0x20) operand = b | ~checkBits; //Condition checks if first bit is negative. If yes then 2's complement
  else operand = b & checkBits;
  return operand;
}
int rgbaToGrayscale(state *s){
  // This equation 0.299R + 0.587G + 0.114B converts RGBA to a greyscale value
  int redComponent = (0xFF000000 & s->data) >> 24;
  int greenComponent = (0xFF0000 & s->data) >> 16;
  int blueComponent = (0xFF00 & s->data) >> 8;
  return round ( (0.299*redComponent) + (0.587*blueComponent) + (0.114*greenComponent) );
}

void obeyTools(int **map, state *s, int operand){
  if (operand == NONE) s->tool = operand;
  else if (operand == LINE) s->tool = operand;
  else if (operand == BLOCK) s->tool = operand;
  else if (operand == COLOUR) s->colour = rgbaToGrayscale(s);
  else if (operand == TARGETX) s->tx = s->data;
  else if (operand == TARGETY) s->ty = s->data;
  s->data = 0;
}

void writeBlockBytestoMap(int **map, state*s){
  int a,row;
  //In a block, all the pixels in the dimension are set to the same colour hence 2 for loops
  for (a = s->y; a < (s->ty);a++) {
    for (row = s->x; row < (s->tx);row++){
      map[a][row]= s->colour;
    }
  }
  s->colour = 0;
}

void obeyDY(int **map, state *s, int operand){
  s->ty += operand;
  if (s->tool == BLOCK) writeBlockBytestoMap(map,s);
  s->x = s->tx;
  s->y = s->ty;
}
void obeyDATA(state *s, int operand){
  s->data = s->data << 6;
  s->data = (s->data) | (operand & 63);
}


void obey(int **map, state *s, byte op) {
  int opCode = getOpcode(op);
  int operand = getOperand(op);
  if (opCode == TOOL) obeyTools(map,s,operand);
  else if (opCode == DX) s->tx += operand;
  else if(opCode == DY) obeyDY(map,s,operand);
  else if (opCode == DATA) obeyDATA(s,operand);
}

void freeMemory(int **map, state *s,char *outputFileName){
  for (int i = 0; i < 200;i++){
    free(map[i]);
  }
  free(map);
  free(s);
  free(outputFileName);
}

void writeMapToPGM(int **map,FILE *out,state *s){
  int row,column;
  char header[] = "P5 200 200 255\n";
  fprintf(out, "%s", header);
  for (column = 0; column < 200;column++) {
    for (row = 0; row < 200;row++){
      fprintf(out, "%c", map[column][row]);
    }
  }
}

//This function creates a 2D array 200x200 which replicates the display and all changes are made to this array
//At the end, this array is written to the pgm file alongside the header details.
int **creatingMap(){
  int **display;
  display = (int**) malloc(200 * sizeof(int*));
  for (int i = 0; i < 200;i++){
    display[i] = (int*) malloc(200 * sizeof(int));
  }
  return display;
}

void readingSKFILE(char *filename, bool skOrNot){
  state *s = malloc(sizeof(state));
  *s = (state) {0,0,0,0,0,0,1};
  int **map = creatingMap();
  FILE *in = fopen(filename,"r");
  char *outputFileName = fileExtension(filename,skOrNot);
  FILE *out = fopen(outputFileName,"w");
  fseek(in,0,0);
  byte b = fgetc(in);
  while (!feof(in)) {
    obey(map,s,b);
    b = fgetc(in);
  }
  writeMapToPGM(map,out,s); //Writes the bytes from the 200x200 map to a pgm file
  freeMemory(map,s,outputFileName);
  fclose(in);
  fclose(out);
}
bool checkSK(char *filename){
  int indexCount = 0;
  //Skips all the characters before the . as the characters after the dot determine the file extension
  while (filename[indexCount] != '.'){
    indexCount++;
 }
  if ((filename[indexCount+1] == 's') && (filename[indexCount+2] == 'k')) return true; //File is sk}
  else if ((filename[indexCount+1] == 'p') && (filename[indexCount+2] == 'g') && (filename[indexCount+3] == 'm')) return false;
  else exit(1);
}

void assert(int line, bool b) {
  if (b) return;
  fprintf(stderr, "ERROR: The test on line %d in test.c fails.\n", line);
  exit(1);
}

void fileExtensionTests(){
  char fileTestName[] = "imperative.pgm";
  bool isSk = checkSK(fileTestName);
  char *outputfileTestName = fileExtension(fileTestName,isSk);
  assert(__LINE__, strcmp(outputfileTestName,"imperative.sk")==0);
  free(outputfileTestName);

  char fileTestName1[] = "functional.pgm";
  isSk = checkSK(fileTestName1);
  outputfileTestName = fileExtension(fileTestName1,isSk);
  assert(__LINE__, strcmp(outputfileTestName,"functional.sk")==0);
  free(outputfileTestName);

  char fileTestName2[] = "squares.sk";
  isSk = checkSK(fileTestName2);
  outputfileTestName = fileExtension(fileTestName2,isSk);
  assert(__LINE__, strcmp(outputfileTestName,"squares.pgm")==0);
  free(outputfileTestName);

  char fileTestName3[] = "football.sk";
  isSk = checkSK(fileTestName3);
  outputfileTestName = fileExtension(fileTestName3,isSk);
  assert(__LINE__, strcmp(outputfileTestName,"football.pgm")==0);
  free(outputfileTestName);
}


void rgbaToGrayscaleTests() {
  state *s = malloc(sizeof(state));
  *s = (state) {0,0,0,0,0,0,1};
  s->data = 0xBBBBBBFF; // binary is 10111011 10111011 10111011 11111111
  assert(__LINE__,rgbaToGrayscale(s) == 187);

  s->data = 0xFFFFFFFF; // binary is 11111111 11111111 11111111 11111111
  assert(__LINE__,rgbaToGrayscale(s) == 255);

  s->data = 0;
  assert(__LINE__,rgbaToGrayscale(s) == 0);

  s->data = 0x646464FF; // binary is 01100100 01100100 01100100 11111111
  assert(__LINE__,rgbaToGrayscale(s) == 100);

  s->data = 0xC8C8C8FF; // binary is 11001000 11001000 11001000 11111111
  assert(__LINE__,rgbaToGrayscale(s) == 200);
  free(s);
}

void testCheckSK(){
  //This function tests that no matter what the length of file is, it is still
  //able to detect .sk or .pgm
  char testName[] = "mathematics.pgm";
  assert(__LINE__, checkSK(testName) == false);

  char testName1[] = "probability.sk";
  assert(__LINE__, checkSK(testName1) == true);

  char testName2[] = "car.sk";
  assert(__LINE__, checkSK(testName2) == true);

  char testName3[] = "music.pgm";
  assert(__LINE__, checkSK(testName3) == false);

  char testName4[] = "mathematics.pgm";
  assert(__LINE__, checkSK(testName4) == false);

  char testName5[] = "garden.pgm";
  assert(__LINE__, checkSK(testName5) == false);

  char testName6[] = "househunting.sk";
  assert(__LINE__, checkSK(testName6) == true);

}

void testObeyDY(){
  //Able to pass NULL as an argument as during testing this argument isnt utilized
  state *s = malloc(sizeof(state));
  *s = (state) {0,0,0,0,0,0,0}; //Initilaised with tool set to NONE
  obeyDY(NULL,s,0xA); //Operand of 10
  assert(__LINE__, s->ty == 10 && s->x == 0);
  obeyDY(NULL,s,0x16); // operand of 22
  assert(__LINE__, s->ty == 32 && s->tx == 0);
  assert(__LINE__, s->y == 32 && s->x == 0);
  obeyDY(NULL,s,0);
  assert(__LINE__, s->ty == 32 && s->tx == 0);
  assert(__LINE__, s->y == 32 && s->x == 0);
  obeyDY(NULL,s,0x5);
  assert(__LINE__, s->ty == 37 && s->tx == 0);
  assert(__LINE__, s->y == 37 && s->x == 0);
  free(s);
}

void testObeyDATA(){
  state *s = malloc(sizeof(state));
  *s = (state) {0,0,0,0,0,0,0};
  obeyDATA(s,0x1E);
  assert(__LINE__, s->data = 94);
  obeyDATA(s,0x02);
  assert(__LINE__, s->data = 6018);
  obeyDATA(s,0x07);
  assert(__LINE__, s->data = 385159);
  s->data = 0;
  obeyDATA(s,0x1F);
  assert(__LINE__, s->data = 1984);
  obeyDATA(s,0x1E);
  assert(__LINE__, s->data = 127006);
  free(s);
}

void testGetOpcode(){
  assert(__LINE__, getOpcode(0xBF) == TOOL);
  assert(__LINE__, getOpcode(0xB6) == TOOL);
  assert(__LINE__, getOpcode(0x74) == DY);
  assert(__LINE__, getOpcode(0xC2) == DATA);
  assert(__LINE__, getOpcode(0xDC) == DATA);
  assert(__LINE__, getOpcode(0x71) == DY);
  assert(__LINE__, getOpcode(0x35) == DX);
  assert(__LINE__, getOpcode(0xA9) == TOOL);
  assert(__LINE__, getOpcode(0x26) == DX);
  assert(__LINE__, getOpcode(0x2F) == DX);
  assert(__LINE__, getOpcode(0x99) == TOOL);
}
void testGetOperand(){
  assert(__LINE__, getOperand(0xAA) == -22);
  assert(__LINE__, getOperand(0xFF) == -1);
  assert(__LINE__, getOperand(0x9D) == 29);
  assert(__LINE__, getOperand(0x1F) == 31);
  assert(__LINE__, getOperand(0xE0) == -32);
  assert(__LINE__, getOperand(0x9E) == 30);
  assert(__LINE__, getOperand(0xB9) == -7);
}

void testObeyTools(){
  state *s = malloc(sizeof(state));
  *s = (state) {0,0,0,0,0,0,0};
  obeyTools(NULL,s,NONE);
  assert(__LINE__, s->tool == NONE);
  obeyTools(NULL,s,LINE);
  assert(__LINE__, s->tool == LINE);
  obeyTools(NULL,s,BLOCK);
  assert(__LINE__, s->tool == BLOCK);
  s->data = 156;
  obeyTools(NULL,s,TARGETX);
  assert(__LINE__, s->tx == 156);
  s->data = 73;
  obeyTools(NULL,s,TARGETY);
  assert(__LINE__, s->ty == 73);
  free(s);
}
int main(int n, char *argv[n]) {
  //run tests
  if (n < 2){
    rgbaToGrayscaleTests();
    fileExtensionTests();
    testCheckSK();
    testObeyDY();
    testObeyDATA();
    testGetOpcode();
    testGetOperand();
    testObeyTools();
    printf("All Tests Passed\n");
  }
  else if (n == 2) {
    char *filename = argv[1];
    bool skOrNot = checkSK(filename);
    if (skOrNot == true){
      readingSKFILE(filename,skOrNot);
    }
    else {
      readingPGMFile(filename,skOrNot);
    }
  }
  else exit(1); // Cant provide more than 2 arguments, not in correct format
  return 1;
}
