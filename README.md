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

**⚠️ IMPORTANT: NEVER EDIT DIRECTLY IN MASTER BRANCH! ⚠️**

All team members MUST create a branch for their work, make changes in that branch, and then merge to main. This keeps the project organized and prevents conflicts.

This section provides the essential Git commands you'll use daily. Keep it simple and follow these steps exactly.

### First Time Setup (Do this once)

#### 1. Clone the Repository
```bash
# Clone the repository to your local machine
git clone https://github.com/willherr72/OpenWatch.git

# Navigate to the project directory
cd OpenWatch
```

#### 2. Configure Git (First time only)
```bash
# Set your name and email
git config --global user.name "Your Name"
git config --global user.email "your.email@example.com"
```

## 🚀 Daily Workflow (Follow These Steps Every Time)

### Step 1: Start Your Work Session
```bash
# 1. Get the latest changes from the team
git pull origin master

# 2. Create a new branch for your work (replace 'your-feature' with what you're working on)
git checkout -b feature/your-feature

# Example: git checkout -b feature/touch-screen
# Example: git checkout -b fix/power-bug
# Example: git checkout -b docs/update-readme
```

### Step 2: Make Your Changes
```bash
# 3. Make your changes to files (edit in your code editor)
# 4. Check what files you've changed
git status

# 5. Add your changes
git add .

# 6. Save your work with a message
git commit -m "Add your feature description here"

# 7. Push your branch to GitHub
git push origin feature/your-feature
```

## 📝 Essential Commands Cheat Sheet

| What you want to do | Command |
|-------------------|---------|
| See what files changed | `git status` |
| Add all changes | `git add .` |
| Save your work | `git commit -m "Your message"` |
| Upload to GitHub | `git push origin feature/your-branch` |
| Get team's latest work | `git pull origin master` |
| Create new branch | `git checkout -b feature/your-name` |
| Switch to master | `git checkout master` |
| See all branches | `git branch` |

## ⚠️ Important Rules

1. **ALWAYS create a branch before making changes**
2. **NEVER work directly in master branch**
3. **Use descriptive branch names** (feature/touch-screen, fix/power-bug, etc.)
4. **Write clear commit messages** (what you did, not how you did it)
5. **Pull before you push** (get latest changes first)

## 🔧 Development Guidelines

### Code Organization
- **Firmware**: Place all embedded code in `Code/Firmware/`
- **Software**: Place application code in `Code/Software/`
- **Mechanical**: Keep all CAD files in `Mechanical/`
- **PCB**: All KiCad files go in `KiCad/`

### Branch Naming Examples
- `feature/touch-screen-support`
- `fix/power-management-bug`
- `docs/update-readme`
- `feature/watch-face-display`

## 🤝 Contributing

1. Create a feature branch (`git checkout -b feature/amazing-feature`)
2. Make your changes
3. Commit your changes (`git commit -m 'Add some amazing feature'`)
4. Push to the branch (`git push origin feature/amazing-feature`)
5. Merge to master following the workflow above

## 📋 Project Status

This project is currently in active development as a school final project. The repository contains:

- ✅ Basic project structure
- ✅ KiCad PCB design files
- ✅ SolidWorks mechanical models
- 🔄 Firmware development 
- 🔄 Software development 

## 📄 License

This project is open source. Please check the LICENSE file for details.

---

**Happy Coding! 🚀**

Remember: When in doubt, `git status` is your friend!
