#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#define MAC

#ifdef MAC
#include <unistd.h>
#else
#include <Windows.h>
#include "rs232.h"
#include <conio.h>
#endif

#include "serial.h"

#define bdrate 115200               /* 115200 baud */

#define MAX_BUFFER_SIZE 256
#define LINE_SPACING 20.0  // 20mm between lines
#define CHAR_WIDTH 6.0     // Default character width in font file units
#define BASE_HEIGHT 18.0   // Base height of font characters in font file units
#define LETTER_SPACING 8.0 // Spacing between letters in mm
#define MAX_GCODE_BUFFER_SIZE 10240 // Maximum size for the accumulated G-code buffer

// Global variables
float scaleFactor = 1.0;  // Scaling factor for font size
float xPosition = 0.0;    // Current x position
float yPosition = 0.0;    // Current y position
char gcodeBuffer[MAX_GCODE_BUFFER_SIZE] = {0}; // Initialize the buffer to zero
int bufferIndex = 0;

// Function prototypes
void readFontData(const char *fileName);
float getTextHeight(float userHeight);
void readTextFile(const char *fileName);
void generateGcode(char letter, float xOffset, float yOffset, char *gcodeBuffer, int *bufferIndex);
void resetPenToOrigin(char *gcodeBuffer, int *bufferIndex);

void SendCommands (char *buffer );

typedef struct {
    int asciiCode;
    int numMovements;
    float *dx;
    float *dy;
    int *penState;
} FontCharacter;

FontCharacter fontData[128]; // Array to store font data for ASCII characters

int main()
{
    char buffer[100];

    // If we cannot open the port then give up immediatly
    if ( CanRS232PortBeOpened() == -1 )
    {
        printf ("\nUnable to open the COM port (specified in serial.h) ");
        exit (0);
    }

    // Time to wake up the robot
    printf ("\nAbout to wake up the robot\n");

    // We do this by sending a new-line
    sprintf (buffer, "\n");
    // printf ("Buffer to send: %s", buffer); // For diagnostic purposes only, normally comment out
    PrintBuffer (&buffer[0]);
    #ifdef MAC
    sleep(0.1);
    #else
    Sleep(100);
    #endif


    // This is a special case - we wait  until we see a dollar ($)
    WaitForDollar();

    printf ("\nThe robot is now ready to draw\n");

    //These commands get the robot into 'ready to draw mode' and need to be sent before any writing commands
    sprintf (buffer, "G1 X0 Y0 F1000\n");
    SendCommands(buffer);
    sprintf (buffer, "M3\n");
    SendCommands(buffer);
    sprintf (buffer, "S0\n");
    SendCommands(buffer);

     // Font data and text input
    char fontFile[] = "SingleStrokeFont.txt";
    char textFile[MAX_BUFFER_SIZE];
    float userHeight;

    printf("Enter text height (between 4mm and 10mm): ");
    scanf("%f", &userHeight);

    if (userHeight < 4.0 || userHeight > 10.0) {
        printf("Error: Height must be between 4mm and 10mm.\n");
        return 1;
    }

    scaleFactor = getTextHeight(userHeight);

    // Load font data
    printf("Loading font data...\n");
    readFontData(fontFile);

    // Get the text file name
    printf("Enter the name of the text file to draw: ");
    scanf("%s", textFile);

    // Process the text file
    printf("Processing text file...\n");
    readTextFile(textFile);

    // Reset pen to origin at the end
    resetPenToOrigin(gcodeBuffer, &bufferIndex);

    // Print all accumulated G-code commands at once
    printf("G-code for the robot:\n%s", gcodeBuffer);
    
    // Before we exit the program we need to close the COM port
    CloseRS232Port();
    printf("Com port now closed\n");

    return (0);
}

void readFontData(const char *fileName) {
    FILE *file = fopen(fileName, "r");
    if (file == NULL) {
        perror("Error opening font file");
        exit(1);
    }

    char line[MAX_BUFFER_SIZE];
    while (fgets(line, MAX_BUFFER_SIZE, file)) {
        int asciiCode, numMovements;
        if (sscanf(line, "999 %d %d", &asciiCode, &numMovements) == 2) {
            FontCharacter *character = &fontData[asciiCode];
            character->asciiCode = asciiCode;
            character->numMovements = numMovements;
            character->dx = malloc(numMovements * sizeof(float));
            character->dy = malloc(numMovements * sizeof(float));
            character->penState = malloc(numMovements * sizeof(int));

            for (int i = 0; i < numMovements; i++) {
                if (fgets(line, MAX_BUFFER_SIZE, file)) {
                    sscanf(line, "%f %f %d", &character->dx[i], &character->dy[i], &character->penState[i]);
                }
            }
        }
    }

    fclose(file);
}

float getTextHeight(float userHeight) {
    return userHeight / BASE_HEIGHT;
}

void readTextFile(const char *fileName) {
    FILE *file = fopen(fileName, "r");
    if (file == NULL) {
        perror("Error opening text file");
        exit(1);
    }

    char word[MAX_BUFFER_SIZE];
    float xOffset = 0.0;
    float yOffset = yPosition;

    while (fscanf(file, "%s", word) != EOF) { // Read one word at a time
        // Calculate the width of the word
        float wordWidth = 0.0;
        for (size_t i = 0; i < strlen(word); i++) {
            wordWidth += scaleFactor * (CHAR_WIDTH + LETTER_SPACING);
        }

        // Check if the word fits in the current line
        if (xOffset + wordWidth > 100.0) { // 100mm is the maximum width
            // Move to the next line
            xOffset = 0.0;
            yOffset -= LINE_SPACING;
        }

        // Draw the word
        for (size_t i = 0; i < strlen(word); i++) {
            generateGcode(word[i], xOffset, yOffset, gcodeBuffer, &bufferIndex);
            xOffset += scaleFactor * (CHAR_WIDTH + LETTER_SPACING); // Move to next character
        }

        // Add a space after the word
        xOffset += scaleFactor * LETTER_SPACING;
    }

    fclose(file);
}

void generateGcode(char letter, float xOffset, float yOffset, char *gcodeBuffer, int *bufferIndex) {
    FontCharacter *character = &fontData[letter];
    char tempBuffer[MAX_BUFFER_SIZE];
    int penState = 0;

    for (int i = 0; i < character->numMovements; i++) {
        float scaledX = xOffset + character->dx[i] * scaleFactor;
        float scaledY = yOffset + character->dy[i] * scaleFactor;

        // Check if pen state needs to change
        if (character->penState[i] != penState) {
            sprintf(tempBuffer, "S%d\n", character->penState[i] ? 1000 : 0);
            sprintf(gcodeBuffer + *bufferIndex, "%s", tempBuffer);
            *bufferIndex += strlen(tempBuffer);
            penState = character->penState[i];
        }

        // Generate movement command
        sprintf(tempBuffer, "G%d X%.2f Y%.2f\n", character->penState[i] ? 1 : 0, scaledX, scaledY);
        sprintf(gcodeBuffer + *bufferIndex, "%s", tempBuffer);
        *bufferIndex += strlen(tempBuffer);
    }
}

void resetPenToOrigin(char *gcodeBuffer, int *bufferIndex) {
    char tempBuffer[MAX_BUFFER_SIZE];

    // Reset pen state to up and move to origin
    sprintf(tempBuffer, "S0\n");
    sprintf(gcodeBuffer + *bufferIndex, "%s", tempBuffer);
    *bufferIndex += strlen(tempBuffer);

    sprintf(tempBuffer, "G0 X0 Y0\n");
    sprintf(gcodeBuffer + *bufferIndex, "%s", tempBuffer);
    *bufferIndex += strlen(tempBuffer);
}

// Send the data to the robot - note in 'PC' mode you need to hit space twice
// as the dummy 'WaitForReply' has a getch() within the function.
void SendCommands (char *buffer )
{
    // printf ("Buffer to send: %s", buffer); // For diagnostic purposes only, normally comment out
    PrintBuffer (&buffer[0]);
    WaitForReply();
    #ifdef MAC
    sleep(0.1);
    #else
    Sleep(100); // Can omit this when using the writing robot but has minimal effect
    #endif
}
