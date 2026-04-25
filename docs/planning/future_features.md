# EmoBot Future Features

## 1. Feed Treats
*   **Action**: Button in Web UI.
*   **Result**: Show cookie icon. Mood [HAPPY].
*   **Code**: `POST /feed`. Update mood state.

## 2. Time Wake (No Sensor)
*   **Logic**: Use system clock.
*   **Day**: 08:00 to 22:59. Mood [NORMAL].
*   **Night**: 23:00 to 07:59. Mood [SLEEP].
*   **Code**: `datetime.now().hour` check in `idle_loop`.

## 3. Dance Music
*   **Action**: Pet move to sound.
*   **Tech**: Use `pyaudio` or `librosa`.
*   **Logic**: Check volume peaks. If high, trigger [DANCE].

## 4. Show Songs
*   **Source**: Web browser / System player.
*   **Tech**: Use `dbus-python` (MPRIS) for Linux.
*   **Result**: Get song title. Scroll on LED. [SING] mood.

## 5. Word of Day
*   **Source**: Dictionary API.
*   **Action**: Random word every morning.
*   **Result**: Scroll word + meaning. Mood [STARS].

## 6. Sit Straight
*   **Trigger**: Timer (30-60 mins).
*   **Action**: Notification + Shout.
*   **Message**: "Fix back!" or "Sit straight!".
*   **Mood**: [ANGRY] or [ANNOYED].
