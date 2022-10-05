# Requirements

- install docker
- install docker-compose (if you have a newer version of docker installed you might have compose built into docker and might not need to install `docker-compose`. Try running `docker compose help` if that works, replace all usages of `docker-compose` in this readme with `docker compose`)

# Build image

```
docker-compose build debian
# OR
docker compose build debian
```

# Start a terminal inside the container to continue playing around with docker

```
docker-compose run --rm debian bash
# OR
docker compose run --rm debian bash
```

Try using `ls` inside the container to see the source files
