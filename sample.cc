
#include "Framework/Server.hh"
#include "Services/SampleSvcImpl.hh"
#include "Services/ExampleSvcImpl.hh"
#include "Services/AnotherSampleSvcImpl.hh"

int main() {
  std::cout << "Hello World!!!" << std::endl;

  SampleSvcImpl sampleSvcHandler;
  ExampleSvcImpl exampleSvcHandler;
  AnotherSampleSvcHandler anotherSampleSvcHandler;
  Server server("0.0.0.0:12345");
  server.AddHandler(&sampleSvcHandler);
  server.AddHandler(&exampleSvcHandler);
  server.AddHandler(&anotherSampleSvcHandler);
  server.Start();
}