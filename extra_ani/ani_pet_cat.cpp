/**
 * PURE VISUAL PET: CYBER-CAT (Inverse Style)
 * Location: extra_ani/ani_pet_cat.cpp
 * * Features: 
 * - Negative space eyes (Void eyes in red face)
 * - Procedural tail flicking
 * - Walking animation with leg cycles
 * - Random "mood" swings
 */

#include <Arduino.h>
#include <MD_MAX72xx.h>

// Assuming these are defined in your main project
extern MD_MAX72XX mx;
extern bool isInverse;

// Pet specific variables
int catX = 12;
int catTargetX = 12;
int catState = 0; // 0: Sit, 1: Walk, 2: Sleep
int catTimer = 0;
bool legFlip = false;

void drawCyberCat(int x, int state, bool flick) {
    // 1. Draw Body (Row 5-6)
    for (int c = x + 1; c < x + 5; c++) mx.setPoint(6, c, !isInverse);
    for (int c = x + 2; c < x + 6; c++) mx.setPoint(5, c, !isInverse);

    // 2. Draw Head (Row 3-6)
    for (int r = 4; r < 7; r++) {
        for (int c = x + 5; c < x + 9; c++) mx.setPoint(r, c, !isInverse);
    }
    // Ears
    mx.setPoint(3, x + 5, !isInverse);
    mx.setPoint(3, x + 8, !isInverse);

    // 3. Eyes (The "Void" points)
    mx.setPoint(5, x + 6, isInverse);
    mx.setPoint(5, x + 8, isInverse);

    // 4. Tail & Legs Animation
    if (state == 1) { // WALKING
        if (flick) { // Leg Cycle A
            mx.setPoint(7, x + 2, !isInverse);
            mx.setPoint(7, x + 6, !isInverse);
            mx.setPoint(4, x, !isInverse); // Tail Mid
        } else { // Leg Cycle B
            mx.setPoint(7, x + 1, !isInverse);
            mx.setPoint(7, x + 5, !isInverse);
            mx.setPoint(3, x, !isInverse); // Tail High
        }
    } else if (state == 2) { // SLEEPING
        mx.clear(); // Overridden by main loop clear, but here for logic
        for (int c = x; c < x + 6; c++) mx.setPoint(7, c, !isInverse);
        if (flick) mx.setPoint(5, x + 6, !isInverse); // "Z"
    } else { // SITTING
        mx.setPoint(7, x + 2, !isInverse);
        mx.setPoint(7, x + 6, !isInverse);
        if (flick) mx.setPoint(4, x, !isInverse); // Tail flick
        else mx.setPoint(5, x, !isInverse);
    }
}

void updatePetLogic() {
    if (catTimer <= 0) {
        catState = random(0, 3); // Randomly choose Sit, Walk, or Sleep
        catTimer = random(30, 100); 
        if (catState == 1) catTargetX = random(0, 23);
    }
    catTimer--;

    if (catState == 1) { // Logic for walking
        if (catX < catTargetX) catX++;
        else if (catX > catTargetX) catX--;
        else catState = 0;
    }

    legFlip = !legFlip;
    drawCyberCat(catX, catState, legFlip);
}