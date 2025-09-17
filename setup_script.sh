#!/bin/bash
# setup.sh - Set up the Othernet P2P project directory structure

set -e

echo "Setting up Othernet P2P project structure..."

# Create main directory structure
mkdir -p othernet-p2p/{basic-p2p,othernet,windows,docs,examples,tools}
mkdir -p othernet-p2p/{basic-p2p/logs,othernet/logs,windows/dist}
mkdir -p othernet-p2p/examples/{bootstrap-node,home-node,mobile-node}

cd othernet-p2p

echo "Created directory structure:"
find . -type d | sort

echo ""
echo "Next steps:"
echo "1. Copy your artifacts to the appropriate directories:"
echo "   - p2p_node.c → basic-p2p/"
echo "   - othernet_node.c → othernet/"
echo "   - win32_othernet_node.c → windows/"
echo "   - Dockerfile (basic) → basic-p2p/Dockerfile"
echo "   - Dockerfile (othernet) → othernet/Dockerfile"
echo "   - docker-compose.yml (basic) → basic-p2p/"
echo "   - othernet-compose.yml → othernet/docker-compose.yml"
echo ""
echo "2. Copy the updated Makefile to the root directory"
echo ""
echo "3. Make scripts executable:"
echo "   chmod +x basic-p2p/*.sh othernet/*.sh tools/*.sh"
echo ""
echo "4. Test the setup:"
echo "   make build           # Build basic P2P"
echo "   make build-othernet  # Build Othernet"
echo "   make up             # Start basic P2P"
echo "   make status         # Check status"

# Create sample .gitignore
cat > .gitignore << 'EOF'
# Build artifacts
*/logs/*
!*/logs/.gitkeep
windows/dist/*
!windows/dist/.gitkeep

# Docker volumes
**/volumes/

# IDE files
.vscode/
.idea/
*.swp
*.swo

# OS files
.DS_Store
Thumbs.db

# Temporary files
*.tmp
*.temp
*~
EOF

# Create placeholder files to preserve directory structure
touch basic-p2p/logs/.gitkeep
touch othernet/logs/.gitkeep
touch windows/dist/.gitkeep

echo ""
echo "Created .gitignore and placeholder files"
echo "Setup complete!"
