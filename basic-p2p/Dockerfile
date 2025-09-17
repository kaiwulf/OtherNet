FROM debian:12-slim

# Install build essentials
RUN apt-get update && apt-get install -y \
    gcc \
    make \
    libc6-dev \
    && rm -rf /var/lib/apt/lists/*

# Create working directory
WORKDIR /app

# Copy source code
COPY p2p_node.c .

# Compile the program
RUN gcc -o p2p_node p2p_node.c -lpthread

# Expose the default port
EXPOSE 59888

# Run the node
CMD ["./p2p_node"]