FROM archlinux:base-devel

RUN pacman -Syu --noconfirm reflector && reflector --save /etc/pacman.d/mirrorlist

# Install dependencies
RUN pacman -S --needed --noconfirm git cmake libsodium opus libogg ffmpeg postgresql-libs

# Build dependencies
WORKDIR /root/Musicat

# Copy source files
COPY include ./include
COPY src ./src
COPY libs ./libs
COPY CMakeLists.txt ./

# Build Musicat
RUN mkdir -p exe build && cd build && \
      cmake .. -DMUSICAT_WITH_CORO=OFF -DMUSICAT_DEBUG_SYMBOL=ON && make all -j12 && mv Shasha ../exe

# !TODO: clean up? need to move libs first after compile

VOLUME ["/root/music"]

WORKDIR /root/Musicat/exe

CMD ./Shasha
