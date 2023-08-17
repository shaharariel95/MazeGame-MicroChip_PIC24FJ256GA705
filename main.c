/*
 * File:   Game.c
 * Author: Shahar
 *
 * Created on July, 2023
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

#include "System/system.h"
#include "System/delay.h"
#include "oledDriver/oledC.h"
#include "oledDriver/oledC_colors.h"
#include "oledDriver/oledC_shapes.h"

#include "i2cDriver/i2c1_driver.h"
#include "Accel_i2c.h"


#define TOTAL_CHARACTERS 26
#define CHAR_START 'A'
#define CHAR_END 'Z'
#define MAX_SCORES 5

static int game_status = 1;
static int seconds_elapsed = 0;
static int game_length = 60;
static int score = 1;
static int inverse_flag = 1;
static int s1_press_duration = 0;
static bool is_s1_stopped = 0;


typedef struct{
    int x;
    int y;
}Ball;

typedef struct{
    int orientation; // 0 if horizontal, 1 if vertical
    int x; // start x position
    int y; // start y position
    int length; // length of the wall
} Wall;

typedef struct{
    Wall walls[50];
    int numWalls;
    int finishX;
    int finishY;
} Maze;

typedef struct{
    int prevY;
    int topY;
    int hight;
}Viewport;

typedef struct{
    char name[7];
    int score;
    int maze;
}PlayerScore;

static PlayerScore scores[MAX_SCORES];

void User_Initialize(void) {
    // Initialize LED/Switch IO Direction (TRISx)
    TRISA |= (1 << 11); //S1 button
    TRISA |= (1 << 12); //S2 button
    TRISA &= ~(1 << 8);
    TRISA &= ~(1 << 9);

    // Set RB12 (AN8) as Analog Input
    TRISB |= (1 << 12);
    ANSB |= (1 << 12);

    // Initialize A/D Circuit (AD1CON1)
    AD1CHS = 8;
    AD1CON1 = 0x00; // Clear all bits
    AD1CON1bits.SSRC = 0; 
    AD1CON1bits.FORM = 0; 
    AD1CON1bits.MODE12 = 1; 
    AD1CON1bits.ADON = 1; 

    // Configure AD1CON2
    AD1CON2 = 0;
    
    // Configure AD1CON3
    AD1CON3 = 0x00; // Clear all bits
    AD1CON3bits.ADCS = 0xFF; 
    AD1CON3bits.SAMC = 0x10;
    
    
    //
    oledC_setBackground(OLEDC_COLOR_BLACK);
    oledC_clearScreen();
    i2c1_driver_driver_close();
    i2c1_open();
}

void Timer_initialize(void){
    T1CONbits.TON = 1;
    T1CONbits.TSIDL = 1;
    T1CONbits.TGATE = 0;
    T1CONbits.TCKPS = 2;
    T1CONbits.TCS = 0;
    INTCON2bits.GIE = 1;
    PR1 = 62500;
    IPC0bits.T1IP = 1;
    IEC0bits.T1IE = 1;
    IFS0bits.T1IF = 0;
    return;
}

I2Cerror Acc_Initialize(I2Cerror rc){
    unsigned char id;
    rc = i2cReadSlaveRegister(0x3A, 0, &id);

    if (rc == OK)
        if(id==0xE5){
            1+1;
        }
        else
            errorStop("Acc!Found");
    else
        errorStop("I2C Error");

    rc = i2cWriteSlave(0x3A, 0x2D, 8);
    return rc;
}

void errorStop(char *msg){
    oledC_DrawString(0, 20, 2, 2, msg, OLEDC_COLOR_DARKRED);
    for (;;);
}

void __attribute__((__interrupt__)) _T1Interrupt(void){
    char time_remained[3]="  ";
//    oledC_DrawString(2,2,1,1,time_remained, OLEDC_COLOR_BLACK);
    seconds_elapsed++;
    int seconds_remainning = game_length - seconds_elapsed;
    sprintf(time_remained, "%d", seconds_remainning);
    oledC_DrawRectangle(0,0,20,20,OLEDC_COLOR_BLACK);
    oledC_DrawString(2,2,1,1,time_remained, OLEDC_COLOR_RED);
    if(seconds_remainning < 5){
        if(inverse_flag) {
            oledC_sendCommand(OLEDC_CMD_SET_DISPLAY_MODE_INVERSE,NULL,0);
            inverse_flag = 0;
        }else {
           oledC_sendCommand(OLEDC_CMD_SET_DISPLAY_MODE_ON,NULL,0);
           inverse_flag = 1;
        }
    }
    if(seconds_elapsed >= game_length){
        game_status = 0;  // A function that ends the game
        oledC_sendCommand(OLEDC_CMD_SET_DISPLAY_MODE_ON,NULL,0);
    }
    
    if(!(PORTA & (1<<11))) { // if S1 is pressed
        s1_press_duration++;
        if(s1_press_duration >= 3) {  // 3 seconds when timer interrupt is 10ms
            game_status = 0;  // or any other flag to indicate exit
            is_s1_stopped = 1;
        }
    } else {
        s1_press_duration = 0;
    }
    
    IFS0bits.T1IF = 0;  // Clear the Timer1 interrupt flag
}

void storeScore(char* name, int maze_choise) {
    oledC_clearScreen();
    int i;
    for(i = 0; i < MAX_SCORES; i++) {
        if(score > scores[i].score) {
            break;
        }
    }

    // Shift scores down to make room for the new score
    for(int j = MAX_SCORES-1; j > i; j--) {
        scores[j] = scores[j-1];
    }

    // Store the new score
strncpy(scores[i].name, name, sizeof(scores[i].name)-1);
scores[i].name[sizeof(scores[i].name) - 1] = '\0';  // Ensure null termination
scores[i].score = score;
scores[i].maze = maze_choise;
}

void displayTopScores() {
    // Display top 3 scores
    oledC_DrawString(2,4,1,1,"Top Scores!", OLEDC_COLOR_CYAN);
    for(int i = 0; i < 3 && i < MAX_SCORES; i++) {
        char displayString[30];
        sprintf(displayString, "%s: %d: M:%d", scores[i].name, scores[i].score, scores[i].maze);
        // Display the string on the screen using your display method. Adjust the x and y positions accordingly.
        oledC_DrawString(4, (i+1)*20, 1, 1, displayString, OLEDC_COLOR_WHITE);
    }
    oledC_DrawString(4,80,1,1,"S1 to try again!",OLEDC_COLOR_CYAN);
}

int CheckS1Pressed(){
    static int buttonPressed = 0; // false
    static unsigned long buttonPressCounter = 0;
    if((PORTA & (1<<11)) == 0){
        ++buttonPressCounter;
        if(buttonPressCounter > 400) //DeBouce
            buttonPressed = 1; //true
    }
    else{
        if(buttonPressed == 1 ){
            buttonPressed = 0;
            return 1;
        }
        buttonPressCounter = 0;
    }
    return 0;
}

int CheckS2Pressed(){
    static int buttonPressed = 0; // false
    static unsigned long buttonPressCounter = 0;
    if((PORTA & (1<<12)) == 0){
        ++buttonPressCounter;
        if(buttonPressCounter > 400) //DeBouce
            buttonPressed = 1; //true
    }
    else{
        if(buttonPressed == 1 ){
            buttonPressed = 0;
            return 1;
        }
        buttonPressCounter = 0;
    }
    return 0;
}

int samplePot() {
    AD1CON1bits.SAMP = 1; // start sampling
    DELAY_milliseconds(65);
    AD1CON1bits.SAMP = 0; // end sampling and start conversion
    while (!AD1CON1bits.DONE); // wait for the conversion to complete
    return ADC1BUF0; // return the result 
}

int choose_difficulty(){
    int difficulty = 1;
    oledC_DrawString(2,2,1,1,"Difficulty:", OLEDC_COLOR_CORAL);
    oledC_DrawString(32,16,1,1,"Easy", OLEDC_COLOR_RED);
    oledC_DrawString(32,32,1,1,"Medium", OLEDC_COLOR_WHITE);
    oledC_DrawString(32,48,1,1,"Hard", OLEDC_COLOR_WHITE);
    oledC_DrawString(2,70,1,1,"S1 to cycle.",OLEDC_COLOR_CYAN);
    oledC_DrawString(2,80,1,1,"S2 to Select.",OLEDC_COLOR_CYAN);
    while(!((PORTA & (1<<12)) == 0)){// exit when button 2 is pressed
        
        if((PORTA & (1<<11)) == 0){ // if button 1 is pressed
            difficulty++;
                oledC_DrawString(32,16,1,1,"Easy", OLEDC_COLOR_BLACK);
                oledC_DrawString(32,32,1,1,"Medium", OLEDC_COLOR_BLACK);
                oledC_DrawString(32,48,1,1,"Hard", OLEDC_COLOR_BLACK);
            if(difficulty == 4) difficulty = 1;
//            oledC_clearScreen();
            if(difficulty == 1){
                oledC_DrawString(32,16,1,1,"Easy", OLEDC_COLOR_RED);
                oledC_DrawString(32,32,1,1,"Medium", OLEDC_COLOR_WHITE);
                oledC_DrawString(32,48,1,1,"Hard", OLEDC_COLOR_WHITE);   

            }
            else if(difficulty == 2){
                oledC_DrawString(32,16,1,1,"Easy", OLEDC_COLOR_WHITE);
                oledC_DrawString(32,32,1,1,"Medium", OLEDC_COLOR_RED);
                oledC_DrawString(32,48,1,1,"Hard", OLEDC_COLOR_WHITE);   
            }
            else if(difficulty == 3){
                oledC_DrawString(32,16,1,1,"Easy", OLEDC_COLOR_WHITE);
                oledC_DrawString(32,32,1,1,"Medium", OLEDC_COLOR_WHITE);
                oledC_DrawString(32,48,1,1,"Hard", OLEDC_COLOR_RED);                
            }
            oledC_DrawString(2,70,1,1,"S1 to Cycle.",OLEDC_COLOR_CYAN);
            oledC_DrawString(2,80,1,1,"S2 to Select.",OLEDC_COLOR_CYAN);
        } 
    } 
   oledC_clearScreen();
   return difficulty;
}

int choose_maze(void){
    int Maze = 1;
    oledC_DrawString(2,2,1,1,"Choose Maze:", OLEDC_COLOR_CORAL);
    oledC_DrawString(32,12,1,1,"Maze 1", OLEDC_COLOR_RED);
    oledC_DrawString(32,28,1,1,"Maze 2", OLEDC_COLOR_WHITE);
    oledC_DrawString(32,44,1,1,"Maze 3", OLEDC_COLOR_WHITE);
    oledC_DrawString(32,60,1,1,"Maze 4", OLEDC_COLOR_WHITE);
    oledC_DrawString(2,70,1,1,"S1 to cycle.",OLEDC_COLOR_CYAN);
    oledC_DrawString(2,80,1,1,"S2 to Select.",OLEDC_COLOR_CYAN);
    while(!((PORTA & (1<<12)) == 0)){
        if((PORTA & (1<<11)) == 0){
            Maze++;
            oledC_DrawString(32,12,1,1,"Maze 1", OLEDC_COLOR_BLACK);
            oledC_DrawString(32,28,1,1,"Maze 2", OLEDC_COLOR_BLACK);
            oledC_DrawString(32,44,1,1,"Maze 3", OLEDC_COLOR_BLACK);
            oledC_DrawString(32,60,1,1,"Maze 4", OLEDC_COLOR_BLACK);
            if(Maze == 5) Maze = 1;
            if(Maze == 1){
                oledC_DrawString(32,12,1,1,"Maze 1", OLEDC_COLOR_RED);
                oledC_DrawString(32,28,1,1,"Maze 2", OLEDC_COLOR_WHITE);
                oledC_DrawString(32,44,1,1,"Maze 3", OLEDC_COLOR_WHITE);
                oledC_DrawString(32,60,1,1,"Maze 4", OLEDC_COLOR_WHITE);
            }else if(Maze == 2){
                oledC_DrawString(32,12,1,1,"Maze 1", OLEDC_COLOR_WHITE);
                oledC_DrawString(32,28,1,1,"Maze 2", OLEDC_COLOR_RED);
                oledC_DrawString(32,44,1,1,"Maze 3", OLEDC_COLOR_WHITE);
                oledC_DrawString(32,60,1,1,"Maze 4", OLEDC_COLOR_WHITE);
            }else if(Maze == 3){
                oledC_DrawString(32,12,1,1,"Maze 1", OLEDC_COLOR_WHITE);
                oledC_DrawString(32,28,1,1,"Maze 2", OLEDC_COLOR_WHITE);
                oledC_DrawString(32,44,1,1,"Maze 3", OLEDC_COLOR_RED);
                oledC_DrawString(32,60,1,1,"Maze 4", OLEDC_COLOR_WHITE);
            }else if(Maze == 4){
                oledC_DrawString(32,12,1,1,"Maze 1", OLEDC_COLOR_WHITE);
                oledC_DrawString(32,28,1,1,"Maze 2", OLEDC_COLOR_WHITE);
                oledC_DrawString(32,44,1,1,"Maze 3", OLEDC_COLOR_WHITE);
                oledC_DrawString(32,60,1,1,"Maze 4", OLEDC_COLOR_RED);
            }
        }
    }
    oledC_clearScreen();
    return Maze;
}

Wall initWall(int orientation, int x, int y, int length) {
    Wall wall;
    wall.orientation = orientation;
    wall.x = x;
    wall.y = y;
    wall.length = length;
    return wall;
}

void initMaze(Maze* maze, int maze_number) {
    int maze1_orientations[] = {0, 0, 0, 1, 1, 0, 0, 0, 1, 0, 1, 0, 1, 0, 1, 0, 0, 0, 1};
    int maze1_xs[] = {12, 0, 42, 77, 13, 6, 0, 22, 15, 15, 58, 68, 48, 10, 20, 0, 16, 0, 6};
    int maze1_ys[] = {18, 29, 29, 18, 29, 41, 52, 61, 52, 71, 71, 95, 84, 104, 116, 128, 150, 162, 186};
    int maze1_lengths[] = {84, 35, 35, 43, 12, 63, 27, 55, 19, 54, 90, 28, 108, 38, 22, 30, 32, 34, 6};

    int maze2_orientations[] = {1, 1, 1, 1, 1 };
    int maze2_xs[] = {80, 60, 38, 16, 6};
    int maze2_ys[] = {0, 14, 0, 14, 186};
    int maze2_lengths[] = {180, 180, 180, 180, 6};
    
    int maze3_orientations[] = {0, 0, 0, 0, 0, 0, 1};
    int maze3_xs[] = {14, 0, 14, 0, 14, 0, 6};
    int maze3_ys[] = {14, 38 , 72, 104, 140, 168, 186};
    int maze3_lengths[] = {82, 82, 82, 82, 82, 82, 6};
    
    int maze4_orientations[] = {0, 0, 0, 0, 1, 1, 1, 1, 1};
    int maze4_xs[] = {14, 0, 14, 0, 80, 60, 40, 20, 6};
    int maze4_ys[] = {14, 38, 72, 104, 104, 122, 104, 122, 186};
    int maze4_lengths[] = {82, 82, 82, 82, 66, 66, 66 ,66 ,6};

    int *orientations;
    int *xs;
    int *ys;
    int *lengths;
    int numWalls;

    if (maze_number == 1) {
        orientations = maze1_orientations;
        xs = maze1_xs;
        ys = maze1_ys;
        lengths = maze1_lengths;
        numWalls = 19;
    }

    if (maze_number == 2) {
        orientations = maze2_orientations;
        xs = maze2_xs;
        ys = maze2_ys;
        lengths = maze2_lengths;
        numWalls = 5;
    }
    
    if (maze_number == 3) {
        orientations = maze3_orientations;
        xs = maze3_xs;
        ys = maze3_ys;
        lengths = maze3_lengths;
        numWalls = 7;
    }
    
    if (maze_number == 4) {
        orientations = maze4_orientations;
        xs = maze4_xs;
        ys = maze4_ys;
        lengths = maze4_lengths;
        numWalls = 9;
    }

    for (int i = 0; i < 21; i++) {
        maze->walls[i] = initWall(orientations[i], xs[i], ys[i], lengths[i]);
    }
    
    maze->numWalls = numWalls;
}

void drawWall(Wall wall, int topY) {
    int yStart = wall.y - topY;
    int yEnd = yStart + wall.length;
    if (wall.orientation == 0) { // If the wall is horizontal
        if(!(yStart < 0 || yStart > 96)){
            oledC_DrawRectangle(wall.x, yStart, wall.x + wall.length, yStart, OLEDC_COLOR_WHITE);
        }
    } else { // If the wall is vertical
        if(yEnd < 0) { return; }
        if(yStart < 0) {yStart = 0;}
        oledC_DrawRectangle(wall.x, yStart, wall.x, yEnd, OLEDC_COLOR_WHITE);
    }
}

void eraseWall(Wall wall, int prevY){
    int yStart = wall.y - prevY;
    int yEnd = yStart + wall.length;
    if (wall.orientation == 0) { // If the wall is horizontal
        if(!(yStart < 0)){
            oledC_DrawRectangle(wall.x, yStart, wall.x + wall.length, yStart, OLEDC_COLOR_BLACK);
        }
    } else { // If the wall is vertical
        if(yEnd < 0) { return; }
        if(yStart < 0) {yStart = 0;}
        oledC_DrawRectangle(wall.x, yStart, wall.x, yEnd, OLEDC_COLOR_BLACK);
    }
}

int isColliding(Ball oldBall, int newx, int newy, Wall wall) {
    if (wall.orientation == 0) { // If the wall is horizontal
        return ((oldBall.y <= wall.y && newy >= wall.y) || (oldBall.y >= wall.y && newy <= wall.y)) &&
               ((newx >= wall.x && newx <= wall.x + wall.length) || (oldBall.x >= wall.x && oldBall.x <= wall.x + wall.length));
    } else { // If the wall is vertical
        return ((oldBall.x <= wall.x && newx >= wall.x) || (oldBall.x >= wall.x && newx <= wall.x)) &&
               ((newy >= wall.y && newy <= wall.y + wall.length) || (oldBall.y >= wall.y && oldBall.y <= wall.y + wall.length));
    }
}

void Enter_Name(char* name) {
    int pot = 0;
//    char name[6] = "     "; 
    name[6] = '\0'; 
    int position = 0;
    char letter = 'A';
    oledC_clearScreen();
    oledC_DrawString(2,4,1,1, "Move the Nob", OLEDC_COLOR_CYAN);
    oledC_DrawString(2,70,1,1,"S1 to cycle.",OLEDC_COLOR_CYAN);
    oledC_DrawString(2,80,1,1,"S2 to Save.",OLEDC_COLOR_CYAN);
    while (PORTA & (1<<12)) { // saves the name if S2 is pressed
        if(!(PORTA & (1<<11))) position++; // moves to the next character if S1 is pressed
        if(position > 4) position = 0; // reset for the first letter
        oledC_DrawCharacter(2+position*12, 30, 2, 2, name[position], OLEDC_COLOR_BLACK); // delete the letter from the screen      
        pot = (int)(samplePot() / (float)158); // gets a number between 1 to 26 from the pot
        letter = pot + 'A'; //transform the number from pot to 'A' to 'Z'
        name[position] = letter; // puts the letter in an array
        oledC_DrawCharacter(2+position*12, 30, 2, 2, letter, OLEDC_COLOR_WHITE); // draw the character to the screen
        DELAY_milliseconds(150); // delay to make the blinking of the letter slower
    }
}

void Game_Instructions(void){
    oledC_DrawString(2,2,1,1,"How to Play", OLEDC_COLOR_CYAN);
    oledC_DrawString(2,12,1,1,"1:", OLEDC_COLOR_WHITE);
    oledC_DrawString(2,22,1,1,"move the board", OLEDC_COLOR_WHITE);
    oledC_DrawString(2,32,1,1,"to move the ball", OLEDC_COLOR_WHITE);
    oledC_DrawString(2,44,1,1,"2:", OLEDC_COLOR_WHITE);
    oledC_DrawString(2,54,1,1,"Press S1 for 3", OLEDC_COLOR_WHITE);
    oledC_DrawString(2,64,1,1,"seconds to reset", OLEDC_COLOR_WHITE);
    oledC_DrawString(2,78,1,1,"Press S2 to", OLEDC_COLOR_CYAN);
    oledC_DrawString(2,88,1,1,"continue", OLEDC_COLOR_CYAN);
    while(PORTA & (1<<12)){}
    oledC_clearScreen();
    oledC_DrawString(2,2,1,1,"How to Play", OLEDC_COLOR_CYAN);
    oledC_DrawString(2,12,1,1,"3:", OLEDC_COLOR_WHITE);
    oledC_DrawString(2,22,1,1,"reach the bottom", OLEDC_COLOR_WHITE);
    oledC_DrawString(2,32,1,1,"left to Finish", OLEDC_COLOR_WHITE);
    oledC_DrawString(2,44,1,1,"4:", OLEDC_COLOR_WHITE);
    oledC_DrawString(2,54,1,1,"Don't Let the", OLEDC_COLOR_WHITE);
    oledC_DrawString(2,64,1,1,"Timer to run out", OLEDC_COLOR_WHITE);
    oledC_DrawString(2,78,1,1,"Press S2 to", OLEDC_COLOR_CYAN);
    oledC_DrawString(2,88,1,1,"continue", OLEDC_COLOR_CYAN);
    while(PORTA & (1<<12)){}

}

/*
                         Main application
 */
int main(void)
{
    I2Cerror rc; // for the ACC driver
    int x, y, pot = 0;
    unsigned char xyz[6] = {0}; // array for XYZ from Acc
    SYSTEM_Initialize();
    User_Initialize();
    rc = Acc_Initialize(rc);
    
    Game_Instructions();
    
    Timer_initialize(); // start counting time for the game

    game_length = 62500; // makes sure the game wont end in the menu;
    Ball ball; // initialize ball
    Maze maze;  // initialize the Maze
    Viewport viewport; // initialize the viewport
    for(;;){
        IEC0bits.T1IE = 0;
        oledC_clearScreen();
        char name[6] = "     \0";
        int difficulty = 0;
        float speedMultiplier = 1;
        int willCollideHorizontally = 0;
        int willCollideVertically = 0;
        int old_pot = -1; // set Old potentiometer to -1 to start the maze when the game start
        int pot_diff = 0; // initialize the potentiometer difference to 0
        int maze_choise = 0;
        difficulty = choose_difficulty();
        maze_choise = choose_maze();
        initMaze(&maze, maze_choise); // put the walls of a maze in the Maze
        switch(difficulty) {
            case 1:
                speedMultiplier = 0.8;
                game_length = 75;
                break;
            case 2:
                speedMultiplier = 1.0; // Normal speed
                game_length = 60;
                break;
            case 3:
                speedMultiplier = 1.2;
                game_length = 45;
                break;
            default:
                speedMultiplier = 1.0;
                game_length = 60;
                break;
        }

        ball.x=92; // start the ball in the right side of the screen
        ball.y=4; // start the ball at the top of the screen

        viewport.topY = 0; // start the viewport at the top of the maze
        viewport.hight = 96; // set the size of the viewport to the size of the screen
        viewport.prevY = 0; // initialize the PrevY of the viewport

        seconds_elapsed = 0;
        game_status = 1;
        IEC0bits.T1IE = 1;
        for (;;) {  // start of game loop
            if(game_status == 0){break;} // when timer ends, exit game loop
            for (int i=0 ; i<6 ; ++i) {
                rc=i2cReadSlaveRegister(0x3A, 0x32+i, &xyz[i]); //read the ACC into xyz[]
                DELAY_microseconds(5);            
            }

            pot = samplePot(); // get the potentiometer position between 0 to 4095
            pot = (pot/42.6); // make it from 4096 to 96        
            pot_diff = abs(pot - old_pot); // checks of the potentiometer position changed
            if(pot_diff){
                IEC0bits.T1IE = 0; // turn off the interrupts so the counter wont smear
                for(int i=0; i< maze.numWalls; i++){
                    eraseWall(maze.walls[i], viewport.topY); // if the potentiometer position changed, remove the walls
              }
                if(ball.y > viewport.topY && ball.y < (viewport.topY + viewport.hight))
                    oledC_DrawCircle(ball.x, ball.y - viewport.topY, 2, OLEDC_COLOR_BLACK); // if the ball moved delete the old ball
                viewport.topY = pot; // update the viewport new position from the potentiometer
                for(int i=0; i< maze.numWalls; i++){
                    drawWall(maze.walls[i], viewport.topY); // draw the walls in the new location based of the new viewport location
                }
                if(ball.y > viewport.topY && ball.y < (viewport.topY + viewport.hight))
                    oledC_DrawCircle(ball.x, ball.y - viewport.topY, 2, OLEDC_COLOR_CYAN); 
                IEC0bits.T1IE = 1; // re-enable interrupts
            }  

            if(ball.y < viewport.topY || ball.y > (viewport.topY + viewport.hight)) continue;

            x = -(xyz[0]+xyz[1]*256); // gets the tilt on the x axis
            y = xyz[2]+xyz[3]*256; // gets the tilt on the y axis
    //        z = xyz[4]+xyz[5]*256;


            int newX = (int) (ball.x + ((float)x/50) * speedMultiplier);
            int newY = (int) (ball.y + ((float)y/50) * speedMultiplier);

            if(!((ball.x == newX) && (ball.y == newY)) ){
                oledC_DrawCircle(ball.x, ball.y - viewport.topY, 2, OLEDC_COLOR_BLACK); // if the ball moved delete the old ball

                willCollideHorizontally = 0;
                willCollideVertically = 0;

                for(int i=0; i < maze.numWalls; i++){
                    if (isColliding(ball, newX, ball.y, maze.walls[i])) { // checks for horizontal collisions with walls
                        willCollideHorizontally = 1;
                    }
                    if (isColliding(ball, ball.x, newY, maze.walls[i])) { // checks for vertical collisions with walls
                        willCollideVertically = 1;
                    }
                }

                if (!willCollideHorizontally) { // if there is no horizontal collision
                    ball.x = newX;
                }
                if (!willCollideVertically) { // if there is no vertical collision
                    ball.y = newY;
                }


                if(ball.x < 6 && ball.y > 186) {
                    IEC0bits.T1IE = 0;
                    game_status = 0;
                    score = (game_length - seconds_elapsed)*difficulty;
                    break;
                }
                // make sure the ball is in the boundaries of the maze
                if(ball.x < 4 ) { ball.x = 4;} if(ball.x > 92 ) { ball.x = 92; }  
                if(ball.y < 4 ) { ball.y = 4;} if(ball.y > 188 ) { ball.y = 188; }



                IEC0bits.T1IE = 0; // turn off the interrupts so the counter wont smear
                if(newY > viewport.topY && newY < (viewport.topY + viewport.hight)) { 
                    oledC_DrawCircle(ball.x, ball.y - viewport.topY, 2, OLEDC_COLOR_CYAN); }
                IEC0bits.T1IE = 1; // re-enable interrupts
            }

    //        DELAY_milliseconds(20);      
        }
        IEC0bits.T1IE = 0;
        oledC_clearScreen();
        if(!is_s1_stopped){
            Enter_Name(name);
            storeScore(name,maze_choise);
        }
        oledC_clearScreen();
        displayTopScores();
        while(PORTA & (1<<11)){}
    }
}
/**
 End of File
*/
