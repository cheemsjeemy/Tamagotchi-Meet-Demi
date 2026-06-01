# Demi Tamagotchi - Development Plan

## Core Stats System (✅ Implemented)
- **Hunger**: Decreases over time, feed Demi to restore
- **Happiness**: Increases with interaction, decreases if ignored
- **Energy**: Decreases over time, replenished by letting Demi sleep
- **Health**: Affected by hunger and energy levels, decreases if neglected
- **Cleanliness**: Decreases over time, clean Demi to restore
- **Weight**: Increases from snacks, decreases from playing/exercise (🆕 New)
- **Discipline**: Increases when scolding misbehavior, affects personality (🆕 New)

## Phase 1: Sleep/Wake Day-Night Cycle (✅ Completed)

### 1.0 DS3231 RTC Hardware Details
- **Stores**: Full date + time (year up to 2099, month, day, hour, minute, second)
- **Coin Cell Battery (CR2032)**: Critical for power-loss survival
  - ✅ With battery: Keeps perfect time across power cycles
  - ❌ No battery/Dead battery: `rtc.lostPower()` returns true, resets to 2024-01-01
- **`getUnixTime()`**: Returns **full Unix timestamp** (seconds since 1970-01-01)
  - Does NOT wrap at 24:00 — keeps counting up forever
  - Automatically accounts for days passed (e.g., 3 days = +259200 seconds)

### 1.1 RTC Time Validation & Sync
- [x] Add `rtcTimeAvailable` flag in `RTCHandler` or `DemiHandler`
- [x] On boot: Check RTC validity:
  - Use `rtc.lostPower()` to detect battery/reset issues
  - Check if year > 2025 (we're in 2026, so 2024 = invalid)
- [x] If RTC invalid: Try WiFi NTP sync via `WiFiHandler`
  - `syncRTCFromUnixTime(ntpTime)` to update RTC
- [x] If NTP fails: Set `rtcTimeAvailable = false` → Demi runs on energy-only mode
- [x] If RTC valid or NTP succeeds: `rtcTimeAvailable = true` → Demi follows day/night cycle

### 1.5 Power-Loss Survival & Stat Simulation
- [x] **Save timestamp on every stat change**:
  - Add `lastSaveTime` to NVS Preferences
  - Update on every stat drop + auto-save (every 5th drop)
- [x] **On boot (`loadAll()`)**:
  - Read `lastSaveTime` from NVS
  - Get `currentTime = getUnixTime()`
  - Calculate `timePassed = currentTime - lastSaveTime`
- [x] **Simulate stat drops for `timePassed`**:
  - Cap `timePassed` to max 30 days (prevent negative stats)
  - Apply drain: `hunger -= (timePassed / STAT_UPDATE_INTERVAL) * HUNGER_DROP_AMOUNT`
  - Same for energy, happiness, cleanliness
  - Health only drops if survival stats are critical during the gap
- [x] **Edge case**: If `lastSaveTime == 0` (first boot) → skip simulation, use defaults
- [x] **Stat subtraction prints**: Shows drop amounts on Serial

### 1.2 Sleep Triggers
- [x] **Automatic Sleep**:
  - Energy ≤ 20% → Demi gets `MOOD_TIRED`, then auto-sleeps
  - RTC valid && local time ≥ 21:00 (9 PM) → Demi gets sleepy, auto-sleeps around 22:00
- [x] **Manual Sleep**: User selects "Sleep" from menu anytime
- [x] **Sleep Animation**: Use `sprite_sleep.h` (SLEEP_1, SLEEP_2 frames)
- [x] **Stats Frozen**: During sleep, only energy restores; hunger/happiness/cleanliness unchanged

### 1.3 Wake-Up Triggers
- [x] **Automatic Wake**:
  - RTC valid && local time 06:00-08:00 && energy ≥ 100% → Auto wake-up
  - RTC invalid && energy ≥ 100% → Wake up (energy-only mode)
- [x] **Manual Wake**: Any button press if energy ≥ 50%
  - If energy < 50%: Show "too sleepy..." message or ignore
- [x] **Local Timezone Support**: Uses `LocalTimeZone` + `timezones[]` table dynamically

### 1.4 Implementation Files
- [x] `DemiHandler.cpp`: Modify `updateStats()` for sleep/wake logic
- [x] `main.cpp`: Update button handler for manual wake
- [x] `RTCHandler.cpp`: Add time validation function
- [x] `WiFiHandler.cpp`: Ensure NTP sync attempts on invalid RTC

---

## Phase 2: Sleep Deprivation Mechanic

### 2.1 Stats Frozen During Sleep
- [x] During sleep, only energy restores; hunger, happiness, cleanliness unchanged (already implemented)

### 2.2 Sleep Debt Tracking
- [ ] Record `sleepStartTime` (time_t) when Demi goes to sleep
- [ ] On wake, calculate `sleepDuration = currentTime - sleepStartTime;`
- [ ] Add `sleepDebt` (int) to track health lost due to early wake
- [ ] Add `sleepDebtHealth` (int) to track health drop amount from sleep deprivation

### 2.3 Early Wake Penalty (Manual Wake Only)
- [ ] If manual wake && sleepDuration < 5 hours:
  - Health drops by ~20 (or formula: `health -= (5 - hoursSlept) * 4`)
  - Record this drop as `sleepDebtHealth = healthDrop;`
  - This debt is **separate** from other health loss (hunger/energy critical)
- [ ] If auto-wake (energy full) → no penalty (natural wake)
- [ ] If manual wake && sleepDuration >= 5 hours → no penalty (proper sleep)

### 2.4 Recovery of Sleep Debt
- [ ] If Demi later gets proper sleep (sleep duration ≥ 5 hours):
  - Recover **only** the `sleepDebtHealth` portion (maybe set health = 100 or add back debt)
  - If full proper sleep, maybe set health to 100 (as user said "recover to 100 or something")
- [ ] Partial recovery: If sleep deprivation debt exists, only that portion can be recovered over time
- [ ] If health dropped due to other causes (e.g., hunger), that part is NOT automatically recovered by sleep

### 2.5 Sleep Debt Display
- [ ] Maybe show "ZZZ" icon with red tint when in debt
- [ ] Or show sleep debt hours:minutes remaining
- [ ] Possibly show debt in stats screen

---

## Phase 3: Random Events System

### 3.1 Demi Needs Attention
- [ ] "Play with me!" popup when happiness < 30%
- [ ] "I'm hungry..." when hunger < 25%
- [ ] "I need sleep..." when energy < 25%
- [ ] "Clean me!" when cleanliness < 30%

### 3.2 Positive Events
- [ ] Demi finds a snack (+10 hunger)
- [ ] Demi discovers a toy (+15 happiness)
- [ ] Random happiness boost when stats are all > 70%

### 3.3 Negative Events (Neglect)
- [ ] **Sick State**: Trigger when hunger < 15% OR energy < 15%
  - Health starts dropping
  - Add `MOOD_SICK` state
  - Add sick sprite animation
- [ ] **Poop Events (🆕 New)**:
  - Random poop appears every 2-4 hours (simulated time)
  - Must clean poop within 30 minutes or happiness drops -10%
  - Cleaning poop restores cleanliness +5
  - If ignored: Demi gets dirty faster, health -5
- [ ] **Medicine (🆕 New)**:
  - Item in inventory to cure sickness
  - Using medicine: health +30, removes sick state
  - Separate from "clean Demi" action
- [ ] **Critical State**: Trigger when health < 20%
  - Flashing alert on screen
  - Need immediate care

### 3.4 Event Timing
- [ ] Check every 5 stat updates (~100 seconds)
- [ ] 30% chance for positive event if stats are good
- [ ] 60% chance for negative event if stats are bad

### 3.5 Discipline System (🆕 New)
- [ ] **Scolding**: When Demi misbehaves (poops, gets sick from neglect)
  - Press scold button → discipline +10
  - Too much scolding → happiness -20
- [ ] **Discipline Meter**: 0-100 scale
  - High discipline: Demi evolves better, fewer poop events
  - Low discipline: More random mischief

---

## Phase 4: Visual Feedback & Sprites

### 4.1 Mood-Based Sprites (✅ Partially Done)
- [x] Happy Sprite: When happiness is high
- [x] Alert Sprite: When needs attention
- [x] Sleeping Sprite: When Demi is sleeping (just added!)
- [ ] Sad Sprite: When happiness is low
- [ ] Sick Sprite: When Demi is unwell
- [ ] Dirty Sprite: When cleanliness is low
- [ ] Excited Sprite: When all stats > 80%
- [ ] Poop Sprite: Show poop on screen when event triggers

### 4.2 Weight & Discipline Display
- [ ] Show weight stat on screen (next to other stats)
- [ ] Show discipline meter (progress bar style)
- [ ] Weight affects sprite appearance (thicker when overweight)

### 4.3 Cosmetic Changes
- [ ] Add `sprite_Demi_Cosmetics.h` support
- [ ] Hat/hair/accessories based on personality type
- [ ] Color changes for NeoPixel LED based on mood

### 4.4 Evolution System (Future)
- [ ] Demi evolves based on care history
- [ ] Different forms: Baby → Child → Adult → Elder
- [ ] Evolution triggers: total cares, personality type, stat averages

---

## Phase 5: Sound Effects

### 5.1 Basic Sounds
- [ ] Happy beep when Demi is fed or played with (high pitch, short)
- [ ] Sad beep when Demi is ignored or stats are low (low pitch, long)
- [ ] Sleep sound when Demi goes to sleep (gradual fade)
- [ ] Wake-up sound when Demi wakes up (rising tone)

### 5.2 Alert Sounds
- [ ] Warning beep when stats < 25%
- [ ] Critical alarm when health < 20%
- [ ] Success chime when all stats > 80%

---

## Phase 6: Mini-Games (Future)

### 6.1 Catch the Falling Object
- [ ] Use buttons to move basket/catcher
- [ ] Objects fall at increasing speed
- [ ] Reward: +20 happiness, -10 energy

### 6.2 Memory Game
- [ ] Demi plays a sequence of beeps
- [ ] User repeats the sequence via buttons
- [ ] Reward: +15 happiness, +10 intelligence (future stat?)

### 6.3 Reaction Test
- [ ] Demi shows a color/icon
- [ ] User must press correct button quickly
- [ ] Reward: +10 happiness, energy bonus if fast

---

## Phase 7: Advanced Features (Future)

### 7.1 WiFi Connectivity
- [ ] NTP time sync (already partially implemented)
- [ ] OTA updates
- [ ] Cloud save/load stats
- [ ] Share Demi's status on social media?

### 7.2 AI Personality (✅ Implemented)
- [x] TinyML mood prediction
- [ ] Personality types affect mini-game difficulty
- [ ] Learning from user interaction patterns

### 7.3 Settings & Customization
- [ ] Name your Demi
- [ ] Choose wake/sleep time preferences
- [ ] Select LED brightness/color themes
- [ ] Mute/unmute sound effects
- [ ] Time Format (12h/24h toggle display)

---

## Implementation Priority Order

1. **Phase 1**: Sleep/Wake Cycle (✅ Completed)
2. **Phase 2**: Sleep Deprivation Mechanic (New)
3. **Phase 3**: Random Events System (Poop, Medicine, Discipline)
4. **Phase 4**: Visual Feedback (Sick/Dirty/Poop sprites, Weight & Discipline display)
5. **Phase 5**: Sound Effects
6. **Phase 6**: Mini-Games (earning mechanism for Shop)
7. **Phase 7**: Advanced Features (Settings, TimeFormat)

---

## Notes
- Always build before uploading
- Test sleep/wake transitions manually
- Ensure RTC fallback works when WiFi is unavailable
- Keep sprite animations smooth (avoid jarring transitions)
- Energy should restore faster during sleep than it drains during wake
- Sleep deprivation penalty only applies to manual wake before 5 hours
