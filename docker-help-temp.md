# Requirements

- install docker
- install docker-compose (if you have a newer version of docker installed you might have compose built into docker and might not need to install `docker-compose`. Try running `docker compose help` if that works, replace all usages of `docker-compose` in this readme with `docker compose`)

# Build image

```
docker-compose build musicat
# OR
docker compose build musicat
```

# Set env variables

```
export BOT_TOKEN=XXXXXXXXXXXXXX
export BOT_CLIENT_ID=1234567890
```

# Start

```
docker-compose up
# OR
docker compose up
```
