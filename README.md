# UVG AWS Linux Server-Client TCP Chat


*Alejandro Martinez - 21430*

*Samuel Argueta - 211024*

```
sudo apt-get install protobuf-compiler libprotobuf-dev
protoc --experimental_allow_proto3_optional --cpp_out=. proyecto.proto
g++ server.cpp proyecto.pb.cc -o serverobj -lprotobuf -lpthread
g++ client.cpp proyecto.pb.cc -o clientobj -lprotobuf -lpthread
```
