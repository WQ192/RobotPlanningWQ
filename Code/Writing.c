#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

// Functions
void readFontData(const char *fileName);
float getTextHeight(float userHeight);
void readTextFile(const char *fileName);
void generateGcode(char letter, float xOffset, float yOffset, FILE *outputFile);

#define MAX_BUFFER_SIZE 256

// Global data
float scaleFactor = 1.0;  // Scaling factor for font size
float xPosition = 0.0;    // Current x position
float yPosition = 0.0;    // Current y position

int main() {
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

    // Read font data
    printf("Loading font data...\n");
    readFontData(fontFile);

    // Get the text file name from the user
    printf("Enter the name of the text file to draw: ");
    scanf("%s", textFile);

    // Process the text file
    printf("Processing text file...\n");
    readTextFile(textFile);

    printf("Done.\n");
    return 0;
}

// Reads the font data from a file
void readFontData(const char *fileName) {
    FILE *file = fopen(fileName, "r");
    if (file == NULL) {
        perror("Error opening font file");
        exit(1);
    }

    char line[MAX_BUFFER_SIZE];
    while (fgets(line, MAX_BUFFER_SIZE, file)) {
        // Load and store font data
        printf("Font data: %s", line); 
    }
    fclose(file);
}

// Calculates and returns the scaling factor based on the text height
float getTextHeight(float userHeight) {
    float baseHeight = 18.0;  // Base height from font file units
    return userHeight / baseHeight;
}

// Reads the text file and generates G-code for each word
void readTextFile(const char *fileName) {
    FILE *file = fopen(fileName, "r");
    if (file == NULL) {
        perror("Error opening text file");
        exit(1);
    }

    FILE *outputFile = fopen("gcode_output.txt", "w");
    if (outputFile == NULL) {
        perror("Error creating output file");
        fclose(file);
        exit(1);
    }

    char word[MAX_BUFFER_SIZE];
    while (fscanf(file, "%s", word) != EOF) {
        printf("Processing word: %s\n", word);
        float xOffset = xPosition;
        float yOffset = yPosition;

        for (size_t i = 0; i < strlen(word); i++) {
            generateGcode(word[i], xOffset, yOffset, outputFile);
            xOffset += scaleFactor * 6;  // Increment x offset for next character
        }
        yPosition -= scaleFactor * 10;  // Increment y position for the next line
    }

    fclose(file);
    fclose(outputFile);
}

void generateGcode(char letter, float xOffset, float yOffset, FILE *outputFile) {
   
    float letterWidth = scaleFactor * 6;  

    // Move to the starting position of the letter
    fprintf(outputFile, "G0 X%.2f Y%.2f ; Move to start of '%c'\n", xOffset, yOffset);

    // Lower the pen
    fprintf(outputFile, "G1 Z-1.0 ; Lower pen\n");

    // Generate G-code for drawing the letter
    // Here we just move to the right across the width of the letter for demonstration
    fprintf(outputFile, "G1 X%.2f Y%.2f ; Draw '%c'\n", xOffset + letterWidth, yOffset, letter);

    // Raise the pen after drawing
    fprintf(outputFile, "G1 Z1.0 ; Raise pen\n");

    // Print status for debugging
    printf("Generated G-code for '%c' at (%.2f, %.2f)\n", letter, xOffset, yOffset);
}

