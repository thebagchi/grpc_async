
#include "framework/server.hh"
#include "services/samplesvc_impl.hh"
#include "services/examplesvc_impl.hh"
#include "services/anothersamplesvc_impl.hh"

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