# OpenWatch - Open Source Smartwatch

An open-source smartwatch project developed as a school final project. This repository contains all the necessary files for building a complete smartwatch, including mechanical designs, PCB layouts, and firmware/software code.

## 📁 Project Structure

```
OpenWatch/
├── Code/                    # All code files
│   ├── Firmware/           # Embedded firmware code
│   └── Software/           # Application software code
├── KiCad/                  # PCB design files and schematics
│   ├── Smartwatch.kicad_pcb    # Main PCB layout
│   ├── Smartwatch.kicad_sch    # Main schematic
│   ├── Smartwatch.kicad_pro    # Project file
│   ├── Usermade.pretty/        # Custom component footprints
│   └── Smartwatch-backups/     # Backup files
└── Mechanical/             # 3D models and mechanical designs
    ├── Watch Assm.SLDASM       # Main assembly
    ├── Watch Body.SLDPRT       # Watch body
    ├── Watch Top.SLDPRT        # Watch top
    └── Watch PCB.SLDPRT        # PCB mechanical model
```

## 🚀 Getting Started

### Prerequisites

- **Git** - Version control system
- **KiCad** - For PCB design and schematic editing
- **SolidWorks** (or compatible CAD software) - For mechanical design files
- **Arduino IDE** - For firmware development
- **Code editor** (VSCode, Cursor, etc.) - For software development

### Installation

1. Clone this repository to your local machine
2. Install the required software listed above
3. Open the appropriate files in their respective applications

## 📚 Git Workflow Guide

This section provides step-by-step instructions for using Git effectively in this project. All team members should be familiar with these commands.

### Initial Setup

#### 1. Clone the Repository
```bash
# Clone the repository to your local machine
git clone https://github.com/yourusername/OpenWatch.git

# Navigate to the project directory
cd OpenWatch
```

#### 2. Configure Git (First time only)
```bash
# Set your name and email
git config --global user.name "Your Name"
git config --global user.email "your.email@example.com"

# Optional: Set default branch name
git config --global init.defaultBranch main
```

### Daily Workflow

#### 3. Check Repository Status
```bash
# See what files have been modified
git status

# See detailed changes
git diff

# See changes in a specific file
git diff filename.txt
```

#### 4. Fetch Latest Changes
```bash
# Download latest changes from remote repository
git fetch origin

# See what changes are available
git log HEAD..origin/main --oneline
```

#### 5. Pull Latest Changes
```bash
# Pull and merge latest changes from main branch
git pull origin main

# Or if you're on a different branch
git pull origin main
```

### Working with Branches

#### 6. Create a New Branch
```bash
# Create and switch to a new branch
git checkout -b feature/your-feature-name

# Or using newer syntax
git switch -c feature/your-feature-name

# List all branches
git branch -a
```

#### 7. Switch Between Branches
```bash
# Switch to main branch
git checkout main
# or
git switch main

# Switch to a feature branch
git checkout feature/your-feature-name
# or
git switch feature/your-feature-name
```

#### 8. Delete a Branch
```bash
# Delete local branch (after switching away from it)
git branch -d feature/your-feature-name

# Force delete if needed
git branch -D feature/your-feature-name

# Delete remote branch
git push origin --delete feature/your-feature-name
```

### Making Changes

#### 9. Stage Changes
```bash
# Stage all changes
git add .

# Stage specific files
git add filename.txt
git add Code/Firmware/main.cpp

# Stage changes interactively
git add -i
```

#### 10. Commit Changes
```bash
# Commit with a message
git commit -m "Add new feature: touch screen calibration"

# Commit with detailed message
git commit -m "Fix PCB layout issue

- Corrected trace routing for power management
- Updated component placement
- Fixed clearance issues"
```

#### 11. Push Changes
```bash
# Push current branch to remote
git push origin feature/your-feature-name

# Push and set upstream (first time)
git push -u origin feature/your-feature-name

# Push to main branch
git push origin main
```

### Merging and Collaboration

#### 12. Merge Branches
```bash
# Switch to target branch (usually main)
git checkout main

# Pull latest changes
git pull origin main

# Merge feature branch
git merge feature/your-feature-name

# Push merged changes
git push origin main
```

#### 13. Resolve Merge Conflicts
```bash
# If conflicts occur during merge
git status  # See conflicted files

# Edit conflicted files to resolve conflicts
# Look for <<<<<<< HEAD markers

# After resolving conflicts
git add resolved-file.txt
git commit -m "Resolve merge conflict in resolved-file.txt"
```

#### 14. Rebase (Advanced)
```bash
# Rebase current branch onto main
git checkout feature/your-feature-name
git rebase main

# Interactive rebase (for cleaning up commits)
git rebase -i HEAD~3
```

### Undoing Changes

#### 15. Undo Unstaged Changes
```bash
# Discard changes to a file
git checkout -- filename.txt

# Discard all unstaged changes
git checkout -- .
```

#### 16. Undo Staged Changes
```bash
# Unstage a file
git reset HEAD filename.txt

# Unstage all files
git reset HEAD
```

#### 17. Undo Commits
```bash
# Undo last commit (keep changes)
git reset --soft HEAD~1

# Undo last commit (discard changes)
git reset --hard HEAD~1

# Undo multiple commits
git reset --hard HEAD~3
```

### Useful Commands

#### 18. View History
```bash
# View commit history
git log --oneline

# View detailed history
git log --graph --pretty=format:'%h -%d %s (%cr) <%an>' --abbrev-commit

# View changes in a specific commit
git show commit-hash
```

#### 19. Stash Changes
```bash
# Save current changes temporarily
git stash

# List stashes
git stash list

# Apply most recent stash
git stash pop

# Apply specific stash
git stash apply stash@{0}
```

#### 20. Tag Releases
```bash
# Create a tag
git tag -a v1.0.0 -m "Release version 1.0.0"

# Push tags
git push origin v1.0.0

# List tags
git tag
```

## 🔧 Development Guidelines

### Code Organization
- **Firmware**: Place all embedded code in `Code/Firmware/`
- **Software**: Place application code in `Code/Software/`
- **Mechanical**: Keep all CAD files in `Mechanical/`
- **PCB**: All KiCad files go in `KiCad/`

### Commit Message Convention
Use clear, descriptive commit messages:
- `feat:` for new features
- `fix:` for bug fixes
- `docs:` for documentation changes
- `style:` for formatting changes
- `refactor:` for code refactoring
- `test:` for adding tests

Examples:
```
feat: Add touch screen calibration algorithm
fix: Resolve power management issue in firmware
docs: Update README with installation instructions
```

### Branch Naming
Use descriptive branch names:
- `feature/touch-screen-support`
- `fix/power-management-bug`
- `docs/update-readme`
- `refactor/firmware-structure`

## 🤝 Contributing

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/amazing-feature`)
3. Make your changes
4. Commit your changes (`git commit -m 'Add some amazing feature'`)
5. Push to the branch (`git push origin feature/amazing-feature`)
6. Open a Pull Request

## 📋 Project Status

This project is currently in active development as a school final project. The repository contains:

- ✅ Basic project structure
- ✅ KiCad PCB design files
- ✅ SolidWorks mechanical models
- 🔄 Firmware development (in progress)
- 🔄 Software development (in progress)

## 📄 License

This project is open source. Please check the LICENSE file for details.

---

**Happy Coding! 🚀**

Remember: When in doubt, `git status` is your friend!
