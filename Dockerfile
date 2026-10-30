FROM gcc:13

WORKDIR /usr/src/app

COPY . .

RUN make test && make demo && ./build/demo

CMD ["./build/demo"]
