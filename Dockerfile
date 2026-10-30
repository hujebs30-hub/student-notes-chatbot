
FROM debian:bookworm-slim AS build

RUN apt-get update && apt-get install -y \
    g++ cmake ninja-build libasio-dev git \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY . .

RUN rm -rf crow
RUN git clone --depth 1 https://github.com/CrowCpp/Crow.git crow

RUN cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
RUN cmake --build build

FROM debian:bookworm-slim

WORKDIR /app

COPY --from=build /app/build/chatbot /app/chatbot
COPY --from=build /app/public /app/public

EXPOSE 10000

CMD ["/app/chatbot"]
