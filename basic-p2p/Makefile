# Makefile for P2P/Othernet Network Testing (Debian)

NAMESPACE := OtherNet
LC_NAMESPACE := $(shell echo $(NAMESPACE) | tr 'A-Z' 'a-z')

DOCKER=docker compose -f docker-compose.yml

.PHONY: build up down logs shell clean test build-othernet up-othernet

# Basic P2P Network Commands
build:
	docker-compose build

up:
	docker-compose up -d --build
	@echo "Basic P2P Network started!"
	@echo "Access nodes with:"
	@echo "  make shell-node1  # Bootstrap node"
	@echo "  make shell-node2  # Second node"
	@echo "  make shell-node3  # Third node"
	@echo "  make shell-node4  # Fourth node"

down:
	docker-compose down

# Enhanced Othernet Network Commands
build-othernet:
	docker-compose -f othernet-compose.yml build

up-othernet:
	docker-compose -f othernet-compose.yml up -d
	@echo "Othernet Network started!"
	@echo "Network topology:"
	@echo "  Realm 1, Cluster 1:"
	@echo "    - Bootstrap (1.1.1000): localhost:8090"
	@echo "    - Node2 (1.1.1001):     localhost:8091"
	@echo "    - Node3 (1.1.1002):     localhost:8092"
	@echo "    - Mobile (1.1.9999):    localhost:8095"
	@echo "  Realm 1, Cluster 2:"
	@echo "    - Node1 (1.2.2000):     localhost:8093"
	@echo "  Realm 2, Cluster 1:"
	@echo "    - Node1 (2.1.3000):     localhost:8094"
	@echo ""
	@echo "Test with: ./othernet_test_script.sh all"
	@echo "Connect to nodes:"
	@echo "  make connect-bootstrap"
	@echo "  make connect-node2"
	@echo "  make connect-mobile"

down-othernet:
	docker-compose -f othernet-compose.yml down

# Basic P2P network connections
logs:
	docker-compose logs -f

logs-node1:
	docker-compose logs -f node1

logs-node2:
	docker-compose logs -f node2

logs-node3:
	docker-compose logs -f node3

logs-node4:
	docker-compose logs -f node4

shell-node1:
	docker exec -it p2p_node1 /bin/bash

shell-node2:
	docker exec -it p2p_node2 /bin/bash

shell-node3:
	docker exec -it p2p_node3 /bin/bash

shell-node4:
	docker exec -it p2p_node4 /bin/bash

connect-node1:
	docker attach p2p_node1

connect-node2:
	docker attach p2p_node2

connect-node3:
	docker attach p2p_node3

connect-node4:
	docker attach p2p_node4

# Othernet network connections
logs-othernet:
	docker-compose -f othernet-compose.yml logs -f

logs-bootstrap:
	docker-compose -f othernet-compose.yml logs -f othernet-1-1-bootstrap

logs-node2-othernet:
	docker-compose -f othernet-compose.yml logs -f othernet-1-1-node2

logs-mobile:
	docker-compose -f othernet-compose.yml logs -f othernet-mobile

shell-bootstrap:
	docker exec -it othernet_1_1_bootstrap /bin/bash

shell-node2-othernet:
	docker exec -it othernet_1_1_node2 /bin/bash

shell-node3-othernet:
	docker exec -it othernet_1_1_node3 /bin/bash

shell-mobile:
	docker exec -it othernet_mobile /bin/bash

shell-realm2:
	docker exec -it othernet_2_1_node1 /bin/bash

connect-bootstrap:
	docker attach othernet_1_1_bootstrap

connect-node2-othernet:
	docker attach othernet_1_1_node2

connect-node3-othernet:
	docker attach othernet_1_1_node3

connect-mobile:
	docker attach othernet_mobile

connect-realm2:
	docker attach othernet_2_1_node1

# Testing commands
test:
	@echo "Testing basic P2P network connectivity..."
	@echo "Checking if all nodes are running:"
	@docker ps --filter "name=p2p_node" --format "table {{.Names}}\t{{.Status}}\t{{.Ports}}"
	@echo ""
	@echo "Testing network connectivity between nodes:"
	@docker exec p2p_node1 ping -c 2 172.20.0.11 || true
	@docker exec p2p_node2 ping -c 2 172.20.0.12 || true
	@docker exec p2p_node3 ping -c 2 172.20.0.13 || true

test-othernet:
	@echo "Testing Othernet network..."
	@if [ -f "./othernet_test_script.sh" ]; then \
		chmod +x ./othernet_test_script.sh && ./othernet_test_script.sh all; \
	else \
		echo "othernet_test_script.sh not found. Testing basic connectivity:"; \
		docker exec othernet_1_1_bootstrap ping -c 2 172.30.1.11 || true; \
		docker exec othernet_1_1_node2 ping -c 2 172.30.2.10 || true; \
	fi

# Status and monitoring
status:
	@echo "=== P2P Network Status ==="
	@docker ps --filter "name=p2p_node" --format "table {{.Names}}\t{{.Status}}\t{{.Ports}}"

status-othernet:
	@echo "=== Othernet Network Status ==="
	@docker ps --filter "name=othernet" --format "table {{.Names}}\t{{.Status}}\t{{.Ports}}"

# Cleanup commands
clean:
	docker-compose down -v
	docker system prune -f


clean-othernet:
	docker-compose -f othernet-compose.yml down -v
	docker system prune -f

clean-all: clean clean-othernet
	docker volume prune -f
	docker image prune -a -f

clean-bin:
	rm -rf ./test-bin/

build-bin:
	mkdir -p test-bin/
	gcc -o test-bin/p2p_node0 p2p_node.c -lpthread
	gcc -o test-bin/p2p_node1 p2p_node.c -lpthread
	gcc -o test-bin/p2p_node2 p2p_node.c -lpthread
	gcc -o test-bin/p2p_node3 p2p_node.c -lpthread
	gcc -o test-bin/p2p_node4 p2p_node.c -lpthread

up-bin:
	./test-bin/p2p_node0 127.0.0.1 59879 0 &
	./test-bin/p2p_node1 127.0.0.1 59888 1 &
	./test-bin/p2p_node2 127.0.0.1 59889 2 &
	./test-bin/p2p_node3 127.0.0.1 59890 3 &
	./test-bin/p2p_node4 127.0.0.1 59891 4 &

test-bin: clean-bin build-bin up-bin

# Build commands
rebuild: clean build up

rebuild-othernet: clean-othernet build-othernet up-othernet

rebuild-all: clean-all
	@echo "Building basic P2P network..."
	@make build
	@echo "Building Othernet network..."
	@make build-othernet
	@echo "Both networks built. Use 'make up' or 'make up-othernet' to start."

# Development helpers
dev-setup:
	@echo "Setting up development environment..."
	@echo "Creating log directories..."
	@mkdir -p logs
	@echo "Making scripts executable..."
	@chmod +x *.sh 2>/dev/null || true
	@echo "Development setup complete!"

# Help command
help:
	@echo "Available commands:"
	@echo ""
	@echo "Basic P2P Network:"
	@echo "  make build          - Build basic P2P Docker images"
	@echo "  make up             - Start basic P2P network"
	@echo "  make down           - Stop basic P2P network"
	@echo "  make test           - Test basic P2P connectivity"
	@echo "  make connect-nodeX  - Connect to specific basic node"
	@echo ""
	@echo "Enhanced Othernet Network:"
	@echo "  make build-othernet   - Build Othernet Docker images"
	@echo "  make up-othernet      - Start Othernet network"
	@echo "  make down-othernet    - Stop Othernet network"
	@echo "  make test-othernet    - Test Othernet functionality"
	@echo "  make connect-bootstrap - Connect to bootstrap node"
	@echo "  make connect-mobile   - Connect to mobile node"
	@echo ""
	@echo "Monitoring:"
	@echo "  make logs[-service]   - View logs"
	@echo "  make shell-[service]  - Open shell in container"
	@echo "  make status           - Show network status"
	@echo ""
	@echo "Cleanup:"
	@echo "  make clean            - Clean basic P2P network"
	@echo "  make clean-othernet   - Clean Othernet network"
	@echo "  make clean-all        - Clean everything"
	@echo ""
	@echo "Development:"
	@echo "  make dev-setup        - Set up development environment"
	@echo "  make rebuild[-othernet] - Rebuild from scratch"