# WiFi Scan & Auto-Connect Optimization Plan

Root cause confirmed: **Endless "Scan Failed" spam is caused by a bug where `isWifiScanning` flag is never cleared on scan failure, triggering infinite render loop.**

## ✅ Proposed Auto-Connect Logic Workflow
```
Boot → Start single WiFi scan → Wait for results:
  ✅ Scan successful:
    → ✅ If 0 networks found: Show "No WiFi sources nearby"
    → ✅ If networks found: Compare scan results with saved networks
    → Filter for networks that have autoConnect enabled AND are present in scan results
    → If multiple matches found: connect ONLY to the one with highest RSSI (strongest signal)
    → If no matches found: do NOT attempt to connect to anything, show "No saved networks in range"
    → NO blocking, NO infinite retries

  ❌ Scan failed (actual hardware/stack error):
    → Show "Scan Failed" message for 3 seconds
    → Clear isWifiScanning flag
    → Stop. Do NOT retry automatically. User can manually scan if needed.
```

## Bugs Identified
1.  **Infinite scan failure loop** - `isWifiScanning` remains true forever after first failed scan
2.  **No scan backoff** - Scan retries immediately with zero delay on failure
3.  **Unlimited render rate** - Menu redraws 30+ times per second during scan
4.  **Blind auto-connect** - Currently attempts to connect to saved networks regardless of whether they are actually in range
5.  **NTP retry flood** - NTP retries every 1 second forever

## Fix Options

| Option | Description | Complexity | Benefit |
|--------|-------------|------------|---------|
| ✅ **Option 1** | Fix the root cause infinite loop | Very low | 100% stops spam immediately |
| ✅ **Option 2** | Smart auto-connect logic as described | Medium | Only connects to networks that are actually present |
| ✅ **Option 3** | Add exponential backoff for scan retries | Medium | Prevents repeated scan attempts when out of range |
| ✅ **Option 4** | Throttle render loop during scan | Low | Reduces CPU usage 90% while scanning |
| ✅ **Option 5** | Proper scan status messages | Very low | Clear user feedback: Scanning → No WiFi → Scan Failed |

## Recommended Implementation Order
1.  First implement Option 1 to stop the infinite loop
2.  Add Option 5 for proper status messages
3.  Add Option 4 for render throttling
4.  Add Option 2 smart auto-connect logic
5.  Add Option 3 for scan backoff

## Expected Results After Fix:
- No more infinite "Scan Failed" spam
- Auto-connect ONLY to networks that are actually present and in range
- Automatically selects strongest signal when multiple auto-connect networks are available
- No connection attempts when away from home networks
- Menu only updates twice per second during scan
- Normal CPU usage restored

This implementation completely solves the issue when you leave your home network - it will silently skip auto-connect without any errors, retries, or spam.
