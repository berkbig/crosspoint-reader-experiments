# Milestone Plan Restructuring - Summary

## What Changed

The original milestone plan has been **completely restructured** to follow a "test integration first" approach instead of starting with the complex YarnSpinner porting work.

### Previous Structure (Old)
```
Phase 1: YarnSpinner Core Adaptation (20+ hours)
Phase 2: Presentation Layer (8 hours)
Phase 3: Activity Integration (22 hours)  ← Complex lifecycle issues discovered late
Phase 4: Session Management (24 hours)
Phase 5: Polish (22 hours)
```

### New Structure (Recommended)
```
Phase 1: Barebones Activity Skeleton (8-12 hours)    ← Proves integration works first
Phase 2: Multi-Line Text & Pagination (6-8 hours)
Phase 3: YarnSpinner Core Adaptation (20 hours)
Phase 4: YarnSpinner Integration (12 hours)
Phase 5: Session Management (16 hours)
Phase 6: Polish & Optimization (16 hours)
```

## Key Benefits

1. **Risk Mitigation First**
   - Phase 1 proves Activity integration works in isolation
   - Discover lifecycle issues early (cheap to fix)
   - No wasted YarnSpinner porting if integration fails

2. **Rapid Prototype**
   - By end of Phase 1 (~1 week), you have a functioning activity on hardware
   - Can gather feedback before diving into YarnSpinner
   - Team confidence boost from working MVP

3. **Small, Testable Increments**
   - Phase 1 broken into 6 focused commits:
     1. Directory structure
     2. Activity skeleton
     3. Text utilities
     4. Rendering implementation
     5. Input handling
     6. Hardware integration test

4. **Easier Debugging**
   - Each phase is isolated and independently testable
   - Rendering issues won't be confused with VM execution issues
   - Clear demarcation between Activity layer and YarnSpinner layer

## Phase 1 Details: Barebones Activity (8-12 hours)

### What You Build
- `InteractiveFictionActivity` class that extends `Activity`
- `TextUtils` helper for word wrapping
- Hardcoded test text blocks (Lorem ipsum)
- Button handling: SELECT cycles text, BACK exits
- Multi-line text rendering to e-ink display

### What You DON'T Build
- No YarnSpinner VM
- No dialogue branching
- No file loading
- No state persistence
- No fancy UI

### Why This Order?
Once Phase 1 works, you know:
- ✅ Activity lifecycle integration is correct
- ✅ Rendering system works for multi-line text
- ✅ Button input flows correctly
- ✅ Display refresh strategy is acceptable

Then Phase 3 (YarnSpinner) can focus purely on VM logic without worrying about Activity integration.

## Timeline Comparison

| Milestone | Old Approach | New Approach | Benefit |
|-----------|-------------|-------------|---------|
| First working build | Week 3 | **Day 3-4** | 1 week earlier! |
| Hardware proof | Week 4 | **Week 1** | Early validation |
| YarnSpinner integration | Week 2.5 | Week 2-3 | Better foundation |
| Full feature | Week 4.5 | Week 4.5 | Same endpoint |

## Implementation Strategy

### Phase 1 (Week 1)
- Day 1: Set up directory structure
- Day 2: Activity skeleton & rendering
- Day 3: Button handling & state cycling
- Day 4: Hardware testing

**Definition of Done:** Activity launches, displays lorem ipsum, responds to button presses

### Phase 2 (Week 1-1.5)
- Enhance text wrapping (word boundaries)
- Add UP/DOWN scrolling
- Optimize display refresh

**Definition of Done:** Long text displays properly with pagination

### Phase 3 (Week 1.5-2.5)
- Port YarnSpinner Core (remove UE deps)
- Implement Variable Storage & Logger
- Unit test VM execution

**Definition of Done:** VM executes sample .yarn file in isolation

### Phase 4 (Week 2.5-3)
- Wire VM into Activity
- Replace dummy text with real dialogue
- Handle Line/Option callbacks

**Definition of Done:** Activity runs YarnSpinner dialogue end-to-end

### Phase 5 (Week 3-3.5)
- File browser integration
- Save/load state
- Bookmarks

**Definition of Done:** Full session management working

### Phase 6 (Week 3.5-4.5)
- Performance tuning
- Edge case handling
- Documentation & testing

**Definition of Done:** Production-ready with >80% test coverage

## Commit Structure for Phase 1

Each commit should be focused and single-concern:

```
1. refactor: Create interactive fiction activity directory structure
2. feat: Add InteractiveFictionActivity skeleton with test blocks
3. feat: Add TextUtils for text wrapping and pagination
4. feat: Implement multi-line text rendering to display
5. feat: Add button input handling and state cycling
6. test: Verify activity integration and hardware responsiveness
7. docs: Add notes on IF activity integration patterns
```

## Risk Reduction

By going "barebones first," you eliminate these risks:

| Risk | Eliminated By |
|------|---------------|
| Activity integration broken | Phase 1 validates |
| Display refresh too slow | Phase 1 measures |
| Button input not responsive | Phase 1 confirms |
| Memory model misunderstood | Phase 1 proves |
| Activity lifecycle confusion | Phase 1 clarifies |

If any Phase 1 step fails, you fix the integration layer before YarnSpinner complexity.

## Success Metrics

### After Phase 1 (1 week)
- ✅ Code compiles, no warnings
- ✅ Activity launches from home screen
- ✅ Lorem ipsum displays correctly
- ✅ Button press cycles text
- ✅ BACK returns to home
- ✅ No crashes or hangs

### After Phase 2 (1.5 weeks)
- ✅ Word wrapping works for long text
- ✅ UP/DOWN scrolling responsive
- ✅ Display refresh optimized (<100ms latency)
- ✅ Visual pagination clear

### After Phase 3 (2.5 weeks)
- ✅ YarnSpinner Core compiles (no UE deps)
- ✅ VM executes sample .yarn file
- ✅ Unit tests pass (>80% coverage)
- ✅ Variable storage works

### After Phase 4 (3 weeks)
- ✅ Activity integrated with VM
- ✅ Dialogue displays and progresses
- ✅ Options selectable
- ✅ Branching works

### After Phase 5 (3.5 weeks)
- ✅ File browser shows .yarn files
- ✅ Save/load seamless
- ✅ Bookmarks functional

### After Phase 6 (4.5 weeks)
- ✅ Performance acceptable
- ✅ All edge cases handled
- ✅ Documented and tested
- ✅ Ready for production

## How to Use This Plan

1. **Week 1**: Execute Phase 1 exactly as specified
   - Don't skip ahead to YarnSpinner
   - Treat it as a hard deadline to have a working prototype

2. **After Phase 1**: Gather feedback
   - Is display readable?
   - Is button responsiveness acceptable?
   - Any Activity integration issues discovered?

3. **Week 2**: Execute Phases 2-3 in parallel (if team size allows)
   - One dev on Phase 2 (rendering optimization)
   - Another dev on Phase 3 (YarnSpinner porting)
   - They don't block each other

4. **Week 3+**: Integrate and test
   - Phase 4: Wire together Phase 2 rendering + Phase 3 VM
   - Phase 5: Add features
   - Phase 6: Polish and test

## Documentation

All details for Phase 1 are in the Research directory:

- **01-OVERVIEW.md** - Project goals, scope, architecture
- **02-CROSSPOINT-ACTIVITY-DEEP-DIVE.md** - Activity system details
- **03-YARNSPINNER-CORE-DEEP-DIVE.md** - YarnSpinner architecture
- **04-MILESTONE-PLAN.md** - This detailed phase breakdown (UPDATED)

Phase 1 implementation details are fully specified in 04-MILESTONE-PLAN.md (Section: "Phase 1: Barebones Activity Skeleton").
