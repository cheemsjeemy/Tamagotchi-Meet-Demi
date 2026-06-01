# Demi Tamagotchi - Recommendations Plan

## Future Considerations (Not Immediately Planned)

---

## 1. Memories System (New Section)

### 1.1 Achievements
- [ ] **Care History Tracking**:
  - Total days survived
  - Total feeds, plays, cleans
  - Longest streak of 100% stats
- [ ] **Achievements List**:
  - "First Feed" (feed Demi 1st time)
  - "Week Survivor" (survive 7 days)
  - "Perfect Care" (24 hours with all stats > 80%)
  - "Early Bird" (wake up at 06:00 sharp)
  - "Gourmet" (feed Demi 50 times)
  - "Playful" (play 30 times)
  - "Neat Freak" (clean 40 times)
- [ ] **Display**: Show achievements in Settings > Memories

### 1.2 Care Mistakes Tracking
- [ ] **Mistake Counter**:
  - Increment when ignoring stat < 20% for > 1 hour
  - Reset when user provides excellent care for 24 hours
- [ ] **Mistake Types**:
  - Hunger mistakes (ignored hunger < 20%)
  - Energy mistakes (ignored energy < 20%)
  - Cleanliness mistakes (ignored cleanliness < 20%)
  - Health mistakes (ignored health < 20%)
- [ ] **Impact**:
  - High mistakes → Future character variety unlocks "Neglected" type
  - Low mistakes → Unlocks "Perfect Care" character
- [ ] **Display**: Show in Settings > Memories > Care History

### 1.3 Attention Calls (Moved from braindump)
- [ ] **Beep Patterns**:
  - Single beep: One stat < 25%
  - Double beep: Two stats < 25%
  - Rapid beeping: Health < 20% (critical)
- [ ] **Visual Cues**:
  - "!" icon appears next to Demi
  - Flashing border on screen
  - "HELP ME!" popup for critical states
- [ ] **Response Tracking**:
  - How quickly user responds → affects care rating
  - Slow responses increment mistake counter
  - Quick responses → happiness +5 bonus

---

## 2. Item Inventory System (Detailed)

### 1.1 Inventory Structure
- [ ] Create `Item` struct with:
  - `itemType` (SNACK, TOY, MEDICINE, ACCESSORY, COSMETIC)
  - `itemName` (string)
  - `quantity` (int, 0-99)
  - `effectType` (HUNGER, HAPPINESS, ENERGY, HEALTH, CLEANLINESS)
  - `effectValue` (int, positive or negative)
  - `iconBitmap` (const unsigned char* for display)

### 1.2 Starting Inventory
- [ ] 3x Snacks (hunger +15, happiness +5)
- [ ] 1x Toys (happiness +20, energy -10)
- [ ] 2x Medicine (health +30, removes sick state)
- [ ] 0x Accessories (unlock via achievements)

### 1.3 Inventory UI
- [ ] New menu section: "Items" (after Demi children)
- [ ] Grid display: 4 items per page, scrollable
- [ ] Select item → "Use" or "Discard" options
- [ ] Show quantity badge on each item icon

### 1.4 Item Acquisition
- [ ] Random item drops from playing mini-games
- [ ] "Gift" from Demi after 24 hours of perfect care
- [ ] Unlock accessories via achievements (Phase 6.4)

### 1.5 Item Limits
- [ ] Max 20 items total in inventory
- [ ] Max 99 per item type
- [ ] Warning when inventory > 18 items

---

## 3. Shop System (Pending Definition)

### 3.1 Concept
- [ ] Shop where users can buy items using an earning mechanism
- [ ] **EARNING MECHANISM: NOT DEFINED YET**
  - Needs mini-games? Daily care streaks? Time-based?
  - No earning system currently implemented
- [ ] **REWARDS/BENEFITS: NOT DEFINED YET**
  - Beyond cosmetics: What do items actually do?
  - Stat boosts? Unlocks? Just cosmetics?

### 3.2 Currency (To Be Defined)
- [ ] Currency name (coins? shells? Demi dollars?)
- [ ] How to earn:
  - ❌ Mini-games (Phase 5 not done yet)
  - ❌ Daily login bonus (not implemented)
  - ❌ Care streak rewards (Memories system not done)
- [ ] Starting currency: 0 (until earning defined)

### 3.3 Shop Inventory (Ideas, Not Finalized)
- [ ] **Consumables**:
  - Snacks (hunger +15) - Price TBD
  - Medicine (health +30) - Price TBD
  - Energy drink (energy +25) - Price TBD
- [ ] **Permanent Items**:
  - Toys (happiness booster) - Price TBD
  - Accessories (cosmetic) - Price TBD
- [ ] **Special Items**:
  - Evolution stones? (Character variety undefined)
  - Seasonal items? (Seasons system not done)

### 3.4 Shop UI
- [ ] Access via menu: "Shop" button
- [ ] Display items in grid with prices
- [ ] "Buy" confirmation dialog
- [ ] "Not enough currency" message

### 3.5 Notes
- **BLOCKED**: Cannot implement until earning mechanism is defined
- **BLOCKED**: Cannot balance prices until rewards/benefits are defined
- **SUGGESTION**: Complete Phase 5 (Mini-Games) first to have earning mechanism

---

## 5. Character Variety (Future - User Thinking Phase)

### 2.1 Character Types (Based on Care)
- [ ] **Classic Demi**: Balanced stats, neutral personality
- [ ] **Athletic Demi**: Higher energy recovery, needs more play
- [ ] **Glutton Demi**: Gains weight faster, hunger drains faster
- [ ] **Genius Demi**: Learns mini-games faster, happiness from achievements
- [ ] **Social Demi**: Happiness drains faster without interaction

### 2.2 Unlock Conditions
- [ ] **Athletic**: Energy > 80% for 7 consecutive days
- [ ] **Glutton**: Feed Demi > 50 times total
- [ ] **Genius**: Complete 30 mini-game sessions
- [ ] **Social**: Interact (feed/play/clean) > 100 times total

### 2.3 Character Switching
- [ ] In settings menu: "Switch Character"
- [ ] Confirmation dialog: "This resets care history?"
- [ ] Keep achievements, reset stats to 100
- [ ] New character gets different sprite set

### 2.4 Character-Specific Behaviors
- [ ] **Athletic**: Asks to play 2x more often
- [ ] **Glutton**: Poop events 1.5x more frequent
- [ ] **Genius**: Mini-games get harder, higher score rewards
- [ ] **Social**: Attention calls happen 2x faster

---

## 6. Other Recommendations (Nice-to-Have)

### 3.1 Social Features
- [ ] Share achievements via WiFi (post to webhook?)
- [ ] "Visit" other Demis via WiFi (future, no Tamagotchi connection)
- [ ] Daily horoscope/fortune for Demi

### 3.2 Customization Deep-Dive
- [ ] Name color (affects NeoPixel LED)
- [ ] Background themes for stat screen
- [ ] Unlockable wallpapers based on achievements

### 3.3 Advanced Mini-Games
- [ ] **Cooking Game**: Combine ingredients, Demi rates the dish
- [ ] **Dress-Up**: Drag accessories onto Demi sprite
- [ ] **Gardening**: Grow plants, Demi waters them

---

## Comparison: Current vs Recommended

| Feature | Current Status | Recommendation |
|---------|---------------|----------------|
| Memories System | ❌ None | ✅ Section 1 (Achievements, Care Mistakes) |
| Inventory | ❌ None | ✅ Section 2 (Item struct, UI, limits) |
| Character Variety | ❌ Single Demi | 🤔 Section 5 (User thinking) |
| Shop System | ❌ None | ⚠️ Section 3 (Pending earning/reward definition) |
| Other Features | ❌ None | ✅ Section 6 (Social, Customization, etc.) |
| Character Switch | ❌ None | 🤔 Section 5.3 (Future consideration) |

---

## Notes
- Shop System (Section 3) is BLOCKED: earning mechanism & rewards undefined
- Character variety (Section 5) is COMPLETELY OPTIONAL — user said "let me think about it"
- These are RECOMMENDATIONS, not requirements
- Prioritize main braindump.md phases first before touching this
- Item Inventory (Section 2) is detailed but depends on Shop System for acquisition balance

---

**Status**: Waiting for user decision on Character Variety & Shop System earning mechanism
**Next Action**: Complete Phase 1-6 in braindump.md before implementing recommendations
**Blocker**: Shop System needs earning mechanism (likely Phase 5 Mini-Games)
