# Interactive Fiction Runtime for Crosspoint - Documentation Index

## 📚 Research Documents

All planning documents are located in the `Research/` directory:

### 1. **01-OVERVIEW.md** (6.4 KB)
📌 **Start here for project vision**

- Executive summary and goals
- Key constraints (hardware, firmware, display)
- Scope (in-scope vs out-of-scope)
- Success criteria
- High-level architecture
- File structure plan
- Risk assessment
- Next steps

**Best for:** Understanding what we're building and why

---

### 2. **02-CROSSPOINT-ACTIVITY-DEEP-DIVE.md** (9.9 KB)
📌 **Deep dive into how Crosspoint Activities work**

- Activity base class and lifecycle hooks (onEnter, loop, render, onExit)
- Multi-threaded rendering model with RenderLock synchronization
- ActivityManager stack-based navigation
- Rendering system and HAL abstraction
- Input handling via MappedInputManager
- Reference: TxtReaderActivity example
- Integration points for IF Activity
- Design patterns to follow

**Best for:** Understanding Activity system before coding Phase 1

---

### 3. **03-YARNSPINNER-CORE-DEEP-DIVE.md** (13.5 KB)
📌 **Deep dive into YarnSpinner architecture**

- Data types and Value system
- Protobuf bytecode format
- VirtualMachine execution states and flow
- Common types (Line, Option, Command, OptionSet)
- Variable storage interface
- Function library system
- Adaptation strategy (what to keep/replace/add)
- Performance considerations
- Debugging aids
- Memory profile
- Critical porting issues

**Best for:** Understanding YarnSpinner before Phase 3 (porting)

---

### 4. **04-MILESTONE-PLAN.md** (20+ KB)
📌 **Detailed implementation plan, RESTRUCTURED**

**NEW STRUCTURE (Barebones First):**

1. **Phase 1** (Week 1, 8-12 hours): Barebones Activity Skeleton
   - ✅ Display multi-line text
   - ✅ Respond to button presses
   - ✅ Cycle through text blocks
   - ❌ NO YarnSpinner yet

2. **Phase 2** (Week 1-1.5, 6-8 hours): Multi-Line Text & Pagination
   - Improve text wrapping (word-break)
   - Implement vertical scrolling
   - Add visual indicators

3. **Phase 3** (Week 1.5-2.5, ~20 hours): YarnSpinner Core Adaptation (Host Validation)
   - Port from Unreal Engine
   - Remove UE dependencies
   - Validate runtime behavior in harness

4. **Phase 4** (Week 2.5-3, ~12 hours): YarnC → Device-Native Compiler
   - Compile `.yarnc` + line tables to a compact runtime format
   - Add golden tests for converter correctness
   - Version the native asset format

5. **Phase 5** (Week 3-3.5, ~16 hours): Firmware Runtime Integration
   - Load native assets in Activity
   - Replace dummy text with real dialogue
   - Keep protobuf out of firmware runtime

6. **Phase 6** (Week 3.5-4.5, ~16 hours): Session Management
   - File browser integration
   - Save/load state
   - Bookmarks

7. **Phase 7** (Week 4.5-5, ~16 hours): Polish & Optimization
   - Performance tuning
   - Edge case testing
   - Documentation

Each phase includes:
- Detailed goals and deliverables
- Effort estimates
- Definition of done
- Review checklists
- Risk mitigation

**Best for:** Planning and executing the implementation roadmap

---

### 5. **RESTRUCTURING-SUMMARY.md** (7.4 KB)
📌 **Summary of why the plan was reorganized**

- Old structure vs. new structure comparison
- Key benefits of "barebones first" approach
- Risk reduction strategy
- Timeline comparison (old vs. new)
- Implementation strategy by phase
- Success metrics by phase
- Why this order matters

**Best for:** Understanding the philosophy and benefits of Phase 1 first

---

### 6. **PHASE-1-QUICK-REFERENCE.md** (8.0 KB)
📌 **Detailed breakdown of Phase 1 (Barebones Activity)**

**6 Tasks, 6-7 Commits:**
1. Directory structure (1 hour)
2. Activity skeleton (2 hours)
3. Text utilities (2 hours)
4. Rendering implementation (2 hours)
5. Input handling (1.5 hours)
6. Hardware integration test (1.5 hours)

Includes:
- Detailed code snippets
- Estimated schedule
- Coding standards
- Definition of done
- Success metrics
- Troubleshooting guide
- File and testing checklists

**Best for:** Implementing Phase 1 right now

---

## 🎯 Quick Navigation

### For Project Managers
1. Read **01-OVERVIEW.md** (10 min) - Project scope
2. Skim **RESTRUCTURING-SUMMARY.md** (5 min) - Why new approach
3. Review **04-MILESTONE-PLAN.md** Phases overview (5 min) - Timeline

**Time:** 20 minutes

### For Developers Starting Phase 1
1. Skim **01-OVERVIEW.md** (10 min) - Context
2. Read **02-CROSSPOINT-ACTIVITY-DEEP-DIVE.md** (15 min) - Foundation
3. Read **PHASE-1-QUICK-REFERENCE.md** (10 min) - Implementation guide

**Time:** 35 minutes

### For YarnSpinner Experts (Phase 3)
1. Read **03-YARNSPINNER-CORE-DEEP-DIVE.md** (15 min) - Architecture review
2. Review **04-MILESTONE-PLAN.md** Phase 3 section (10 min) - Porting tasks

**Time:** 25 minutes

### For Code Reviewers
1. Reference **02-CROSSPOINT-ACTIVITY-DEEP-DIVE.md** (Design patterns)
2. Reference **PHASE-1-QUICK-REFERENCE.md** (Checklist)
3. Check against **04-MILESTONE-PLAN.md** (Definition of done)

**Time:** On-demand

---

## 📊 Document Statistics

| Document | Size | Scope |
|----------|------|-------|
| 01-OVERVIEW.md | 6.4 KB | Project vision, scope, architecture |
| 02-CROSSPOINT-ACTIVITY-DEEP-DIVE.md | 9.9 KB | Activity system deep dive |
| 03-YARNSPINNER-CORE-DEEP-DIVE.md | 13.5 KB | YarnSpinner architecture |
| 04-MILESTONE-PLAN.md | 20+ KB | Detailed phase breakdown (7 phases) |
| RESTRUCTURING-SUMMARY.md | 7.4 KB | Plan reorganization rationale |
| PHASE-1-QUICK-REFERENCE.md | 8.0 KB | Phase 1 implementation guide |
| **TOTAL** | **~65 KB** | Complete planning documentation |

---

## 🚀 Getting Started

### Week 1: Barebones Activity (Phase 1)
```
Monday:    Read docs (02, PHASE-1-QR) → Start Task 1.1
Tuesday:   Tasks 1.2-1.3 (Activity skeleton + utilities)
Wednesday: Tasks 1.4-1.5 (Rendering + input handling)
Thursday:  Task 1.6 (Hardware testing + fixes)
Friday:    Code review + merge
```

### Week 1.5-2: Pagination & Rendering (Phase 2)
- While Phase 1 is being reviewed, start Phase 2
- Improve text wrapping, add scrolling

### Week 1.5-2.5: YarnSpinner Core (Phase 3)
- While Phase 1-2 are being tested, port/validate YarnSpinner core behavior
- Focus on removing UE dependencies and harness parity

### Week 2.5-3: Asset Compiler (Phase 4)
- Build `.yarnc` -> device-native converter
- Validate native assets against harness transcript output

### Week 3-3.5: Firmware Integration (Phase 5)
- Wire Phase 2 rendering + native runtime together
- Test with compiled native story assets

### Week 3.5+: Session + Polish (Phase 6-7)
- Session management, bookmarks
- Performance tuning, testing

---

## 📋 Key Files to Review

### Before Coding Phase 1
- ✅ **02-CROSSPOINT-ACTIVITY-DEEP-DIVE.md** - Understand Activity lifecycle
- ✅ **PHASE-1-QUICK-REFERENCE.md** - 6 tasks, estimated timeline
- ✅ Reference existing activities in `src/activities/reader/`

### Before Coding Phase 3
- ✅ **03-YARNSPINNER-CORE-DEEP-DIVE.md** - YarnSpinner architecture
- ✅ **04-MILESTONE-PLAN.md** Phase 3 section - Porting tasks
- ✅ Download YarnSpinner source from GitHub (prototype)

### Before Integration (Phase 5)
- ✅ **04-MILESTONE-PLAN.md** Phase 5 section - Integration checklist
- ✅ Completed Phase 1-4 code review

---

## ✅ Success Criteria

### Phase 1 Completion
- ✅ Activity compiles and launches
- ✅ Text displays and wraps correctly
- ✅ Button input works responsively
- ✅ Code review approved
- ✅ Tested on hardware

### Overall Project Completion
- ✅ Native-format dialogue executes end-to-end
- ✅ Options and branching work
- ✅ State persists across sessions
- ✅ >80% test coverage
- ✅ Hardware validated
- ✅ Documentation complete

---

## 📞 Questions?

### "How do I know where to start?"
→ Read **PHASE-1-QUICK-REFERENCE.md** and Task 1.1

### "What's the Activity lifecycle?"
→ Read **02-CROSSPOINT-ACTIVITY-DEEP-DIVE.md**, section "Activity Lifecycle"

### "Why not start with YarnSpinner porting?"
→ Read **RESTRUCTURING-SUMMARY.md**, "Key Benefits"

### "How long will this take?"
→ Read **04-MILESTONE-PLAN.md**, project phases overview: ~4-5 weeks total

### "What do I need to know about YarnSpinner?"
→ Read **03-YARNSPINNER-CORE-DEEP-DIVE.md**, sections 1-2

### "What are the integration points?"
→ Read **02-CROSSPOINT-ACTIVITY-DEEP-DIVE.md**, section "Integration Points"

---

## 🎓 Learning Path

**For New Team Members:**
1. Read 01-OVERVIEW.md (project context)
2. Read 02-CROSSPOINT-ACTIVITY-DEEP-DIVE.md (system to understand)
3. Skim 03-YARNSPINNER-CORE-DEEP-DIVE.md (future knowledge)
4. Review existing TxtReaderActivity code (reference)
5. Read PHASE-1-QUICK-REFERENCE.md (what you're building)

**For Experienced Developers:**
1. Skim 01-OVERVIEW.md (quick context)
2. Reference 02-CROSSPOINT-ACTIVITY-DEEP-DIVE.md as needed
3. Jump into PHASE-1-QUICK-REFERENCE.md (implementation details)

**For Technical Leads/Architects:**
1. Read all planning documents
2. Review risk assessments in each
3. Establish code review criteria from "Design Patterns" sections

---

## 📌 Important Notes

- **All planning is in Research/** - these are reference documents, not to be committed as code
- **Phase 1 is barebones** - focus on proving integration works, not building full feature
- **No YarnSpinner in Phase 1** - keep scopes separate initially
- **Hardware testing is mandatory** - simulator alone is not sufficient
- **One concern per commit** - keep history clean and reviewable

---

## 🔗 Related Files

- Source code will go in: `src/activities/interactive_fiction/`
- YarnSpinner core will go in: `src/lib/yarnspinner_core/`
- Asset compiler will go in: `tools/yarn_asset_compiler/`
- Tests will go in: `test/lib/yarnspinner_core/`
- Reference implementations: `src/activities/reader/TxtReaderActivity.*`

---

**Last Updated:** June 30, 2026  
**Status:** ✅ Planning Complete - Ready for Implementation  
**Next Steps:** Execute native-asset compiler and firmware runtime integration phases
