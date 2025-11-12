# OpenWatch GUI Navigation Guidelines

## Overview
This document outlines the navigation structure and user interface guidelines for the OpenWatch smartwatch application.

## Main Navigation Structure

### Primary Navigation (Vertical Swipes from Watch Face)

The watch face is the central hub. Vertical swipes navigate through main app screens in a linear sequence:

```

┌─────────────────────────────────────┐
│            Settings                 │
│        (Current: Active)            │
│    ↑ Swipe UP: Back to Watch Face   │
│    ↓ Swipe DOWN: NA                 │
└─────────────────────────────────────┘
                 ↓
┌─────────────────────────────────────┐
│         Watch Face (Home)           │
│                                     │
│    ↑ Swipe DOWN: Enter settings     │
│    ↓ Swipe UP: Return from below    │
└─────────────────────────────────────┘
                 ↑
┌─────────────────────────────────────┐
│            Weather                  │
│           (Future Dev)              │
│    ↑ Swipe UP: Fitness              │
│    ↓ Swipe DOWN: Watch Face         │
└─────────────────────────────────────┘
                 ↑
┌─────────────────────────────────────┐
│            Fitness                  │
│           (Future Dev)              │
│    ↑ Swipe UP: Music                │
│    ↓ Swipe DOWN: Weather            │
└─────────────────────────────────────┘
                 ↑
┌─────────────────────────────────────┐
│             Music                   │
│           (Future Dev)              │
│    ↑ Swipe UP: Navigation           │
│    ↓ Swipe DOWN: Fitness            │
└─────────────────────────────────────┘
                 ↑
┌─────────────────────────────────────┐
│          Navigation                 │
│           (Future Dev)              │
│    ↑ Swipe UP: Back to Watch Face   │
│    ↓ Swipe DOWN: Music              │
└─────────────────────────────────────┘
```

### Desired Swipe Flow Sequence:
1. **Watch Face** (starting point)
2. Swipe DOWN → **Settings**
3. Swipe UP → **Weather**
4. Swipe UP → **Fitness**
5. Swipe UP → **Music**
6. Swipe UP → **Navigation**
7. Swipe UP → **Back to Watch Face** (loops back to start)

**Note:** Swiping DOWN at any point goes back to the previous screen in the sequence.

## Secondary Navigation (Horizontal Swipes - Future Development)

### Within Each App Screen
Once inside any main app (Settings, Weather, Fitness, Music, Navigation), horizontal swipes navigate sub-menus:

- **Swipe LEFT/RIGHT**: Navigate between sub-screens within that app
- Example for Settings:
  ```
  Settings Main → LEFT/RIGHT → WiFi Settings
                → LEFT/RIGHT → Bluetooth Settings
                → LEFT/RIGHT → Display Settings
                → etc.
  ```

### Current Implementation (Temporary - Will be replaced)
**Legacy horizontal navigation from watch face:**
- Swipe LEFT from Watch Face → Music (direct access)
- Swipe RIGHT from Watch Face → Heart Rate (direct access)

**This will be deprecated** once the new vertical menu structure is fully implemented.

## Design Principles

### 1. **Vertical Navigation = Main Menu Hierarchy**
   - Swipe down to go deeper into menus
   - Swipe down to go back/up in hierarchy
   - Linear, predictable flow

### 2. **Horizontal Navigation = Sub-Menu Browsing**
   - Used only within a specific app/screen
   - Browse related settings or options
   - Swipe left/right to cycle through options

### 3. **Home is Always Accessible**
   - Watch face is the main "home" screen
   - Can return to home from any screen with repeated swipes UP
   - Long press button can also return to home (alternative)

### 4. **Circular Flow**
   - The menu "wraps around" - swiping down from Navigation returns to Watch Face
   - This creates an infinite loop for easy navigation

## Screen Layouts

### Watch Face Requirements
- Display time (analog + digital)
- Show key status indicators:
  - Battery level (top)
  - Steps counter (lower left)
  - Heart rate (bottom center)
  - Weather temp (lower right)
  - WiFi/BT/Notifications (center below hands)
- Date display (left side)
- All elements must fit within circular 240x240 display

### Settings Screen Requirements
- Circular menu layout (like LVGL demo reference)
- Main options arranged in a circle:
  - WiFi (top)
  - Bluetooth (right)
  - Find Phone (bottom right)
  - More Settings (left)
- No "SELECT" or "HOME" text labels (cleaner design)
- Swipe hint at bottom

### Other App Screens
- Each screen should have:
  - Clear title/header
  - Main content area
  - Swipe hints when appropriate
  - Consistent dark theme (#000000 background)
  - Color-coded visual elements

## Color Scheme

### Primary Colors
- **Background**: `#000000` (black) to `#0a0a0a` (near black)
- **Text Primary**: `#FFFFFF` (white)
- **Text Secondary**: `#888888` (gray)
- **Text Dim**: `#666666`, `#444444` (darker grays)

### Accent Colors by Function
- **Battery/Cyan**: `#00d9ff` (cyan/blue)
- **Warning/Orange**: `#FFB74D` (gold/orange)
- **Steps**: `#FF6B35` (orange)
- **Heart Rate**: `#FF4444` (red)
- **Weather**: `#88CCFF` (light blue)
- **Music**: `#BB86FC` (purple)

## Touch Gesture Specifications

### Swipe Detection Parameters
- **Minimum swipe distance**: 60 pixels
- **Maximum swipe time**: 500ms
- **Gesture priority**: Favor direction with larger delta

### Gesture Types
1. **Swipe Up**: Vertical delta < -60px
2. **Swipe Down**: Vertical delta > 60px
3. **Swipe Left**: Horizontal delta < -60px
4. **Swipe Right**: Horizontal delta > 60px
5. **Tap**: Touch and release in < 500ms with < 10px movement (future)
6. **Long Press**: Hold for > 1000ms (current button behavior)

## Animation Guidelines

### Screen Transitions
- **Duration**: 300ms (smooth but responsive)
- **Direction matches swipe**:
  - Swipe down → slide from bottom
  - Swipe up → slide from top
  - Swipe left → slide from left
  - Swipe right → slide from right

### Element Animations
- **Clock hands**: Update smoothly every second/minute
- **Arcs/Progress**: Smooth value transitions
- **Fade in/out**: Use for overlays and non-directional transitions

## Future Development Roadmap

### Phase 1: Core Navigation (Current)
- [x] Watch face with status indicators
- [x] Settings menu with circular layout
- [x] Basic swipe detection
- [ ] Complete vertical menu structure

### Phase 2: Main Apps
- [ ] Weather app with forecast
- [ ] Fitness app with activity tracking
- [ ] Music player with Bluetooth control
- [ ] Navigation app with GPS

### Phase 3: Sub-Menus
- [ ] Settings sub-menus (WiFi config, BT pairing, etc.)
- [ ] Weather details (hourly, daily, radar)
- [ ] Fitness details (goals, history, workouts)
- [ ] Music playlists and controls

### Phase 4: Advanced Features
- [ ] Notifications system
- [ ] Heart rate monitoring integration
- [ ] Real-time sensor data
- [ ] Voice control
- [ ] App store/watchfaces

## Technical Notes

### Display Specifications
- **Resolution**: 240x240 pixels
- **Shape**: Circular
- **Driver**: GC9A01A
- **Touch**: CST816S capacitive touch controller

### Software Stack
- **Graphics**: LVGL v8.3.11
- **MCU**: ESP32-S3 @ 240MHz
- **Framework**: Arduino/PlatformIO
- **Language**: C++

### Performance Targets
- **Screen refresh**: 60 FPS minimum
- **Touch latency**: < 50ms
- **Animation smoothness**: 60 FPS during transitions
- **Battery life**: TBD (power optimization ongoing)

## Notes
- NFC functionality is not included (hardware not available)
- Current implementation has temporary horizontal navigation that will be replaced
- All future apps should follow the vertical menu structure
- Maintain consistent visual language across all screens
- Prioritize usability in circular display format

---

**Document Version**: 1.0  
**Last Updated**: 2025-01-12  
**Status**: Living Document - Update as features are implemented

