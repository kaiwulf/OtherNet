FROM debian:12-slim

# Install build essentials and tools
RUN apt-get update && apt-get install -y \
    gcc \
    make \
    libc6-dev \
    gdb \
    valgrind \
    net-tools \
    iputils-ping \
    telnet \
    && rm -rf /var/lib/apt/lists/*

# Create working directory
WORKDIR /app

# Create logs directory
RUN mkdir -p /app/logs

# Copy source code
COPY othernet_node.c .

# Compile with debug symbols
RUN gcc -g -o othernet_node othernet_node.c -lpthread -Wall -Wextra

# Create startup script that uses environment variables
RUN echo '#!/bin/bash\n\
if [ -n "$OTHERNET_REALM" ] && [ -n "$OTHERNET_CLUSTER" ] && [ -n "$OTHERNET_NODE_ID" ]; then\n\
    echo "Starting Othernet node: $OTHERNET_REALM.$OTHERNET_CLUSTER.$OTHERNET_NODE_ID"\n\
    echo "Capabilities: $OTHERNET_CAPABILITIES"\n\
    echo "Arguments: $@"\n\
fi\n\
exec ./othernet_node "$@"' > /app/start.sh && chmod +x /app/start.sh

# Expose the default port
EXPOSE 8080

# Use the startup script
ENTRYPOINT ["/app/start.sh"]