# EmoBot Extra Expressions — Detailed Animation Spec

Each expression describes exact pixel behavior on an 8x32 LED grid (two 8x6 eye zones).

---

## 1. DIZZY (ID: 19)
- **Motion**: 2x2 pupil orbits in a circle inside each eye zone
- **Math**: `x = sin(t/200)*2`, `y = cos(t/200)*2` — fast spin
- **Speed**: ~5 revolutions/sec
- **Trigger**: High CPU, rapid commands, or manual

## 2. LOVE (ID: 20)
- **Shape**: Heart pattern per eye (replace rectangle)
  ```
  .##.
  ####
  ####
  .##.
  ..#.
  ```
- **Motion**: Heart pulses (scale 0.8→1.2) using `sin(t/300)`
- **Trigger**: Praise, feed action, compliments

## 3. CRY (ID: 21)
- **Base**: SAD eyes (droopy corners removed)
- **Extra**: 1-pixel "tear" drops from bottom of each eye
- **Motion**: Tear falls row 4→7, resets. Staggered per eye (left leads by 200ms)
- **Speed**: One drop every 1.6s per eye
- **Trigger**: Network down, low battery, sad chat

## 4. THINK (ID: 22)
- **Shape**: Normal 4x5 rectangle eyes
- **Motion**: Eyes dart rapidly left→right→left using `sin(t/300)*3`
- **Extra**: Small dot (1px) appears above right eye, toggles on/off every 500ms (thought bubble)
- **Trigger**: AI processing, complex question

## 5. SNEEZE (ID: 23)
- **Phase 1** (0-1.5s): Eyes squint to 3-row height, whole face jitters ±2px horizontally every 50ms (buildup)
- **Phase 2** (1.5-2.0s): Eyes disappear, 8-10 random pixels scatter across full 8x32 grid (burst)
- **Cycle**: 2s total, repeats
- **Trigger**: Random "dust bunny" event, idle quirk

## 6. WINK (ID: 24)
- **Left eye**: Normal 4x5 rectangle with drift
- **Right eye**: Closed line (1 row at y=4, 4px wide) — permanent wink
- **Motion**: Left eye still drifts/blinks normally
- **Trigger**: Jokes, naughty comments, witty replies

## 7. DEEP_SLEEP (ID: 25) — Future
- **Eyes**: Fully closed (1px line each)
- **Extra**: "Zzz" text floats upward from right side
- **Brightness**: Auto-dim to level 1
- **Trigger**: 1+ hour idle

---

## Implementation Status

| Expression | Web Grid | ESP32 C++ | Mood ID |
|------------|----------|-----------|---------|
| DIZZY      | ✅        | ❌         | 19      |
| LOVE       | ✅        | ❌         | 20      |
| CRY        | ✅        | ❌         | 21      |
| THINK      | ✅        | ❌         | 22      |
| SNEEZE     | ✅        | ❌         | 23      |
| WINK       | ✅        | ❌         | 24      |
| DEEP_SLEEP | ❌        | ❌         | 25      |
